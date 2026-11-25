"""Bounded retail library ownership. All addresses are PS2 virtual addresses."""
from bisect import bisect_right
from collections import Counter, defaultdict
import csv
import hashlib
import json
import re
import struct

BASE = "SLUS_214.10"
SEEDS = {0x19F0B0, 0x19F1E0, 0x1A2BC0, 0x1A64A0}
SAMPLED_E1_ROLES = {0x1A2020}
STATUSES = ("verified-renderware", "verified-game", "verified-other-library",
            "inferred-renderware", "inferred-game", "inferred-other", "unresolved")
KINDS = tuple(f"E{i}" for i in range(1, 8))
GAME_WORDS = re.compile(rb"konquest|fatalit(?:y|ies)|kombat|mkarmageddon", re.I)
MAX_CLUSTER_GAP = 1024
MAX_INTERVENING = 3


def csv_rows(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        yield from csv.DictReader(stream, delimiter="\t")


def code_ranges(directory):
    ranges = sorted((int(row["start"], 16), int(row["end"], 16), int(row["entry"], 16))
                    for row in csv_rows(directory / "function-bodies.tsv"))
    starts = [low for low, _, _ in ranges]
    if any(a[1] >= b[0] for a, b in zip(ranges, ranges[1:])):
        raise ValueError("Overlapping code ranges require explicit disambiguation")

    def owner(site):
        i = bisect_right(starts, site) - 1
        return ranges[i][2] if i >= 0 and site <= ranges[i][1] else None
    return owner


def direct_calls(directory, original, starts):
    """Decode the original JAL at every aligned instruction in a catalogue body."""
    edges = defaultdict(list)
    for row in csv_rows(directory / "function-bodies.tsv"):
        caller, low, end = (int(row[key], 16) for key in ("entry", "start", "end"))
        length = (end - low + 1) // 4 * 4
        if not length:
            continue
        for index, (word,) in enumerate(struct.iter_unpack("<I", original(low, length))):
            if word >> 26 == 3:
                site = low + index * 4
                target = ((site + 4) & 0xF0000000) | ((word & 0x03ffffff) << 2)
                if target in starts:
                    edges[caller, target].append(site)
    return edges


def game_string_anchors(image, segments, owner):
    """Require adjacent LUI + ADDIU/ORI with matching base and a named game string."""
    references = defaultdict(list)
    for kind, offset, va, _, size, _, _, _ in segments:
        if kind != 1:
            continue
        data = memoryview(image)[offset:offset+size]
        names = {}
        for match in re.finditer(rb"[ -~]{6,}\x00", data):
            value = match[0][:-1]
            if GAME_WORDS.search(value):
                names[va+match.start()] = value.decode("ascii")
        if not names:
            continue
        for at in range(0, size-7, 4):
            hi, lo = struct.unpack_from("<II", data, at)
            if hi >> 26 != 15 or lo >> 26 not in (9, 13) or (hi >> 16 & 31) != (lo >> 21 & 31):
                continue
            lower = lo & 0xFFFF
            if lo >> 26 == 9 and lower >= 0x8000:
                lower -= 0x10000
            target = ((hi & 0xFFFF) * 65536 + lower) & 0xFFFFFFFF
            if target in names:
                pc = va + at
                start = owner(pc)
                if start is not None and owner(pc+4) == start:
                    references[start].append({"site":f"{pc:08x}", "string_va":f"{target:08x}",
                                              "string":names[target], "words_le":bytes(data[at:at+8]).hex()})
    return dict(references)


def clusters(starts, anchors, max_gap=MAX_CLUSTER_GAP):
    """Span nearby anchored starts while bounding both distance and unanchored entries."""
    indices = [i for i, address in enumerate(starts) if address in anchors]
    runs = []
    for i in indices:
        if (not runs or i-runs[-1][-1]-1 > MAX_INTERVENING
                or starts[i]-starts[runs[-1][-1]] > max_gap):
            runs.append([])
        runs[-1].append(i)
    return [(starts[run[0]:run[-1]+1], [starts[i] for i in run]) for run in runs if len(run) >= 2]


def exact_verdict(statuses):
    """A complete attribution includes the complement of RenderWare."""
    counts = Counter(statuses)
    return bool(counts) and set(counts).issubset(STATUSES[:3])


def census_exit_code(missing_callables, require_exact, exact):
    if missing_callables:
        return 1
    return 2 if require_exact and not exact else 0


def validate_ledger_rows(rows, measured=None, *, integrated_receipts=None):
    seen = set()
    for row in rows:
        key = (row["module"], row["address"])
        assert key not in seen, f"Duplicate bounded ledger entry {key}"
        seen.add(key)
        if measured is not None and row["status"] != "out-of-scope-stubbed":
            expected = measured.get(f"{key[0]}:{key[1]}")
            assert expected and all(row[k] == v for k, v in expected.items()), f"Ownership differs from original witnesses at {key}"
        level = row["verification_level"]
        assert level in ("none", "static", "cross-platform", "differential"), f"Invalid native level at {key}"
        # A per-function integrated verification receipt is required for a new
        # R14 level. Older component receipts remain in their original field.
        if level != "none":
            assert row.get("native_verification_ref"), f"Native level without implementation verification at {key}"
            assert (integrated_receipts or {}).get(key) == (level, row["native_verification_ref"]), f"Unverified integrated receipt at {key}"
        kind = row["catalogue_kind"]
        status = row["ownership_status"]
        refs = row["ownership_evidence_ref"]
        assert refs, f"Missing ownership reference at {key}"
        if kind != "callable":
            assert status == "not-applicable" and not row["ownership_evidence_kinds"]
            assert (kind == "excluded" and row["status"] == "out-of-scope-stubbed") or (
                kind == "descriptor" and key == (BASE, "005bf218"))
            continue
        assert row["status"] != "out-of-scope-stubbed" and status in STATUSES
        kinds = set(filter(None, row["ownership_evidence_kinds"].split("|")))
        assert kinds <= set(KINDS)
        if status in ("verified-renderware", "verified-other-library"):
            assert "E1" in kinds and "sha256=" in refs and "calls=" in refs, f"Missing E1 body/callers at {key}"
            assert f"@{key[1]};sha256=" in refs, f"E1 reference belongs to another entry at {key}"
        elif status == "verified-game":
            assert "E4" in kinds and re.search(r"game/E4:[^@]+@[0-9a-fA-F]{8};", refs), f"Unnamed vtable at {key}"
            assert all(label in refs for label in ("constructor=", "factory=", "name_source=", "code=")), f"Missing game factory/vtable evidence at {key}"
        elif status != "unresolved":
            assert kinds & {"E3", "E5", "E6", "E7"}, f"Inference without witness at {key}"
        for k in kinds:
            assert f"/{k}:" in refs, f"No {k} reference at {key}"
    if measured is not None:
        assert set(measured) == {r["module"] + ":" + r["address"] for r in rows if r["status"] != "out-of-scope-stubbed"}, "Bounded keys differ from retail scope"


def anchor_words(original):
    """Check the instructions that encode the stream protocol in the four original bodies."""
    witnesses = {
        "header_read_size_and_result": [(0x19F0E0, "0c000624"), (0x19F0E4, "0c004238")],
        "short_read_error_8000001a": [(0x19F0F8, "0080023c"), (0x19F104, "1a004434")],
        "minimum_version_34000": [(0x19F234, "0300013c"), (0x19F238, "00402134"),
                                  (0x19F23C, "2b084100"), (0x19F240, "0b002050")],
        "exclusive_version_limit_36004": [(0x19F244, "0300013c"), (0x19F270, "04602134"),
                                         (0x19F274, "2b084100"), (0x19F278, "0b002014")],
        "version_error_80000004": [(0x19F24C, "0080023c"), (0x19F258, "04004434")],
        "extension_chunk_3_passed_to_finder": [(0x1A2BE8, "03000524"), (0x1A2BEC, "787c060c")],
    }
    result = {}
    for purpose, words in witnesses.items():
        result[purpose] = {}
        for address, expected in words:
            observed = original(address, 4).hex()
            if observed != expected:
                raise ValueError(f"Stream anchor {purpose} changed at {address:08x}: {observed}")
            result[purpose][f"{address:08x}"] = observed
    return result


def runtime_edge(event, graph, original, owner=None):
    """Join the target, caller, saved return address and original instruction."""
    caller = int(event["caller"], 16)
    callee = int(event["callee"], 16)
    site = int(event["call_site"], 16)
    raw = original(site, 4)
    proved = site in graph.get((caller, callee), ())
    if event.get("call_kind") == "jalr":
        from trace_oracle_dispatch import writes_register
        word = int.from_bytes(raw, "little")
        register = word >> 21 & 31
        delay = original(site + 4, 4)
        writer = writes_register(int.from_bytes(delay, "little"))
        proved = (word & 0xfc1fffff == 0x0000f809 and register not in (0, 31)
                  and owner is not None and owner(site) == caller and owner(site + 4) == caller
                  and register == event["jalr_register"] and writer not in (-1, register, 31)
                  and delay.hex() == event["delay_word_le"] and int(event["jalr_register_value"], 16) == callee)
    if (not proved or not event["evidence_ref"] or int(event["return_address"], 16) != site + 8
            or raw.hex() != event["jal_word_le"]):
        raise ValueError(f"Unproved runtime caller at {site:08x}")
    return callee, f"renderer-scope.json#bounded_inputs.admitted_runtime_traces/{event['evidence_ref']};caller={caller:08x};site={site:08x};ra={site+8:08x}"


def verified_trace_edges(trace, oracle):
    """Rejoin each reported live edge to its events and the pinned host executable."""
    oracle_sha = hashlib.sha256(oracle).hexdigest()
    if trace["oracle_exe_sha256"] != oracle_sha or oracle_sha != "b9eef7dad3cfa0e1216493cf12301f14aa4b3a8b1456fcdc60d9c39502a0d860":
        raise ValueError("Wrong saved-state oracle identity")
    events = {e["event"]: e for e in trace["selected_dispatch_observations"]}
    pe, = struct.unpack_from("<I", oracle, 0x3c)
    sections, = struct.unpack_from("<H", oracle, pe + 6)
    optional_size, = struct.unpack_from("<H", oracle, pe + 20)
    first = pe + 24 + optional_size
    spans = [struct.unpack_from("<IIII", oracle, first + i*40 + 8) for i in range(sections)]
    for edge in trace["runtime_calls"]:
        number = int(edge["evidence_ref"].rsplit("#event=", 1)[1])
        e = events[number]
        if not e["caller_admitted"] or any(e.get(k) != v for k, v in edge.items() if k != "evidence_ref"):
            raise ValueError("Runtime row differs from dispatch event")
        host = e["preceding_host_call_event"]
        rva = int(host["host_prefix_begin_rva"], 16)
        prefix = bytes.fromhex(host["host_prefix_hex"])
        end = int(host["host_return_rva"], 16)
        mapped = [ptr + rva-start for _, start, size, ptr in spans if start <= rva and end <= start+size]
        helper = trace["host_abi_symbols"]["_ZN3ps24callERNS_3CpuEjj"]["rva"]
        ra = int(edge["return_address"], 16)
        stores = host["matching_cpu_ra_link_stores"]
        if (host["event"] + 1 != number or host["guest_ra"] != ra or host["callee"] != int(edge["callee"], 16)
                or host["depth"] != e["depth"] or end != rva + len(prefix) or len(mapped) != 1
                or oracle[mapped[0]:mapped[0]+len(prefix)] != prefix or prefix[-5] != 0xe8
                or end + int.from_bytes(prefix[-4:], "little", signed=True) != helper
                or not any(bytes.fromhex(s["instruction_hex"]) == prefix[s["prefix_offset"]:s["prefix_offset"]+11]
                           and bytes.fromhex(s["instruction_hex"])[-8:] == struct.pack("<II", 0x1f0, ra) for s in stores)):
            raise ValueError(f"Unproved host call/return linkage at dispatch event {number}")
        yield edge


def named_member(member, original, admitted_classes=None):
    """Verify the compiler-descriptor evidence form against original bytes."""
    from audit_ps2_rtti import mangled_name
    required = ("class_name", "encoded_name", "name_va", "descriptor_va", "name_pointer_slot",
                "header_slot", "vtable_va", "method_slot", "method", "game_owner_evidence_ref", "evidence_ref")
    if not all(member.get(field) for field in required):
        raise ValueError("Missing named class/vtable evidence")
    def address(key):
        return int(member[key], 16)
    def pointer(slot):
        return int.from_bytes(original(slot, 4), "little")
    encoded = member["encoded_name"].encode("ascii")
    name = member["class_name"]
    table, method = address("vtable_va"), address("method")
    if (admitted_classes or {}).get((name, table)) != member["game_owner_evidence_ref"]:
        raise ValueError("Game class ownership has no independent source witness")
    if (original(address("name_va"), len(encoded) + 1) != encoded + b"\0"
            or name not in (member["encoded_name"], mangled_name(member["encoded_name"]))
            or address("name_pointer_slot") not in (address("descriptor_va"), address("descriptor_va") + 4)
            or pointer(address("name_pointer_slot")) != address("name_va")
            or address("header_slot") != table - 4
            or pointer(address("header_slot")) != address("descriptor_va")
            or address("method_slot") < table or (address("method_slot") - table) % 4
            or pointer(address("method_slot")) != method):
        raise ValueError("Unproved original RTTI name/descriptor/vtable/method chain")
    return method, f"{name}@{table:08x};{member['evidence_ref']};{member['game_owner_evidence_ref']}"


def game_class_provenance(member, original, owner):
    """Verify a class-qualified MK7 source load when this evidence form is used."""
    name = member["class_name"]
    source = member.get("game_source", {})
    path = source.get("path", "")
    parts = path.replace("\\", "/").lower().split("/")
    if not (parts[-1] == name.rsplit("::", 1)[-1].lower() + ".cpp"
            and any(part in ("mk7", "mk7ps2") for part in parts[:-1])):
        raise ValueError("Missing class-qualified MK7 source path")
    pc, va = int(source["site"], 16), int(source["string_va"], 16)
    high, low = struct.unpack("<II", original(pc, 8))
    value = low & 0xffff
    if low >> 26 == 9 and value >= 0x8000:
        value -= 0x10000
    if (owner(pc) != int(member["method"], 16) or owner(pc + 4) != owner(pc)
            or high >> 26 != 15 or low >> 26 not in (9, 13) or (high >> 16 & 31) != (low >> 21 & 31)
            or (high & 0xffff) * 65536 + value != va
            or original(va, len(path) + 1) != path.encode("ascii") + b"\0"):
        raise ValueError("Class source reference is not in the verified original method")
    return (name, int(member["vtable_va"], 16)), f"SLUS:{pc:08x}>{va:08x}:{path}"


def resource_anchor(record, original, owner):
    """Check the retail pointer-table load and the bytes of its named resource."""
    pc, start = int(record["reference_pc"], 16), int(record["method"], 16)
    slot, string_va = int(record["pointer_word_va"], 16), int(record["string_va"], 16)
    raw_name = record["string"].encode("ascii")
    code = original(pc - 4, 8)
    lui, addiu = struct.unpack("<II", code)
    low = int.from_bytes(code[4:6], "little", signed=True)
    if (owner(pc) != start or owner(pc - 4) != start or lui >> 26 != 15 or addiu >> 26 != 9
            or (lui >> 16 & 31) != (addiu >> 21 & 31) or ((lui & 0xffff) << 16) + low != slot
            or int.from_bytes(original(slot, 4), "little") != string_va
            or original(string_va, len(raw_name) + 1) != raw_name + b"\0" or not GAME_WORDS.search(raw_name)):
        raise ValueError(f"Unverified game-specific resource load at {pc:08x}")
    return start, {"site": f"{pc:08x}", "string_va": f"{string_va:08x}", "string": record["string"],
                   "pointer_table_va": f"{slot:08x}", "words_le": code.hex()}


def recovery_report(context, image, segments):
    """Validate a bounded recovery record without requiring a positive discovery."""
    cli, audit = context["rtti_cli"], context["rtti_load_audit"]
    identity = hashlib.sha256(image).hexdigest()
    if cli["elf_sha256"] != identity or audit["file_sha256"] != identity:
        raise ValueError("RTTI recovery original image changed")
    commands = {row["name"]: row for row in cli["commands"]}
    variants = {"raw", "strict", "catalogue", "catalogue-strict"}
    if not variants <= commands.keys() or any(commands[name]["exit"] != 0 for name in variants):
        raise ValueError("RTTI recovery command set is incomplete")
    measured_bytes = sum(size for kind, _, _, _, size, _, _, _ in segments if kind == 1)
    if audit["file_backed_load_bytes"] != measured_bytes or audit["self_test"]["status"] != "PASS":
        raise ValueError("RTTI recovery load coverage or controls failed")
    if len(audit["named_vtable_methods"]) != audit["E4_count"]:
        raise ValueError("RTTI named-method count differs from recovered entries")
    return {"result": "BOUNDED_RECOVERY_RECORDED", "image_sha256": identity,
            "load_bytes": measured_bytes, "tool_variants": sorted(variants),
            "conventional_named_methods": audit["E4_count"],
            "unnamed_pointer_runs": len(audit["unnamed_pointer_runs"]),
            "scope": "Completed supported recovery attempts. Unresolved class identity remains explicit and does not require a positive discovery to close the bounded census."}


def installed_table_report(context, directory, original, segments, catalogue):
    """Recheck A2/A3 from the preserved candidate set and original code bytes."""
    from installed_vtable_members import classify_candidates
    stores = context["nis_class_frontier"]["global_abi_audit"]["constructor_like_table_stores"]
    grouped = defaultdict(list)
    for store in stores:
        grouped[store["candidate_vptr_va"]].append(store)
    candidates = [{"vtable_va": address, "stores": values} for address, values in sorted(grouped.items())]
    code = set()
    for row in csv_rows(directory / "instructions.tsv"):
        pc = int(row["address"], 16)
        expected = bytes.fromhex(row["bytes"])
        if original(pc, len(expected)) != expected:
            raise ValueError(f"Installed-table code listing changed at {pc:08x}")
        code.add(pc)
    ranges = [(start, start + size) for kind, _, start, _, size, _, flags, _ in segments
              if kind == 1 and flags & 1 and size]
    report = classify_candidates(original, candidates, ranges, code, catalogue)
    report["joined_code_address_count"] = len(code)
    return report


def collect(rows, directory, original, image, segments, candidates, descriptors, fork, parent):
    scoped = {(r["module"], int(r["address"],16)) for r in rows if r["status"] != "out-of-scope-stubbed"}
    callable_keys = scoped - descriptors
    base = sorted(a for module,a in callable_keys if module==BASE)
    owner = code_ranges(directory)
    starts = {int(row["address"], 16) for row in rows if row["module"] == BASE}
    graph = direct_calls(directory, original, starts)
    game_anchors = game_string_anchors(image, segments, owner)
    context = json.loads((fork / "research/runs/renderer-scope.json").read_bytes()).get("bounded_inputs", {})
    recovery = recovery_report(context, image, segments)
    table_inventory = installed_table_report(context, directory, original, segments, base)
    from custom_vtable_members import recover_camera_cut
    custom_class = recover_camera_cut(original, base)
    class_observations = {int(member["method"], 16): member for member in custom_class["catalogued_methods"]}
    recovery["custom_named_classes"] = 1
    recovery["custom_catalogued_methods"] = len(class_observations)
    for item in context.get("game_resource_anchors", []):
        start, witness = resource_anchor(item, original, owner)
        game_anchors.setdefault(start, []).append(witness)
    claims = defaultdict(lambda: defaultdict(lambda: defaultdict(set)))
    verified = {}

    def add(pc, family, kind, ref):
        if (BASE,pc) in callable_keys:
            claims[pc][family][kind].add(ref)

    for member in table_inventory["game_members"]:
        if member["promotion_eligible"]:
            pc = int(member["method"], 16)
            verified[pc] = "game"
            identifier = member["name"] or member["tag"] or "vt_" + member["table"]
            add(pc, "game", "E4", f"{identifier}@{member['table']};constructor={member['constructor']};factory={member['factory']};name_source={member['name_source']};code={member['method']};renderer-scope.json#bounded_census.installed_tables")

    seed_details = {}
    for pc in sorted(SEEDS | SAMPLED_E1_ROLES):
        role = next(r for r in candidates[pc]["reasons"] if r["kind"]=="sdk_role_and_verified_retail_callee")
        end = int(role["end_exclusive"],16)
        if hashlib.sha256(original(pc,end-pc)).hexdigest()!=role["original_body_sha256"]:
            raise ValueError(f"Unverified seed body {pc:08x}")
        sites = [int(site,16) for site in role["caller_sites"]]
        if not sites or any(site not in graph.get((owner(site),pc),()) for site in sites):
            raise ValueError(f"Unverified seed callers {pc:08x}")
        seed_details[f"{pc:08x}"] = {**role, "caller_entries":[f"{owner(site):08x}" for site in sites]}
        verified[pc] = "renderware"
        add(pc,"renderware","E1",f"renderer_sdk_roles.py@{pc:08x};sha256={role['original_body_sha256']};calls="+",".join(role["caller_sites"]))

    # One direct edge only. Callers may be game clients and callees may be utility code.
    rw_seeds = set(SEEDS)
    for (caller,callee), sites in sorted(graph.items()):
        for seeds, family in ((rw_seeds,"renderware"),(game_anchors,"game")):
            if caller not in seeds and callee not in seeds:
                continue
            witness=f"SLUS:JAL:{caller:08x}>{callee:08x}@{sites[0]:08x}"
            if caller in seeds:
                add(callee,family,"E5",witness)
            if callee in seeds:
                add(caller,family,"E5",witness)

    boundaries={}
    adjacent_anchors={}
    coarse_intervals = {}
    ordinal={pc:i for i,pc in enumerate(base)}
    for family,anchors in (("renderware",set(candidates)),("game",set(game_anchors))):
        bounds=[]
        for members, seeds in clusters(base,anchors):
            bounds.append({"first":f"{members[0]:08x}", "last":f"{members[-1]:08x}",
                           "anchors":[f"{p:08x}" for p in seeds],"catalogue_entries":len(members)})
            ref=f"SLUS:cluster:{family}:{members[0]:08x}..{members[-1]:08x}"
            for pc in members:
                add(pc,family,"E6",ref)
        boundaries[family]=bounds
        coarse_intervals[family] = [
            {"first": f"{members[0]:08x}", "last": f"{members[-1]:08x}",
             "anchors": [f"{p:08x}" for p in seeds], "catalogue_entries": len(members)}
            for members, seeds in clusters(base, anchors, max_gap=4096)
        ]
        ordered=sorted(anchors & ordinal.keys())
        adjacent_anchors[family]=[{"left":f"{a:08x}", "right":f"{b:08x}",
                                  "start_distance_bytes":b-a, "intervening_starts":ordinal[b]-ordinal[a]-1}
                                 for a,b in zip(ordered,ordered[1:])]

    members = context.get("named_vtable_methods", [])
    game_classes = dict(game_class_provenance(r, original, owner) for r in members if r.get("game_source"))
    for member in members:
        pc, witness = named_member(member, original, game_classes)
        if (BASE, pc) not in callable_keys:
            raise ValueError(f"Class method outside catalogue at {pc:08x}")
        # Descriptor membership is class evidence. A2 game promotion comes from
        # the independently verified factory/constructor table inventory above.
        class_observations[pc] = member
    traces = context.get("admitted_runtime_traces", [])
    runtime_calls = []
    if traces:
        oracle = (parent.parent.parent / "milestone_work_verified_playable_20261110/ps2run.exe").read_bytes()
        for trace in traces:
            runtime_calls.extend(verified_trace_edges(trace, oracle))
    if runtime_calls != context.get("runtime_calls", []):
        raise ValueError("Runtime summary disagrees with original trace events")
    for event in runtime_calls:
        pc, ref = runtime_edge(event, graph, original, owner)
        if pc not in candidates and pc not in SEEDS:
            raise ValueError(f"No renderer role for observed callee {pc:08x}")
        add(pc,"renderware","E7",ref)
    donor=json.loads((parent/"research/runs/xbox-donor-20261121/PUBLICATION.json").read_bytes())["capability_comparison"]
    for pair in donor["mapping_entries"]:
        if pair["ps2_module"]==BASE and pair.get("ownership_status")=="inferred-renderware":
            add(int(pair["ps2_address"],16),"renderware","E3",f"Xbox:{pair['xbox_address']};{pair['evidence_path']}")
    # The independently checked camera counterpart predates the expanded capability table.
    add(0x1A64A0,"renderware","E3","renderer-scope.json#four_admissions_publication_audit;Xbox:002bb870")

    records={}
    for module,pc in sorted(scoped):
        key=f"{module}:{pc:08x}"
        if (module,pc) in descriptors:
            records[key]={"catalogue_kind":"descriptor","ownership_status":"not-applicable","ownership_evidence_kinds":"",
                          "ownership_evidence_ref":"aot_coverage_inventory.json:original descriptor data"}
            continue
        families=claims.get(pc,{}) if module==BASE else {}
        status="unresolved"
        if module==BASE and pc in verified:
            status="verified-"+verified[pc]
        elif len(families)==1:
            status="inferred-"+next(iter(families))
        kinds=set()
        refs=[]
        for family,items in sorted(families.items()):
            for k,notes in sorted(items.items()):
                kinds.add(k)
                refs.extend(f"{family}/{k}:{ref}" for ref in sorted(notes))
        if module == BASE and pc in class_observations:
            member = class_observations[pc]
            kinds.add("E4")
            refs.append(f"unattributed/E4:{member['class_name']}@{member['vtable_va']};slot={member['method_slot']};renderer-scope.json#bounded_census.custom_classes")
        records[key]={"catalogue_kind":"callable","ownership_status":status,"ownership_evidence_kinds":"|".join(sorted(kinds)),
                      "ownership_evidence_ref":" | ".join(refs) or f"catalogue:{key};no admitted ownership witness"}
    calls=[r for r in records.values() if r["catalogue_kind"]=="callable"]
    counts=Counter(r["ownership_status"] for r in calls)
    kinds=Counter(k for r in calls for k in r["ownership_evidence_kinds"].split("|") if k)
    def digest_object(value):
        return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()

    return {"rows":records, "status_counts":{k:counts[k] for k in STATUSES}, "rtti_recovery":recovery, "custom_classes":[custom_class], "installed_tables":table_inventory,
            "input_record_fragment_sha256":{"bounded_inputs":digest_object(context),
                                            "capability_comparison":digest_object(donor)},
            "evidence_counts":{k:kinds[k] for k in KINDS}, "raw":len(scoped),"callable":len(calls),
            "descriptors":len(descriptors),"exact":exact_verdict(r["ownership_status"] for r in calls),
            "e1_seeds":{key: value for key, value in seed_details.items() if int(key, 16) in SEEDS},
            "e1_admissions":seed_details, "propagation_seed_vas":[f"{pc:08x}" for pc in sorted(SEEDS)],
            "stream_anchor_words":anchor_words(original), "direct_jal_edges":len(graph),
            "direct_jal_sites":sum(len(sites) for sites in graph.values()),
            "game_string_anchors":{f"{a:08x}":v for a,v in sorted(game_anchors.items())},
            "clusters":boundaries,"adjacent_anchors":adjacent_anchors,
            "coarse_intervals_report_only":coarse_intervals,
            "cluster_policy":{"max_neighbor_distance_bytes":MAX_CLUSTER_GAP,
            "coarse_report_only_max_bytes":4096, "coarse_intervals_award_evidence":False,
            "max_intervening_catalogue_entries":MAX_INTERVENING,"minimum_anchors":2},
            "scope":"E5 is a one-hop inference. E6 uses prior renderer-family observations or original game-specific string loads as endpoints. Multiple inferred families retain unresolved ownership. Counts by evidence kind overlap."}
