// Exact-function and reference-only export in this agent's read-only private Xbox project.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.listing.Function;
import java.nio.file.*;
import java.util.*;
public class ExportXbox0403 extends GhidraScript {
    private void save(Path path,String text)throws Exception{
        Files.writeString(path,text,StandardOpenOption.CREATE_NEW);
    }
    public void run()throws Exception{
        if(!currentProgram.getName().equals("default.xbe")||
           !currentProgram.getExecutableSHA256().equals("78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393")||
           !currentProgram.getLanguageID().toString().equals("x86:LE:32:default"))
            throw new IllegalStateException("Unexpected Xbox image/language");
        String[] args=getScriptArgs();Path out=Path.of(args[0]);Files.createDirectory(out);
        save(out.resolve("PROGRAM.txt"),currentProgram.getName()+"\n"+currentProgram.getExecutableSHA256()+
            "\n"+currentProgram.getLanguageID()+"\n");
        StringBuilder refs=new StringBuilder(),created=new StringBuilder(),index=new StringBuilder();
        TreeMap<Long,Function> functions=new TreeMap<>();
        for(int i=1;i<args.length;++i){
            String[] mode=args[i].split("=");String[] query=mode[1].split(":");
            var at=toAddr(query[0]);
            if(mode[0].equals("data"))continue;
            if(mode[0].equals("ref")){
                int length=Integer.parseInt(query[1]);if(length<1||length>512)throw new IllegalArgumentException("Unbounded ref");
                for(int n=0;n<length;++n)for(var ref:getReferencesTo(at.add(n))){
                    Function f=getFunctionContaining(ref.getFromAddress());
                    refs.append(at.add(n)+"\t"+ref.getFromAddress()+"\t"+ref.getReferenceType()+"\t"+
                        (f==null?"NO_SAVED_FUNCTION":f.getEntryPoint())+"\n");
                }
                continue;
            }
            if(!mode[0].equals("fn"))throw new IllegalArgumentException("Unknown mode");
            Function f=getFunctionAt(at);
            if(f==null){
                Function other=getFunctionContaining(at);
                if(other!=null)throw new IllegalStateException("Interior entry:"+at+" in "+other);
                disassemble(at);f=createFunction(at,"module0403_"+at);
                if(f==null)throw new IllegalStateException("Cannot establish exact function:"+at);
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
                String name=f.getEntryPoint().toString();StringBuilder asm=new StringBuilder(),calls=new StringBuilder();
                var it=currentProgram.getListing().getInstructions(f.getBody(),true);int count=0;
                while(it.hasNext()){
                    var ins=it.next();StringBuilder hex=new StringBuilder();
                    for(byte b:ins.getBytes())hex.append(String.format("%02x",b&255));
                    asm.append(ins.getAddress()+"\t"+hex+"\t"+ins+"\n");++count;
                    if(ins.getFlowType().isCall())for(var target:ins.getFlows())calls.append(ins.getAddress()+"\t"+target+"\n");
                }
                save(out.resolve(name+".asm"),asm.toString());save(out.resolve(name+".calls.tsv"),calls.toString());
                index.append(name+"\t"+f.getBody().getMaxAddress()+"\t"+count+"\t"+f.getBody()+"\n");
                var c=d.decompileFunction(f,20,monitor);
                if(c.decompileCompleted()&&c.getDecompiledFunction()!=null)save(out.resolve(name+".c"),c.getDecompiledFunction().getC());
                else save(out.resolve(name+".error.txt"),c.getErrorMessage());
            }
        }finally{d.dispose();}
        save(out.resolve("functions.tsv"),index.toString());
    }
}
