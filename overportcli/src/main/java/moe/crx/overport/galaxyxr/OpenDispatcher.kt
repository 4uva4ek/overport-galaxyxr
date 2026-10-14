package moe.crx.overport.galaxyxr

import java.security.MessageDigest

/**
 * Open replacement for the closed Overport dispatcher that converted games load as
 * `lib/arm64-v8a/libopenxr_loader.so`: Molier/openxr-dispatcher-open (MIT, notice in the
 * resource `/galaxyxr/LICENSE-openxr-dispatcher-open.txt`). `native/galaxyxr/build_dispatcher.sh`
 * builds the pinned upstream commit into the `.bin` resource; the APK keeps the `.so` name.
 * The chain behind it is unchanged: dispatcher, adapter (`libopenxr_loader_generic.so`),
 * original generic loader, system runtime.
 */
object OpenDispatcher {
    const val VERSION = "1.0.0"
    const val COMMIT = "68479ea28e69202577cba51fbcab9baeb827a7fc"
    const val RESOURCE = "/galaxyxr/arm64-v8a/libopenxr_loader.bin"

    fun load(): ByteArray {
        val bytes = OpenDispatcher::class.java.getResourceAsStream(RESOURCE)?.use { it.readBytes() }
            ?: error("Missing open dispatcher: run native/galaxyxr/build_dispatcher.sh before building the CLI")
        validate(bytes)
        return bytes
    }

    /** An ARM64 ELF shared object that carries the dispatcher start-up message, version and log tag. */
    fun validate(bytes: ByteArray) {
        require(bytes.size > 64 && bytes[0] == 0x7f.toByte() && bytes[1] == 'E'.code.toByte() &&
            bytes[2] == 'L'.code.toByte() && bytes[3] == 'F'.code.toByte()) { "Open dispatcher resource is not an ELF file" }
        require(bytes[4] == 2.toByte() && bytes[5] == 1.toByte()) { "Open dispatcher resource is not a 64-bit little-endian ELF" }
        val type = (bytes[16].toInt() and 255) or ((bytes[17].toInt() and 255) shl 8)
        val machine = (bytes[18].toInt() and 255) or ((bytes[19].toInt() and 255) shl 8)
        require(type == 3 && machine == 0xb7) { "Open dispatcher resource is not an ARM64 shared object" }
        val text = String(bytes, Charsets.ISO_8859_1)
        require(text.contains("open dispatcher ") && text.contains("OpenXRDispatch") && text.contains("\u0000$VERSION\u0000")) {
            "Open dispatcher resource lacks the dispatcher $VERSION identity strings"
        }
    }

    fun sha256(bytes: ByteArray): String =
        MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") { "%02x".format(it) }
}
