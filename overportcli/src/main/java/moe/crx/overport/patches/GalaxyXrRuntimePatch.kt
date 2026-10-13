package moe.crx.overport.patches

import moe.crx.overport.galaxyxr.SwapchainCompatibility
import moe.crx.overport.patching.Patch

val PATCH_GALAXYXR_SWAPCHAIN = Patch("patch_galaxyxr_swapchain", isRecommended = false) {
    var found = false
    selectLibrary("libopenxr_loader.so") {
        if (file.parentFile.name == "arm64-v8a") {
            writeBytes(SwapchainCompatibility.patch(file.readBytes()))
            found = true
        }
    }
    require(found) { "Galaxy XR requires the ARM64 Overport loader." }
    println("\nGalaxy XR: swapchain size validation delegated to the native runtime (no 4096px abort).")
}

val PATCH_GALAXYXR_PERFORMANCE = Patch("patch_galaxyxr_performance", isRecommended = false) { args ->
    val mode = args.firstOrNull() ?: "max"
    require(mode == "max" || mode == "app") { "Performance mode must be max or app." }
    var found = false
    selectLibrary("libopenxr_loader_generic.so") {
        if (file.parentFile.name == "arm64-v8a") {
            val adapter = SwapchainCompatibility::class.java.getResourceAsStream("/galaxyxr/arm64-v8a/libopenxr_loader_generic.bin")
                ?.use { it.readBytes() } ?: error("Missing Galaxy XR adapter: run native/galaxyxr/build.sh before building the CLI")
            // copy_libraries has just restored the original generic loader from the runtime cache.
            // Never rename or modify the system runtime or the cached third-party libraries.
            val backup = file.resolveSibling("libopenxr_loader_gxrbase.so")
            val current = file.readBytes()
            if (!current.contentEquals(adapter)) backup.writeBytes(current)
            require(backup.exists()) { "Original generic loader missing; repatch the original APK with patch_copy_libraries" }
            writeBytes(adapter)
            file.resolveSibling("liboverport.galaxyxr.config.so").writeText("performance=$mode\n")
            found = true
        }
    }
    require(found) { "Galaxy XR performance defaults require the ARM64 generic loader." }
    selectConfig {
        // Overport maps config n to OpenXR (n - 1) * 25: 4 == BOOST (75), NOT 75.
        suggestedCpuLevel = if (mode == "max") 4 else 0
        suggestedGpuLevel = if (mode == "max") 4 else 0
    }
    println(if (mode == "max") "\nGalaxy XR: request CPU/GPU BOOST and supported 90 Hz for every XR session." else "\nGalaxy XR: application-controlled performance and refresh rate.")
}
