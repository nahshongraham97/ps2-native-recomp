import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import java.io.*;
public class FullFontListing extends GhidraScript {
 public void run()throws Exception{
  String[] a=getScriptArgs();File out=new File(a[0]);out.mkdirs();
  for(int n=1;n<a.length;n++){
   String[] span=a[n].split(":");long begin=Long.parseUnsignedLong(span[0],16),end=Long.parseUnsignedLong(span[1],16);
   try(PrintWriter w=new PrintWriter(new File(out,String.format("%08x.asm",begin)))){
    for(long at=begin;at<end;at+=4){Address p=toAddr(at);Instruction ins=getInstructionAt(p);if(ins==null){disassemble(p);ins=getInstructionAt(p);}
     if(ins==null||ins.getLength()!=4)throw new RuntimeException("Missing EE word at "+p);
     byte[] b=getBytes(p,4);w.printf("%08x\t%02x%02x%02x%02x\t%s%s%n",at,b[0]&255,b[1]&255,b[2]&255,b[3]&255,ins.isInDelaySlot()?"_":"",ins.toString());
    }
   }
  }
 }
}
