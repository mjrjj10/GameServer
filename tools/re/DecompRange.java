import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.address.Address;
import java.io.FileWriter;

public class DecompRange extends GhidraScript {
	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		FileWriter out = new FileWriter(args[0], false);
		Address lo = toAddr(Long.decode(args[1]));
		Address hi = toAddr(Long.decode(args[2]));
		DecompInterface ifc = new DecompInterface();
		ifc.openProgram(currentProgram);
		for (Function f : currentProgram.getFunctionManager().getFunctions(lo, true)) {
			if (f.getEntryPoint().compareTo(hi) > 0) break;
			DecompileResults r = ifc.decompileFunction(f, 60, monitor);
			out.write("\n// ===== " + f.getEntryPoint() + " =====\n");
			if (r != null && r.getDecompiledFunction() != null) out.write(r.getDecompiledFunction().getC());
		}
		out.close();
	}
}
