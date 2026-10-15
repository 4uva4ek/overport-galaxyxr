package moe.crx.overport.patches

import com.reandroid.json.JSONArray
import com.reandroid.json.JSONObject
import moe.crx.overport.patching.Patch
import moe.crx.overport.utils.nameAttribute
import moe.crx.overport.utils.named
import moe.crx.overport.utils.takeNodes
import moe.crx.overport.utils.takeNodesEach

fun createMetadata(name: String, value: String): JSONObject {
    return JSONObject().put("node_type", "element").put("name", "meta-data").put(
        "attributes", JSONArray().put(
            JSONObject().put("name", "name").put("id", 16842755)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "STRING").put("data", name)
        ).put(
            JSONObject().put("name", "value").put("id", 16842788)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "STRING").put("data", value)
        )
    )
}

fun createMetadataFloat(name: String, value: Float): JSONObject {
    return JSONObject().put("node_type", "element").put("name", "meta-data").put(
        "attributes", JSONArray().put(
            JSONObject().put("name", "name").put("id", 16842755)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "STRING").put("data", name)
        ).put(
            JSONObject().put("name", "value").put("id", 16842788)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "FLOAT").put("data", value.toRawBits())
        )
    )
}

fun createUsesFeature(name: String, required: Boolean): JSONObject {
    return JSONObject().put("node_type", "element").put("name", "uses-feature").put(
        "attributes", JSONArray().put(
            JSONObject().put("name", "name").put("id", 16842755)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "STRING").put("data", name)
        ).put(
            JSONObject().put("name", "required").put("id", 16843406)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "BOOLEAN").put("data", required)
        )
    )
}

fun createUsesLibrary(name: String, required: Boolean): JSONObject {
    return JSONObject().put("node_type", "element").put("name", "uses-library").put(
        "attributes", JSONArray().put(
            JSONObject().put("name", "name").put("id", 16842755)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "STRING").put("data", name)
        ).put(
            JSONObject().put("name", "required").put("id", 16843406)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "BOOLEAN").put("data", required)
        )
    )
}

fun createUsesPermission(name: String): JSONObject {
    return JSONObject().put("node_type", "element").put("name", "uses-permission").put(
        "attributes", JSONArray().put(
            JSONObject().put("name", "name").put("id", 16842755)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "STRING").put("data", name)
        )
    )
}

fun createProperty(key: String, value: String): JSONObject {
    return JSONObject().put("node_type", "element").put("name", "property").put(
        "attributes", JSONArray().put(
            JSONObject().put("name", "name").put("id", 16842755)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "STRING").put("data", key)
        ).put(
            JSONObject().put("name", "value").put("id", 16842788)
                .put("uri", "http://schemas.android.com/apk/res/android").put("prefix", "android")
                .put("value_type", "STRING").put("data", value)
        )
    )
}

val PATCH_VR_METADATA = Patch("patch_vr_metadata") {
    // Keep the old explicit galaxyxr argument compatible; no vendor-wide mode.
    require(it.firstOrNull().isNullOrBlank() || it.first().lowercase() == "galaxyxr") {
        "VR metadata supports Galaxy XR only."
    }

    // Galaxy XR (Android XR) target: only the relevant markers,
    // no Pico/YVR/Huawei/HTC vendor-specific meta-data.
    val galaxyManifestNodes = listOf(
        createUsesFeature("android.software.xr.api.openxr", true),
        createUsesFeature("android.software.xr.api.spatial", true),
        createUsesFeature("android.software.xr.input.controller", false),
        createUsesFeature("android.hardware.xr.input.hand_tracking", false),
        createUsesFeature("android.hardware.xr.input.eye_tracking", false),
        createUsesPermission("org.khronos.openxr.permission.OPENXR"),
        createUsesPermission("org.khronos.openxr.permission.OPENXR_SYSTEM"),
        createUsesPermission("android.permission.EYE_TRACKING_COARSE"),
        createUsesPermission("android.permission.EYE_TRACKING_FINE"),
        createUsesPermission("android.permission.HAND_TRACKING"),
        createUsesPermission("android.permission.SCENE_UNDERSTANDING_COARSE"),
        createUsesPermission("android.permission.SCENE_UNDERSTANDING_FINE"),
    )

    val galaxyAppNodes = listOf(
        createMetadata("com.oculus.supportedDevices", "all"),
        createMetadataFloat("android.max_aspect", 2.1f),
        createUsesLibrary("libopenxr.google.so", false),
    )

    selectManifestJson {
        takeNodesEach({ named("manifest") }) {
            takeNodes {
                val nodes = this
                if (nodes != null) for (node in galaxyManifestNodes) nodes.put(node)
                nodes
            }
            takeNodesEach({ named("application") }) {
                takeNodesEach({ named("activity") }) {
                    takeNodes {
                        this?.put(
                            createProperty(
                                "android.window.PROPERTY_XR_ACTIVITY_START_MODE",
                                "XR_ACTIVITY_START_MODE_FULL_SPACE_UNMANAGED"
                            )
                        )?.put(
                            createProperty(
                                "android.window.PROPERTY_XR_BOUNDARY_TYPE_RECOMMENDED",
                                "XR_BOUNDARY_TYPE_LARGE"
                            )
                        )
                    }
                }
                takeNodesEach({ named("meta-data") }) {
                    if (nameAttribute() == "com.oculus.supportedDevices") null else this
                }
                takeNodes {
                    val nodes = this
                    if (nodes != null) for (node in galaxyAppNodes) nodes.put(node)
                    nodes
                }
            }
        }
    }
}