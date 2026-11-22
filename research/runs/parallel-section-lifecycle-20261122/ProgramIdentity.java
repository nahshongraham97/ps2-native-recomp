import ghidra.app.script.GhidraScript;
import java.nio.file.*;
public class ProgramIdentity extends GhidraScript { public void run()throws Exception {String s="name="+currentProgram.getName()+"\nlanguage="+currentProgram.getLanguageID()+"\ncompiler="+currentProgram.getCompilerSpec().getCompilerSpecID()+"\nsha256="+currentProgram.getExecutableSHA256()+"\n";Files.writeString(Path.of(getScriptArgs()[0]),s);}}
