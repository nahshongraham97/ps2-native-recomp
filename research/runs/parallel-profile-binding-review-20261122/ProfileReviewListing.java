import java.nio.file.*;
import ghidra.app.script.GhidraScript;
public class ProfileReviewListing extends GhidraScript {
 public void run() throws Exception {
  Path out=Path.of(getScriptArgs()[0]); Files.createDirectories(out);
  String meta=currentProgram.getName()+"\n"+currentProgram.getLanguageID()+"\n"+currentProgram.getExecutableSHA256()+"\n";
  Files.writeString(out.resolve("program.txt"),meta);
  long[][] ranges={{0x171b10,0x171c28},{0x245cb0,0x245dd0},{0x245dd0,0x246070},{0x246070,0x2460b0},{0x2817b0,0x281830},{0x281830,0x281870}};
  for(long[] r:ranges){StringBuilder s=new StringBuilder(); for(long a=r[0];a<r[1];a+=4){var ad=toAddr(a);var i=getInstructionAt(ad);byte[] b=new byte[4];currentProgram.getMemory().getBytes(ad,b);String hex="";for(byte v:b)hex+=String.format("%02x",v&255);s.append(String.format("%08x\t%s\t%s\n",a,hex,i==null?"UNDECODED":i.toString()));}Files.writeString(out.resolve(String.format("%08x.tsv",r[0])),s.toString());}
 }
}
