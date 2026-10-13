package moe.crx.overport.galaxyxr

import moe.crx.overport.patching.PatchStore
import moe.crx.overport.patches.PATCH_GALAXYXR_UNITY_GLES
import moe.crx.overport.patches.PATCH_TEXTURE_SIZE
import moe.crx.overport.patches.PATCH_GALAXYXR_SWAPCHAIN
import moe.crx.overport.patches.PATCH_GALAXYXR_PERFORMANCE

/**
 * Samsung Galaxy XR (Android XR) patch target.
 *
 * Galaxy XR runs Android XR, the same platform family as Wave and other
 * Android XR headsets. The closed overport runtime selects a
 * platform-specific OpenXR loader by `ro.product.brand` (bytedance -> pico,
 * etc.); unknown brands such as samsung fall through to the "generic" loader,
 * which is a full Khronos OpenXR loader implementation: it scans
 * `/system/etc/openxr` and `/vendor/etc/openxr` manifests and loads the
 * runtime listed there. On Galaxy XR that is Google's OpenXR runtime, so the
 * existing runtime stack should work without a dedicated galaxy loader.
 *
 * This object provides the patch set preset used by `--target=galaxyxr`.
 */
object GalaxyXrPatches {
    const val TARGET = "galaxyxr"

    /**
     * All recommended patches, with the VR metadata patch filtered to the
     * Android XR subset (no Pico/YVR/Huawei/HTC vendor meta-data).
     */
    fun recommended(): Map<String, List<String>> = PatchStore.recommended().associate { patch ->
        patch.name to if (patch.name == "patch_vr_metadata") listOf(TARGET) else emptyList()
    } + mapOf(
        PATCH_GALAXYXR_UNITY_GLES.name to emptyList(),
        PATCH_TEXTURE_SIZE.name to listOf("3552x3840"),
        PATCH_GALAXYXR_SWAPCHAIN.name to emptyList(),
        PATCH_GALAXYXR_PERFORMANCE.name to emptyList(),
    )
}
