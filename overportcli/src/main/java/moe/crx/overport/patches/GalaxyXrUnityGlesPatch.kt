package moe.crx.overport.patches

import moe.crx.overport.galaxyxr.UnityGlesCompatibility
import moe.crx.overport.patching.Patch

val PATCH_GALAXYXR_UNITY_GLES = Patch(
    "patch_galaxyxr_unity_gles", isRecommended = false, requiresOverportConfig = false
) {
    var found = false
    selectLibrary("libunity.so") {
        found = true
        val bytes = file.readBytes()
        when (UnityGlesCompatibility.patch(bytes)) {
            UnityGlesCompatibility.Result.APPLIED -> {
                writeBytes(bytes)
                println("\nGalaxy XR EGL: disabled Unity no-error context (${file.parentFile.name}).")
            }
            UnityGlesCompatibility.Result.ALREADY_APPLIED ->
                println("\nGalaxy XR EGL: already applied (${file.parentFile.name}).")
            UnityGlesCompatibility.Result.UNSUPPORTED ->
                println("\nGalaxy XR EGL: NOT APPLIED; unrecognized libunity.so (${file.parentFile.name}), left unchanged.")
        }
    }
    if (!found) println("\nGalaxy XR EGL: not applicable (no libunity.so).")
}
