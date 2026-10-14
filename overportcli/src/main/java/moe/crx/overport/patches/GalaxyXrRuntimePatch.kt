package moe.crx.overport.patches

import moe.crx.overport.galaxyxr.OpenDispatcher
import moe.crx.overport.galaxyxr.PERMISSION_EYE_TRACKING_FINE
import moe.crx.overport.galaxyxr.PERMISSION_HAND_TRACKING
import moe.crx.overport.galaxyxr.SwapchainCompatibility
import moe.crx.overport.galaxyxr.addPermissionLauncher
import moe.crx.overport.galaxyxr.declaresQuestHandTracking
import moe.crx.overport.patching.Patch

/** Closed Overport dispatcher only: hash-guarded removal of its 4096px swapchain abort. */
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

/**
 * Replace the closed Overport dispatcher copied by patch_copy_libraries with the open one.
 * The adapter, the preserved generic loader and the configuration files stay as they are;
 * the open dispatcher reads the same liboverport.config.so keys and passes oversized
 * swapchains to the runtime, so patch_galaxyxr_swapchain is neither needed nor applicable.
 */
val PATCH_GALAXYXR_DISPATCHER = Patch("patch_galaxyxr_dispatcher", isRecommended = false) {
    val dispatcher = OpenDispatcher.load()
    var found = false
    selectLibrary("libopenxr_loader.so") {
        if (file.parentFile.name == "arm64-v8a") {
            writeBytes(dispatcher)
            found = true
        }
    }
    require(found) { "Galaxy XR requires the ARM64 Overport loader; apply patch_copy_libraries first." }
    println("\nGalaxy XR: open OpenXR dispatcher ${OpenDispatcher.VERSION} (${OpenDispatcher.sha256(dispatcher)}) replaces the closed Overport dispatcher.")
}

/** Arguments: performance (max|app), foveation (app|eye), hands (auto|on|off), handmesh (fix|runtime). */
val PATCH_GALAXYXR_PERFORMANCE = Patch("patch_galaxyxr_performance", isRecommended = false) { args ->
    val mode = args.firstOrNull() ?: "max"
    require(mode == "max" || mode == "app") { "Performance mode must be max or app." }
    val foveation = args.getOrNull(1) ?: "app"
    require(foveation in listOf("app", "eye")) { "Foveation must be app or eye." }
    val hands = args.getOrNull(2) ?: "auto"
    require(hands in listOf("auto", "on", "off")) { "Hands must be auto, on or off." }
    // The Galaxy XR runtime hand mesh skeleton (parents, radii, tip and metacarpal bind poses) is incomplete;
    // the adapter rebuilds it unless handmesh=runtime keeps the runtime description.
    val handMesh = args.getOrNull(3) ?: "fix"
    require(handMesh in listOf("fix", "runtime")) { "Hand mesh must be fix or runtime." }
    // Android XR delivers hand joints only after the runtime HAND_TRACKING permission is granted.
    // auto requests it only for games that declare Quest hand tracking in their manifest.
    val handTracking = hands == "on" || (hands == "auto" && declaresQuestHandTracking())
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
            val handsValue = if (handTracking) "on" else "off"
            file.resolveSibling("liboverport.galaxyxr.config.so")
                .writeText("performance=$mode\nfoveation=$foveation\nhands=$handsValue\nhandmesh=$handMesh\n")
            found = true
        }
    }
    require(found) { "Galaxy XR performance defaults require the ARM64 generic loader." }
    val permissions = listOfNotNull(
        PERMISSION_EYE_TRACKING_FINE.takeIf { foveation == "eye" },
        PERMISSION_HAND_TRACKING.takeIf { handTracking },
    )
    if (permissions.isNotEmpty()) addPermissionLauncher(permissions)
    selectConfig {
        // Overport maps config n to OpenXR (n - 1) * 25: 4 == BOOST (75), NOT 75.
        suggestedCpuLevel = if (mode == "max") 4 else 0
        suggestedGpuLevel = if (mode == "max") 4 else 0
    }
    println(if (mode == "max") "\nGalaxy XR: request CPU/GPU BOOST and supported 90 Hz for every XR session." else "\nGalaxy XR: application-controlled performance and refresh rate.")
    println(if (handMesh == "fix") "Galaxy XR: hand skeleton rebuilt from the runtime hand mesh." else "Galaxy XR: runtime hand mesh skeleton kept as is (handmesh=runtime).")
    println(when {
        handTracking -> "Galaxy XR: hand tracking permission is requested at launch (hands=$hands)."
        hands == "off" -> "Galaxy XR: hand tracking permission is not requested (hands=off)."
        else -> "Galaxy XR: no Quest hand tracking declared in the manifest; hand tracking permission is not requested."
    })
}
