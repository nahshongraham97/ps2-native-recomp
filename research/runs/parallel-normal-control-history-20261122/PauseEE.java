import java.nio.file.*;
import ghidra.app.script.GhidraScript;
public class PauseEE extends GhidraScript {
public void run() throws Exception {
Path out=Path.of(getScriptArgs()[0]);Files.createDirectories(out);Files.writeString(out.resolve("program.txt"),currentProgram.getName()+"\n"+currentProgram.getLanguageID()+"\n"+currentProgram.getExecutableSHA256());
long[] entries={0x1299b0,0x129950,0x129920,0x1298f0,0x315620,0x3155c0,0x3158d0};
for(long start:entries){var f=getFunctionAt(toAddr(start));if(f==null)throw new Exception("missing function "+start);long end=f.getBody().getMaxAddress().getOffset()+1;StringBuilder s=new StringBuilder();
for(long a=start;a<end;a+=4){var ad=toAddr(a);var i=getInstructionAt(ad);byte[] b=new byte[4];currentProgram.getMemory().getBytes(ad,b);String h="";for(byte v:b)h+=String.format("%02x",v&255);s.append(String.format("%08x\t%s\t%s\n",a,h,i==null?"UNDECODED":i.toString()));}Files.writeString(out.resolve(String.format("%08x.tsv",start)),s.toString());}
}}
