package moe.crx.overport.patches

import moe.crx.overport.patching.Patch

/** Dimensions are per-eye recommendations; the application still controls its render scale. */
data class TextureSize(val width: Int, val height: Int) {
    init {
        require((width == 0 && height == 0) || (width in 64..4096 && height in 64..4096)) {
            "Texture size must be native (0x0), or WIDTHxHEIGHT with both dimensions in 64..4096."
        }
    }

    companion object {
        fun parse(value: String): TextureSize {
            if (value == "native") return TextureSize(0, 0)
            val match = Regex("([0-9]+)x([0-9]+)").matchEntire(value)
            requireNotNull(match) { "Expected --texture-size=WIDTHxHEIGHT or --texture-size=native." }
            return TextureSize(match.groupValues[1].toInt(), match.groupValues[2].toInt())
        }
    }
}

val PATCH_TEXTURE_SIZE = Patch("patch_texture_size", isRecommended = false) { args ->
    require(args.size == 1) { "patch_texture_size requires WIDTHxHEIGHT or native." }
    val size = TextureSize.parse(args.single())
    selectConfig {
        suggestedTextureWidth = size.width
        suggestedTextureHeight = size.height
    }
    println("\nOverport per-eye texture recommendation: ${if (size.width == 0) "native" else "${size.width}x${size.height}"}.")
}

val PATCH_VERBOSE_RUNTIME = Patch("patch_verbose_runtime", isRecommended = false) {
    selectConfig { verboseLogging = 1 }
    println("\nOverport verbose runtime logging enabled in this APK.")
}
