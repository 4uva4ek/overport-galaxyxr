package moe.crx.overport.galaxyxr

import moe.crx.overport.patching.PatchStore
import moe.crx.overport.patches.PATCH_GALAXYXR_DISPATCHER
import moe.crx.overport.patches.PATCH_GALAXYXR_UNITY_GLES
import moe.crx.overport.patches.PATCH_TEXTURE_SIZE
import moe.crx.overport.patches.PATCH_GALAXYXR_SWAPCHAIN
import moe.crx.overport.patches.PATCH_GALAXYXR_PERFORMANCE

/**
 * Samsung Galaxy XR (Android XR) patch target.
 *
 * Galaxy XR runs Android XR, the same platform family as Wave and other
 * Android XR headsets. The dispatcher loaded by the game selects a
 * platform-specific OpenXR loader by `ro.product.brand` (bytedance -> pico,
 * etc.); unknown brands such as samsung fall through to the "generic" loader,
 * which is a full Khronos OpenXR loader implementation: it scans
 * `/system/etc/openxr` and `/vendor/etc/openxr` manifests and loads the
 * runtime listed there. On Galaxy XR that is Google's OpenXR runtime, so the
 * existing runtime stack should work without a dedicated galaxy loader.
 *
 * The dispatcher itself is either the open one (openxr-dispatcher-open, the
 * default) or the closed Overport dispatcher from the runtime cache with the
 * hash-guarded swapchain abort fix.
 *
 * This object provides the patch set preset used by `--target=galaxyxr`.
 */
object GalaxyXrPatches {
    const val TARGET = "galaxyxr"
    const val DEFAULT_RUNTIME = "3.4.3-aa54c3f"
    const val DISPATCHER_OPEN = "open"
    const val DISPATCHER_OVERPORT = "overport"
    val DISPATCHERS = listOf(DISPATCHER_OPEN, DISPATCHER_OVERPORT)

    /**
     * All recommended patches, with the VR metadata patch filtered to the
     * Android XR subset (no Pico/YVR/Huawei/HTC vendor meta-data).
     */
    fun recommended(dispatcher: String = DISPATCHER_OPEN): Map<String, List<String>> =
        (PatchStore.recommended().associate { patch ->
            patch.name to if (patch.name == "patch_vr_metadata") listOf(TARGET) else emptyList()
        } + mapOf(
            PATCH_GALAXYXR_UNITY_GLES.name to emptyList(),
            PATCH_TEXTURE_SIZE.name to listOf("3552x3840"),
            PATCH_GALAXYXR_PERFORMANCE.name to emptyList(),
        )).toMutableMap().also { selectDispatcher(it, dispatcher) }

    /**
     * The open dispatcher replaces the closed one and needs no swapchain fix; the closed
     * dispatcher keeps the hash-guarded fix. Patches run in registration order, so the
     * map order does not matter.
     */
    fun selectDispatcher(patches: MutableMap<String, List<String>>, dispatcher: String) {
        require(dispatcher in DISPATCHERS) { "Dispatcher must be open or overport." }
        if (dispatcher == DISPATCHER_OPEN) {
            patches.remove(PATCH_GALAXYXR_SWAPCHAIN.name)
            patches[PATCH_GALAXYXR_DISPATCHER.name] = emptyList()
        } else {
            patches.remove(PATCH_GALAXYXR_DISPATCHER.name)
            patches[PATCH_GALAXYXR_SWAPCHAIN.name] = emptyList()
        }
    }
}
