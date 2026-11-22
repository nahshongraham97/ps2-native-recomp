// Exact selected functions/references in private read-only Xbox/PS2 projects.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
import java.nio.file.*;
import java.util.*;
public class Export0406 extends GhidraScript {
    private void save(Path p,String text)throws Exception{
        Files.writeString(p,text,StandardOpenOption.CREATE_NEW);
    }
    public void run()throws Exception{
        String[] args=getScriptArgs();String platform=args[1];
        String hash=platform.equals("xbox")?
            "78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393":
            "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f";
        String language=platform.equals("xbox")?"x86:LE:32:default":"r5900:LE:32:default";
        String name=platform.equals("xbox")?"default.xbe":"SLUS_214.10";
        if(!currentProgram.getName().equals(name)||!currentProgram.getExecutableSHA256().equals(hash)||
           !currentProgram.getLanguageID().toString().equals(language))
            throw new IllegalStateException("Wrong original program/language");
        Path out=Path.of(args[0]);Files.createDirectory(out);
        save(out.resolve("PROGRAM.txt"),name+"\n"+hash+"\n"+language+"\n");
        save(out.resolve("PROCESSOR.txt"),currentProgram.getLanguage().getLanguageDescription().toString()+
            "\n"+currentProgram.getCompilerSpec().getCompilerSpecID()+"\n");
        TreeMap<Long,Function> functions=new TreeMap<>();
        StringBuilder refs=new StringBuilder(),created=new StringBuilder(),index=new StringBuilder();
        for(int i=2;i<args.length;++i){
            String[] mode=args[i].split("="),query=mode[1].split(":");var at=toAddr(query[0]);
            if(mode[0].equals("data"))continue;
            if(mode[0].equals("ref")){
                int n=Integer.parseInt(query[1]);if(n<1||n>512)throw new IllegalArgumentException("Bounded refs only");
                for(int j=0;j<n;++j)for(var ref:getReferencesTo(at.add(j))){
                    Function f=getFunctionContaining(ref.getFromAddress());
                    refs.append(at.add(j)+"\t"+ref.getFromAddress()+"\t"+ref.getReferenceType()+"\t"+
                        (f==null?"NO_SAVED_FUNCTION":f.getEntryPoint())+"\n");
                }
                continue;
            }
            if(!mode[0].equals("fn"))throw new IllegalArgumentException("Unknown query mode");
            Function f=getFunctionAt(at);
            if(f==null){
                Function owner=getFunctionContaining(at);
                if(owner!=null)throw new IllegalStateException("Interior entry:"+at+" in "+owner);
                disassemble(at);f=createFunction(at,"recovery0406_"+at);
                if(f==null)throw new IllegalStateException("No exact function:"+at);
                created.append(at+"\tTEMPORARY_READ_ONLY_DISCOVERY\n");
            }
            functions.put(f.getEntryPoint().getOffset(),f);
        }
        save(out.resolve("references.tsv"),refs.toString());
        save(out.resolve("temporary-functions.tsv"),created.toString());
        DecompInterface d=new DecompInterface();
        try{
            if(!d.openProgram(currentProgram))throw new IllegalStateException(d.getLastMessage());
            for(Function f:functions.values()){
                if(f.getBody().getNumAddresses()>65536)throw new IllegalStateException("Unbounded function");
                String entry=f.getEntryPoint().toString();StringBuilder listing=new StringBuilder(),calls=new StringBuilder();
                var it=currentProgram.getListing().getInstructions(f.getBody(),true);int count=0;
                while(it.hasNext()){
                    var ins=it.next();StringBuilder hex=new StringBuilder();
                    for(byte b:ins.getBytes())hex.append(String.format("%02x",b&255));
                    listing.append(ins.getAddress()+"\t"+hex+"\t"+ins+"\n");++count;
                    if(ins.getFlowType().isCall())for(var target:ins.getFlows())calls.append(ins.getAddress()+"\t"+target+"\n");
                }
                save(out.resolve(entry+".asm"),listing.toString());save(out.resolve(entry+".calls.tsv"),calls.toString());
                index.append(entry+"\t"+f.getBody().getMaxAddress()+"\t"+count+"\t"+f.getBody()+"\n");
                var result=d.decompileFunction(f,20,monitor);
                if(result.decompileCompleted()&&result.getDecompiledFunction()!=null)
                    save(out.resolve(entry+".c"),result.getDecompiledFunction().getC());
                else save(out.resolve(entry+".error.txt"),result.getErrorMessage());
            }
        }finally{d.dispose();}
        save(out.resolve("functions.tsv"),index.toString());
    }
}
