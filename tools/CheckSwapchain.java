import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import moe.crx.overport.galaxyxr.SwapchainCompatibility;

// Pass the original ARM64 Overport 3.4.3-aa54c3f libopenxr_loader.so, not a vendor file in git.
class CheckSwapchain {
    public static void main(String[] args) throws Exception {
        var input = Files.readAllBytes(Path.of(args[0]));
        var patch = SwapchainCompatibility.INSTANCE;
        var output = patch.patch(input);
        for (int i = 0; i < input.length; ++i) {
            boolean allowed = (i >= 0x18b8c && i < 0x18b90) || (i >= 0x18b94 && i < 0x18b98);
            if (!allowed && input[i] != output[i]) throw new AssertionError("Unexpected change at " + i);
        }
        if (Arrays.equals(input, output) || !Arrays.equals(output, patch.patch(output))) throw new AssertionError("Change/idempotence");
        var unknown = input.clone(); unknown[1000] ^= 1;
        var before = unknown.clone();
        try { patch.patch(unknown); throw new AssertionError("Unknown accepted"); } catch (IllegalArgumentException expected) { }
        if (!Arrays.equals(before, unknown)) throw new AssertionError("Unknown mutated");
        try { patch.patch(new byte[0]); throw new AssertionError("Truncated accepted"); } catch (IllegalArgumentException expected) { }
        System.out.println("PASS: only two abort branches replaced, idempotence, unknown/truncated refusal without mutation");
    }
}
