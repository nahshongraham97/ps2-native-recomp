import pathlib,json,subprocess,hashlib,re,gzip,time
r=pathlib.Path.cwd(); d=r/'research/runs/parallel-first-scene-220900-20261122'; v=d/'validation-01'
def raw(p):return p.read_bytes() if p.exists() else gzip.decompress(p.with_name(p.name+'.gz').read_bytes())
def sha(p):return hashlib.sha256(raw(p)).hexdigest()
def save(p,x):p.write_text(json.dumps(x,indent=2)+'\n',encoding='utf8')
def run(name,args):
 t=time.time(); p=subprocess.run(args,cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT); (v/(name+'.log')).write_bytes(p.stdout); save(v/(name+'.json'),{'argv':list(map(str,args)),'exit':p.returncode,'seconds':time.time()-t}); return p
assert subprocess.run(['python',str(d/'join.py')],cwd=r).returncode==0
cache=(r/'build-first-scene/CMakeCache.txt').read_text(); nm=re.search(r'CMAKE_NM:FILEPATH=(.*)',cache)[1]; compiler=re.search(r'CMAKE_CXX_COMPILER:STRING=(.*)',cache)[1]
cmake=json.loads((v/'build.json').read_text())['argv'][0]
p=type('SavedResult',(),{'stdout':raw(v/'full-entry-negative.log'),'returncode':json.loads((v/'full-entry-negative.json').read_text())['exit']})(); assert p.returncode!=0
s=p.stdout.decode(errors='replace'); symbols=[x.strip() for x in re.findall(r'undefined symbol: (.*)',s)]
prior=json.loads((r/'research/runs/parallel-scene-bootstrap-continuation-20261122/owner-boundary-correction/validation-01/VERIFICATION.json').read_text())['remaining_undefined_symbols']
old='mka::retail::scene_bootstrap_required::continue_first_scene_at_220900(mka::retail::NativeRetailMainState&, mka::retail::StartupFailure&)'
new='mka::retail::scene_bootstrap_required::continue_first_scene_at_1a6dd0(mka::retail::NativeRetailMainState&, std::__1::shared_ptr<mka::retail::NativeRetailRendererAllocatorService>, mka::retail::StartupFailure&)'
expected=set(prior)-{old}|{new}; assert set(symbols)==expected,(len(symbols),set(symbols)^expected)
manifest={}
for root in ['runtime','recomp','cmake','tests']:
 for f in sorted((r/root).rglob('*')):
  if f.is_file():manifest[f.relative_to(r).as_posix()]=sha(f)
manifest['CMakeLists.txt']=sha(r/'CMakeLists.txt'); save(v/'SOURCE_FILES.json',manifest)
clients={}
for short,target in [('allocator','test_native_retail_renderer_allocator'),('camera','test_native_retail_camera_bootstrap'),('memory','test_native_retail_memory_owners')]:
 exe=r/'build-first-scene'/(target+'.exe'); receipt=json.loads((v/(short+'.json')).read_text()); assert receipt['exit']==0 and receipt['executable_sha256']==sha(exe)
 clients[short]={'target':target,'sha256':sha(exe),'result_sha256':sha(v/(short+'.log'))}
p=run('allocator-symbols',[nm,'--demangle',str(r/'build-first-scene/test_native_retail_renderer_allocator.exe')]); assert p.returncode==0
sy=p.stdout.decode(errors='replace'); assert 'NativeRetailRendererAllocatorService::allocate(' in sy and 'continue_first_scene_at_220900(' in sy
assert re.search(r' T SDL_InitSubSystem',sy) and '__imp_SDL_InitSubSystem' not in sy # linked window imports remain unreachable behind client abort guard
save(v/'VERIFICATION.json',{'status':'PASS_NATIVE_ALLOCATOR_PREFIX_REQUIRED_RENDERER_REMAINDER','base_commit':subprocess.check_output(['git','rev-parse','HEAD']).decode().strip(),'source_files':len(manifest),'source_manifest_sha256':sha(v/'SOURCE_FILES.json'),'compiler_sha256':sha(pathlib.Path(compiler)),'cache_sha256':sha(r/'build-first-scene/CMakeCache.txt'),'clients':clients,'remaining_undefined_symbols':symbols,'expected_count':37,'scope':'Entire1A6DD0 suffix remains required;client supplies explicit fixture. No gate rebuild/rerun;privatebase predates f667.'})
print('PASS pins, three previous fresh client results, symbols, exact37 substitution')
