package moe.crx.overport.galaxyxr

/**
 * App-local EGL workaround for Unity's GLES context creation on Android XR.
 *
 * When the driver advertises GL_KHR_no_error, Unity appends
 * EGL_CONTEXT_OPENGL_NO_ERROR_KHR (0x31b3) = 1 to its context attributes.
 * The Android XR runtime creates a shared context for the compositor without that
 * attribute, so eglCreateContext fails with EGL_BAD_MATCH and xrCreateSession fails.
 * Changing the attribute value to 0 keeps an ordinary error-checking context.
 *
 * The site is located by its ARM64 instruction signature rather than a whole-file
 * digest, so every Unity ARM64 build containing exactly one such sequence is handled:
 *
 *     mov w1, #0x31b3      ; EGL_CONTEXT_OPENGL_NO_ERROR_KHR
 *     mov w2, #1           ; attribute value, changed to #0
 *     bl  <append attribute>
 *
 * Verified on Unity 2022.3.33f1 (Beat Saber 1.40.8) and 2022.3.67f1 (Job Simulator 1.4.0.6515).
 * Binaries with no match or with several matches are left untouched and reported.
 */
object UnityGlesCompatibility {
    private const val MOV_W1_NO_ERROR = 0x52863661 // mov w1, #0x31b3
    private const val MOV_W2_ONE = 0x52800022      // mov w2, #1
    private const val MOV_W2_ZERO = 0x52800002     // mov w2, #0
    private const val BL_MASK = 0xFC000000.toInt()
    private const val BL_OPCODE = 0x94000000.toInt()

    enum class Result { APPLIED, ALREADY_APPLIED, UNSUPPORTED }

    /** File offset of the `mov w2` instruction and whether it already carries #0. */
    data class Site(val valueOffset: Int, val patched: Boolean)

    /** Unknown or ambiguous binaries remain untouched. */
    fun patch(bytes: ByteArray): Result {
        val site = locate(bytes).singleOrNull() ?: return Result.UNSUPPORTED
        if (site.patched) return Result.ALREADY_APPLIED
        // mov w2, #1 -> mov w2, #0 (one byte: 0x22 -> 0x02).
        bytes[site.valueOffset] = 0x02
        return Result.APPLIED
    }

    /** Signature matches inside executable PT_LOAD segments of a little-endian ARM64 ELF. */
    fun locate(bytes: ByteArray): List<Site> {
        if (!isArm64Elf(bytes)) return emptyList()
        val sites = mutableListOf<Site>()
        for ((offset, size) in executableSegments(bytes)) {
            var at = offset
            val end = offset + size - 12
            while (at <= end) {
                if (readInt(bytes, at) == MOV_W1_NO_ERROR) {
                    val value = readInt(bytes, at + 4)
                    val call = readInt(bytes, at + 8)
                    if ((value == MOV_W2_ONE || value == MOV_W2_ZERO) && (call and BL_MASK) == BL_OPCODE) {
                        sites += Site(at + 4, value == MOV_W2_ZERO)
                    }
                }
                at += 4
            }
        }
        return sites
    }

    private fun isArm64Elf(bytes: ByteArray): Boolean =
        bytes.size >= 0x40 && bytes[0] == 0x7f.toByte() && bytes[1] == 'E'.code.toByte() &&
            bytes[2] == 'L'.code.toByte() && bytes[3] == 'F'.code.toByte() &&
            bytes[4] == 2.toByte() && bytes[5] == 1.toByte() && // ELFCLASS64, little-endian
            bytes[18] == 0xb7.toByte() && bytes[19] == 0.toByte() // EM_AARCH64

    /** (file offset, file size) of every PT_LOAD segment with PF_X, clamped to the file. */
    private fun executableSegments(bytes: ByteArray): List<Pair<Int, Int>> {
        val phoff = readLong(bytes, 0x20)
        val phentsize = readShort(bytes, 0x36)
        val phnum = readShort(bytes, 0x38)
        if (phentsize < 56 || phoff < 0 || phoff > bytes.size - phnum.toLong() * phentsize) return emptyList()
        val segments = mutableListOf<Pair<Int, Int>>()
        for (index in 0 until phnum) {
            val header = (phoff + index.toLong() * phentsize).toInt()
            val type = readInt(bytes, header)
            val flags = readInt(bytes, header + 4)
            if (type != 1 || flags and 1 == 0) continue // PT_LOAD with PF_X
            val fileOffset = readLong(bytes, header + 8)
            val fileSize = readLong(bytes, header + 32)
            if (fileOffset < 0 || fileOffset >= bytes.size || fileSize <= 0) continue
            val clamped = minOf(fileSize, bytes.size - fileOffset)
            segments += Pair(fileOffset.toInt(), clamped.toInt())
        }
        return segments
    }

    private fun readShort(bytes: ByteArray, at: Int): Int =
        (bytes[at].toInt() and 0xff) or ((bytes[at + 1].toInt() and 0xff) shl 8)

    private fun readInt(bytes: ByteArray, at: Int): Int =
        (bytes[at].toInt() and 0xff) or ((bytes[at + 1].toInt() and 0xff) shl 8) or
            ((bytes[at + 2].toInt() and 0xff) shl 16) or ((bytes[at + 3].toInt() and 0xff) shl 24)

    private fun readLong(bytes: ByteArray, at: Int): Long =
        (readInt(bytes, at).toLong() and 0xffffffffL) or ((readInt(bytes, at + 4).toLong() and 0xffffffffL) shl 32)
}
