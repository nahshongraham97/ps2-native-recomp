"""Read-only original compiler/debug fingerprints and fixed40 codegen idioms.

The original .comment string, container markers, retained debug metadata and
instruction-shape observations are separate evidence. MWo3 alone never decides
compiler identity. No compiler, oracle/game process, ownership or credit change.
"""
from __future__ import annotations
import argparse,collections,csv,hashlib,importlib.util,json,re,shlex,shutil,struct,subprocess
from pathlib import Path

INPUT_PINS={"SLUS_214.10":"a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f",
 "MOVIER.ovl":"3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8",
 "GAMER.ovl":"70714c6f147128fb6462ac57bfe28e3e27a0dcf90730c4054e90c70908feba89",
 "NETR.ovl":"6371c8bd507b62a6cb54431a8735703dcf69919c794c2d2c404957840106d08d"}
SAMPLE_PIN="a451c3206baa9e9eb58ce64cbbd051c9071e2b0c49762e4812d558191a6b6e47"
POPULATION_PIN="01239cb9ec1cd96ce5830bfca12e4f0f7beefa0e497a7e9184f4c5abcc7437cb"
MARKERS={"metrowerks":rb"MW MIPS C Compiler|Metrowerks|CodeWarrior|\bMWCC\b",
 "gcc":rb"GCC:|\bGCC\b|GNU C(?:\+\+)?|__GNUC__|cc1plus",
 "sn_systems":rb"SN Systems|ProDG|\bSNC\b|SN-Compiler|SN C(?:\+\+)? Compiler",
 "generic_compiler":rb"\bcompiler\b",
 "cxx_abi_candidate":rb"^_ZTV|^_ZTI|^_ZTS|^__vt__|^__RTTI__"}

def sha(data):return hashlib.sha256(data).hexdigest()
def need(value,why):
    if not value:raise ValueError(why)

def elf_metadata(raw):
    need(raw[:6]==b"\x7fELF\x01\x01","ELF32LE required")
    fields=struct.unpack_from("<HHIIIIIHHHHHH",raw,16)
    typ,machine,version,entry,phoff,shoff,flags,ehsize,phsize,phnum,shsize,shnum,shnames=fields
    need(phsize>=32 and shsize>=40,"ELF entry sizes")
    need(phoff+phsize*phnum<=len(raw) and shoff+shsize*shnum<=len(raw),"ELF table bounds")
    programs=[]
    for i in range(phnum):
        p=struct.unpack_from("<8I",raw,phoff+i*phsize)
        need(p[1]+p[4]<=len(raw),"program file bounds")
        programs.append(dict(index=i,type=p[0],file_offset=p[1],vaddr=p[2],physical_address=p[3],file_bytes=p[4],memory_bytes=p[5],flags=p[6],alignment=p[7]))
    rows=[struct.unpack_from("<10I",raw,shoff+i*shsize)for i in range(shnum)]
    need(shnames<shnum,"section-name table index")
    ns=rows[shnames];need(ns[4]+ns[5]<=len(raw),"section-name table bounds");names=raw[ns[4]:ns[4]+ns[5]]
    sections=[]
    for i,s in enumerate(rows):
        need(s[0]<len(names) or s[0]==0,"section name bounds")
        name=names[s[0]:].split(b"\0",1)[0].decode("ascii",errors="replace")
        if s[1]!=8:need(s[4]+s[5]<=len(raw),"section bounds")
        item=dict(index=i,name=name,type=s[1],flags=s[2],vaddr=s[3],file_offset=s[4],size=s[5],link=s[6],info=s[7],alignment=s[8],entry_bytes=s[9],allocated=bool(s[2]&2))
        if name==".comment":
            blob=raw[s[4]:s[4]+s[5]];item.update(raw_hex=blob.hex(),sha256=sha(blob),strings=[b.decode("ascii",errors="replace")for b in blob.split(b"\0")if b],runtime_VA_provenance="Not allocated; sh_addr0 is metadata, not a runtime virtual address.")
        if s[1]==0x70000006:
            need(s[5]==24,"MIPS REGINFO serialized size")
            regwords=struct.unpack_from("<6I",raw,s[4]);blob=raw[s[4]:s[4]+24]
            item["MIPS_REGINFO"]=dict(gpr_mask=regwords[0],coprocessor_masks=list(regwords[1:5]),gp_value=regwords[5],gp_value_hex=f"{regwords[5]:08x}",gp_value_file_offset=s[4]+20,raw_hex=blob.hex(),sha256=sha(blob))
        if s[1]in(2,11):item["symbol_count"]=s[5]//s[9] if s[9] else 0
        sections.append(item)
    sought=[".debug",".mdebug",".exceptix",".lit4",".lit8",".sdata",".sbss",".rodata",".rdata",".text",".data"]
    section_facts={name:dict(present=any(s["name"]==name for s in sections),matching_sections=[s["index"]for s in sections if s["name"]==name])for name in sought}
    merged=[dict(section_index=s["index"],vaddr=s["vaddr"],file_offset=s["file_offset"],size=s["size"],flags=s["flags"])for s in sections if not s["name"]and s["size"]and s["flags"]&7==7]
    return dict(format="original_ELF32LE",section_facts=section_facts,unnamed_allocated_RWX_sections=merged,section_absence_limit="Absent .rodata/.sdata/.lit4/.exceptix names in a merged main section cannot establish -strreadonly, small-data thresholds, Cpp_exceptions, or other original compiler flags.",header=dict(type=typ,machine=machine,version=version,entry=entry,flags=flags,section_count=shnum,program_count=phnum),program_headers=programs,sections=sections,
        debug_sections=[s for s in sections if s["name"]in(".mdebug",".debug",".stab",".stabstr")or s["name"].startswith(".debug")],
        retained_symbol_count=sum(s.get("symbol_count",0)for s in sections),debug_limit="Section-table observation only; this does not infer compiler options, symbol contents in absent sections, or a uniform toolchain for every linked input object.")

def locate(metadata,offset,length):
    if metadata["format"]=="original_ELF32LE":
        mappings=[dict(vaddr=p["vaddr"]+offset-p["file_offset"],program_index=p["index"],mapping="original PT_LOAD file mapping")for p in metadata["program_headers"]if p["type"]==1 and p["file_offset"]<=offset and offset+length<=p["file_offset"]+p["file_bytes"]]
        sections=[s["name"]or f"<unnamed section {s['index']}>"for s in metadata["sections"]if s["type"]!=8 and s["file_offset"]<=offset and offset+length<=s["file_offset"]+s["size"]]
        return dict(runtime_mappings=mappings,containing_sections=sections)
    return dict(runtime_mappings=[dict(vaddr=metadata["container"]["load_address"]+offset,mapping="Original MWo3 load-image base + file offset; observed loader mapping, not an invented ELF section")],containing_sections=[],section_limit="Raw container has no ELF section table; original internal debug encoding, if any, is not established by an analysis wrapper.")

def string_markers(raw,metadata):
    hits=[]
    for match in re.finditer(rb"[\x20-\x7e\t]{4,}",raw):
        blob=match[0];categories=[name for name,pattern in MARKERS.items()if re.search(pattern,blob,re.I)]
        if categories:hits.append(dict(file_offset=match.start(),bytes=len(blob),text=blob.decode("ascii"),categories=categories,sha256=sha(blob),**locate(metadata,match.start(),len(blob))))
    return hits

def input_word(raw,metadata,va):
    if metadata["format"]=="original_ELF32LE":
        for p in metadata["program_headers"]:
            delta=va-p["vaddr"]
            if p["type"]==1 and 0<=delta and delta+4<=p["file_bytes"]:
                at=p["file_offset"]+delta;return at,struct.unpack_from("<I",raw,at)[0]
    else:
        at=va-metadata["container"]["load_address"]
        if 0<=at and at+4<=len(raw):return at,struct.unpack_from("<I",raw,at)[0]
    raise ValueError(f"sample VA not file-backed:{va:08x}")

def opcode_name(word):
    op=word>>26
    return {0:"SPECIAL",1:"REGIMM",2:"J",3:"JAL",4:"BEQ",5:"BNE",6:"BLEZ",7:"BGTZ",8:"ADDI",9:"ADDIU",10:"SLTI",11:"SLTIU",12:"ANDI",13:"ORI",14:"XORI",15:"LUI",16:"COP0",17:"COP1",18:"COP2",20:"BEQL",21:"BNEL",22:"BLEZL",23:"BGTZL",24:"DADDI",25:"DADDIU",28:"MMI",30:"LQ",31:"SQ",32:"LB",33:"LH",35:"LW",36:"LBU",37:"LHU",39:"LWU",40:"SB",41:"SH",43:"SW",49:"LWC1",55:"LD",57:"SWC1",63:"SD"}.get(op,f"primary_{op:02x}")

def sample_fingerprints(sample,images):
    output=[];aggregate=collections.Counter()
    for row in sample["sample"]:
        raw,metadata=images[row["module"]];words=[]
        for low,high in row["ranges"]:
            begin,end=int(low,16),int(high,16);need((end-begin)%4==0,"sample instruction alignment")
            for pc in range(begin,end,4):
                offset,w=input_word(raw,metadata,pc);words.append(dict(va=f"{pc:08x}",file_offset=offset,word_le=w.to_bytes(4,"little").hex(),primary_opcode=opcode_name(w),raw_word=w))
        body=b"".join(bytes.fromhex(w["word_le"])for w in words)
        need(sha(body)==row["original_body_sha256"] and len(words)==row["instructions"],"frozen sample original-body join")
        counts=collections.Counter();idioms=[]
        for i,item in enumerate(words):
            w=item["raw_word"];op=w>>26;rs=(w>>21)&31;rt=(w>>16)&31;funct=w&63;imm=w&0xffff
            counts["opcode_"+item["primary_opcode"]]+=1
            tags=[]
            if w==0:tags.append("NOP")
            if op in (9,25)and rs==29 and rt==29:
                signed=imm-65536 if imm&0x8000 else imm;tags.append("stack_adjust");item["signed_immediate"]=signed
                if signed<0 and signed%16==0:tags.append("negative_16byte_aligned_stack_frame")
            if rs==29 and op in (0x1e,0x1f,0x37,0x3f):tags.append({0x1e:"stack_LQ128",0x1f:"stack_SQ128",0x37:"stack_LD64",0x3f:"stack_SD64"}[op])
            if op==0 and funct==0x2d and rt==0:tags.append("GPR_move_via_DADDU")
            if op==0 and funct==0x25 and rt==0:tags.append("GPR_move_via_OR")
            if op==0 and funct==0x21 and rt==0:tags.append("GPR_move_via_ADDU32")
            if op in (20,21,22,23)or(op==1 and rt in(2,3,18,19))or(op in(16,17,18)and rs==8 and rt in(2,3)):tags.append("branch_likely")
            if op==11 and imm==1:tags.append("SLTIU_equals_zero_idiom")
            if w==0x03e00008:
                tags.append("JR_RA")
                if i+1<len(words)and int(words[i+1]["va"],16)==int(item["va"],16)+4:
                    delay=words[i+1]["raw_word"];item["return_delay_word_le"]=delay.to_bytes(4,"little").hex()
                    if delay==0:tags.append("return_delay_NOP")
                    if delay>>26==9 and(delay>>21)&31==0:tags.append("return_delay_load_immediate")
                    if delay>>26 in(0x28,0x29,0x2b,0x39,0x3f):tags.append("return_delay_store")
            for tag in tags:counts[tag]+=1
            if tags:idioms.append(dict(va=item["va"],word_le=item["word_le"],tags=tags))
        aggregate.update(counts)
        for item in words:del item["raw_word"]
        output.append(dict(module=row["module"],address=row["address"],instruction_count=len(words),threshold_25_to_120=25<=len(words)<=120,selection=row["selection"],original_body_sha256=sha(body),idiom_counts=dict(counts),idioms=idioms,original_words=words))
    return output,dict(aggregate)

def wsl_path(path):
    value=Path(path).resolve().as_posix();need(len(value)>2 and value[1:3]==":/","WSL drive mapping required")
    return "/mnt/"+value[0].lower()+"/"+value[3:]

def gp_identity(images,fork,parent):
    raw,metadata=images["SLUS_214.10"]
    sections=[s for s in metadata["sections"]if "MIPS_REGINFO"in s]
    need(len(sections)==1,"unique original MIPS REGINFO")
    reginfo=sections[0]["MIPS_REGINFO"]
    expected={0x100198:0x3c040065,0x1001ac:0x248440f0,0x1001c0:0x0080e025}
    words=[]
    for pc in range(0x100198,0x1001c4,4):
        offset,word=input_word(raw,metadata,pc)
        if pc in expected:need(word==expected[pc],"original startup GP word changed")
        words.append(dict(va=f"{pc:08x}",file_offset=offset,word_le=word.to_bytes(4,"little").hex()))
    need(reginfo["gp_value"]==0x6540f0,"original REGINFO/startup GP disagreement")
    mapping=fork/"tools/research/retail_data_symbol_map.py"
    return dict(main_gp_value=reginfo["gp_value"],main_gp_value_hex=reginfo["gp_value_hex"],reginfo=reginfo,startup_words=words,
       original_dataflow="00100198 LUI a0,0x65; 001001AC ADDIU a0,a0,0x40F0; 001001C0 OR gp,a0,zero. Intervening words set other argument registers; no a0 clobber. Both original proofs yield 006540F0 before SetupThread syscall.",
       old_GP_map_source=dict(path=str(mapping),sha256=sha(mapping.read_bytes()),scope="Earlier offline source map verifies the same original ELF, REGINFO bytes and GP; no native owner/pointer binding."),
       GP_REL16_scope="Static compiler matching may compute S+A-006540F0 only where that main GP contract is proved. This is address relocation evidence, not native field or pointer binding.",
       overlays={name:dict(gp_value=None,status="UNPROVED_REFUSE_GP_REL16",reason="Raw MWo3 container has no independently authenticated module GP metadata/initializer in this receipt; never inherit main GP merely by filename or container magic.")for name in("MOVIER.ovl","GAMER.ovl","NETR.ovl")})

def callable_traits(images,fork):
    population_path=fork/"build/oracle-telemetry/leaf-cohort-full-05-frozen17/POPULATION.json"
    data=population_path.read_bytes();need(sha(data)==POPULATION_PIN,"fixed callable population pin")
    ledger_path=fork/"research/ledger/functions.csv";ledger={r["module"]+":"+r["address"]:r for r in csv.DictReader(ledger_path.open(encoding="utf-8-sig"))}
    rows=[r for r in json.loads(data)if r["key"]in ledger and ledger[r["key"]]["catalogue_kind"]=="callable"]
    need(len(rows)==15669,"full callable denominator")
    partitions=collections.Counter();by_module=collections.defaultdict(collections.Counter);output=[];verified=[]
    for row in rows:
        key=row["key"];entry=int(row["address"],16);alignment=16 if entry%16==0 else 8 if entry%8==0 else 4 if entry%4==0 else 1
        words=[];raw,metadata=images[row["module"]]
        try:
            for span in row["body_ranges"]:
                for pc in range(int(span["start"],16),int(span["end_exclusive"],16),4):words.append(input_word(raw,metadata,pc)[1])
            need(sha(b"".join(word.to_bytes(4,"little")for word in words))==row["body_bytes_sha256"],"original callable body pin:"+key)
            likely=sum((w>>26 in(20,21,22,23))or(w>>26==1 and(w>>16)&31 in(2,3,18,19))or(w>>26 in(16,17,18)and(w>>21)&31==8 and(w>>16)&31 in(2,3))for w in words)
            trait="branch_likely_present"if likely else"no_branch_likely_observed_in_catalogued_body"
            status="ORIGINAL_BODY_BYTES_JOINED"
        except ValueError as error:
            likely=None;trait="UNRESOLVED_BODY_MAPPING";status=str(error)
        label=trait+";entry_alignment="+str(alignment)
        record=dict(key=key,module=row["module"],entry_alignment_bytes=alignment,branch_likely_count=likely,partition=label,compiler_attribution="HEURISTIC_ONLY_UNASSIGNED",instruction_count=len(words),original_body_sha256=row["body_bytes_sha256"],body_join=status,
          decoded_listing_coverage=dict(joined_words=row.get("listing_joined_words"),missing_words=row.get("missing_listing_words")),original_CFG_authority=row.get("body_authority"),
          limit="Trait extraction covers original catalogued ranges, not proof every byte is reachable code, compiler-unique, or one translation unit. Overlay rows inherit their declared original range authority, not newly recovered ELF/debug/CFG metadata.")
        partitions[label]+=1;by_module[row["module"]][label]+=1;output.append(record)
        owner=ledger[key]["ownership_status"]
        if owner.startswith("verified-"):verified.append(dict(record,existing_ownership=owner,comparison_scope="Existing independently admitted ownership only; style traits never infer or change that ownership."))
    need(sum(partitions.values())==15669 and len(verified)==18,"complete heuristic partition and unchanged18 owners")
    return dict(label="HEURISTIC_ONLY_NO_COMPILER_OR_OWNERSHIP_ADMISSION",population_sha256=POPULATION_PIN,ledger_sha256=sha(ledger_path.read_bytes()),callable_count=15669,
       partition_counts=dict(partitions),module_partition_counts={module:dict(counts)for module,counts in by_module.items()},verified_owner_comparison=verified,functions=output,
       inference_limit="Branch-likely usage/alignment depend on ISA, ABI, optimization, function size, linker padding and mixed libraries. These partitions do not identify MW/GCC/SN, prove exact flags, or provide an ownership denominator.")

def stdump_runs(args,paths,output):
    if not args.stdump and not args.stdump_wsl:return dict(status="NOT_RUN_TOOL_NOT_YET_SUPPLIED",limit="No symbols inferred from format markers; source ELF and raw overlays await the separately installed stdump tool.")
    records=[]
    def command(arguments):
        if not args.stdump_wsl:return [str(args.stdump),*arguments]
        script="source /opt/mkar-decomp/toolchain/env.sh; exec taskset -c 0 "+shlex.quote(args.stdump_wsl)+" "+" ".join(shlex.quote(argument)for argument in arguments)
        return ["wsl.exe","-d",args.wsl_distro,"-u","root","--exec","bash","-lc",script]
    def run(argv,label,input_path=None):
        done=subprocess.run(argv,cwd=output.parent,capture_output=True,timeout=60);logs={}
        for name,data in [("stdout",done.stdout),("stderr",done.stderr)]:
            target=output.parent/("stdump-"+label+"."+name+".log");need(not target.exists(),"preserve_existing_stdump_log");target.write_bytes(data);logs[name]=dict(path=str(target),sha256=sha(data),bytes=len(data))
        record=dict(argv=argv,exit=done.returncode,logs=logs,stdout_preview=done.stdout[:1500].decode(errors="replace"),stderr_preview=done.stderr[:1500].decode(errors="replace"))
        if input_path:record.update(input=str(input_path),input_sha256=sha(input_path.read_bytes()))
        return record
    if args.stdump_wsl:
        identity=run(["wsl.exe","-d",args.wsl_distro,"-u","root","--exec","sha256sum",args.stdump_wsl,"/opt/mkar-decomp/toolchain/env.sh"],"tool-identity")
    else:identity=dict(path=str(args.stdump),sha256=sha(args.stdump.read_bytes()))
    help_result=run(command(["--help"]),"help")
    for path in paths:
        for subcommand in("identify","symbols"):
            argument=wsl_path(path)if args.stdump_wsl else str(path)
            records.append(run(command([subcommand,argument]),path.name+"-"+subcommand,path))
    return dict(status="EXECUTED_RAW_ORIGINAL_INPUTS",tool_identity=identity,help=help_result,single_CPU="WSL taskset CPU0; one sequential stdump process at a time"if args.stdump_wsl else"one process at a time",results=records,
       limit="Record unsupported-format/no-debug responses literally. A synthetic analysis ELF does not recover original debug sections or symbols.")

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("--sample",type=Path,required=True);ap.add_argument("--output",type=Path,required=True);ap.add_argument("--stdump",type=Path);ap.add_argument("--stdump-wsl");ap.add_argument("--wsl-distro",default="MKAR-Decomp");ap.add_argument("--sample-sha256",default=SAMPLE_PIN);args=ap.parse_args()
    need(not(args.stdump and args.stdump_wsl),"choose one stdump runtime")
    fork=Path(__file__).resolve().parents[2];parent=fork.parents[2];output=args.output.resolve()
    need(not output.exists()and output.is_relative_to((fork/"build/oracle-telemetry").resolve()),"new owned private output required")
    need(shutil.disk_usage(fork).free>=12*(1<<30),"12GiB reserve")
    sample_raw=args.sample.read_bytes();need(sha(sample_raw)==args.sample_sha256,"frozen sample changed");sample=json.loads(sample_raw)
    need((sample["total"],sample["threshold_denominator"])in[(40,29),(51,40)]and sum(r["module"]=="MOVIER.ovl"for r in sample["sample"])>=10,"fixed sample identity")
    spec=importlib.util.spec_from_file_location("original_overlay_inspection",parent/"tools/research/overlay_image.py");module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    paths=[parent/"research/inputs/SLUS_214.10",*[parent/"research/inputs/modules"/name for name in("MOVIER.ovl","GAMER.ovl","NETR.ovl")]]
    images={};inputs=[]
    for path in paths:
        data=path.read_bytes();need(sha(data)==INPUT_PINS[path.name],"original input identity:"+path.name)
        metadata=elf_metadata(data)if data[:4]==b"\x7fELF"else dict(format="original_raw_MWo3",container=module.inspect(data),original_ELF_section_table=False,compiler_inference="MWo3 is an observed container marker, not standalone compiler identity or version evidence.")
        markers=string_markers(data,metadata);images[path.name]=(data,metadata)
        inputs.append(dict(path=str(path),module=path.name,bytes=len(data),sha256=sha(data),metadata=metadata,compiler_and_ABI_string_markers=markers,marker_count_by_category=dict(collections.Counter(category for row in markers for category in row["categories"]))))
    fingerprints,aggregate=sample_fingerprints(sample,images)
    report=dict(schema="mkar.retail-compiler-identification.v1",inputs=inputs,sample=dict(path=str(args.sample),sha256=args.sample_sha256,total=sample["total"],threshold_denominator=sample["threshold_denominator"],MOVIER_count=sum(r["module"]=="MOVIER.ovl"for r in sample["sample"]),functions=fingerprints,aggregate_idiom_counts=aggregate),
      main_GP_identity=gp_identity(images,fork,parent),callable_style_heuristics=callable_traits(images,fork),
      compiler_assessment=dict(main_ELF_direct_evidence="Original nonallocated .comment names MW MIPS C Compiler (2.4.1.01) and PlayStation2.",scope="Direct backend compiler-version string in the linked main image; it does not identify an exact Metrowerks frontend/product release or prove all linked objects/overlays used that compiler/version. User-supplied release/flag claims remain separate hypotheses.",overlay_assessment="Raw MWo3 containers and sampled instruction idioms do not independently establish compiler identity/version.",idiom_limit="Stack/SQ/SD saves, DADDU moves, delayed returns and branch-likely scheduling are descriptive R5900/ABI/codegen facts, not unique compiler fingerprints. Matching still requires the fixed compiler/flag experiment.",original_debug="Main ELF has empty .symtab/.strtab and no .mdebug/.debug/.stab section names. No original debug symbols are recovered by wrapping overlays for analysis."),
      stdump=stdump_runs(args,paths,output),tool_sha256=sha(Path(__file__).read_bytes()),overlay_inspector_sha256=sha((parent/"tools/research/overlay_image.py").read_bytes()),native_or_ownership_credit=0,game_run=False)
    output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(json.dumps(dict(output=str(output),sha256=sha(output.read_bytes()),sample_functions=sample["total"],threshold_denominator=sample["threshold_denominator"],stdump=report["stdump"]["status"],markers={row["module"]:row["marker_count_by_category"]for row in inputs})))
if __name__=="__main__":main()
