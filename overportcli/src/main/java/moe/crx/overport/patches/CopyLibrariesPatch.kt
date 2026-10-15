package moe.crx.overport.patches

import moe.crx.overport.patching.Patch
import java.io.File

// These vendor OpenXR loaders are not used by the Galaxy XR generic-loader chain.
private val OTHER_HEADSET_LOADERS = setOf(
    "libopenxr_loader_huawei.so", "libopenxr_loader_meta.so",
    "libopenxr_loader_pico.so", "libopenxr_loader_yvr.so",
)

val PATCH_COPY_LIBRARIES = Patch("patch_copy_libraries") {
    selectWorkspace {
        file.resolve("root/lib").listFiles()?.forEach { archDirectory ->
            val arch = getLibraries().resolve("lib/${archDirectory.name}")

            // Remove leftovers only from the extracted output, never from the input APK/cache.
            OTHER_HEADSET_LOADERS.forEach { name ->
                val unused = archDirectory.resolve(name)
                require(!unused.exists() || unused.delete()) { "Cannot remove unused loader $unused" }
            }
            if (arch.exists()) {
                arch.listFiles().filter { it.name != "libOVRPlugin.so" && it.name !in OTHER_HEADSET_LOADERS }.forEach { library ->
                    library.copyTo(File(archDirectory, library.name), true)
                }
            }
        }
    }
}