import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import java.nio.file.*;
public class FindEngine extends GhidraScript {public void run() throws Exception {StringBuilder b=new StringBuilder();InstructionIterator it=currentProgram.getListing().getInstructions(true);while(it.hasNext()){Instruction i=it.next();for(int o=0;o<i.getNumOperands();o++)for(Object obj:i.getOpObjects(o))if(obj instanceof Scalar && ((Scalar)obj).getUnsignedValue()==0x40f){Function f=getFunctionContaining(i.getAddress());if(f!=null)b.append(f.getEntryPoint()+"\t"+i+"\n");}}Files.writeString(Path.of(getScriptArgs()[0]),b.toString());}}
