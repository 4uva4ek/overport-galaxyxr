package moe.crx.overport.galaxyxr

import com.reandroid.json.JSONArray
import com.reandroid.json.JSONObject
import moe.crx.overport.patching.PatchExecutor
import moe.crx.overport.utils.*

/** Android XR runtime permissions the generated launcher can request before the game starts. */
const val PERMISSION_HAND_TRACKING = "android.permission.HAND_TRACKING"
const val PERMISSION_EYE_TRACKING_FINE = "android.permission.EYE_TRACKING_FINE"

/** Quest manifest markers of built-in hand tracking support. */
private val QUEST_HAND_TRACKING_MARKERS = setOf(
    "com.oculus.permission.HAND_TRACKING",
    "oculus.permission.handtracking",
    "oculus.software.handtracking",
)

/** True when the manifest declares Quest hand tracking (permission or feature). */
fun PatchExecutor.declaresQuestHandTracking(): Boolean {
    var declared = false
    selectManifestJson {
        val manifest = elem<JSONArray>("nodes").elemEach<JSONObject> { named("manifest") }.firstOrNull()
        declared = manifest.elem<JSONArray>("nodes")
            .elemEach<JSONObject> { named("uses-permission") || named("uses-feature") }
            .any { it.nameAttribute() in QUEST_HAND_TRACKING_MARKERS }
    }
    return declared
}

/**
 * Request the given runtime permissions before starting the original XR activity, for any engine.
 * Launcher intent filters move to a small bridge activity; the original activity keeps its XR properties.
 */
fun PatchExecutor.addPermissionLauncher(permissions: List<String>) {
    require(permissions.isNotEmpty()) { "No permissions to request" }
    require(permissions.all { Regex("[A-Za-z0-9_.]+").matches(it) }) { "Invalid permission name" }
    val prefix = "moe.crx.overport.injected.GxrPermission"
    val targets = mutableListOf<Pair<String, String>>()
    fun attribute(name: String, id: Int, type: String, value: Any) = JSONObject()
        .put("name", name).put("id", id).put("uri", "http://schemas.android.com/apk/res/android")
        .put("prefix", "android").put("value_type", type).put("data", value)
    selectManifestJson {
        takeNodesEach({ named("manifest") }) {
            takeNodesEach({ named("application") }) {
                val children = elem<JSONArray>("nodes") ?: error("Application has no manifest nodes")
                val existing = children.elemEach<JSONObject> { named("activity") || named("activity-alias") }
                require(existing.none { it.nameAttribute()?.startsWith(prefix) == true }) {
                    "Permission launcher already exists. Repatch the original APK."
                }
                for (activity in existing) {
                    val nodes = activity.elem<JSONArray>("nodes") ?: continue
                    val filters = nodes.elemEach<JSONObject> { named("intent-filter") }.filter { filter ->
                        val entries = filter.elem<JSONArray>("nodes").elemEach<JSONObject>()
                        entries.any { it.named("action") && it.nameAttribute() == "android.intent.action.MAIN" } &&
                            entries.any { it.named("category") && it.nameAttribute() in listOf("android.intent.category.LAUNCHER", "com.oculus.intent.category.VR") }
                    }
                    if (filters.isEmpty()) continue
                    val name = activity.nameAttribute() ?: error("Launcher activity has no name")
                    val target = when {
                        name.startsWith('.') -> applicationPackage() + name
                        '.' !in name -> "${applicationPackage()}.$name"
                        else -> name
                    }
                    require(Regex("[A-Za-z0-9_.$]+").matches(target)) { "Invalid launcher class" }
                    val bridge = "$prefix${targets.size}"
                    targets += bridge to target
                    activity.takeNodes { takeEach<JSONObject>({ this in filters }) { null } }
                    val bridgeNodes = JSONArray()
                    for (filter in filters) bridgeNodes.put(filter)
                    // A normal Activity hosts the system permission dialog; only the original
                    // activity enters full-space XR. Keep that activity's properties untouched.
                    children.put(JSONObject().put("node_type", "element").put("name", "activity")
                        .put("attributes", JSONArray()
                            .put(attribute("name", 16842755, "STRING", bridge))
                            .put(attribute("exported", 16842768, "BOOLEAN", true)))
                        .put("nodes", bridgeNodes))
                }
                this
            }
        }
    }
    require(targets.isNotEmpty()) { "No launcher entry found for the permission request" }
    val template = PermissionLauncherResource::class.java.getResourceAsStream("/galaxyxr/XrPermissionActivity.smali")
        ?.bufferedReader()?.use { it.readText() } ?: error("Missing permission launcher template")
    // Any missing permission opens one system dialog for the whole list; granted entries are skipped by the OS.
    val checks = permissions.joinToString("\n") { permission ->
        listOf(
            "    const-string v0, \"$permission\"",
            "    invoke-virtual {p0, v0}, Landroid/app/Activity;->checkSelfPermission(Ljava/lang/String;)I",
            "    move-result v1",
            "    if-nez v1, :missing",
        ).joinToString("\n")
    }
    val array = permissions.withIndex().joinToString("\n") { (index, permission) ->
        listOf(
            "    const-string v0, \"$permission\"",
            "    const/16 v2, $index",
            "    aput-object v0, v1, v2",
        ).joinToString("\n")
    }
    for ((bridge, target) in targets) {
        createSmali(bridge.replace('.', '/')) {
            writeText(template
                .replace("__BRIDGE__", bridge.replace('.', '/'))
                .replace("__TARGET__", target)
                .replace("__PERMISSION_CHECKS__", checks)
                .replace("__PERMISSION_ARRAY__", array)
                .replace("__PERMISSION_COUNT__", permissions.size.toString())
                .replace("__PERMISSION_NAMES__", permissions.joinToString(" ")))
        }
    }
    println("\nGalaxy XR: ${permissions.joinToString(", ")} requested before the original game activity starts.")
}

private object PermissionLauncherResource
