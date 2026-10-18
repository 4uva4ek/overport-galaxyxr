import java.lang.reflect.Method;
import java.util.List;
import java.util.Map;

/** Run against the built CLI JAR: java -cp CLI.jar tools/CheckFpsHud.java. */
public class CheckFpsHud {
    private static final String PERFORMANCE = "patch_galaxyxr_performance";

    @SuppressWarnings("unchecked")
    private static Map<String, List<String>> selection(String... args) throws Exception {
        Class<?> command = Class.forName("moe.crx.overport.commands.PatchApplicationCommand");
        Object instance = command.getField("INSTANCE").get(null);
        Method scan = command.getDeclaredMethod("scanArguments", List.class);
        scan.setAccessible(true);
        Object parsed = scan.invoke(instance, List.of(args));
        Method select = command.getDeclaredMethod("selectedPatches", parsed.getClass());
        select.setAccessible(true);
        return (Map<String, List<String>>) select.invoke(instance, parsed);
    }

    private static void equal(Object expected, Object actual) {
        if (!expected.equals(actual)) throw new AssertionError("Expected " + expected + ", got " + actual);
    }

    public static void main(String[] args) throws Exception {
        Map<String, List<String>> defaults = selection();
        equal(List.of(), defaults.get(PERFORMANCE)); // Patch's existing default remains off.
        Map<String, List<String>> enabled = selection("--fps-hud=on");
        equal(List.of("max", "app", "auto", "fix", "meta", "edited", "0.35", "24.0", "0.15", "on"), enabled.get(PERFORMANCE));
        equal(defaults.keySet(), enabled.keySet());
        equal(List.of("3552x3840"), enabled.get("patch_texture_size"));
        equal(List.of("max", "app", "auto", "fix", "meta", "edited", "0.35", "24.0", "0.15", "off"),
            selection("--fps-hud=off").get(PERFORMANCE));
        equal(List.of("app", "eye", "off", "runtime", "runtime", "rebuild", "0.35", "24.0", "0.15", "on"),
            selection("--patches=" + PERFORMANCE + "=app,eye,off,runtime,runtime,rebuild", "--fps-hud=on").get(PERFORMANCE));
        equal(List.of("app", "app", "off", "fix", "meta", "edited", "0.35", "24.0", "0.15", "on"),
            selection("--performance=app", "--hands=off", "--fps-hud=on").get(PERFORMANCE));
        equal(List.of("app", "eye", "off", "runtime", "runtime", "rebuild", "0.5", "28.0", "0.5", "on"),
            selection("--patches=" + PERFORMANCE + "=app,eye,off,runtime,runtime,rebuild,0.5,28.0,0.5,on", "--performance=app").get(PERFORMANCE));
        equal(List.of("app", "runtime-eye", "off", "fix", "meta", "edited", "0.25", "32", "1", "on"),
            selection("--patches=" + PERFORMANCE + "=app,runtime-eye,off,fix,meta,edited,0.25,32,1", "--fps-hud=on").get(PERFORMANCE));
        equal(List.of("max", "runtime-eye", "auto", "fix", "meta", "edited", "0.35", "16", "0.15", "on"),
            selection("--foveation=runtime-eye", "--foveation-gain=16", "--fps-hud=on").get(PERFORMANCE));
        System.out.println("FPS HUD CLI parsing and patch propagation checks passed.");
    }
}
