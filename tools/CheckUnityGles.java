import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import moe.crx.overport.galaxyxr.UnityGlesCompatibility;
import moe.crx.overport.galaxyxr.GalaxyXrPatches;
import moe.crx.overport.patching.PatchStore;
import moe.crx.overport.patches.TextureSize;

// Run with the built CLI JAR on the classpath and one or more inspected, unmodified
// ARM64 libunity.so files as arguments. No vendor binary is stored in the repository.
class CheckUnityGles {
    public static void main(String[] args) throws Exception {
        var engine = UnityGlesCompatibility.INSTANCE;
        for (String path : args) {
            var original = Files.readAllBytes(Path.of(path));
            var sites = engine.locate(original);
            require(sites.size() == 1 && !sites.get(0).getPatched(), "one unpatched signature in " + path);
            int site = sites.get(0).getValueOffset();
            require((original[site] & 0xff) == 0x22, "mov w2, #1 at site");

            var patched = original.clone();
            require(engine.patch(patched) == UnityGlesCompatibility.Result.APPLIED, "supported binary");
            int differences = 0;
            for (int i = 0; i < original.length; ++i) if (original[i] != patched[i]) ++differences;
            require(differences == 1 && (patched[site] & 0xff) == 0x02, "exactly one changed byte at the site");
            var once = patched.clone();
            require(engine.patch(patched) == UnityGlesCompatibility.Result.ALREADY_APPLIED, "idempotence");
            require(Arrays.equals(once, patched), "repatch must not mutate");

            var noSignature = original.clone();
            noSignature[site - 4] ^= 1; // break the EGL attribute constant
            var noSignatureBefore = noSignature.clone();
            require(engine.patch(noSignature) == UnityGlesCompatibility.Result.UNSUPPORTED, "no signature");
            require(Arrays.equals(noSignatureBefore, noSignature), "no signature must not mutate");

            var ambiguous = original.clone();
            System.arraycopy(original, site - 4, ambiguous, site - 4 + 0x1000, 12); // second copy in the same text segment
            require(engine.locate(ambiguous).size() == 2, "duplicate detected");
            var ambiguousBefore = ambiguous.clone();
            require(engine.patch(ambiguous) == UnityGlesCompatibility.Result.UNSUPPORTED, "ambiguous");
            require(Arrays.equals(ambiguousBefore, ambiguous), "ambiguous must not mutate");

            var truncated = Arrays.copyOf(original, site);
            require(engine.patch(truncated) == UnityGlesCompatibility.Result.UNSUPPORTED, "truncated before site");
            System.out.println("PASS: " + path + " site 0x" + Integer.toHexString(site));
        }
        require(engine.patch(new byte[0]) == UnityGlesCompatibility.Result.UNSUPPORTED, "empty input");
        var notElf = new byte[0x100];
        require(engine.patch(notElf) == UnityGlesCompatibility.Result.UNSUPPORTED, "non-ELF input");
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
        System.out.println("PASS: signature match, one-byte change, idempotence, missing/duplicate signature, truncated/non-ELF input, target isolation");
        System.out.println("PASS: texture size parsing, native reset, invalid bounds, opt-in render overrides");
    }
    static void require(boolean ok, String message) {
        if (!ok) throw new AssertionError(message);
    }
}
