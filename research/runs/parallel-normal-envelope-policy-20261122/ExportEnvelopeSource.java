// Bounded original-offset MSL context consumer; never execute guest code.
// @category MKAR.Research
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import com.google.gson.GsonBuilder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Map;
public class ExportEnvelopeSource extends GhidraScript {
    public void run() throws Exception {
        String[] args=getScriptArgs();
        Path output=Path.of(args[0]);
        if(Files.exists(output))throw new IllegalArgumentException("Output exists");
        Files.createDirectories(output);
        Map<String,Object> result=new LinkedHashMap<>();
        result.put("program",currentProgram.getName());
        result.put("language",currentProgram.getLanguageID().toString());
        result.put("image_base",currentProgram.getImageBase().toString());
        ArrayList<Map<String,Object>> rows=new ArrayList<>();
        DecompInterface decompiler=new DecompInterface();
        decompiler.openProgram(currentProgram);
        try {
            for(int n=1;n<args.length;n++) {
                String[] range=args[n].split(":");
                long start=Long.parseLong(range[0],16),end=Long.parseLong(range[1],16);
                Address first=toAddr(start);
                disassemble(first);
                if(getFunctionAt(first)==null)createFunction(first,null);
                var function=getFunctionAt(first);
                if(function!=null){
                    var d=decompiler.decompileFunction(function,10,monitor);
                    if(d.decompileCompleted())Files.writeString(output.resolve(range[0]+".c"),d.getDecompiledFunction().getC());
                }
                for(long at=start;at<end;at+=4){
                    Instruction i=getInstructionAt(toAddr(at));
                    if(i==null){disassemble(toAddr(at));i=getInstructionAt(toAddr(at));}
                    Map<String,Object> row=new LinkedHashMap<>();
                    row.put("original_vma",String.format("%08x",at));
                    row.put("prior_loaded_notation",String.format("%08x",at+0x28100));
                    row.put("word",String.format("%08x",getInt(toAddr(at))));
                    row.put("instruction",i==null?null:i.toString());
                    rows.add(row);
                }
            }
        } finally {decompiler.dispose();}
        result.put("instructions",rows);
        Files.writeString(output.resolve("LISTING.json"),new GsonBuilder().setPrettyPrinting().create().toJson(result)+"\n");
    }
}
