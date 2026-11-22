import ghidra.app.script.GhidraScript;
import java.nio.file.*;
public class FindAliasFunctions extends GhidraScript {
 public void run() throws Exception {
  StringBuilder text=new StringBuilder();
  String[] args=getScriptArgs();
  for(int i=1;i<args.length;i++){
   var at=toAddr(args[i]);var f=getFunctionContaining(at);
   if(f==null)throw new IllegalStateException("No saved containing function at "+at);
   text.append(at+"\t"+f.getEntryPoint()+"\t"+f.getBody().getMaxAddress()+"\n");
  }
  Files.writeString(Path.of(args[0]),text.toString(),StandardOpenOption.CREATE_NEW);
 }
}
