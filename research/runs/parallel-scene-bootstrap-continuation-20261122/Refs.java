import ghidra.app.script.GhidraScript;
import java.nio.file.*;
public class Refs extends GhidraScript {
 public void run() throws Exception {StringBuilder s=new StringBuilder();for(String a:getScriptArgs()[1].split(",")){var addr=toAddr(a);for(var r:getReferencesTo(addr)){var f=getFunctionContaining(r.getFromAddress());s.append(a+"\t"+r.getFromAddress()+"\t"+(f==null?"none":f.getEntryPoint())+"\n");}}Files.writeString(Path.of(getScriptArgs()[0]),s.toString());}
}
