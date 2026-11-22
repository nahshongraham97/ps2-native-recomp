// Temporary callback discovery in a read-only private project; original bytes are joined by Python.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
import java.nio.file.*;
import java.util.*;
public class ExportXboxScene extends GhidraScript {
    private void save(Path p,String text)throws Exception{
        Files.writeString(p,text,StandardOpenOption.CREATE_NEW);
    }
    public void run()throws Exception{
        if(!currentProgram.getName().equals("default.xbe")||
           !currentProgram.getExecutableSHA256().equals("78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393")||
           !currentProgram.getLanguageID().toString().equals("x86:LE:32:default"))
            throw new IllegalStateException("Unexpected Xbox program");
        String[] args=getScriptArgs();Path out=Path.of(args[0]);Files.createDirectory(out);
        save(out.resolve("PROGRAM.txt"),currentProgram.getName()+"\n"+
            currentProgram.getExecutableSHA256()+"\n"+currentProgram.getLanguageID()+"\n");
        TreeMap<Long,Function> functions=new TreeMap<>();StringBuilder created=new StringBuilder();
        for(int i=1;i<args.length;++i){
            var at=toAddr(args[i].split(":")[0]);Function f=getFunctionAt(at);
            if(f==null){
                Function containing=getFunctionContaining(at);
                if(containing!=null)throw new IllegalStateException("Query inside different function:"+at+" "+containing);
                disassemble(at);f=createFunction(at,"scene_callback_"+at);
                if(f==null)throw new IllegalStateException("Cannot discover exact callback:"+at);
                created.append(at+"\tTEMPORARY_READ_ONLY_DISCOVERY\n");
            }
            functions.put(f.getEntryPoint().getOffset(),f);
        }
        save(out.resolve("temporary-functions.tsv"),created.toString());
        DecompInterface d=new DecompInterface();StringBuilder index=new StringBuilder();
        try{
            if(!d.openProgram(currentProgram))throw new IllegalStateException(d.getLastMessage());
            for(Function f:functions.values()){
                if(f.getBody().getNumAddresses()>65536)throw new IllegalStateException("Unbounded body");
                String name=f.getEntryPoint().toString();StringBuilder asm=new StringBuilder(),calls=new StringBuilder();
                var it=currentProgram.getListing().getInstructions(f.getBody(),true);int count=0;
                while(it.hasNext()){
                    var ins=it.next();StringBuilder hex=new StringBuilder();
                    for(byte b:ins.getBytes())hex.append(String.format("%02x",b&255));
                    asm.append(ins.getAddress()+"\t"+hex+"\t"+ins+"\n");++count;
                    if(ins.getFlowType().isCall())for(var target:ins.getFlows())
                        calls.append(ins.getAddress()+"\t"+target+"\n");
                }
                index.append(name+"\t"+f.getBody().getMaxAddress()+"\t"+count+"\t"+f.getBody()+"\n");
                save(out.resolve(name+".asm"),asm.toString());save(out.resolve(name+".calls.tsv"),calls.toString());
                var result=d.decompileFunction(f,20,monitor);
                if(result.decompileCompleted()&&result.getDecompiledFunction()!=null)
                    save(out.resolve(name+".c"),result.getDecompiledFunction().getC());
                else save(out.resolve(name+".error.txt"),result.getErrorMessage());
            }
        }finally{d.dispose();}
        save(out.resolve("functions.tsv"),index.toString());
    }
}
