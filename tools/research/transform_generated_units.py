#!/usr/bin/env python3
"""Offline, fail-closed Clang-AST to typed-SSA transform of pinned AOT units.

The contract binds provenance/entry/field/ABI identities, never output operation
values. Retail values and effects come from the selected generated C++ AST.
Only the closed initializer subset is admitted. This is not a guest runtime.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import uuid


class Refusal(RuntimeError):
    pass


def require(value, reason):
    if not value:
        raise Refusal(reason)


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def children(node):
    return node.get("inner", [])


def walk(node):
    yield node
    for child in children(node):
        yield from walk(child)


def unwrap(node):
    while node.get("kind") in ("ImplicitCastExpr", "ParenExpr", "ConstantExpr"):
        if node.get("kind") == "ImplicitCastExpr":
            require(node.get("castKind") in ("LValueToRValue", "IntegralCast",
                    "FunctionToPointerDecay", "ArrayToPointerDecay", "IntegralToBoolean", "NoOp"),
                    "unsupported_implicit_cast:" + str(node.get("castKind")))
        require(len(children(node)) == 1, "malformed_scalar_wrapper")
        node = children(node)[0]
    return node


def literal(node):
    node = unwrap(node)
    require(node.get("kind") == "IntegerLiteral", "unbound_integer_literal")
    return int(node["value"])


def declaration(node):
    node = unwrap(node)
    require(node.get("kind") == "DeclRefExpr", "unbound_declaration_reference")
    return node["referencedDecl"]


def same_parameter(node, identifier):
    node = unwrap(node)
    return node.get("kind") == "DeclRefExpr" and node.get("referencedDecl", {}).get("id") == identifier


def member(node, name):
    node = unwrap(node)
    require(node.get("kind") == "MemberExpr" and node.get("name") == name
            and len(children(node)) == 1 and not node.get("isArrow", False),
            "unbound_member:" + name)
    return children(node)[0]


def register_object(node, cpu):
    node = unwrap(node)
    require(node.get("kind") == "ArraySubscriptExpr" and len(children(node)) == 2,
            "unbound_register_object")
    base, index = children(node)
    require(same_parameter(member(base, "r"), cpu), "unbound_register_owner")
    index = literal(index)
    require(0 <= index < 32, "unbound_register_index")
    return index


def register_part(node, cpu):
    node = unwrap(node)
    require(node.get("kind") == "ArraySubscriptExpr" and len(children(node)) == 2,
            "unbound_scalar_register_part")
    base, index = children(node)
    base = unwrap(base)
    require(base.get("kind") == "MemberExpr" and base.get("name") in ("w", "d")
            and literal(index) == 0 and len(children(base)) == 1,
            "unbound_scalar_register_width")
    return register_object(children(base)[0], cpu), 32 if base["name"] == "w" else 64


def parse_json_stream(data):
    text = data.decode("utf-8")
    decoder = json.JSONDecoder()
    result, offset = [], 0
    while offset < len(text):
        while offset < len(text) and text[offset].isspace():
            offset += 1
        if offset == len(text):
            break
        item, offset = decoder.raw_decode(text, offset)
        result.append(item)
    require(result, "empty_clang_ast")
    return result


def clang_ast(compiler, source, includes, filter_name, repo):
    argv = [str(compiler), "-std=c++20", "-fsyntax-only", "-Wno-unused-label",
            "-Wno-unused-variable"]
    for include in includes:
        argv += ["-I", str(include)]
    argv += ["-Xclang", "-ast-dump=json", "-Xclang",
             "-ast-dump-filter=" + filter_name, str(source)]
    env = dict(os.environ)
    # No compilation products or target process. Keep any compiler temporary
    # activity inside the repository's already-owned scratch directory.
    scratch = repo / "b/check-temp"
    require(scratch.is_dir(), "compiler_scratch_missing:b/check-temp")
    env["TMP"] = env["TEMP"] = str(scratch)
    kwargs = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
    try:
        run = subprocess.run(argv, cwd=repo, env=env, capture_output=True,
                             timeout=90, **kwargs)
    except subprocess.TimeoutExpired as error:
        raise Refusal("clang_ast_timeout") from error
    require(run.returncode == 0, "clang_ast_failed:" + run.stderr.decode(errors="replace")[-1200:])
    return parse_json_stream(run.stdout)


def function_definition(roots, name):
    found = [n for root in roots for n in walk(root) if n.get("kind") == "FunctionDecl"
             and n.get("name") == name and any(x.get("kind") == "CompoundStmt" for x in children(n))]
    require(len(found) == 1, "ambiguous_or_missing_function:" + name)
    return found[0]


class Lowering:
    """Selected-CFG semantic lowering. No name-to-retail-body templates."""
    def __init__(self, function, descriptor, contract):
        self.function, self.descriptor, self.contract = function, descriptor, contract
        parameters = [x for x in children(function) if x.get("kind") == "ParmVarDecl"]
        require(len(parameters) == 3 and parameters[0]["type"]["qualType"] == "Cpu &",
                "unsupported_generated_function_abi")
        self.cpu, self.entry, self.depth = [x["id"] for x in parameters]
        self.values, self.effects, self.regs = [], [], {}
        self.return_local = None
        self.return_guard = self.cursor_return = self.returned = False
        self.pc, self.debit = int(descriptor["source_entry"], 16), 0

    def expression(self, node):
        node = unwrap(node)
        kind = node.get("kind")
        if kind == "IntegerLiteral":
            return {"op": "constant", "type": "u32", "value": int(node["value"]) & 0xffffffff}
        if kind == "BinaryOperator" and node.get("opcode") in ("+", "-"):
            require(len(children(node)) == 2, "malformed_integer_operation")
            def unsigned32(expression):
                ty = expression.get("type", {})
                return ty.get("desugaredQualType", ty.get("qualType")) == "unsigned int"
            require(unsigned32(node) and all(unsigned32(x) for x in children(node)),
                    "unsupported_integer_operation_width_or_signedness")
            return {"op": "add32" if node["opcode"] == "+" else "subtract32", "type": "u32",
                    "arguments": [self.expression(x) for x in children(node)]}
        if kind == "ArraySubscriptExpr":
            index, width = register_part(node, self.cpu)
            if index == 0:
                return {"op": "constant", "type": "u32" if width == 32 else "u64", "value": 0}
            require(index in self.regs, "unbound_value:register_" + str(index))
            return {"op": "value" if width == 64 else "truncate32", "type": "u64" if width == 64 else "u32",
                    "value": self.regs[index]}
        raise Refusal("unsupported_value_ast:" + str(kind))

    def evaluate(self, expression):
        op = expression["op"]
        if op == "constant":
            return expression["value"]
        if op in ("value", "truncate32"):
            value = self.values[expression["value"]]["constant_value"]
            return value if op == "value" else value & 0xffffffff
        if op == "sign_extend32":
            word = self.evaluate(expression["argument"]) & 0xffffffff
            return (word if word < 0x80000000 else word - 0x100000000) & 0xffffffffffffffff
        a, b = [self.evaluate(x) for x in expression["arguments"]]
        return (a + b if op == "add32" else a - b) & 0xffffffff

    def helper(self, call):
        require(call.get("kind") == "CallExpr" and children(call), "unbound_indirect_target")
        ref = declaration(children(call)[0])
        require(ref.get("kind") == "FunctionDecl", "unbound_indirect_target")
        name = ref.get("name")
        expected = self.contract["helper_contracts"].get(name)
        require(expected is not None, "unbound_call:" + str(name))
        require(ref.get("type", {}).get("qualType") == expected["type"], "unbound_call_signature:" + name)
        return name, children(call)[1:]

    def address(self, node):
        node = unwrap(node)
        require(node.get("kind") == "BinaryOperator" and node.get("opcode") in ("+", "-")
                and len(children(node)) == 2, "unbound_memory:address_expression")
        base, offset = children(node)
        if self.contract.get("schema") == "generated_relative_receiver_contract_v1":
            require(register_part(base, self.cpu) == (self.contract["receiver"]["index"], 32),
                    "unbound_memory:relative_receiver_base")
            displacement = literal(offset) * (1 if node["opcode"] == "+" else -1)
            fields = [f for f in self.contract["fields"] if f["source_offset"] == displacement]
            require(len(fields) == 1 and fields[0]["type"] == "i32", "unbound_memory:relative_field_or_width")
            return fields[0]
        require(register_part(base, self.cpu) == (28, 32), "unbound_memory:non_GP_base")
        displacement = literal(offset)
        address = (int(self.contract["source_gp"], 16) +
                   (displacement if node["opcode"] == "+" else -displacement)) & 0xffffffff
        fields = [f for f in self.contract["fields"] if int(f["source_address"], 16) == address]
        require(len(fields) == 1 and fields[0]["type"] == "u32", "unbound_memory:GP_field_or_width")
        return fields[0]

    def statement(self, node):
        kind = node.get("kind")
        if kind == "LabelStmt":
            name = node.get("name", "")
            require(name.startswith("L_") and len(name) == 10, "unsupported_control_label")
            self.pc = int(name[2:], 16)
            require(len(children(node)) == 1, "malformed_label")
            self.statement(children(node)[0])
        elif kind == "CompoundStmt":
            for child in children(node):
                require(not self.returned, "effect_after_native_return")
                self.statement(child)
        elif kind == "CompoundAssignOperator":
            require(node.get("opcode") == "-=" and len(children(node)) == 2
                    and same_parameter(member(children(node)[0], "budget"), self.cpu),
                    "unbound_owner_effect:compound_assignment")
            self.debit += literal(children(node)[1])
        elif kind == "CallExpr":
            name, args = self.helper(node)
            if name == "set32":
                require(len(args) == 2, "malformed_set32")
                index = register_object(args[0], self.cpu)
                require(index not in (0, 28, 31), "unbound_abi_register_write")
                expression = {"op": "sign_extend32", "type": "u64", "argument": self.expression(args[1])}
                value = {"id": len(self.values), "type": "u64", "expression": expression,
                         "constant_value": self.evaluate(expression), "source_pc": f"{self.pc:08x}"}
                self.values.append(value)
                self.regs[index] = value["id"]
                self.effects.append({"kind": "define", "value": value["id"], "source_pc": f"{self.pc:08x}"})
            elif name == "st32":
                require(len(args) == 2, "malformed_st32")
                field = self.address(args[0])
                expression = self.expression(args[1])
                self.effects.append({"kind": "store_field", "field": field["name"], "type": field["type"],
                                     "expression": expression,
                                     "source_pc": f"{self.pc + (4 if self.return_local else 0):08x}"})
            else:
                raise Refusal("unbound_call_context:" + name)
        elif kind == "DeclStmt":
            require(len(children(node)) == 1, "unsupported_local_declaration")
            var = children(node)[0]
            require(var.get("kind") == "VarDecl" and len(children(var)) == 1
                    and self.return_local is None
                    and register_part(children(var)[0], self.cpu) == (31, 32),
                    "unbound_indirect_target:return_capture")
            self.return_local = var["id"]
        elif kind == "IfStmt":
            require(len(children(node)) == 2 and self.return_local, "unsupported_conditional_control")
            condition, stop = children(node)
            condition = unwrap(condition)
            require(condition.get("kind") == "BinaryOperator" and condition.get("opcode") == "&"
                    and len(children(condition)) == 2
                    and declaration(children(condition)[0])["id"] == self.return_local
                    and literal(children(condition)[1]) == self.contract["return_abi"]["alignment"] - 1,
                    "unbound_indirect_target:alignment_guard")
            name, args = self.helper(stop)
            require(name == "stop" and len(args) == 3 and same_parameter(args[0], self.cpu)
                    and declaration(args[1])["id"] == self.return_local
                    and unwrap(args[2]).get("kind") == "StringLiteral", "unbound_alignment_failure")
            self.return_guard = True
        elif kind == "BinaryOperator" and node.get("opcode") == "=":
            require(len(children(node)) == 2 and self.return_local
                    and same_parameter(member(children(node)[0], "pc"), self.cpu)
                    and declaration(children(node)[1])["id"] == self.return_local,
                    "unbound_indirect_target:cursor_assignment")
            self.cursor_return = True
        elif kind == "ReturnStmt":
            require(not children(node) and self.return_guard and self.cursor_return,
                    "unbound_indirect_target:return")
            output = self.contract["liveout"]
            require(output["index"] in self.regs, "unbound_abi_liveout")
            require(any(x["kind"] == "store_field" for x in self.effects), "unbound_owner_effect:no_store")
            self.effects.append({"kind": "return_value", "value": self.regs[output["index"]]})
            self.returned = True
        else:
            raise Refusal("unsupported_statement_ast:" + str(kind))

    def lower(self):
        body = next(x for x in children(self.function) if x.get("kind") == "CompoundStmt")
        dispatch = next((x for x in children(body) if x.get("kind") == "LabelStmt"
                         and x.get("name") == "dispatch_entry"), None)
        require(dispatch is not None and len(children(dispatch)) == 1
                and children(dispatch)[0].get("kind") == "SwitchStmt", "unbound_entry_dispatch")
        switch = children(dispatch)[0]
        require(same_parameter(children(switch)[0], self.entry), "unbound_entry_selector")
        cases = [x for x in walk(switch) if x.get("kind") == "CaseStmt"
                 and literal(children(x)[0]) == self.descriptor["entry_case"]]
        require(len(cases) == 1 and len(children(cases[0])) == 2
                and children(cases[0])[1].get("kind") == "GotoStmt", "unbound_entry_case")
        target = children(cases[0])[1]["targetLabelDeclId"]
        labels = [(i, x) for i, x in enumerate(children(body)) if x.get("kind") == "LabelStmt"
                  and x.get("declId") == target]
        require(len(labels) == 1 and labels[0][1].get("name") == "L_" + self.descriptor["source_entry"].upper(),
                "unbound_entry_label")
        for node in children(body)[labels[0][0]:]:
            self.statement(node)
            if self.returned:
                break
        require(self.returned, "unclosed_selected_CFG")
        return {"values": self.values, "effects": self.effects, "host_budget_debit": self.debit,
                "return": "structured_native_return_after_delay_effect"}


def emit(functions, relative=False):
    owner = "IndexSelection" if relative else "OverlaySelection"
    field = "std::int32_t index" if relative else "std::uint32_t selected_mode"
    filename = "native_index_selection.hpp" if relative else "native_overlay_initializers.hpp"
    header = ("#pragma once\n#include <cstdint>\n\nnamespace mka::retail::generated {\n"
              f"struct {owner} {{ {field}{{}}; }};\n")
    source = f'#include "{filename}"\n\nnamespace mka::retail::generated {{\n'
    for desc, ir in functions:
        signature = "std::uint64_t " + desc["native_name"] + f"({owner}& selection) noexcept"
        header += signature + ";\n"
        source += signature + " {\n"
        values = ir["values"]
        for effect in ir["effects"]:
            if effect["kind"] == "define":
                value = values[effect["value"]]
                source += f'    const std::uint64_t value_{value["id"]} = {value["constant_value"]}ULL;\n'
            elif effect["kind"] == "store_field":
                expression = effect["expression"]
                require(expression["op"] in ("truncate32", "value"), "unbound_native_store_value")
                width = "std::int32_t" if relative else "std::uint32_t"
                source += (f'    selection.{effect["field"]} = static_cast<{width}>'
                           f'(value_{expression["value"]});\n')
            elif effect["kind"] == "return_value":
                source += f'    return value_{effect["value"]};\n'
            else:
                raise Refusal("unsupported_IR_effect")
        source += "}\n"
    return header + "}\n", source + "}\n"


def native_audit(compiler, source, repo, relative=False):
    """Structural allowlist; no expected output hash or memory-helper names."""
    roots = clang_ast(compiler, source, [source.parent], "", repo)
    # Audit the whole user translation unit, including declarations outside the
    # exported namespace. Compiler SDK declarations are not application effects.
    sdk = compiler.parent.parent.resolve()
    def owned_nodes(node, inherited=None):
        # Statements (notably asm) may have only a range, and macro-generated
        # declarations put the real application location in expansionLoc.
        # Prefer the expansion over spelling so a system-header macro cannot
        # hide an application effect. Preserve SDK ownership for SDK bodies.
        location = node.get("loc") or node.get("range", {}).get("begin", {})
        while "expansionLoc" in location:
            location = location["expansionLoc"]
        origin = location.get("file") or location.get("includedFrom", {}).get("file")
        owned = inherited
        if origin:
            owned = not Path(origin).resolve().is_relative_to(sdk)
        elif "offset" in location and inherited is None:
            owned = True
        if owned:
            yield node
        for child in children(node):
            yield from owned_nodes(child, owned)
    nodes = [n for root in roots for n in owned_nodes(root)]
    require(not any(n.get("kind") in ("FileScopeAsmDecl", "GCCAsmStmt", "MSAsmStmt")
                    for n in nodes), "native_assembly_structure")
    require(not any(n.get("kind") in ("FunctionTemplateDecl", "ClassTemplateDecl",
            "ClassTemplateSpecializationDecl") for n in nodes), "native_template_definition")
    records = [n for n in nodes if n.get("kind") == "CXXRecordDecl" and n.get("completeDefinition")]
    owner_type = "IndexSelection" if relative else "OverlaySelection"
    field_name = "index" if relative else "selected_mode"
    field_type = "int" if relative else "unsigned int"
    function_names = {"invalidate_index"} if relative else {"initialize_gamer", "initialize_movier"}
    require(len(records) == 1 and records[0].get("name") == owner_type, "native_unbound_owner_type")
    record = records[0]
    require(not record.get("bases") and record.get("tagUsed") == "struct"
            and record.get("definitionData", {}).get("isAggregate")
            and record.get("definitionData", {}).get("isStandardLayout")
            and record.get("definitionData", {}).get("isTriviallyCopyable"),
            "native_nonordinary_owner_or_base")
    fields = [x for x in children(record) if x.get("kind") == "FieldDecl"]
    require(len(fields) == 1 and fields[0].get("name") == field_name
            and fields[0].get("type", {}).get("desugaredQualType") == field_type,
            "native_unbound_field_or_width")
    require(not fields[0].get("isMutable", False) and not fields[0].get("mutable", False),
            "native_mutable_field")
    field_init = children(fields[0])
    require(len(field_init) == 1 and field_init[0].get("kind") == "InitListExpr"
            and not children(field_init[0]), "native_nonzero_or_attributed_field")
    for child in children(record):
        require(child.get("kind") in ("CXXRecordDecl", "FieldDecl"), "native_owner_method_or_vtable")
    field_id = fields[0]["id"]
    definitions = [n for n in nodes if n.get("kind") == "FunctionDecl"
                   and any(x.get("kind") == "CompoundStmt" for x in children(n))]
    require({x.get("name") for x in definitions} == function_names
            and len(definitions) == len(function_names), "native_unbound_function_or_helper")
    # Global/static storage, hidden declarations and arbitrary helper bodies are
    # rejected even if the exported functions never mention a banned token.
    body_ids = {id(x) for fn in definitions for x in walk(fn)}
    for node in nodes:
        if node.get("kind") == "VarDecl" and id(node) not in body_ids:
            raise Refusal("native_global_or_static_storage")
    for fn in definitions:
        parameters = [x for x in children(fn) if x.get("kind") == "ParmVarDecl"]
        require(len(parameters) == 1 and parameters[0]["type"]["qualType"] == owner_type + " &"
                and fn["type"]["qualType"] in (
                    "std::uint64_t (" + owner_type + " &) noexcept",
                    "unsigned long long (" + owner_type + " &) noexcept")
                and not fn.get("variadic", False)
                and all(x.get("kind") in ("ParmVarDecl", "CompoundStmt") for x in children(fn)),
                "native_unbound_function_abi_or_attribute")
        owner = parameters[0]["id"]
        locals_seen, stores, returns = set(), 0, 0
        body = next(x for x in children(fn) if x.get("kind") == "CompoundStmt")
        def scalar(node):
            node = unwrap(node)
            if node.get("kind") == "IntegerLiteral":
                require(0 <= int(node["value"]) <= 0xffffffffffffffff, "native_scalar_width")
                return
            if node.get("kind") == "DeclRefExpr":
                require(node.get("referencedDecl", {}).get("id") in locals_seen, "native_unbound_scalar")
                return
            if node.get("kind") == "CXXStaticCastExpr":
                require(node.get("castKind") in ("IntegralCast", "NoOp") and len(children(node)) == 1
                        and node.get("type", {}).get("desugaredQualType") in (("int", "unsigned int", "unsigned long long") if relative else ("unsigned int", "unsigned long long")),
                        "native_pointer_or_unknown_cast")
                scalar(children(node)[0]); return
            raise Refusal("native_unbound_scalar_structure:" + str(node.get("kind")))
        for statement in children(body):
            kind = statement.get("kind")
            if kind == "DeclStmt":
                require(len(children(statement)) == 1, "native_unbound_local")
                var = children(statement)[0]
                require(var.get("kind") == "VarDecl" and var.get("storageClass") is None
                        and var.get("type", {}).get("desugaredQualType") == "const unsigned long long"
                        and len(children(var)) == 1, "native_non_scalar_or_static_local")
                scalar(children(var)[0]); locals_seen.add(var["id"])
            elif kind == "BinaryOperator" and statement.get("opcode") == "=":
                require(len(children(statement)) == 2, "native_unbound_store")
                lhs, rhs = children(statement); lhs = unwrap(lhs)
                require(lhs.get("kind") == "MemberExpr" and lhs.get("referencedMemberDecl") == field_id
                        and len(children(lhs)) == 1 and not lhs.get("isArrow", False)
                        and same_parameter(children(lhs)[0], owner), "native_arbitrary_memory_store")
                scalar(rhs); stores += 1
            elif kind == "ReturnStmt":
                require(len(children(statement)) == 1, "native_unbound_return")
                scalar(children(statement)[0]); returns += 1
            else:
                raise Refusal("native_unbound_statement_structure:" + str(kind))
        require(stores == 1 and returns == 1 and children(body)[-1].get("kind") == "ReturnStmt",
                "native_owner_effect_or_return_count")
        for node in walk(body):
            require(node.get("kind") not in ("CallExpr", "CXXMemberCallExpr", "ArraySubscriptExpr",
                    "CXXReinterpretCastExpr", "CStyleCastExpr", "UnaryOperator", "LambdaExpr"),
                    "native_call_pointer_or_address_structure:" + str(node.get("kind")))
    return {"status": "PASS", "scope": "closed typed owner/" + ("i32" if relative else "u32") + " field/u64 scalar effects; no helper calls, pointers or array addressing"}


def verify_inputs(repo, generated, contract):
    relative = contract.get("schema") == "generated_relative_receiver_contract_v1"
    require(relative or contract.get("schema") == "generated_overlay_initializer_contract_v1", "unsupported_contract_schema")
    if relative:
        require(len(contract["functions"]) == 1 and contract["functions"][0]["source_entry"] == "0014DE80"
                and contract["functions"][0]["native_name"] == "invalidate_index", "unbound_relative_function_contract")
        require(contract["receiver"]["bank"] == "r" and contract["receiver"]["index"] == 4
                and contract["receiver"]["portion"] == "low32" and contract["receiver"]["owner_type"] == "IndexSelection"
                and contract["receiver"]["saved_state_source"] == {"gp_offset": -24880, "pointer_width": 32}
                and contract["fields"] == [{"source_offset": 3240, "owner_type": "IndexSelection", "name": "index", "type": "i32"}],
                "unbound_relative_receiver_contract")
    require(contract["liveout"] == {"bank": "r", "index": 3, "portion": "low64", "native_type": "u64",
            "scope": "Explicit V1 low64 clobber projection, not V0/C ABI boolean return"}, "unsupported_liveout_contract")
    for name, expected in contract["header_pins"].items():
        require(digest(repo / name) == expected, "source_helper_identity:" + name)
    for name, expected in contract["generated_header_pins"].items():
        require(digest(generated / name) == expected, "generated_header_identity:" + name)
    caller = contract["original_caller"]
    elf_path = (repo / caller["elf_path"]).resolve()
    require(digest(elf_path) == caller["elf_sha256"], "original_caller_identity")
    elf = elf_path.read_bytes()
    phoff = struct.unpack_from("<I", elf, 28)[0]
    phsize, count = struct.unpack_from("<HH", elf, 42)
    for row in caller["words"]:
        address = int(row["address"], 16)
        matches = []
        for i in range(count):
            kind, offset, base, _, extent, *_ = struct.unpack_from("<8I", elf, phoff + i * phsize)
            if kind == 1 and base <= address and address + 4 <= base + extent:
                matches.append(struct.unpack_from("<I", elf, offset + address - base)[0])
        require(matches == [int(row["word"], 16)], "original_caller_word:" + row["address"])
    for fn in contract["functions"]:
        require(digest(generated / fn["generated_unit"]) == fn["source_unit_sha256"], "generated_unit_identity:" + fn["module"])
        if relative:
            entry = int(fn["source_entry"], 16)
            spans = []
            for i in range(count):
                kind, offset, base, _, extent, *_ = struct.unpack_from("<8I", elf, phoff + i * phsize)
                if kind == 1 and base <= entry and entry + 12 <= base + extent:
                    spans.append(elf[offset + entry-base:offset + entry-base+12])
            require(len(spans) == 1 and hashlib.sha256(spans[0]).hexdigest() == fn["original_body_sha256"]
                    and list(struct.unpack("<3I", spans[0])) == [int(w, 16) for w in fn["original_words"]],
                    "original_relative_body_contract")
            continue
        module = (repo / fn["module_path"]).resolve()
        require(digest(module) == fn["module_sha256"] and module.stat().st_size == fn["module_size"], "original_module_identity")
        data = module.read_bytes()
        magic, _, base, _, _, _, begin, end = struct.unpack_from("<8I", data)
        require(magic == 0x336f574d and base == int(fn["load_base"], 16)
                and begin == int(fn["initializer_table_begin"], 16)
                and end == int(fn["initializer_table_end"], 16) and end - begin == 4,
                "original_initializer_table_contract")
        entry = int(fn["source_entry"], 16)
        require(struct.unpack_from("<I", data, begin - base)[0] == entry, "original_initializer_target")
        require(list(struct.unpack_from("<3I", data, entry - base)) == [int(w, 16) for w in fn["initializer_words"]],
                "original_initializer_words")


def publish_new_or_identical(path, data):
    raw = data.encode() if isinstance(data, str) else data
    if path.exists():
        require(path.read_bytes() == raw, "existing_output_differs:" + path.name)
    else:
        with path.open("xb") as stream:
            stream.write(raw)


def self_tests(functions, definitions, contract, compiler, out_dir, repo):
    passed = []
    def refuses(name, callback):
        try:
            callback()
        except Refusal as error:
            passed.append({"name": name, "result": "EXPECTED_REFUSAL", "reason": str(error)})
            return
        raise Refusal("self_test_false_success:" + name)
    desc = contract["functions"][0]
    original = definitions[0]
    mutated = copy.deepcopy(original)
    call = next(n for n in walk(mutated) if n.get("kind") == "CallExpr"
                and declaration(children(n)[0]).get("name") == "set32")
    constants = [n for n in walk(children(call)[2]) if n.get("kind") == "IntegerLiteral"]
    constants[-1]["value"] = "7"
    changed = Lowering(mutated, desc, contract).lower()
    require(changed["values"][-1]["constant_value"] == 7, "self_test_constant_not_AST_derived")
    passed.append({"name": "actual_AST_constant_mutation", "result": "DERIVED_VALUE_CHANGED"})
    for ty in ("unsigned long long", "int"):
        mutated = copy.deepcopy(original)
        call = next(n for n in walk(mutated) if n.get("kind") == "CallExpr"
                    and declaration(children(n)[0]).get("name") == "set32")
        operation = next(n for n in walk(children(call)[2]) if n.get("kind") == "BinaryOperator")
        operation["type"] = {"qualType": ty}
        for operand in children(operation):
            operand["type"] = {"qualType": ty}
        refuses("arithmetic_width_" + ty.replace(" ", "_"),
                lambda mutated=mutated: Lowering(mutated, desc, contract).lower())
    for helper in ("st32", "set32"):
        mutated = copy.deepcopy(original)
        call = next(n for n in walk(mutated) if n.get("kind") == "CallExpr"
                    and declaration(children(n)[0]).get("name") == helper)
        declaration(children(call)[0])["name"] = "innocuous_accessor"
        refuses("renamed_" + helper, lambda: Lowering(mutated, desc, contract).lower())
    mutated = copy.deepcopy(original)
    store = next(n for n in walk(mutated) if n.get("kind") == "CallExpr"
                 and declaration(children(n)[0]).get("name") == "st32")
    address_literals = [n for n in walk(children(store)[1]) if n.get("kind") == "IntegerLiteral"]
    address_literals[-1]["value"] = str(int(address_literals[-1]["value"]) + 4)
    refuses("unknown_GP_field", lambda: Lowering(mutated, desc, contract).lower())
    _, positive_source = emit(functions)
    plants = {
        "renamed_memory_helper": ("static unsigned char storage[33554432];\n"
            "static void renamed_write(unsigned x,unsigned v) { storage[x]=static_cast<unsigned char>(v); }\n",
            "    renamed_write(17, 1);\n"),
        "renamed_array_addressing": ("", "    static unsigned harmless[64]; harmless[selection.selected_mode & 63u]=1;\n"),
        "pointer_address_cast": ("", "    auto pointer = reinterpret_cast<unsigned*>(&selection); pointer[0]=1;\n"),
        "outside_namespace_global": ("", ""),
        "owner_base": ("", ""),
        "function_wrong_return_width": ("", ""),
        "global_assembly": ("", ""),
        "macro_global_assembly": ("", ""),
        "function_assembly": ("", '    asm volatile("nop");\n'),
    }
    for name, (declarations, effect) in plants.items():
        plant = positive_source.replace("namespace mka::retail::generated {\n",
                "namespace mka::retail::generated {\n" + declarations, 1)
        plant = plant.replace(" noexcept {\n", " noexcept {\n" + effect, 1)
        if name == "outside_namespace_global":
            plant += "\nstatic unsigned char unrelated_storage[33554432];\nstatic void innocuous() {}\n"
        elif name == "owner_base":
            plant = plant.replace('#include "native_overlay_initializers.hpp"',
                '#include <cstdint>\nnamespace mka::retail::generated { struct Base { unsigned x; }; '
                'struct OverlaySelection : Base { std::uint32_t selected_mode{}; }; }')
        elif name == "function_wrong_return_width":
            # Change declarations and definitions together so semantic AST audit,
            # not an unrelated redeclaration error, must reject the ABI.
            plant = plant.replace('#include "native_overlay_initializers.hpp"',
                '#include <cstdint>\nnamespace mka::retail::generated { '
                'struct OverlaySelection { std::uint32_t selected_mode{}; }; }')
            plant = plant.replace("std::uint64_t initialize_", "std::uint32_t initialize_")
        elif name == "global_assembly":
            plant += '\nasm("phase5_hidden_storage: .byte 0");\n'
        elif name == "macro_global_assembly":
            plant += ('\n#define PHASE5_HIDDEN_ASSEMBLY asm("phase5_macro_storage: .byte 0");\n'
                      'PHASE5_HIDDEN_ASSEMBLY\n')
        path = out_dir / ("planted_" + name + ".cpp")
        publish_new_or_identical(path, plant)
        refuses(name, lambda path=path: native_audit(compiler, path, repo))
    require(functions[0][1]["values"][-1]["constant_value"] != functions[1][1]["values"][-1]["constant_value"],
            "self_test_second_actual_module_not_distinct")
    passed.append({"name": "two_actual_modules_distinct_AST_dataflow", "result": "PASS"})
    return {"status": "PASS", "cases": passed}


def publish_outputs(out_dir, candidate, replace, names=None):
    names = names or ("native_overlay_initializers.hpp", "native_overlay_initializers.cpp", "transform_proof.json")
    for name in names:
        path = out_dir / name
        if path.exists() and path.read_bytes() != (candidate / name).read_bytes():
            require(replace, "existing_output_differs:" + name)
    if replace:
        for name in names:
            path = out_dir / name
            if path.exists():
                history = out_dir / "history" / digest(path)
                history.mkdir(parents=True, exist_ok=True)
                publish_new_or_identical(history / name, path.read_bytes())
        # Each file replacement is atomic. All input, AST, self-test and native
        # audit admission has already completed; prior bytes remain hash-pinned.
        for name in names:
            os.replace(candidate / name, out_dir / name)
    else:
        for name in names:
            publish_new_or_identical(out_dir / name, (candidate / name).read_bytes())


def relative_self_tests(functions, definitions, contract, compiler, out_dir, repo):
    passed = []
    def refuses(name, callback):
        try:
            callback()
        except Refusal as error:
            passed.append({"name": name, "result": "EXPECTED_REFUSAL", "reason": str(error)})
            return
        raise Refusal("self_test_false_success:" + name)
    desc, original = contract["functions"][0], definitions[0]
    def selected_call(definition, helper):
        pc = int(desc["source_entry"], 16) + (4 if helper == "st32" else 0)
        body = next(n for n in children(definition) if n.get("kind") == "CompoundStmt")
        begin = next(i for i, n in enumerate(children(body)) if n.get("kind") == "LabelStmt" and n.get("name") == f"L_{pc:08X}")
        for i, statement in enumerate(children(body)[begin:]):
            if i and statement.get("kind") == "LabelStmt":
                break
            for node in walk(statement):
                if node.get("kind") == "CallExpr" and declaration(children(node)[0]).get("name") == helper:
                    return node
        raise Refusal("relative_self_test_selected_call_missing:" + helper)
    changed = copy.deepcopy(original)
    call = selected_call(changed, "set32")
    literal_node = [n for n in walk(children(call)[2]) if n.get("kind") == "IntegerLiteral"][-1]
    literal_node["value"] = "2"
    ir = Lowering(changed, desc, contract).lower()
    require(ir["values"][-1]["constant_value"] == 0xfffffffffffffffe, "relative_AST_value_not_derived")
    passed.append({"name": "actual_AST_negative_constant_mutation", "result": "DERIVED_VALUE_CHANGED"})
    for name, change in (("wrong_relative_field", "offset"), ("wrong_receiver_register", "base"), ("renamed_store", "helper")):
        changed = copy.deepcopy(original)
        call = selected_call(changed, "st32")
        if change == "offset":
            node = [n for n in walk(children(call)[1]) if n.get("kind") == "IntegerLiteral"][-1]
            node["value"] = str(int(node["value"]) + 4)
        elif change == "base":
            node = next(n for n in walk(children(call)[1]) if n.get("kind") == "IntegerLiteral" and n.get("value") == "4")
            node["value"] = "5"
        else:
            declaration(children(call)[0])["name"] = "innocuous_accessor"
        refuses(name, lambda changed=changed: Lowering(changed, desc, contract).lower())
    _, source = emit(functions, True)
    plants = {
        "renamed_flat_memory_disguise": ('static unsigned char store[33554432];\nstatic void innocuous(unsigned v){store[v]=1;}\n', '    innocuous(17);\n'),
        "pointer_alias_disguise": ('', '    auto q = reinterpret_cast<unsigned*>(&selection); q[0]=1;\n'),
        "array_field_disguise": ('', '    static unsigned words[16]; words[0]=1;\n'),
    }
    for name, (declarations, effect) in plants.items():
        text = source.replace('namespace mka::retail::generated {\n', 'namespace mka::retail::generated {\n' + declarations, 1)
        text = text.replace(' noexcept {\n', ' noexcept {\n' + effect, 1)
        path = out_dir / ('planted_' + name + '.cpp')
        publish_new_or_identical(path, text)
        refuses(name, lambda path=path: native_audit(compiler, path, repo, True))
    return {"status": "PASS", "cases": passed}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--generated-dir", type=Path)
    parser.add_argument("--contract", type=Path)
    parser.add_argument("--out-dir", type=Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--replace-generated", action="store_true")
    parser.add_argument("--audit-native", type=Path)
    parser.add_argument("--audit-index-native", type=Path)
    args = parser.parse_args(argv)
    repo = args.repo.resolve()
    compiler = (args.compiler or repo.parents[3] / "dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin/clang++.exe").resolve()
    require(compiler.is_file(), "clang_compiler_missing")
    if args.audit_native or args.audit_index_native:
        relative = bool(args.audit_index_native)
        source = (args.audit_index_native or args.audit_native).resolve()
        if source.is_dir():
            source /= "native_index_selection.cpp" if relative else "native_overlay_initializers.cpp"
        result = native_audit(compiler, source, repo, relative)
        print(json.dumps({"schema": "native_overlay_ast_audit_v1", **result, "source_sha256": digest(source)}))
        return 0
    require(args.out_dir is not None, "out_dir_required")
    generated = (args.generated_dir or repo / "build/generated-aot/phase4-all").resolve()
    contract_path = (args.contract or repo / "recomp/overlay_initializer_contract.json").resolve()
    contract = json.loads(contract_path.read_text())
    verify_inputs(repo, generated, contract)
    relative = contract["schema"] == "generated_relative_receiver_contract_v1"
    functions, definitions = [], []
    for descriptor in contract["functions"]:
        roots = clang_ast(compiler, generated / descriptor["generated_unit"],
                          [generated, repo / "runtime/include"], descriptor["unit_name"], repo)
        definition = function_definition(roots, descriptor["unit_name"])
        definitions.append(definition)
        functions.append((descriptor, Lowering(definition, descriptor, contract).lower()))
    header, source = emit(functions, relative)
    out_dir = args.out_dir.resolve()
    require(out_dir != generated and not generated.is_relative_to(out_dir)
            and not out_dir.is_relative_to(generated), "output_overlaps_AOT_inputs")
    if args.replace_generated:
        require(any(out_dir != repo / n and out_dir.is_relative_to(repo / n) for n in ("b", "build"))
                and not any("telemetry" in p.lower() for p in out_dir.relative_to(repo).parts),
                "replace_output_not_owned_generated_directory")
    out_dir.mkdir(parents=True, exist_ok=True)
    candidate = out_dir / ("candidate-" + uuid.uuid4().hex)
    candidate.mkdir()
    stem = "native_index_selection" if relative else "native_overlay_initializers"
    proof_name = "index_transform_proof.json" if relative else "transform_proof.json"
    publish_new_or_identical(candidate / (stem + ".hpp"), header)
    publish_new_or_identical(candidate / (stem + ".cpp"), source)
    audit = native_audit(compiler, candidate / (stem + ".cpp"), repo, relative)
    tester = relative_self_tests if relative else self_tests
    tests = tester(functions, definitions, contract, compiler, candidate, repo) if args.self_test else {"status": "NOT_REQUESTED"}
    proof = {
        "schema": "actual_generated_cpp_relative_receiver_transform_v1" if relative else "actual_generated_cpp_overlay_transform_v1", "status": "PASS",
        "scope": "One generated signed-index leaf to typed receiver field; prototype only, no production caller adoption" if relative else "Two actual generated initializer CFGs to typed native field/SSA; no native-game integration or whole-game acceptance",
        "tool_sha256": digest(__file__), "contract_sha256": digest(contract_path),
        "compiler_sha256": digest(compiler),
        "functions": [{"module": d["module"], "source_entry": d["source_entry"].lower(),
                       "native_name": d["native_name"], "source_unit_sha256": d["source_unit_sha256"],
                       "original_module_sha256": d.get("module_sha256", contract["original_caller"]["elf_sha256"]), "selected_entry_case": d["entry_case"],
                       "typed_ir": ir} for d, ir in functions],
        "outputs": {n: digest(candidate / n) for n in (stem + ".hpp", stem + ".cpp")},
        "AST_admission": {"status": "PASS", "input": "Clang semantic JSON AST of actual pinned generated units",
                          "unknown_memory": "unbound_memory", "unknown_calls": "unbound_call",
                          "struct_pointer_vtable_indirect": "unbound_indirect_target"},
        "native_audit": audit, "self_tests": tests,
        "ABI_projection": "V1 low64 explicit scalar liveout; V0 untouched in differential adapter; upper64 preservation outside native kernel",
        "Not verified": ["General memory/ABI transform", "Native owner integration", "Whole game"],
        "Contradictions with earlier documents": ["Word-decoder typed frontend alone is not generated-C++ AST transformation"],
        "Falsification attempts": tests,
    }
    publish_new_or_identical(candidate / proof_name, json.dumps(proof, indent=2) + "\n")
    publish_outputs(out_dir, candidate, args.replace_generated, (stem + ".hpp", stem + ".cpp", proof_name))
    print(json.dumps({"schema": proof["schema"], "status": "PASS", "proof_sha256": digest(out_dir / proof_name),
                      "outputs": proof["outputs"], "functions": [{k: v for k, v in f.items() if k != "typed_ir"} for f in proof["functions"]]}))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (Refusal, OSError, ValueError, KeyError, struct.error) as error:
        print(json.dumps({"status": "FAIL", "reason": str(error)}), file=sys.stderr)
        raise SystemExit(1)
