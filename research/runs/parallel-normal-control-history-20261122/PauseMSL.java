import java.nio.file.*;
import ghidra.app.script.GhidraScript;
public class PauseMSL extends GhidraScript {
public void run() throws Exception {
Path out=Path.of(getScriptArgs()[0]);Files.createDirectories(out);Files.writeString(out.resolve("program.txt"),currentProgram.getName()+"\n"+currentProgram.getLanguageID()+"\n"+currentProgram.getExecutableSHA256());
long[] entries={0x959c,0x9534,0x934c,0x3fcc,0xa49c};
for(long entry:entries){var f=getFunctionContaining(toAddr(entry));if(f==null)throw new Exception("missing function "+entry);long start=f.getEntryPoint().getOffset(),end=f.getBody().getMaxAddress().getOffset()+1;StringBuilder s=new StringBuilder();
for(long a=start;a<end;a+=4){var ad=toAddr(a);var i=getInstructionAt(ad);byte[] b=new byte[4];currentProgram.getMemory().getBytes(ad,b);String h="";for(byte v:b)h+=String.format("%02x",v&255);s.append(String.format("%08x\t%s\t%s\n",a,h,i==null?"UNDECODED":i.toString()));}Files.writeString(out.resolve(String.format("%08x.tsv",start)),s.toString());
var di=new ghidra.app.decompiler.DecompInterface();di.openProgram(currentProgram);var dr=di.decompileFunction(f,60,monitor);if(dr.decompileCompleted())Files.writeString(out.resolve(String.format("%08x.c",start)),dr.getDecompiledFunction().getC());di.dispose();}
}}
