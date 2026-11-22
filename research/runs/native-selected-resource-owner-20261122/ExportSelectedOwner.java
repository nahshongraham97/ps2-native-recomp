// Read-only private-session evidence; every output is exclusively created.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.app.decompiler.DecompInterface;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.BufferedWriter;

public class ExportSelectedOwner extends GhidraScript {
    private void save(Path path, String text) throws Exception {
        Files.writeString(path, text, StandardCharsets.UTF_8, StandardOpenOption.CREATE_NEW);
    }
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Path out = Path.of(args[0]);
        Files.createDirectory(out);
        if (!currentProgram.getName().equals("SLUS_214.10") ||
            !currentProgram.getExecutableSHA256().equals("a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f") ||
            !currentProgram.getLanguageID().toString().equals("r5900:LE:32:default"))
            throw new IllegalStateException("Unexpected program/image/language");
        save(out.resolve("PROGRAM.txt"), currentProgram.getName() + "\n" +
            currentProgram.getExecutableSHA256() + "\n" + currentProgram.getLanguageID() +
            "\nlanguage-version=" + currentProgram.getLanguage().getVersion() +
            "\nlanguage-minor=" + currentProgram.getLanguage().getMinorVersion() + "\n");
        DecompInterface d = new DecompInterface();
        try {
            if (!d.openProgram(currentProgram)) throw new IllegalStateException(d.getLastMessage());
            for (int i = 1; i < args.length; ++i) {
                Address at = toAddr(args[i]);
                Function f = getFunctionAt(at);
                if (f == null) { disassemble(at); f = createFunction(at, null); }
                if (f == null) throw new IllegalArgumentException("No function " + at);
                long begin = at.getOffset(), end = f.getBody().getMaxAddress().getOffset() + 1;
                if (end - begin > 65536) throw new IllegalArgumentException("Unbounded body " + at);
                StringBuilder asm = new StringBuilder(), calls = new StringBuilder();
                for (long pc = begin; pc < end; pc += 4) {
                    Address p = toAddr(pc);
                    Instruction ins = getInstructionAt(p);
                    if (ins == null) { disassemble(p); ins = getInstructionAt(p); }
                    if (ins == null || ins.getLength() != 4)
                        throw new IllegalStateException("Undecoded EE word " + p);
                    byte[] b = getBytes(p, 4);
                    asm.append(String.format("%08x\t%02x%02x%02x%02x\t%s%s\n", pc,
                        b[0]&255,b[1]&255,b[2]&255,b[3]&255,
                        ins.isInDelaySlot() ? "_" : "", ins.toString()));
                    if (ins.getFlowType().isCall())
                        for (Address target : ins.getFlows()) calls.append(p + "\t" + target + "\n");
                }
                save(out.resolve(at + ".asm"), asm.toString());
                save(out.resolve(at + ".calls.tsv"), calls.toString());
                var result = d.decompileFunction(f, 10, monitor);
                if (result.decompileCompleted() && result.getDecompiledFunction() != null)
                    save(out.resolve(at + ".c"), result.getDecompiledFunction().getC());
                else save(out.resolve(at + ".error.txt"), String.valueOf(result.getErrorMessage()));
                StringBuilder refs = new StringBuilder();
                var it = getReferencesTo(at);
                for (var ref : it) refs.append(ref.getFromAddress() + "\t" + ref.getReferenceType() + "\n");
                save(out.resolve(at + ".refs.tsv"), refs.toString());
            }
        } finally { d.dispose(); }
    }
}
