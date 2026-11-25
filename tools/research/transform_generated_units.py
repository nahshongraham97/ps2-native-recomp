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
import re
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


class BorrowedReferenceSymbols:
    """One original-validated experimental nullable-reference clear, never pointer bits."""
    def __init__(self,lowering,image,claim=None):
        import retail_data_symbol_map as symbols
        try:expected=symbols.validated_symbol_map(image)
        except symbols.SymbolMapRefusal as error:raise Refusal(str(error)) from error
        if claim is not None:require(canonical(claim)==canonical(expected),'source_symbol_map_forged_identity_or_owner')
        self.lowering=lowering;self.symbols=expected;self.effects=[]
        symbol=expected['0064e348']
        require(symbol['owner_binding'] is None and symbol['pointee_binding'] is None,'source_symbol_native_binding_not_established')
        require(symbol['semantic_kind']=='nullable_borrowed_object_reference' and symbol['allowed_projection']=='clear_borrowed_reference',
                'source_symbol_not_borrowed_reference_clear')

    def statement(self,node):
        if node.get('kind')!='CallExpr':return False
        try:name,args=self.lowering.helper(node)
        except Refusal:return False
        if name!='st32' or len(args)!=2:return False
        symbol=self.symbols['0064e348'];pc=self.lowering.pc
        resets=[w for w in symbol['reset_words'] if int(w['pc'],16)==pc and int(w['word'],16)>>16&31==0
                and ((int(symbol['gp_base'],16)+((int(w['word'],16)&0xffff)-0x10000))&0xffffffff)==int(symbol['address'],16)]
        if not resets:return False
        address=unwrap(args[0]);require(address.get('kind')=='BinaryOperator' and address.get('opcode') in ('+','-'),'source_borrowed_ref_address_expression')
        base,offset=children(address);require(register_part(base,self.lowering.cpu)==(28,32),'source_borrowed_ref_GP_base')
        displacement=literal(offset)*(1 if address['opcode']=='+' else -1)
        require(displacement==symbol['signed_GP_displacement'],'source_borrowed_ref_wrong_field')
        value=self.lowering.expression(args[1]);require(self.lowering.evaluate(value)==0,'source_borrowed_ref_clear_requires_zero')
        effect={'kind':'clear_borrowed_reference','source_pc':f'{pc:08x}','source_symbol':symbol['address'],
                'semantic_kind':symbol['semantic_kind'],'native_value':'nullptr','owner_binding':None,'pointee_binding':None,
                'scope':'experimental typed IR only; no original pointer bits, handle, native owner or production adoption'}
        self.effects.append(effect);self.lowering.effects.append(effect)
        return True

    def report(self):
        return {'schema':'source_backed_borrowed_reference_IR_v1','validated_symbols':self.symbols,'effects':self.effects,
                'native_emission_blockers':['borrowed_reference_native_owner_unbound','borrowed_reference_pointee_ABI_unbound'] if self.effects else [],
                'scope':'Only original SWzero projection. Companion cookie and all other fields remain unbound.'}


def borrowed_reference_self_tests(definition,descriptor,contract):
    """Actual selected AST effect controls; no source file or original byte writes."""
    require(descriptor['source_entry'].lower()=='0018da30','symbol_controls_require_original_clear_continuation')
    baseline=Lowering(definition,descriptor,contract)
    try:baseline.lower()
    except Refusal as error:require(str(error)=='unbound_memory:GP_field_or_width','symbol_control_next_cookie_not_reached')
    else:raise AssertionError('Unexpected complete unit from one reference binding')
    require(len(baseline.data_symbols.effects)==1 and baseline.pc==0x18da34,'symbol_control_clear_or_next_effect_missing')
    cases=[]
    for key in ('owner_binding','pointee_binding'):
        claim=copy.deepcopy(baseline.data_symbols.symbols);claim['0064e348'][key]='invented'
        try:BorrowedReferenceSymbols(baseline,descriptor['_data_symbol_image'],claim)
        except Refusal as error:
            require(str(error)=='source_symbol_map_forged_identity_or_owner','symbol_control_wrong_forgery_gate');cases.append({'plant':key,'rejection':str(error)})
        else:raise AssertionError('Forged native binding admitted')
    label=next(n for n in walk(definition) if n.get('kind')=='LabelStmt' and n.get('name')=='L_0018DA30')
    call=next(n for n in walk(label) if n.get('kind')=='CallExpr' and declaration(children(n)[0]).get('name')=='st32')
    original=copy.deepcopy(call)
    for label,mutation,expected in (
        ('wrong GP field',lambda n:unwrap(children(unwrap(children(n)[1]))[1]).update(value='23972'),'source_borrowed_ref_wrong_field'),
        ('nonzero reference store',lambda n:unwrap(children(n)[2]).update(value='1'),'source_borrowed_ref_clear_requires_zero'),
    ):
        try:
            call.clear();call.update(copy.deepcopy(original));mutation(call)
            try:Lowering(definition,descriptor,contract).lower()
            except Refusal as error:
                require(str(error)==expected,'symbol_control_wrong_AST_rejection:'+str(error));cases.append({'plant':label,'rejection':str(error)})
            else:raise AssertionError('Invalid source reference effect admitted')
        finally:call.clear();call.update(copy.deepcopy(original))
    ir={'values':baseline.values,'effects':baseline.effects,'retail_data_symbols':baseline.data_symbols.report()}
    try:emit([(dict(descriptor,native_name='never_emitted'),ir)])
    except Refusal as error:
        require(str(error)=='native_emission_unbound_borrowed_reference_owner_or_pointee','symbol_control_native_gate_changed');cases.append({'plant':'native emission with unbound owner/pointee','rejection':str(error)})
    else:raise AssertionError('Unbound reference projection emitted natively')
    return {'status':'PASS','cases':cases,'next_unbound_source_pc':'0018da34','new_credit':0}


class SymbolicFrame:
    """Actual generated-AST frame lowering, without guessed SP/register values.

    Reference transport survives as explicit IR until its complete proof closes.
    The native emitter rejects that IR; advancing a compiler frontier is not an
    elision, a complete native function or transformed credit.
    """
    def __init__(self, lowering, original, plan):
        import stack_frame_analysis as frames
        try:self.plan=frames.validate_frame_plan(original,plan)
        except frames.FrameRefusal as error:raise Refusal(str(error)) from error
        self.lowering=lowering;self.offsets={29:0};self.operations=[];self.reference_effects=[];self.locals={}
        self.words={int(x['pc'],16):int(x['word'],16) for x in self.plan['original_words']}
        self.accesses=self.plan['stack_accesses'];self.adjustments=self.plan['SP_adjustments']
        self.slots={(s['offset_from_allocated_sp'],s['width_bits']):s for s in self.plan['slots']}

    def pcs(self):
        import stack_frame_analysis as frames
        pc=self.lowering.pc
        return [pc,pc+4] if pc in self.words and frames.control(pc,self.words[pc]) else [pc]

    def address(self,node):
        node=unwrap(node);aligned=False
        if node.get('kind')=='BinaryOperator' and node.get('opcode')=='&':
            left,right=children(node);right=unwrap(right)
            require(right.get('kind')=='UnaryOperator' and right.get('opcode')=='~' and literal(children(right)[0])==15,
                    'unbound_stack_alignment_mask')
            node=unwrap(left);aligned=True
        displacement=0;base=node
        if node.get('kind')=='BinaryOperator' and node.get('opcode') in ('+','-'):
            base,offset=children(node);displacement=literal(offset)*(1 if node['opcode']=='+' else -1)
        try:reg,width=register_part(base,self.lowering.cpu)
        except Refusal:return None
        if reg not in self.offsets:return None
        require(width in (32,64),'unbound_stack_address_width')
        offset=self.offsets[reg]+displacement+self.plan['frame_bytes']
        if aligned:require(offset%16==0 and self.plan['entry_alignment']==16,'unbound_stack_alignment')
        return offset

    def match_access(self,offset,width,direction,reg):
        matches=[a for a in self.accesses if int(a['pc'],16) in self.pcs()
                 and a['offset_from_allocated_sp']==offset and a['width_bits']==width and a['direction']==direction and a['register']==reg and a['register_bank']=='GPR']
        require(matches,'frame_AST_original_slot_join_mismatch')
        return matches[0]

    def ref_effect(self,access,kind):
        effect={'kind':kind,'source_pc':access['pc'],'slot_offset':access['offset_from_allocated_sp'],
                'width_bits':access['width_bits'],'source_register':access['register'],
                'value':'symbolic original input/slot identity; never a concrete guest value'}
        slot=self.slots[(access['offset_from_allocated_sp'],access['width_bits'])]
        if slot.get('transport_elision_proved'):
            effect['kind']='elided_proved_frame_transport';effect['reference_RAM_projection_required']=True
        else:self.reference_effects.append(effect)
        self.operations.append(effect)

    def expression(self,node):
        call=self.call(unwrap(node))
        if not call or call[0] not in ('ld8','ld16','ld32','ld64'):return None
        name,args,_=call;require(len(args)==1,'frame_local_load_arity')
        offset=self.address(args[0])
        if offset is None:return None
        bits={'ld8':8,'ld16':16,'ld32':32,'ld64':64}[name]
        matches=[a for a in self.accesses if int(a['pc'],16) in self.pcs() and a['offset_from_allocated_sp']==offset
                 and a['width_bits']==bits and a['direction']=='load' and a['register_bank']=='GPR']
        require(matches,'frame_AST_original_local_load_join_mismatch')
        local=self.locals.get((offset,bits));require(local is not None and 'current_expression' in local,'unbound_stack_local_read_before_native_definition')
        self.operations.append({'kind':'typed_local_load','source_pc':matches[0]['pc'],'local':local['name']})
        return local['current_expression']

    def call(self,node):
        if node.get('kind')!='CallExpr' or not children(node):return None
        try:decl=declaration(children(node)[0])
        except Refusal:return None
        if decl.get('kind')!='FunctionDecl':return None
        return decl.get('name'),children(node)[1:],decl.get('type',{}).get('qualType')

    def invalidate_GPR_SSA(self,reg):
        # Reference-only frame transport/aliases do not produce a native value.
        # MTC1 must never reuse an antecedent scalar definition after that write.
        if getattr(self.lowering,'mtc1_original',None):self.lowering.regs.pop(reg,None)

    def statement(self,node):
        kind=node.get('kind');call=self.call(node)
        if call and call[0]=='set32':
            _,args,ty=call
            if len(args)!=2:return False
            try:reg=register_object(args[0],self.lowering.cpu)
            except Refusal:return False
            if reg!=29:
                try:offset=self.address(args[1])
                except Refusal:return False
                if offset is None:return False
                require(ty==self.lowering.contract['helper_contracts']['set32']['type'],'frame_alias_set32_signature')
                matches=[a for a in self.plan['aliases'] if int(a['pc'],16) in self.pcs() and a['register']==reg
                         and str(offset-self.plan['frame_bytes']) in a['offsets']]
                require(matches,'frame_AST_original_set32_alias_join_mismatch')
                self.offsets[reg]=offset-self.plan['frame_bytes']
                self.invalidate_GPR_SSA(reg)
                effect={'kind':'reference_only_symbolic_frame_alias','source_pc':matches[0]['pc'],'register':reg,'offset':offset}
                self.operations.append(effect);self.reference_effects.append(effect)
                return True
            require(ty==self.lowering.contract['helper_contracts']['set32']['type'],'frame_set32_helper_signature')
            rhs=unwrap(args[1]);require(rhs.get('kind')=='BinaryOperator' and rhs.get('opcode') in ('+','-'),'frame_SP_adjustment_AST')
            base,amount=children(rhs);require(register_part(base,self.lowering.cpu)==(29,32),'frame_SP_adjustment_base')
            delta=literal(amount)*(1 if rhs['opcode']=='+' else -1)
            matches=[a for a in self.adjustments if int(a['pc'],16) in self.pcs() and a['delta']==delta]
            require(matches,'frame_AST_original_SP_delta_mismatch')
            self.offsets[29]+=delta
            self.invalidate_GPR_SSA(29)
            require(str(self.offsets[29]) in matches[0]['offsets_after'],'frame_symbolic_SP_original_join_mismatch')
            self.operations.append({'kind':'symbolic_SP_adjustment','source_pc':matches[0]['pc'],'delta':delta,'offset_from_entry_sp':self.offsets[29]})
            return True
        if call and call[0] in ('st8','st16','st32','st64','st128'):
            name,args,ty=call
            require(len(args)==2,'frame_store_arity')
            offset=self.address(args[0])
            if offset is None:return False
            width={'st8':8,'st16':16,'st32':32,'st64':64,'st128':128}[name]
            require(not any(int(a['pc'],16) in self.pcs() and a['offset_from_allocated_sp']==offset and a['register_bank']!='GPR' for a in self.accesses),
                    'unbound_stack_FPR_or_mixed_source_type')
            try:
                if width==128:reg=register_object(args[1],self.lowering.cpu)
                else:
                    reg,part_width=register_part(args[1],self.lowering.cpu)
                    require(part_width==(64 if width==64 else 32),'frame_store_source_register_width_mismatch')
            except Refusal:
                require(unwrap(args[1]).get('kind')=='IntegerLiteral' and literal(args[1])==0,'unbound_stack_store_source_expression')
                reg=0
            access=self.match_access(offset,width,'store',reg);slot=self.slots[(offset,width)]
            expected={'st8':'void (std::uint32_t, std::uint8_t)','st16':'void (std::uint32_t, std::uint16_t)',
                      'st32':'void (std::uint32_t, std::uint32_t)','st64':'void (std::uint32_t, std::uint64_t)',
                      'st128':'void (std::uint32_t, const Reg128 &)'}[name]
            require(ty==expected,'frame_store_helper_signature:'+str(ty))
            if slot['classification'] in ('RA_transport_candidate','callee_save_transport_candidate'):
                self.ref_effect(access,'reference_only_frame_transport_store');return True
            require('frame_callee_stack_effects_unproved' not in self.plan['blockers'],'unbound_stack_local_callee_effects')
            require(not self.plan['blockers'],'unbound_stack_local_frame_or_alias_effects')
            require(not self.plan['escapes'] and not any('overlap' in b or 'read_before' in b or 'outside_owned' in b for b in self.plan['blockers']),
                    'unbound_stack_local_escape_or_clobber')
            require(width in (8,16,32,64),'unbound_stack_local_128_type')
            expression=self.lowering.expression(args[1])
            local=self.locals.setdefault((offset,width),{'name':'local_'+str(len(self.locals)),'type':'u'+str(width),'offset':offset,'width_bits':width})
            local['current_expression']={'op':'constant','type':local['type'],
                                         'value':self.lowering.evaluate(expression)&((1<<width)-1)}
            self.lowering.effects.append({'kind':'store_local','local':local['name'],'type':local['type'],'expression':expression,'source_pc':access['pc']})
            self.operations.append({'kind':'typed_local_store','source_pc':access['pc'],'local':local['name']})
            return True
        if call and call[0]=='ld128':
            _,args,ty=call
            require(len(args)==2,'frame_load128_arity');offset=self.address(args[0])
            if offset is None:return False
            reg=register_object(args[1],self.lowering.cpu);access=self.match_access(offset,128,'load',reg)
            require(ty=='void (std::uint32_t, Reg128 &)','frame_load128_helper_signature')
            self.invalidate_GPR_SSA(reg)
            self.ref_effect(access,'reference_only_frame_transport_load');return True
        if kind=='BinaryOperator' and node.get('opcode')=='=':
            lhs,rhs=children(node)
            try:reg,width=register_part(lhs,self.lowering.cpu)
            except Refusal:return False
            rhs=unwrap(rhs);load=self.call(rhs)
            if load and load[0] in ('ld8','ld16','ld32','ld64'):
                name,args,_=load;require(len(args)==1,'frame_load_arity');offset=self.address(args[0])
                if offset is None:return False
                bits={'ld8':8,'ld16':16,'ld32':32,'ld64':64}[name]
                require(bits==64 and width==64,'unbound_stack_direct_load_width_or_sign_semantics')
                access=self.match_access(offset,bits,'load',reg)
                self.invalidate_GPR_SSA(reg)
                self.ref_effect(access,'reference_only_frame_transport_load');return True
            try:offset=self.address(rhs)
            except Refusal:return False
            if offset is not None:
                matches=[a for a in self.plan['aliases'] if int(a['pc'],16) in self.pcs() and a['register']==reg
                         and str(offset-self.plan['frame_bytes']) in a['offsets']]
                require(matches,'frame_AST_original_alias_join_mismatch')
                self.offsets[reg]=offset-self.plan['frame_bytes']
                self.invalidate_GPR_SSA(reg)
                effect={'kind':'reference_only_symbolic_frame_alias','source_pc':matches[0]['pc'],'register':reg,'offset':offset}
                self.operations.append(effect);self.reference_effects.append(effect)
                return True
        return False

    def report(self):
        blockers=list(self.plan['blockers'])
        if self.reference_effects:blockers.append('native_emission_residual_reference_only_frame_transport')
        if self.offsets[29]!=0:blockers.append('native_emission_frame_not_yet_restored')
        return {'schema':'symbolic_generated_frame_IR_v1','frame_bytes':self.plan['frame_bytes'],
                'original_plan_sha256':hashlib.sha256(canonical(self.plan)).hexdigest(),
                'normalized_operations':self.operations,'reference_only_effects':self.reference_effects,
                'typed_locals':list(self.locals.values()),'symbolic_SP_offset':self.offsets[29],
                'native_emission_blockers':sorted(set(blockers)),
                'scope':'Actual AST normalization/frontier only; reference-only frame effects cannot enter native emission'}


def bind_lui_assignment(node,cpu,pc,original):
    """Join one actual LUI/AST write; produce bits, never a pointer or owner."""
    require(pc%4==0,'lui_unaligned_source_pc')
    raw=original(pc,4);require(len(raw)==4,'lui_short_original_word')
    word=int.from_bytes(raw,'little')
    require(word>>26==0x0f and word>>21&31==0,'lui_source_not_LUI')
    destination=word>>16&31
    require(destination not in (0,28,29,31),'lui_forbidden_abi_destination')
    require(node.get('kind')=='BinaryOperator' and node.get('opcode')=='=' and len(children(node))==2,'lui_assignment_shape')
    def signed_type(n,expected):
        ty=n.get('type',{});return ty.get('desugaredQualType',ty.get('qualType'))==expected
    require(signed_type(node,'long long'),'lui_assignment_width_or_signedness')
    lhs,rhs=children(node);lhs=unwrap(lhs)
    require(lhs.get('kind')=='ArraySubscriptExpr' and len(children(lhs))==2,'lui_destination_shape')
    bank,lane=children(lhs);bank=unwrap(bank)
    require(bank.get('kind')=='MemberExpr' and bank.get('name')=='sd' and literal(lane)==0
            and len(children(bank))==1 and signed_type(lhs,'long long'),'lui_destination_not_signed_low64')
    require(register_object(children(bank)[0],cpu)==destination,'lui_AST_destination_word_mismatch')
    require(rhs.get('kind')=='ImplicitCastExpr' and rhs.get('castKind')=='IntegralCast'
            and signed_type(rhs,'long long') and len(children(rhs))==1,'lui_missing_signed64_extension')
    cast=children(rhs)[0]
    require(cast.get('kind')=='CXXStaticCastExpr' and cast.get('castKind') in ('NoOp','IntegralCast')
            and signed_type(cast,'int') and len(children(cast))==1,'lui_missing_explicit_signed32_cast')
    conversion=children(cast)[0]
    require(conversion.get('kind')=='ImplicitCastExpr' and conversion.get('castKind')=='IntegralCast'
            and signed_type(conversion,'int') and len(children(conversion))==1,'lui_signed32_literal_conversion')
    value=children(conversion)[0]
    require(value.get('kind')=='IntegerLiteral' and value.get('type',{}).get('qualType')=='unsigned int','lui_literal_type_or_shape')
    bits=int(value['value']);require(0<=bits<=0xffffffff and bits==((word&0xffff)<<16),'lui_AST_literal_word_mismatch')
    return {'schema':'original_AST_LUI_low64_v1','source_pc':f'{pc:08x}','original_word':f'{word:08x}',
            'original_destination':destination,'expression':{'op':'sign_extend32','type':'u64',
                'argument':{'op':'constant','type':'u32','value':bits}},
            'write_projection':{'portion':'low64','upper64':'PRESERVED'},
            'literal_role':'Source bit pattern only; no pointer, float-role or ownership inference'}


def emit_lui_scalar_probe(binding,name):
    """Test-only scalar IR emission; not a complete retail function or owner ABI."""
    require(re.fullmatch(r'lui_probe_\d+',name) is not None,'lui_probe_name')
    require(binding.get('write_projection')=={'portion':'low64','upper64':'PRESERVED'},'lui_probe_projection_unproved')
    expression=binding['expression']
    require(expression.get('op')=='sign_extend32' and expression.get('type')=='u64','lui_probe_requires_explicit_sign_extend32')
    value=expression['argument']
    require(value.get('op')=='constant' and value.get('type')=='u32' and 0<=value['value']<=0xffffffff,'lui_probe_constant_width')
    return (f'std::uint64_t {name}() noexcept {{\n'
            f'    const std::uint64_t value_0 = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(0x{value["value"]:08x}u)));\n'
            '    return value_0;\n}\n')


def require_mtc1_source_expression(expression):
    require(expression.get('type')=='u32' and
            (expression.get('op')=='truncate32' and type(expression.get('value')) is int and expression['value']>=0
             or expression.get('op')=='constant' and type(expression.get('value')) is int and 0<=expression['value']<=0xffffffff),
            'mtc1_source_SSA_not_admitted_raw32')


def bind_mtc1_assignment(node,cpu,pc,original,source_expression):
    """Join MTC1 and its raw unsigned32 AST; never perform numeric conversion."""
    require(pc%4==0,'mtc1_unaligned_source_pc')
    raw=original(pc,4);require(len(raw)==4,'mtc1_short_original_word')
    word=int.from_bytes(raw,'little')
    require(word>>26==0x11 and word>>21&31==4 and word&0x7ff==0,'mtc1_source_not_MTC1')
    source,destination=word>>16&31,word>>11&31
    def raw32(n):
        ty=n.get('type',{});return ty.get('desugaredQualType',ty.get('qualType'))=='unsigned int'
    require(node.get('kind')=='BinaryOperator' and node.get('opcode')=='=' and len(children(node))==2,'mtc1_assignment_shape')
    require(raw32(node),'mtc1_assignment_not_raw32')
    lhs,rhs=children(node);lhs=unwrap(lhs)
    require(lhs.get('kind')=='ArraySubscriptExpr' and len(children(lhs))==2 and raw32(lhs),'mtc1_destination_not_raw32')
    bank,index=children(lhs);bank=unwrap(bank)
    require(bank.get('kind')=='MemberExpr' and bank.get('name')=='f' and len(children(bank))==1
            and same_parameter(children(bank)[0],cpu),'mtc1_destination_not_FPR_owner')
    require(literal(index)==destination,'mtc1_AST_destination_word_mismatch')
    require(rhs.get('kind')=='ImplicitCastExpr' and rhs.get('castKind')=='LValueToRValue'
            and len(children(rhs))==1 and raw32(rhs),'mtc1_source_numeric_conversion_or_type')
    operand=children(rhs)[0]
    require(raw32(operand),'mtc1_source_not_raw32')
    require(register_part(operand,cpu)==(source,32),'mtc1_AST_source_word_mismatch')
    require_mtc1_source_expression(source_expression)
    return {'schema':'original_AST_MTC1_raw32_v1','source_pc':f'{pc:08x}','original_word':f'{word:08x}',
            'original_source_GPR':source,'original_destination_FPR':destination,
            'expression':{'op':'bit_move32','type':'raw32','argument':source_expression},
            'write_projection':{'destination':'FPR_raw32','source_GPR':'PRESERVED','other_FPRs':'PRESERVED','FCR31':'PRESERVED'},
            'scope':'Raw bits only; no numeric conversion, arithmetic, memory, owner or complete native ABI binding.'}


def emit_mtc1_scalar_probe(binding,name):
    """Test-only raw-bit scalar projection; no Cpu or runtime state interface."""
    require(re.fullmatch(r'mtc1_probe_\d+',name) is not None,'mtc1_probe_name')
    require(binding.get('write_projection')=={'destination':'FPR_raw32','source_GPR':'PRESERVED','other_FPRs':'PRESERVED','FCR31':'PRESERVED'},'mtc1_probe_projection_unproved')
    expression=binding.get('expression',{})
    require(expression.get('op')=='bit_move32' and expression.get('type')=='raw32'
            and expression.get('argument',{}).get('type')=='u32','mtc1_probe_requires_raw_bit_move32')
    require_mtc1_source_expression(expression['argument'])
    return f'std::uint32_t {name}(std::uint32_t source_bits) noexcept {{ return source_bits; }}\n'


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
        self.frame = SymbolicFrame(self,descriptor['_frame_original'],descriptor['_frame_plan']) if '_frame_plan' in descriptor else None
        self.data_symbols=BorrowedReferenceSymbols(self,descriptor['_data_symbol_image'],descriptor.get('_data_symbol_claim')) if '_data_symbol_image' in descriptor else None
        self.lui_original=descriptor.get('_lui_original');self.lui_bindings=[]
        self.mtc1_original=descriptor.get('_mtc1_original');self.mtc1_bindings=[];self.fprs={}

    def expression(self, node):
        node = unwrap(node)
        if self.frame:
            local=self.frame.expression(node)
            if local is not None:return local
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
        if op == 'bit_move32':return self.evaluate(expression['argument']) & 0xffffffff
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
        if self.frame and self.frame.statement(node):return
        if self.data_symbols and self.data_symbols.statement(node):return
        if self.mtc1_original and kind=='BinaryOperator' and node.get('opcode')=='=':
            word=int.from_bytes(self.mtc1_original(self.pc,4),'little')
            if word>>26==0x11 and word>>21&31==4:
                source=word>>16&31
                require(source==0 or source in self.regs,'mtc1_unbound_GPR_value_ABI:register_'+str(source))
                expression={'op':'constant','type':'u32','value':0} if source==0 else {'op':'truncate32','type':'u32','value':self.regs[source]}
                binding=bind_mtc1_assignment(node,self.cpu,self.pc,self.mtc1_original,expression)
                value={'id':len(self.values),'type':'raw32','expression':binding['expression'],
                       'constant_value':self.evaluate(binding['expression']),'source_pc':binding['source_pc']}
                self.values.append(value);self.fprs[binding['original_destination_FPR']]=value['id']
                self.effects.append({'kind':'define_FPR_raw32','value':value['id'],'source_pc':binding['source_pc']})
                self.mtc1_bindings.append(binding);return
        if self.lui_original and kind=='BinaryOperator' and node.get('opcode')=='=':
            word=int.from_bytes(self.lui_original(self.pc,4),'little')
            if word>>26==0x0f:
                binding=bind_lui_assignment(node,self.cpu,self.pc,self.lui_original)
                value={'id':len(self.values),'type':'u64','expression':binding['expression'],
                       'constant_value':self.evaluate(binding['expression']),'source_pc':binding['source_pc'],
                       'write_projection':binding['write_projection']}
                self.values.append(value);self.regs[binding['original_destination']]=value['id']
                if self.frame:
                    old_alias=self.frame.offsets.pop(binding['original_destination'],None)
                    binding['overwritten_symbolic_frame_alias']=old_alias is not None
                    if old_alias is not None:self.frame.operations.append({'kind':'frame_alias_killed_by_LUI','source_pc':binding['source_pc'],'register':binding['original_destination']})
                self.effects.append({'kind':'define','value':value['id'],'source_pc':binding['source_pc']})
                self.lui_bindings.append(binding);return
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
        result={"values": self.values, "effects": self.effects, "host_budget_debit": self.debit,
                "return": "structured_native_return_after_delay_effect"}
        if self.frame:result['frame']=self.frame.report()
        if self.data_symbols:result['retail_data_symbols']=self.data_symbols.report()
        if self.lui_original:result['lui_bindings']=self.lui_bindings
        if self.mtc1_original:result['mtc1_bindings']=self.mtc1_bindings
        return result


def emit(functions, relative=False):
    owner = "IndexSelection" if relative else "OverlaySelection"
    field = "std::int32_t index" if relative else "std::uint32_t selected_mode"
    filename = "native_index_selection.hpp" if relative else "native_overlay_initializers.hpp"
    header = ("#pragma once\n#include <cstdint>\n\nnamespace mka::retail::generated {\n"
              f"struct {owner} {{ {field}{{}}; }};\n")
    source = f'#include "{filename}"\n\nnamespace mka::retail::generated {{\n'
    for desc, ir in functions:
        require(not ir.get('mtc1_bindings'),'native_emission_unbound_FPR_liveout_or_function_ABI')
        symbols=ir.get('retail_data_symbols')
        if symbols:
            require(not symbols['effects'] and not symbols['native_emission_blockers'],'native_emission_unbound_borrowed_reference_owner_or_pointee')
        frame=ir.get('frame')
        if frame:
            require(not frame['native_emission_blockers'] and not frame['reference_only_effects'],
                    'native_emission_residual_reference_only_frame_effects')
        signature = "std::uint64_t " + desc["native_name"] + f"({owner}& selection) noexcept"
        header += signature + ";\n"
        source += signature + " {\n"
        if frame:
            for local in frame['typed_locals']:
                require(local['type'] in ('u8','u16','u32','u64'),'native_unbound_frame_local_type')
                require(re.fullmatch(r'local_\d+',local['name']) is not None,'native_unbound_frame_local_name')
                source+=f"    std::uint{local['width_bits']}_t {local['name']}{{}};\n"
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
            elif effect['kind']=='store_local' and frame:
                expression=effect['expression'];require(expression['op'] in ('constant','truncate32','value'),'native_unbound_frame_local_value')
                rhs=str(expression['value'])+'ULL' if expression['op']=='constant' else 'value_'+str(expression['value'])
                require(any(local['name']==effect['local'] for local in frame['typed_locals']),'native_unbound_frame_local')
                source+=f"    {effect['local']} = static_cast<std::uint{effect['type'][1:]}_t>({rhs});\n"
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
