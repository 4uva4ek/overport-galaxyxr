package moe.crx.overport.galaxyxr

import java.security.MessageDigest

/** Remove the inspected loader's arbitrary 4096px abort; the actual runtime validates limits. */
object SwapchainCompatibility {
    private const val ORIGINAL = "21821370b886ac25b048836573a68438dc45c0cf9f3e7e6c9f415e28e7578b05"
    private val replacements = mapOf(0x18b8c to 0x54000ae8, 0x18b94 to 0x54000aa8)
    private const val NOP = 0xd503201f.toInt()

    fun patch(bytes: ByteArray): ByteArray {
        val restored = bytes.copyOf()
        for ((offset, original) in replacements) {
            require(bytes.size >= offset + 4) { "Unsupported Overport loader: truncated input" }
            val instruction = (0..3).fold(0) { value, i -> value or ((bytes[offset + i].toInt() and 255) shl (8 * i)) }
            require(instruction == original || instruction == NOP) { "Unsupported Overport loader instruction; use runtime 3.4.3-aa54c3f (ARM64)" }
            for (i in 0..3) restored[offset + i] = (original ushr (8 * i)).toByte()
        }
        val hash = MessageDigest.getInstance("SHA-256").digest(restored).joinToString("") { "%02x".format(it) }
        require(hash == ORIGINAL) { "Unsupported Overport loader digest; use runtime 3.4.3-aa54c3f (ARM64)" }
        return bytes.copyOf().also { output ->
            for (offset in replacements.keys) for (i in 0..3) output[offset + i] = (NOP ushr (8 * i)).toByte()
        }
    }
}
