// Export explicitly selected native function candidates, including assembly.
// @category Armageddon.Research
import java.io.BufferedWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.InstructionIterator;

public class ExportFirstScene extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("Expected output directory and addresses");
        ArrayList<String> addresses = new ArrayList<>();
        if (args.length == 2 && Files.isRegularFile(Path.of(args[1]))) {
            for (String address : Files.readAllLines(Path.of(args[1]))) {
                if (!address.isBlank()) addresses.add(address.trim());
            }
        } else {
            for (int index = 1; index < args.length; index++) {
                for (String address : args[index].split(",")) addresses.add(address);
            }
        }
        if (addresses.size() > 512) throw new IllegalArgumentException("At most 512 candidates per run");
        Path output = Path.of(args[0]);
        Files.createDirectories(output);
        DecompInterface decompiler = new DecompInterface();
        int complete = 0;
        long start = System.nanoTime();
        try (BufferedWriter calls = Files.newBufferedWriter(output.resolve("calls.tsv"))) {
            calls.write("caller\tcallee\n");
            if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException(decompiler.getLastMessage());
            for (String text : addresses) {
                if ((System.nanoTime()-start)/1_000_000_000L >= 180) break;
                monitor.checkCancelled();
                Address address = toAddr(text);
                Function function = getFunctionAt(address);
                if(function==null){disassemble(address);function=createFunction(address,null);}
                if (function == null) throw new IllegalArgumentException("No discovered function at "+address);
                InstructionIterator instructions = currentProgram.getListing().getInstructions(function.getBody(),true);
                try (BufferedWriter listing = Files.newBufferedWriter(output.resolve(address+".asm"))) {
                    while (instructions.hasNext()) {
                        var instruction = instructions.next();
                        String bytes=""; for(byte b:instruction.getBytes()) bytes+=String.format("%02x",b & 255);
                        listing.write(instruction.getAddress()+"\t"+bytes+"\t"+instruction+"\n");
                    }
                }
                for (Function called : function.getCalledFunctions(monitor)) {
                    calls.write(address+"\t"+called.getEntryPoint()+"\n");
                }
                DecompileResults result = decompiler.decompileFunction(function,3,monitor);
                if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                    Files.writeString(output.resolve(address+".c"),result.getDecompiledFunction().getC());
                    complete++;
                } else {
                    Files.writeString(output.resolve(address+".error.txt"),String.valueOf(result.getErrorMessage()));
                }
            }
        } finally { decompiler.dispose(); }
        Files.writeString(output.resolve("batch-report.txt"),"requested="+addresses.size()+"\ndecompiled="+complete+"\nidentities=unconfirmed explicit candidates\n");
    }
}
