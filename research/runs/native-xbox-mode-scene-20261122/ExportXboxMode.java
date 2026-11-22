// Bounded read-only export from the existing private Xbox program.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.nio.file.*;
import java.util.*;
public class ExportXboxMode extends GhidraScript {
    private void save(Path path,String text)throws Exception{
        Files.writeString(path,text,StandardOpenOption.CREATE_NEW);
    }
    public void run()throws Exception{
        if(!currentProgram.getName().equals("default.xbe")||
           !currentProgram.getExecutableSHA256().equals("78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393")||
           !currentProgram.getLanguageID().toString().equals("x86:LE:32:default"))
            throw new IllegalStateException("Unexpected Xbox program/image/language");
        String[] args=getScriptArgs();Path out=Path.of(args[0]);Files.createDirectory(out);
        save(out.resolve("PROGRAM.txt"),currentProgram.getName()+"\n"+currentProgram.getExecutableSHA256()+
            "\n"+currentProgram.getLanguageID()+"\n");
        TreeMap<Long,Function> functions=new TreeMap<>();StringBuilder refs=new StringBuilder();
        for(int i=1;i<args.length;++i){
            String[] query=args[i].split(":");Address first=toAddr(query[0]);
            int length=query.length==1?1:Integer.parseInt(query[1]);
            if(length>512)throw new IllegalArgumentException("Unbounded query");
            Function containing=getFunctionContaining(first);
            if(containing!=null)functions.put(containing.getEntryPoint().getOffset(),containing);
            for(int j=0;j<length;++j){
                Address at=first.add(j);
                for(var ref:getReferencesTo(at)){
                    Function f=getFunctionContaining(ref.getFromAddress());
                    refs.append(at+"\t"+ref.getFromAddress()+"\t"+ref.getReferenceType()+"\t"+
                        (f==null?"NO_SAVED_FUNCTION":f.getEntryPoint())+"\n");
                    if(f!=null)functions.put(f.getEntryPoint().getOffset(),f);
                }
            }
        }
        save(out.resolve("references.tsv"),refs.toString());
        if(functions.size()>24)throw new IllegalStateException("Too many candidate functions");
        DecompInterface d=new DecompInterface();StringBuilder index=new StringBuilder();
        try{
            if(!d.openProgram(currentProgram))throw new IllegalStateException(d.getLastMessage());
            for(Function f:functions.values()){
                if(f.getBody().getNumAddresses()>65536)throw new IllegalStateException("Unbounded function");
                String name=f.getEntryPoint().toString();StringBuilder asm=new StringBuilder(),calls=new StringBuilder();
                var it=currentProgram.getListing().getInstructions(f.getBody(),true);int count=0;
                while(it.hasNext()){
                    var ins=it.next();byte[] bytes=ins.getBytes();StringBuilder hex=new StringBuilder();
                    for(byte b:bytes)hex.append(String.format("%02x",b&255));
                    asm.append(ins.getAddress()+"\t"+hex+"\t"+ins+"\n");++count;
                    if(ins.getFlowType().isCall())
                        for(Address target:ins.getFlows())calls.append(ins.getAddress()+"\t"+target+"\n");
                }
                index.append(name+"\t"+f.getBody().getMaxAddress()+"\t"+count+"\t"+f.getBody()+"\n");
                save(out.resolve(name+".asm"),asm.toString());save(out.resolve(name+".calls.tsv"),calls.toString());
                var c=d.decompileFunction(f,20,monitor);
                if(c.decompileCompleted()&&c.getDecompiledFunction()!=null)
                    save(out.resolve(name+".c"),c.getDecompiledFunction().getC());
                else save(out.resolve(name+".error.txt"),c.getErrorMessage());
            }
        }finally{d.dispose();}
        save(out.resolve("functions.tsv"),index.toString());
    }
}
