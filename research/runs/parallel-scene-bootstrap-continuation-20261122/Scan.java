import ghidra.app.script.GhidraScript;
import java.nio.file.*;
public class Scan extends GhidraScript {public void run() throws Exception {StringBuilder s=new StringBuilder();var it=currentProgram.getListing().getInstructions(true);while(it.hasNext()){var i=it.next();String t=i.toString();if(t.contains("0x61f4(gp)")||t.contains("0x59a8(gp)")||t.contains("0x621c(gp)")||t.contains("0x5ff4(gp)")){var f=getFunctionContaining(i.getAddress());s.append(i.getAddress()+"\t"+(f==null?"none":f.getEntryPoint())+"\t"+t+"\n");}}Files.writeString(Path.of(getScriptArgs()[0]),s.toString());}}
