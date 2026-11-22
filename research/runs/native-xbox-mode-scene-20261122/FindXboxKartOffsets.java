import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import java.nio.file.*;
import java.util.regex.Pattern;
public class FindXboxKartOffsets extends GhidraScript {
 public void run()throws Exception{
  if(!currentProgram.getExecutableSHA256().equals("78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393"))
   throw new IllegalStateException("Wrong Xbox image");
  var range=new AddressSet(toAddr("000c0000"),toAddr("00105fff"));
  var it=currentProgram.getListing().getInstructions(range,true);
  var pattern=Pattern.compile("0x(?:240|278)(?![0-9a-fA-F])");
  StringBuilder rows=new StringBuilder();
  while(it.hasNext()){
   var ins=it.next();
   if(!pattern.matcher(ins.toString()).find())continue;
   var function=getFunctionContaining(ins.getAddress());StringBuilder hex=new StringBuilder();
   for(byte b:ins.getBytes())hex.append(String.format("%02x",b&255));
   rows.append(ins.getAddress()+"\t"+hex+"\t"+ins+"\t"+
       (function==null?"NO_SAVED_FUNCTION":function.getEntryPoint())+"\n");
  }
  Files.writeString(Path.of(getScriptArgs()[0]),rows.toString(),StandardOpenOption.CREATE_NEW);
 }
}
