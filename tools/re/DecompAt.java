import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import java.io.FileWriter;

public class DecompAt extends GhidraScript {
	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		String outPath = args[0];
		FileWriter out = new FileWriter(outPath, true);
		DecompInterface ifc = new DecompInterface();
		ifc.openProgram(currentProgram);
		for (int i = 1; i < args.length; i++) {
			Address a = toAddr(Long.decode(args[i]));
			Function f = getFunctionContaining(a);
			if (f == null) {
				DisassembleCommand dis = new DisassembleCommand(a, null, true);
				dis.applyTo(currentProgram, monitor);
				CreateFunctionCmd cf = new CreateFunctionCmd(a);
				cf.applyTo(currentProgram, monitor);
				f = getFunctionAt(a);
			}
			out.write("\n// ===== " + args[i] + " =====\n");
			if (f == null) {
				out.write("// could not create function\n");
				continue;
			}
			DecompileResults r = ifc.decompileFunction(f, 120, monitor);
			if (r != null && r.getDecompiledFunction() != null) {
				out.write(r.getDecompiledFunction().getC());
			} else {
				out.write("// decompile failed: " + (r != null ? r.getErrorMessage() : "null") + "\n");
			}
		}
		out.close();
	}
}
