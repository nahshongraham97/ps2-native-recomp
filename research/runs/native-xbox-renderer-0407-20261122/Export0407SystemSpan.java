// Decode the explicitly established original PS2 system-function span,including omitted switch cases.
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
public class Export0407SystemSpan extends GhidraScript {
    private void save(Path p,String text)throws Exception{Files.writeString(p,text,StandardOpenOption.CREATE_NEW);}
    public void run()throws Exception{
        if(!currentProgram.getName().equals("SLUS_214.10")||
           !currentProgram.getExecutableSHA256().equals("a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f")||
           !currentProgram.getLanguageID().toString().equals("r5900:LE:32:default"))
            throw new IllegalStateException("Wrong PS2 original program/language");
        String[] args=getScriptArgs();
        if(!args[1].equals("ps2")||!args[2].equals("fn=001b9cf0"))throw new IllegalArgumentException("Exact bounded span only");
        Path out=Path.of(args[0]);Files.createDirectory(out);
        save(out.resolve("PROGRAM.txt"),currentProgram.getName()+"\n"+currentProgram.getExecutableSHA256()+"\n"+currentProgram.getLanguageID()+"\n");
        StringBuilder listing=new StringBuilder(),calls=new StringBuilder();int count=0;
        for(long address=0x1b9cf0;address<0x1ba108;address+=4){
            var at=toAddr(address);var ins=getInstructionAt(at);
            if(ins==null){disassemble(at);ins=getInstructionAt(at);}
            if(ins==null||ins.getLength()!=4)throw new IllegalStateException("Undecoded original word:"+at);
            StringBuilder hex=new StringBuilder();
            for(byte b:ins.getBytes())hex.append(String.format("%02x",b&255));
            listing.append(at+"\t"+hex+"\t"+ins+"\n");++count;
            if(ins.getFlowType().isCall())for(var target:ins.getFlows())calls.append(at+"\t"+target+"\n");
        }
        save(out.resolve("001b9cf0.asm"),listing.toString());
        save(out.resolve("001b9cf0.calls.tsv"),calls.toString());
        save(out.resolve("functions.tsv"),"001b9cf0\t001ba107\t"+count+"\tEXPLICIT_ORIGINAL_SWITCH_SPAN\n");
        save(out.resolve("references.tsv"),"");
        save(out.resolve("COVERAGE.txt"),"Original0x1B9CF0..0x1BA107 explicit span,including switch arms/alignment;not a decompiler/full-semantics claim.\n");
    }
}
