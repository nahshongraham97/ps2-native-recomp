import ghidra.app.script.GhidraScript;
import java.nio.file.*;
public class FindXboxProfileRefs extends GhidraScript {
 public void run()throws Exception{
  if(!currentProgram.getExecutableSHA256().equals("78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393")||
     !currentProgram.getLanguageID().toString().equals("x86:LE:32:default"))
   throw new IllegalStateException("Wrong Xbox image/language");
  String[] args=getScriptArgs();StringBuilder rows=new StringBuilder();
  for(int i=1;i<args.length;++i){
   var address=toAddr(args[i]);
   for(var ref:getReferencesTo(address)){
    var function=getFunctionContaining(ref.getFromAddress());
    rows.append(address+"\t"+ref.getFromAddress()+"\t"+ref.getReferenceType()+"\t"+
       (function==null?"NO_SAVED_FUNCTION":function.getEntryPoint())+"\n");
   }
  }
  Files.writeString(Path.of(args[0]),rows.toString(),StandardOpenOption.CREATE_NEW);
 }
}
