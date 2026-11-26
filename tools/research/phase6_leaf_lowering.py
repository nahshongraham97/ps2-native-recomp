"""Capability-only generated-AST -> typed block-parameter SSA Stage A.

Original labels form static C++ blocks, never an opcode VM. Storage stays in
the explicit GuestMemory layer. Mechanical bit-vector input/output boundaries
are NOT recovered production ABI signatures or ownership admissions.
"""
import copy
import re
import sys
import transform_generated_units as tr
if sys.getrecursionlimit() < 4096:
    sys.setrecursionlimit(4096)

Refusal=tr.Refusal
need=tr.require
def strip(n):
    while n.get('kind') in ('ParenExpr','ConstantExpr') or n.get('kind')=='ImplicitCastExpr' and n.get('castKind') in ('LValueToRValue','NoOp','FunctionToPointerDecay','ArrayToPointerDecay'):
        need(len(tr.children(n))==1,'N1_bad_expression_wrapper');n=tr.children(n)[0]
    return n
def reg(n,cpu):
    n=strip(n)
    if n.get('kind')!='ArraySubscriptExpr':return None
    a=tr.children(n)
    if len(a)!=2:return None
    member=strip(a[0])
    if member.get('kind')!='MemberExpr' or member.get('name') not in ('w','sw','d','sd') or tr.literal(a[1])!=0:return None
    r=tr.register_object(tr.children(member)[0],cpu)
    return r,32 if member['name'] in ('w','sw') else 64,member['name'] in ('sw','sd')
def typename(n):
    ty=n.get('type',{});s=ty.get('desugaredQualType',ty.get('qualType',''))
    # Const local capture changes assignability, never scalar width/sign.
    # Volatile/pointer/record types remain unproved rather than discarded.
    s=re.sub(r'\bconst\s+','',s).strip()
    d={'bool':'bool','unsigned int':'u32','int':'i32','unsigned long long':'u64','long long':'i64','unsigned long':'u64','long':'i64','unsigned char':'u8','signed char':'i8','char':'i8','unsigned short':'u16','short':'i16'}
    need(s in d,'N1_unproved_AST_type:'+s);return d[s]

class Leaf:
    def __init__(self,definition,entry,entry_case,words):
        self.definition=definition;self.entry=entry;self.words=words;self.blocks={};self.used=set();self.written=set();self.local_types={};self.synthetic=0;self.memory=[]
        params=[x for x in tr.children(definition) if x.get('kind')=='ParmVarDecl'];need(len(params)==3,'N2_generated_ABI_shape')
        self.cpu,self.selector,self.depth=[x['id'] for x in params]
        body=next(x for x in tr.children(definition) if x.get('kind')=='CompoundStmt');nodes=tr.children(body)
        switch=next(x for x in nodes if x.get('kind')=='LabelStmt' and x.get('name')=='dispatch_entry')
        case=[x for x in tr.walk(switch) if x.get('kind')=='CaseStmt' and tr.literal(tr.children(x)[0])==entry_case]
        need(len(case)==1 and tr.children(case[0])[1].get('kind')=='GotoStmt','N2_entry_case_identity')
        target=tr.children(case[0])[1]['targetLabelDeclId']
        need(any(x.get('declId')==target and x.get('name')=='L_'+entry.upper() for x in nodes),'N2_entry_label_identity')
        self.label_ids={x.get('declId'):x['name'] for x in tr.walk(body) if x.get('kind')=='LabelStmt' and x.get('name','').startswith('L_')}
        selected={}
        for i,n in enumerate(nodes):
            if n.get('kind')!='LabelStmt' or n.get('name','')!='L_'+n.get('name','')[2:]:continue
            name=n.get('name','')
            if not re.fullmatch('L_[0-9A-F]{8}',name) or int(name[2:],16) not in words:continue
            end=next((j for j in range(i+1,len(nodes)) if nodes[j].get('kind')=='LabelStmt'),len(nodes))
            seq=tr.children(n)+nodes[i+1:end];next_label=nodes[end].get('name') if end<len(nodes) else None
            selected[name]=(seq,next_label)
        self.selected=selected;self.root='L_'+entry.upper();need(self.root in selected,'N3_entry_block_missing')
        self.reachable=set();self.visit(self.root)
        self.blocks={k:v for k,v in self.blocks.items() if k in self.reachable}
        self.cyclic=set()
        for origin in self.blocks:
            pending=[origin];seen=set()
            while pending:
                node=pending.pop()
                if node in seen:continue
                seen.add(node);term=self.blocks[node]['term']
                targets=term[1:] if term[0]=='jump' else term[2:] if term[0]=='branch' else ()
                if origin in targets:self.cyclic.add(origin);break
                pending.extend(target for target in targets if target not in seen)
        self.dynamic_loop=False
        self._bound_memo={}
        self.fuel=self.bound(self.root,{},{},set(),0)
    def expr(self,n):
        n=strip(n);k=n.get('kind');a=tr.children(n);r=reg(n,self.cpu)
        if r is not None:
            index,width,signed=r;self.used.add(index)
            return ('reg',index,width,signed)
        if k=='IntegerLiteral':return ('const',int(n['value']),typename(n))
        if k=='CXXBoolLiteralExpr':return ('const',int(n['value']),'bool')
        if k in ('ImplicitCastExpr','CXXStaticCastExpr','CStyleCastExpr'):
            need(len(a)==1 and n.get('castKind') in ('IntegralCast','IntegralToBoolean','NoOp'),'N1_cast_unproved:'+str(n.get('castKind')))
            return ('cast',typename(n),self.expr(a[0]))
        if k=='BinaryOperator':
            need(n.get('opcode') in ('+','-','&','|','^','<<','>>','==','!=','<','<=','>','>=','&&','||'),'N3_scalar_operation_unproved:'+str(n.get('opcode')))
            return ('binary',n['opcode'],typename(n),self.expr(a[0]),self.expr(a[1]))
        if k=='UnaryOperator':
            need(n.get('opcode') in ('~','!','-'), 'N3_unary_operation_unproved');return ('unary',n['opcode'],typename(n),self.expr(a[0]))
        if k=='ConditionalOperator':return ('select',self.expr(a[0]),self.expr(a[1]),self.expr(a[2]))
        if k=='DeclRefExpr':return ('local',n['referencedDecl']['id'])
        if k=='MemberExpr' and n.get('name')=='budget' and tr.same_parameter(a[0],self.cpu):return ('budget',)
        if k=='CallExpr':
            name=tr.declaration(a[0]).get('name');need(name in ('ld8','ld16','ld32','ld64'),'N1_load_helper_unproved:'+str(name))
            need(len(a)==2,'N1_load_arity');self.memory.append(dict(kind='load',width=int(name[2:]),address='explicit guest access; semantic receiver/field unproved'))
            return ('load',int(name[2:]),self.expr(a[1]))
        raise Refusal('N1_value_AST_unproved:'+str(k))
    def flatten(self,seq):
        out=[]
        for n in seq:
            if n.get('kind')=='CompoundStmt':out+=self.flatten(tr.children(n))
            else:out.append(n)
        return out
    def sequence(self,name,seq,fallthrough,pc):
        ops=[];seq=self.flatten(seq)
        def block(term):self.blocks[name]=dict(pc=pc,ops=ops,term=term)
        for i,n in enumerate(seq):
            k=n.get('kind');a=tr.children(n)
            if k=='NullStmt':continue
            if k=='CompoundAssignOperator':
                need(n.get('opcode')=='-=' and strip(a[0]).get('name')=='budget','N3_owner_assignment_unproved');ops.append(('debit',tr.literal(a[1])));continue
            if k=='DeclStmt':
                need(len(a)==1 and a[0].get('kind')=='VarDecl' and len(tr.children(a[0]))==1,'N3_declaration_unproved')
                v=a[0];self.local_types[v['id']]=typename(v);ops.append(('local',v['id'],self.expr(tr.children(v)[0])));continue
            if k=='CallExpr':
                helper=tr.declaration(a[0]).get('name')
                if helper=='set32':
                    need(len(a)==3,'N1_set32_arity');index=tr.register_object(a[1],self.cpu);self.written.add(index);self.used.add(index);ops.append(('reg',index,64,('cast','i32',self.expr(a[2]))));continue
                need(helper in ('st8','st16','st32','st64') and len(a)==3,'N1_store_helper_unproved:'+str(helper))
                width=int(helper[2:]);ops.append(('store',width,self.expr(a[1]),self.expr(a[2])));self.memory.append(dict(kind='store',width=width,address='explicit guest access; semantic receiver/field unproved'));continue
            if k=='BinaryOperator' and n.get('opcode')=='=':
                target=reg(a[0],self.cpu)
                if target is not None:
                    r,width,signed=target;self.used.add(r);self.written.add(r);ops.append(('reg',r,width,self.expr(a[1])));continue
                t=strip(a[0]);need(t.get('kind')=='MemberExpr' and t.get('name')=='pc' and tr.same_parameter(tr.children(t)[0],self.cpu),'N3_cursor_assignment_unproved')
                ops.append(('cursor',self.expr(a[1])));continue
            if k=='IfStmt':
                need(len(a) in (2,3),'N3_if_shape_unproved')
                # Exact source guards are kept. Yield is unsupported; the
                # static path bound must prove it unreachable at admitted fuel.
                calls=[tr.declaration(tr.children(x)[0]).get('name') for x in tr.walk(a[0]) if x.get('kind')=='CallExpr']
                if calls:
                    need(calls==['preempt'],'N3_call_in_condition_unproved');ops.append(('yield_guard',));continue
                then_calls=[tr.declaration(tr.children(x)[0]).get('name') for x in tr.walk(a[1]) if x.get('kind')=='CallExpr']
                if then_calls==['stop']:
                    ops.append(('return_guard',self.expr(a[0])));continue
                self.synthetic+=1;suffix=str(self.synthetic);yes=name+'_T'+suffix;no=name+'_F'+suffix;rest=seq[i+1:]
                self.sequence(yes,[a[1]]+rest,fallthrough,pc)
                self.sequence(no,([a[2]] if len(a)==3 else [])+rest,fallthrough,pc)
                block(('branch',self.expr(a[0]),yes,no));return
            if k=='GotoStmt':
                target=self.label_ids.get(n.get('targetLabelDeclId'));need(target is not None and re.fullmatch('L_[0-9A-F]{8}',target),'N3_indirect_or_entry_dispatch_unproved');block(('jump',target));return
            if k=='ReturnStmt':
                need(not a,'N3_external_or_tail_return_unproved');block(('return',));return
            raise Refusal('N3_statement_AST_unproved:'+str(k))
        need(fallthrough is not None,'N3_unclosed_body');block(('jump',fallthrough))
    def visit(self,name):
        if name not in self.blocks and name in self.selected:
            seq,nxt=self.selected[name];self.sequence(name,seq,nxt,int(name[2:],16))
        need(name in self.blocks,'N3_edge_outside_authenticated_body:'+str(name))
        if name in self.reachable:return
        self.reachable.add(name);t=self.blocks[name]['term']
        for target in t[1:] if t[0]=='jump' else t[2:] if t[0]=='branch' else ():self.visit(target)
    @staticmethod
    def cast(value,ty):
        if value is None:return None
        if ty=='bool':return int(bool(value))
        bits=int(ty[1:]);value&=(1<<bits)-1
        return value-(1<<bits) if ty[0]=='i' and value>>(bits-1) else value
    def constant(self,e,env,locals_):
        op=e[0]
        if op=='const':return self.cast(e[1],e[2])
        if op=='reg':return self.cast(env.get(e[1],0 if e[1]==0 else None),('i' if e[3] else 'u')+str(e[2]))
        if op=='local':
            need(e[1] in locals_,'N3_read_before_proved_local_definition');return locals_[e[1]]
        if op=='cast':return self.cast(self.constant(e[2],env,locals_),e[1])
        if op in ('load','budget'):return None
        if op=='select':
            c=self.constant(e[1],env,locals_);return self.constant(e[2 if c else 3],env,locals_) if c is not None else None
        if op=='unary':
            v=self.constant(e[3],env,locals_);return None if v is None else self.cast({'~':lambda:~v,'!':lambda:not v,'-':lambda:-v}[e[1]](),e[2])
        a=self.constant(e[3],env,locals_);b=self.constant(e[4],env,locals_)
        if a is None or b is None:return None
        need(e[1] not in ('<<','>>') or 0<=b<int(e[2][1:]),'N3_unproved_shift_domain')
        ops={'+':lambda:a+b,'-':lambda:a-b,'&':lambda:a&b,'|':lambda:a|b,'^':lambda:a^b,'<<':lambda:a<<b,'>>':lambda:a>>b,'==':lambda:a==b,'!=':lambda:a!=b,'<':lambda:a<b,'<=':lambda:a<=b,'>':lambda:a>b,'>=':lambda:a>=b,'&&':lambda:bool(a and b),'||':lambda:bool(a or b)}
        return self.cast(ops[e[1]](),e[2])
    def bound(self,name,env,locals_,active,depth):
        need(depth<768,'N3_static_analysis_path_bound_768_exceeded');env=dict(env);locals_=dict(locals_)
        key=(name,tuple(sorted(env.items())),tuple(sorted(locals_.items())))
        if key in self._bound_memo:return self._bound_memo[key]
        if key in active:
            need(name in self.cyclic or self.dynamic_loop,'N3_loop_PHI_fuel_bound_unproved')
            self.dynamic_loop=True;return 0
        active=active|{key};cost=0
        for op in self.blocks[name]['ops']:
            if op[0]=='debit':cost+=op[1]
            elif op[0]=='reg':
                value=self.constant(op[3],env,locals_)
                env[op[1]]=None if value is None else value&0xffffffffffffffff
            elif op[0]=='local':locals_[op[1]]=self.constant(op[2],env,locals_)
            elif op[0]=='store':self.constant(op[2],env,locals_);self.constant(op[3],env,locals_)
            elif op[0] in ('cursor','return_guard'):self.constant(op[1],env,locals_)
        t=self.blocks[name]['term']
        if t[0]=='return':res=cost
        elif t[0]=='jump':res=cost+self.bound(t[1],env,locals_,active,depth+1)
        else:
            value=self.constant(t[1],env,locals_)
            if value is None and name in self.cyclic:self.dynamic_loop=True
            targets=t[2:] if value is None else (t[2] if value else t[3],)
            res=cost+max(self.bound(target,env,locals_,active,depth+1) for target in targets)
        self._bound_memo[key]=res
        return res

def emit(leaf,name):
    """Static block-parameter SSA; mutable phi transports only on CFG edges."""
    regs=sorted(leaf.used-{0});locals_=sorted(leaf.local_types);fields=['r'+str(r) for r in regs]+['l'+str(i) for i in range(len(locals_))];localfield={v:'l'+str(i) for i,v in enumerate(locals_)}
    header='struct '+name+'_in{'+''.join('std::uint64_t r'+str(r)+';' for r in regs)+'};\n'
    header+='struct '+name+'_out{'+''.join('std::uint64_t r'+str(r)+';' for r in sorted(leaf.written-{0}))+'std::int64_t budget;};\n'
    text=header+name+'_out '+name+'(mka::stage_a::GuestMemory&memory,const '+name+'_in&input,std::int64_t entry_budget){\n'
    text+=f'if(entry_budget<{leaf.fuel})throw mka::stage_a::MemoryRefusal("N3 unproved scheduling/input fuel");\n'
    for block in leaf.blocks:
        for field in fields:text+='std::uint64_t '+block+'_'+field+'{};\n'
        text+='std::int64_t '+block+'_fuel{};\n'
    for r in regs:text+=leaf.root+'_r'+str(r)+'=input.r'+str(r)+';\n'
    text+=leaf.root+'_fuel=entry_budget;goto '+leaf.root+';\n';serial=0;report=[]
    ctype=lambda ty:'bool' if ty=='bool' else 'std::'+('u' if ty[0]=='u' else '')+'int'+ty[1:]+'_t'
    for block,b in leaf.blocks.items():
        env={r:block+'_r'+str(r) for r in regs};local={v:block+'_'+localfield[v] for v in locals_};budget=block+'_fuel';ops=[]
        def fresh(expr,ty='u64'):
            nonlocal serial,text
            serial+=1;v='value_'+str(serial);text+='const '+ctype(ty)+' '+v+'='+expr+';\n';return v
        def expression(e):
            op=e[0]
            if op=='const':return ctype(e[2])+'{'+str(e[1])+('ULL' if e[2][0]=='u' and e[1]>2147483647 else '')+'}'
            if op=='reg':return 'static_cast<'+ctype(('i' if e[3] else 'u')+str(e[2]))+'>(0ULL)' if e[1]==0 else 'static_cast<'+ctype(('i' if e[3] else 'u')+str(e[2]))+'>('+env[e[1]]+')'
            if op=='local':return 'static_cast<'+ctype(leaf.local_types[e[1]])+'>('+local[e[1]]+')'
            if op=='budget':return budget
            if op=='cast':return 'static_cast<'+ctype(e[1])+'>( '+expression(e[2])+')'
            if op=='load':return fresh('memory.load<std::uint'+str(e[1])+'_t>(static_cast<std::uint32_t>('+expression(e[2])+'))','u'+str(e[1]))
            if op=='binary':return '('+expression(e[3])+' '+e[1]+' '+expression(e[4])+')'
            if op=='unary':return '('+e[1]+expression(e[3])+')'
            if op=='select':return '('+expression(e[1])+' ? '+expression(e[2])+' : '+expression(e[3])+')'
            raise Refusal('N3_emission_expression_unproved')
        def edge(target):
            nonlocal text
            incoming={r:fresh(env[r]) for r in regs};incoming_locals={v:fresh(local[v]) for v in locals_};incoming_budget=fresh(budget,'i64')
            if leaf.dynamic_loop and target in leaf.cyclic:text+='if('+incoming_budget+'<0)throw mka::stage_a::MemoryRefusal("N3 dynamic loop fuel exhausted");\n'
            for r in regs:text+=target+'_r'+str(r)+'='+incoming[r]+';\n'
            for v in locals_:text+=target+'_'+localfield[v]+'='+incoming_locals[v]+';\n'
            text+=target+'_fuel='+incoming_budget+';goto '+target+';\n'
        text+=block+':{\n'
        for op in b['ops']:
            if op[0]=='debit':budget=fresh(budget+'-'+str(op[1]),'i64')
            elif op[0]=='reg':
                e=expression(op[3]);value=fresh('static_cast<std::uint64_t>('+e+')')
                if op[1]:env[op[1]]=value if op[2]==64 else fresh('('+env[op[1]]+'&0xffffffff00000000ULL)|static_cast<std::uint32_t>('+value+')')
                ops.append(dict(kind='define_GPR_low',source_register=op[1],width=op[2],value=value))
            elif op[0]=='local':local[op[1]]=fresh('static_cast<std::uint64_t>('+expression(op[2])+')')
            elif op[0]=='store':
                at=fresh('static_cast<std::uint32_t>('+expression(op[2])+')','u32');v=fresh('static_cast<std::uint'+str(op[1])+'_t>('+expression(op[3])+')','u'+str(op[1]));text+='memory.store<std::uint'+str(op[1])+'_t>('+at+','+v+');\n'
                ops.append(dict(kind='ordered_store',width=op[1],address=at,value=v))
            elif op[0]=='yield_guard':text+='if('+budget+'<0)throw mka::stage_a::MemoryRefusal("N3 unexpected scheduling edge");\n'
            elif op[0]=='return_guard':text+='if('+expression(op[1])+')throw mka::stage_a::MemoryRefusal("N2 invalid ordinary return address");\n'
            elif op[0]=='cursor':pass # Reconstructed only by the explicit AOT-facing test shim.
        term=b['term']
        if term[0]=='return':text+='return {'+','.join([env[r] for r in sorted(leaf.written-{0})]+[budget])+'};\n'
        elif term[0]=='jump':edge(term[1])
        else:
            condition=fresh(expression(term[1]),'bool');text+='if('+condition+'){\n';edge(term[2]);text+='}else{\n';edge(term[3]);text+='}\n'
        text+='}\n';report.append(dict(block=block,source_pc=f'{b["pc"]:08x}',parameters=fields+['fuel'],effects=ops,terminator=term[0]))
    text+='}\n';need(not re.search(r'\bc\.r\b|g_ram|Reg128|Cpu\s*&',text),'N1_native_body_retains_register_interface')
    return text,dict(schema='phase6_block_parameter_SSA_v1',blocks=report,inputs=regs,outputs=sorted(leaf.written-{0}),maximum_no_yield_debit=leaf.fuel,memory=leaf.memory,scope='Capability-only typed bit-vector interface from generated reads/writes; semantic ABI/receivers/fields unadmitted. No runtime opcode stream or dynamic register bank.')
