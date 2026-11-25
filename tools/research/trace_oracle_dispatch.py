"""Externally observe a disposable pinned B9/Practice oracle using an x64 hardware breakpoint.

No software breakpoint, injected code, binary write, or original oracle launch.
Requires a fresh --Prepare --Headless plan from the existing protected launcher.
Records guest dispatch targets and admits callers only when original JAL/JALR
words at RA-8 target that exact callee inside a bounded catalogue span. Indirect
calls require the actual source register and a non-clobbering delay instruction.
"""
from __future__ import annotations
import argparse, bisect, collections, csv, ctypes as C, hashlib, json, os
from pathlib import Path
import struct, subprocess, time, importlib.util

B9='b9eef7dad3cfa0e1216493cf12301f14aa4b3a8b1456fcdc60d9c39502a0d860'
PRACTICE='8b763dcc3e0c47188b535803117fa44cf81d9b7f36d103df3247336b16a321b9'
ELF='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def u32(b,o):return struct.unpack_from('<I',b,o)[0]
def u64(b,o):return struct.unpack_from('<Q',b,o)[0]

def host_symbols(raw):
    pe=u32(raw,0x3c)
    if raw[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',raw,pe+4)[0]!=0x8664:raise ValueError('x64 PE required')
    count=struct.unpack_from('<H',raw,pe+6)[0];table=u32(raw,pe+12);n=u32(raw,pe+16);opt=struct.unpack_from('<H',raw,pe+20)[0]
    image=u64(raw,pe+24+24);first=pe+24+opt;strings=table+n*18;sections=[]
    for i in range(count):
        at=first+40*i;sections.append((u32(raw,at+12),u32(raw,at+20),u32(raw,at+16)))
    result={};i=0;units=[]
    while i<n:
        at=table+i*18;part=raw[at:at+18];i+=1+part[17]
        section=struct.unpack_from('<h',part,12)[0]
        if section<1 or section>len(sections):continue
        if part[:4]==b'\0'*4:
            off=strings+u32(part,4);end=raw.find(b'\0',off);name=raw[off:end].decode('ascii',errors='replace')
        else:name=part[:8].split(b'\0')[0].decode('ascii',errors='replace')
        if name.startswith('_ZN3gen') and 'unit_' in name:
            va,off,size=sections[section-1];units.append({'rva':va+u32(part,8),'symbol':name})
        if name in ['_ZN3ps28dispatchERNS_3CpuEjj','_ZN3ps24callERNS_3CpuEjj','_ZN3ps25probeERNS_3CpuEj']:
            va,off,size=sections[section-1];delta=u32(part,8)
            result[name]={'rva':va+delta,'bytes':raw[off+delta:off+delta+112].hex()}
    dispatch=result['_ZN3ps28dispatchERNS_3CpuEjj'];call=result['_ZN3ps24callERNS_3CpuEjj'];probe=result['_ZN3ps25probeERNS_3CpuEj']
    if bytes.fromhex(call['bytes'])[:6]!=bytes.fromhex('899124020000'):raise ValueError('Cpu.pc ABI instruction mismatch')
    if not any(bytes.fromhex(op) in bytes.fromhex(dispatch['bytes']) for op in ['8bb124020000','8bb924020000']):raise ValueError('dispatch pc read ABI mismatch')
    if bytes.fromhex('448b9ff0010000') not in bytes.fromhex(probe['bytes']):raise ValueError('Cpu.ra ABI instruction mismatch')
    result['_unit_symbols']=sorted(units,key=lambda x:x['rva'])
    return result

def host_link_stores(prefix,registers,cpu,ra):
    """Small exact x64 C7 /0 disp32+imm32 decoder, never a text-pattern verdict."""
    found=[]
    for at in range(len(prefix)-10):
        rex,op,modrm=prefix[at:at+3]
        if rex not in [0x48,0x49] or op!=0xc7 or modrm>>6!=2 or (modrm>>3)&7:continue
        base=(modrm&7)+(8 if rex&1 else 0);cursor=at+3
        if modrm&7==4:
            sib=prefix[cursor];cursor+=1
            if (sib>>3)&7!=4:continue
            base=(sib&7)+(8 if rex&1 else 0)
        if cursor+8>len(prefix):continue
        displacement,immediate=struct.unpack_from('<II',prefix,cursor)
        if displacement==0x1f0 and immediate==ra and registers[base]==cpu:
            found.append({'prefix_offset':at,'host_base_register':base,'base_value':f'{cpu:016x}','instruction_hex':prefix[at:cursor+8].hex()})
    return found

def writes_register(word):
    if word==0:return None
    op=word>>26
    if op in [0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x3f,0x1f,0x39,0x3e]:return None
    if op in [8,9,10,11,12,13,14,15,0x18,0x19,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x37,0x1e]:return (word>>16)&31
    if op==0 and (word&63) in [0,2,3,4,6,7,0x10,0x12,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x38,0x3a,0x3b,0x3c,0x3e,0x3f]:return (word>>11)&31
    return -1

def callsite(raw,ra,target,spans,registers=None):
    site=(ra-8)&0xffffffff
    if not 0x100000<=site<=0x64de7c:return None,'return address outside original file-backed image'
    word=u32(raw,site-0x100000+0x100)
    actual=((site+4)&0xf0000000)|((word&0x3ffffff)<<2)
    extra={}
    if word>>26==3 and actual==target:extra['call_kind']='jal'
    elif word>>26==0 and word&63==9 and (word>>11)&31==31 and registers is not None:
        rs=(word>>21)&31;delay=u32(raw,site-0x100000+0x104)
        writer=writes_register(delay)
        if rs==31 or writer in [-1,rs,31] or registers[rs]!=target:
            return None,'JALR target register or delay preservation not proven'
        extra={'call_kind':'jalr','jalr_register':rs,'jalr_register_value':f'{registers[rs]:08x}','delay_word_le':struct.pack('<I',delay).hex(),'target_register_preserved_by_delay':True}
    else:return None,'RA is inherited, indirect or unmatched; not an admitted caller'
    starts=[x[0] for x in spans];i=bisect.bisect_right(starts,site)-1
    if i<0 or not spans[i][0]<=site<spans[i][1]:return None,'JAL site lacks a bounded source-map caller'
    begin,end,name=spans[i]
    return {'caller':f'{begin:08x}','caller_callsite':f'{site:08x}','call_site':f'{site:08x}','original_call_site_word':f'{word:08x}','caller_contiguous_end':f'{end:08x}','caller_name':name,'jal_word_le':struct.pack('<I',word).hex(),'return_address':f'{ra:08x}','caller_contiguous_span_sha256':hashlib.sha256(raw[begin-0x100000+0x100:end-0x100000+0x100]).hexdigest(),**extra},None

def self_test():
    raw=bytearray(0x200);struct.pack_into('<I',raw,0x120,0x0c040100)
    rows=[(0x100000,0x100100,'synthetic')]
    assert callsite(raw,0x100028,0x100400,rows)[0]
    assert callsite(raw,0x100028,0x100404,rows)[0] is None
    struct.pack_into('<I',raw,0x120,0x03e00008)
    assert callsite(raw,0x100028,0x100400,rows)[0] is None
    struct.pack_into('<II',raw,0x120,0x0040f809,0x27a50040);regs=[0]*32;regs[2]=0x100400
    assert callsite(raw,0x100028,0x100400,rows,regs)[0]
    regs[2]+=4;assert callsite(raw,0x100028,0x100400,rows,regs)[0] is None
    regs[2]-=4;struct.pack_into('<I',raw,0x124,0x24020001)
    assert callsite(raw,0x100028,0x100400,rows,regs)[0] is None
    hr=[0]*16;hr[3]=0x10000000;prefix=bytes.fromhex('48c783f001000060da3100')
    assert host_link_stores(prefix,hr,hr[3],0x31da60)
    assert not host_link_stores(prefix,hr,hr[3]+16,0x31da60)
    return {'status':'PASS','planted_failures':['wrong direct target rejected','inherited/return RA rejected','wrong indirect register rejected','delay-slot target-register clobber rejected','host RA store with wrong CPU owner rejected'],'synthetic_only':True}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--plan',type=Path);ap.add_argument('--abi',type=Path);ap.add_argument('--targets',type=Path);ap.add_argument('--elf',type=Path);ap.add_argument('--source-map',type=Path);ap.add_argument('--output',type=Path);ap.add_argument('--seconds',type=int,default=45);ap.add_argument('--max-events',type=int,default=200000);ap.add_argument('--stop-after-edges',type=int,default=0);ap.add_argument('--self-test',action='store_true');a=ap.parse_args()
    if a.self_test:print(json.dumps(self_test(),indent=2));return
    if os.name!='nt':raise RuntimeError('Windows debugger required')
    if not all([a.plan,a.abi,a.targets,a.elf,a.source_map,a.output]):ap.error('all input paths and --output required')
    import msvcrt
    plan=json.loads(a.plan.read_text('utf-8-sig'));abi=json.loads(a.abi.read_text('utf-8-sig'));root=Path(plan['run_root']).resolve();exe=Path(plan['executable']).resolve()
    fork=Path(__file__).resolve().parents[2];parent=fork.parents[2]
    spec=importlib.util.spec_from_file_location('protected_oracle_audit',parent/'tools/research/verify_oracle.py');verifier=importlib.util.module_from_spec(spec);spec.loader.exec_module(verifier)
    issues=verifier.audit_plan(plan)
    if issues:raise ValueError('protected launcher plan audit refused: '+repr(issues))
    if not plan.get('prepared') or not plan.get('requested_headless') or plan['blockers']:raise ValueError('fresh prepared headless plan required')
    expected=PRACTICE if plan['mode']=='FullHooks' else B9
    if not exe.is_relative_to(root) or sha(exe)!=expected:raise ValueError('only the exact mode-pinned disposable oracle is permitted')
    if any(x in plan['arguments'] for x in ['--window','--fullscreen']):raise ValueError('visible route refused')
    if '--frames' not in plan['arguments'] or not 1<=int(plan['arguments'][plan['arguments'].index('--frames')+1])<=300000:raise ValueError('protected launcher frame bound required')
    for p in plan['mutable_paths'].values():
        if not Path(p).resolve().is_relative_to(root):raise ValueError('mutable path escaped disposable root')
    original=a.elf.read_bytes()
    if hashlib.sha256(original).hexdigest()!=ELF:raise ValueError('original ELF pin changed')
    symbols=host_symbols(exe.read_bytes());rva=symbols['_ZN3ps28dispatchERNS_3CpuEjj']['rva']
    target_doc=json.loads(a.targets.read_text('utf-8'));targets={int(x['address'],16) for x in target_doc['entries']+target_doc.get('source_proved_starts_outside_in_scope_raw_catalogue_not_counted',[])}
    catalogue_raw=(parent/'research/ledger/functions.csv').read_bytes()
    catalogue={int(r['address'],16) for r in csv.DictReader(catalogue_raw.decode('utf-8-sig').splitlines()) if r['module']=='SLUS_214.10' and int(r['address'],16)!=0x005bf218}
    spans=sorted((int(r['Start'],16),int(r['End'],16),r['Name']) for r in csv.DictReader(a.source_map.read_text('utf-8-sig').splitlines()) if int(r['Start'],16) in catalogue)
    k=C.WinDLL('kernel32',use_last_error=True);P=C.c_void_p;U=C.c_uint32;B=C.c_int
    declarations={'CreateProcessW':([C.c_wchar_p,C.c_wchar_p,P,P,B,U,P,C.c_wchar_p,P,P],B),'WaitForDebugEvent':([P,U],B),'ContinueDebugEvent':([U,U,U],B),'OpenThread':([U,B,U],P),'GetThreadContext':([P,P],B),'SetThreadContext':([P,P],B),'ReadProcessMemory':([P,P,P,C.c_size_t,P],B),'CloseHandle':([P],B),'TerminateProcess':([P,U],B)}
    for name,(args,res) in declarations.items():getattr(k,name).argtypes=args;getattr(k,name).restype=res
    def check(ok,name):
        if not ok:raise C.WinError(C.get_last_error(),name)
    def context(tid):
        handle=k.OpenThread(0x1a,False,tid);check(handle,'OpenThread');storage=C.create_string_buffer(abi['context_size']+16);at=(C.addressof(storage)+15)&~15
        struct.pack_into('<I',storage,at-C.addressof(storage)+abi['context_flags'],0x100013)
        check(k.GetThreadContext(handle,at),'GetThreadContext');return handle,storage,at,at-C.addressof(storage)
    def arm(tid,bp,call_bp):
        h,b,at,off=context(tid)
        try:
            struct.pack_into('<Q',b,off+abi['dr0'],bp);struct.pack_into('<Q',b,off+abi['dr1'],call_bp);struct.pack_into('<Q',b,off+abi['dr7'],5);struct.pack_into('<Q',b,off+abi['dr6'],0);check(k.SetThreadContext(h,at),'SetThreadContext')
        finally:k.CloseHandle(h)
    out_path=Path(plan['mutable_paths']['stdout']);err_path=Path(plan['mutable_paths']['stderr'])
    env={key:value for key,value in os.environ.items() if not key.startswith(('PS2_','MKA_','SDL_'))};env.update(plan['environment']);env['SDL_AUDIODRIVER']='dummy'
    compiler=parent.parent/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
    system_root=os.environ['SystemRoot']
    env['PATH']=';'.join([str(root),str(compiler),str(Path(system_root)/'System32'),system_root])
    block=C.create_unicode_buffer('\0'.join(key+'='+str(value) for key,value in sorted(env.items()))+'\0\0')
    si=C.create_string_buffer(abi['startup_size']);pi=C.create_string_buffer(abi['pi_size']);struct.pack_into('<I',si,0,abi['startup_size']);struct.pack_into('<I',si,abi['startup_flags'],0x100)
    command=C.create_unicode_buffer(subprocess.list2cmdline([str(exe),*plan['arguments']]))
    events=0;hits=[];seen=collections.Counter();rejected=collections.Counter();exit_code=None;termination=None;threads=set();errors=[];initial_bp=False;start=time.monotonic();pid=None;ph=None;edges=set();pending_calls={};image_base=None
    with open(out_path,'xb') as stdout,open(err_path,'xb') as stderr,open(os.devnull,'rb') as stdin:
        for offset,file in [(abi['stdin'],stdin),(abi['stdout'],stdout),(abi['stderr'],stderr)]:
            handle=msvcrt.get_osfhandle(file.fileno());os.set_handle_inheritable(handle,True);struct.pack_into('<Q',si,offset,handle)
        check(k.CreateProcessW(str(exe),command,None,None,True,0x08000000|0x400|2,block,str(root),si,pi),'CreateProcessW')
        ph=u64(pi,0);main_thread=u64(pi,8);pid=u32(pi,abi['pi_pid']);bp=None
        event=C.create_string_buffer(abi['event_size'])
        try:
            while True:
                if termination is None and (time.monotonic()-start>a.seconds or events>=a.max_events or (a.stop_after_edges and len(edges)>=a.stop_after_edges)):
                    termination='admitted_edge_bound' if a.stop_after_edges and len(edges)>=a.stop_after_edges else 'wall_bound' if events<a.max_events else 'event_bound';check(k.TerminateProcess(ph,124),'TerminateProcess')
                if not k.WaitForDebugEvent(event,100):
                    if C.get_last_error()==121:continue
                    raise C.WinError(C.get_last_error(),'WaitForDebugEvent')
                code,epid,tid=struct.unpack_from('<III',event,0);assert epid==pid;off=abi['event_union'];status=0x10002;events+=1
                if code==3:
                    image_base=u64(event,off+abi['create_image_base']);bp=image_base+rva;call_bp=image_base+symbols['_ZN3ps24callERNS_3CpuEjj']['rva'];threads.add(tid);arm(tid,bp,call_bp)
                elif code==2:
                    threads.add(tid);arm(tid,bp,call_bp)
                elif code==4:threads.discard(tid)
                elif code==5:exit_code=u32(event,off)
                elif code==1:
                    exception=u32(event,off);address=u64(event,off+abi['exception_address'])
                    if exception==0x80000004 and address in [bp,call_bp]:
                        h,b,at,co=context(tid)
                        try:
                            cpu=u64(b,co+abi['rcx']);mem=C.create_string_buffer(0x228);got=C.c_size_t()
                            check(k.ReadProcessMemory(ph,cpu,mem,len(mem),C.byref(got)) and got.value==len(mem),'ReadProcessMemory Cpu')
                            callee=u32(mem,0x224);ra=u32(mem,0x1f0);depth=u64(b,co+abi['r8'])&0xffffffff
                            if address==call_bp:
                                target=u64(b,co+abi['rdx'])&0xffffffff;rsp=u64(b,co+abi['rsp']);retbuf=C.create_string_buffer(8);got=C.c_size_t()
                                check(k.ReadProcessMemory(ph,rsp,retbuf,8,C.byref(got)) and got.value==8,'Read host return')
                                ret=u64(retbuf,0);prefix=C.create_string_buffer(96)
                                check(k.ReadProcessMemory(ph,ret-96,prefix,96,C.byref(got)) and got.value==96,'Read host caller prefix')
                                prefix=prefix.raw;direct=prefix[-5]==0xe8 and ret+struct.unpack_from('<i',prefix,92)[0]==call_bp
                                registers=[u64(b,co+x) for x in abi['gpr_offsets']]
                                stores=host_link_stores(prefix[:-5],registers,cpu,ra) if direct else []
                                rv=ret-image_base;unit=next((x for x in reversed(symbols['_unit_symbols']) if x['rva']<=rv),None)
                                pending_calls[(tid,cpu)]={'callee':target,'guest_ra':ra,'depth':depth,'event':events,'host_return_va':f'{ret:016x}','host_return_rva':f'{rv:08x}','host_caller_symbol':unit,'host_prefix_begin_rva':f'{rv-96:08x}','host_prefix_hex':prefix.hex(),'host_direct_call_to_helper':direct,'matching_cpu_ra_link_stores':stores}
                            else:seen[callee]+=1
                            if address==bp and callee in targets:
                                caller,reason=callsite(original,ra,callee,spans,[u32(mem,i*16) for i in range(32)])
                                if callee not in catalogue:caller=None;reason='callee is outside the current callable catalogue; raw observation only'
                                previous=pending_calls.pop((tid,cpu),None)
                                fresh=bool(previous and previous['callee']==callee and previous['guest_ra']==ra and previous['depth']==depth and previous['host_direct_call_to_helper'] and previous['matching_cpu_ra_link_stores'])
                                if caller and not fresh:caller=None;reason='host call-helper sequence and matching CPU RA link store not corroborated'
                                hit={'callee':f'{callee:08x}','guest_ra':f'{ra:08x}','depth':depth,'thread_id':tid,'event':events,'caller_admitted':caller is not None}
                                if previous:hit['preceding_host_call_event']=previous
                                if caller:hit.update(caller);edges.add((callee,caller['caller'],caller['caller_callsite']))
                                else:hit['caller_rejection']=reason;rejected[reason]+=1
                                hits.append(hit)
                            elif address==bp:pending_calls.pop((tid,cpu),None)
                            struct.pack_into('<I',b,co+abi['eflags'],u32(b,co+abi['eflags'])|0x10000);struct.pack_into('<Q',b,co+abi['dr6'],0);check(k.SetThreadContext(h,at),'ResumeFlag hardware breakpoint')
                        finally:k.CloseHandle(h)
                    elif exception==0x80000003 and not initial_bp:initial_bp=True
                    else:status=0x80010001
                if code in [3,6]:
                    hf=u64(event,off)
                    if hf:k.CloseHandle(hf)
                check(k.ContinueDebugEvent(epid,tid,status),'ContinueDebugEvent')
                if code==5:break
        except BaseException as error:
            errors.append(repr(error))
            if ph:k.TerminateProcess(ph,125)
            raise
        finally:
            if main_thread:k.CloseHandle(main_thread)
            if ph:k.CloseHandle(ph)
    admitted=[];dedup=set()
    for h in hits:
        if h['caller_admitted']:
            key=(h['callee'],h['caller'],h['caller_callsite'])
            if key not in dedup:admitted.append(h);dedup.add(key)
    result={'schema':'mkar.oracle-dispatch-hardware-trace.v1','oracle_exe_sha256':sha(exe),'original_elf_sha256':ELF,'plan_sha256':sha(a.plan),'targets_sha256':sha(a.targets),'catalogue_sha256':hashlib.sha256(catalogue_raw).hexdigest(),'source_map_sha256':sha(a.source_map),'host_abi_symbols':symbols,'windows_abi':abi,'run_root':str(root),'pid':pid,'exit_code':exit_code,'termination':termination,'elapsed_seconds':time.monotonic()-start,'debug_events':events,'dispatch_hits':sum(seen.values()),'unique_dispatch_targets':len(seen),'selected_dispatch_observations':hits,'admitted_direct_caller_records':admitted,'runtime_calls':[{k:v for k,v in x.items() if k in ['callee','caller','caller_callsite','call_site','return_address','original_call_site_word','jal_word_le','caller_contiguous_span_sha256','call_kind','jalr_register','jalr_register_value','delay_word_le','target_register_preserved_by_delay']}|{'evidence_ref':str(a.output.name)+'#event='+str(x['event'])} for x in admitted],'rejected_caller_counts':dict(rejected),'stdout_sha256':sha(out_path),'stderr_sha256':sha(err_path),'self_test':self_test(),'safety':{'fresh_disposable_process':True,'hardware_breakpoint_only':True,'binary_memory_patch_or_injection':False,'guest_state_writes':False,'persistent_card_opened':False,'headless':True,'audio_driver':'dummy'},'scope':'E7 dispatcher-entry observation with exact JAL or observed-register/delay-verified JALR callers only; inherited/unmatched RA is not admitted. Does not prove all RenderWare paths or native credit.'}
    a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ['selected_dispatch_observations','admitted_direct_caller_records','host_abi_symbols','windows_abi']},indent=2))

if __name__=='__main__':main()
