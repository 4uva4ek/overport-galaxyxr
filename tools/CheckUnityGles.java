import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import moe.crx.overport.galaxyxr.UnityGlesCompatibility;
import moe.crx.overport.galaxyxr.GalaxyXrPatches;
import moe.crx.overport.patching.PatchStore;
import moe.crx.overport.patches.TextureSize;

// Run with the built CLI JAR on the classpath and the inspected, unmodified
// libunity.so as the only argument. No vendor binary is stored in the repository.
class CheckUnityGles {
    public static void main(String[] args) throws Exception {
        var original = Files.readAllBytes(Path.of(args[0]));
        var patched = original.clone();
        var engine = UnityGlesCompatibility.INSTANCE;
        require(engine.patch(patched) == UnityGlesCompatibility.Result.APPLIED, "known binary");
        int differences = 0;
        for (int i = 0; i < original.length; ++i) if (original[i] != patched[i]) ++differences;
        require(differences == 1, "exactly one changed byte");
        var once = patched.clone();
        require(engine.patch(patched) == UnityGlesCompatibility.Result.ALREADY_APPLIED, "idempotence");
        require(Arrays.equals(once, patched), "repatch must not mutate");
        var unknown = original.clone();
        unknown[unknown.length - 1] ^= 1;
        var unknownBefore = unknown.clone();
        require(engine.patch(unknown) == UnityGlesCompatibility.Result.UNSUPPORTED, "unknown digest");
        require(Arrays.equals(unknownBefore, unknown), "unknown must not mutate");
        require(engine.patch(new byte[0]) == UnityGlesCompatibility.Result.UNSUPPORTED, "truncated input");
        String name = "patch_galaxyxr_unity_gles";
        require(PatchStore.INSTANCE.get(name) != null, "registered");
        require(PatchStore.INSTANCE.recommended().stream().noneMatch(p -> p.getName().equals(name)), "default target unchanged");
        require(GalaxyXrPatches.INSTANCE.recommended().containsKey(name), "Galaxy target includes patch");
        require(GalaxyXrPatches.INSTANCE.recommended().get("patch_texture_size").equals(java.util.List.of("3552x3840")), "Galaxy panel-size default");
        require(TextureSize.Companion.parse("2320x2700").equals(new TextureSize(2320, 2700)), "texture dimensions");
        require(TextureSize.Companion.parse("native").equals(new TextureSize(0, 0)), "native resolution reset");
        for (String invalid : new String[]{"-1x2160", "0x2160", "5000x2160", "2320", "1x1", "999999999999x2000"}) {
            try { TextureSize.Companion.parse(invalid); throw new AssertionError("Accepted " + invalid); }
            catch (IllegalArgumentException expected) { }
        }
        require(PatchStore.INSTANCE.recommended().stream().noneMatch(p -> p.getName().equals("patch_texture_size") || p.getName().equals("patch_verbose_runtime")), "render overrides opt-in");
        System.out.println("PASS: known input, one-byte change, idempotence, unknown/truncated input, target isolation");
        System.out.println("PASS: texture size parsing, native reset, invalid bounds, opt-in render overrides");
    }
    static void require(boolean ok, String message) {
        if (!ok) throw new AssertionError(message);
    }
}
