package moe.crx.overport.galaxyxr

import java.security.MessageDigest

/** App-local EGL workaround, limited to the inspected Unity 2022.3.33f1 ARM64 binary. */
object UnityGlesCompatibility {
    private const val ORIGINAL_SHA256 = "8203664e3b9203e9731251ea958ca2b1d9f0fc1e3b10032c34a61e5ee8068891"
    private const val PATCHED_SHA256 = "7d468de5616967bd87a38a03e685641ade94c0d1f9abe4c8e6a642e5dadc9c05"
    // File offset, NOT VA 0x7f3a78. ELF PT_LOAD maps VA to file with a 0x1000 delta.
    private const val VALUE_OFFSET = 0x7f2a78
    private val expected = byteArrayOf(0x61, 0x36, 0x86.toByte(), 0x52, 0x22, 0x00, 0x80.toByte(), 0x52)

    enum class Result { APPLIED, ALREADY_APPLIED, UNSUPPORTED }

    /** Unknown binaries remain untouched. Validate the whole output before mutating the caller's bytes. */
    fun patch(bytes: ByteArray): Result {
        val hash = sha256(bytes)
        if (hash == PATCHED_SHA256) return Result.ALREADY_APPLIED
        if (hash != ORIGINAL_SHA256) return Result.UNSUPPORTED

        check(bytes.take(6) == listOf(0x7f, 0x45, 0x4c, 0x46, 2, 1).map { it.toByte() }
            && bytes[18] == 0xb7.toByte() && bytes[19] == 0.toByte()) {
            "Galaxy XR EGL patch: expected little-endian ARM64 ELF"
        }
        check(bytes.copyOfRange(VALUE_OFFSET - 4, VALUE_OFFSET + 4).contentEquals(expected)) {
            "Galaxy XR EGL patch: context attribute instructions do not match"
        }
        val output = bytes.copyOf()
        // mov w2, #1 -> mov w2, #0 for EGL_CONTEXT_OPENGL_NO_ERROR_KHR (0x31b3).
        // Ordinary error-checking contexts can share textures with the Android XR compositor.
        output[VALUE_OFFSET] = 0x02
        check(sha256(output) == PATCHED_SHA256) { "Galaxy XR EGL patch: unexpected output digest" }
        output.copyInto(bytes)
        return Result.APPLIED
    }

    private fun sha256(bytes: ByteArray): String =
        MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") { "%02x".format(it) }
}
