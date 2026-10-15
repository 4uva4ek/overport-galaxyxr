package moe.crx.overport.app.update

import kotlinx.serialization.json.Json
import moe.crx.overport.app.model.GithubRelease
import moe.crx.overport.app.model.ReleaseAsset
import java.io.File
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URI
import java.security.MessageDigest

data class AppUpdate(val version: String, val asset: ReleaseAsset, val sha256: String)

object UpdateClient {
    const val REPOSITORY = "https://github.com/4uva4ek/overport-galaxyxr"
    private const val API = "https://api.github.com/repos/4uva4ek/overport-galaxyxr/releases/latest"
    private val json = Json { ignoreUnknownKeys = true }
    private val versionPattern = Regex("(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)")

    fun newer(candidate: String, installed: String): Boolean {
        fun parts(s: String): List<Int> = versionPattern.matchEntire(s)?.groupValues?.drop(1)
            ?.map { it.toInt() } ?: error("Invalid release version: $s")
        val left = parts(candidate); val right = parts(installed)
        return left.zip(right).firstOrNull { it.first != it.second }?.let { it.first > it.second } ?: false
    }

    fun select(release: GithubRelease, installed: String, platform: String): AppUpdate? {
        if (release.draft || release.prerelease || !release.tagName.startsWith("galaxyxr-v")) return null
        val version = release.tagName.removePrefix("galaxyxr-v")
        if (!newer(version, installed)) return null
        require(platform in listOf("android.apk", "windows-x64.zip", "linux-x64.tar.gz", "macos-arm64.dmg", "macos-x64.dmg"))
        val name = "overport-galaxyxr-$version-$platform"
        val asset = release.assets.singleOrNull { it.name == name } ?: error("This release has no unique $platform package yet.")
        require(asset.url == "$REPOSITORY/releases/download/${release.tagName}/$name") { "Unexpected update download URL." }
        require(asset.size in 1..536870912L) { "Invalid update size." }
        val digest = asset.digest?.removePrefix("sha256:") ?: error("The release has no SHA-256 digest yet.")
        require(asset.digest.startsWith("sha256:") && Regex("[a-fA-F0-9]{64}").matches(digest)) { "Invalid release digest." }
        return AppUpdate(version, asset, digest.lowercase())
    }

    fun latest(installed: String, platform: String): AppUpdate? {
        val connection = connect(API)
        try {
            if (connection.responseCode == 404) return null
            require(connection.responseCode == 200) { "Update check failed (HTTP ${connection.responseCode}). Try again later." }
            val bytes = connection.inputStream.use { it.readBytesBounded(2 * 1024 * 1024) }
            return select(json.decodeFromString<GithubRelease>(bytes.decodeToString()), installed, platform)
        } finally { connection.disconnect() }
    }

    fun download(update: AppUpdate, directory: File, progress: (Float) -> Unit): File {
        directory.mkdirs()
        val destination = File(directory, update.asset.name)
        if (destination.isFile && destination.length() == update.asset.size && sha256(destination) == update.sha256) return destination
        val temporary = File.createTempFile("download-", ".part", directory)
        val connection = connect(update.asset.url)
        try {
            require(connection.responseCode == 200) { "Download failed (HTTP ${connection.responseCode})." }
            connection.inputStream.use { input -> copyVerified(input, temporary, update.asset.size, update.sha256, progress) }
            // Only this updater's cached artifact is replaced, after its replacement is verified.
            if (destination.exists()) check(destination.delete()) { "Cannot replace cached download." }
            check(temporary.renameTo(destination)) { "Cannot save verified update." }
            return destination
        } finally { connection.disconnect(); temporary.delete() }
    }

    internal fun copyVerified(input: InputStream, file: File, size: Long, expected: String, progress: (Float) -> Unit) {
        val digest = MessageDigest.getInstance("SHA-256")
        var count = 0L
        file.outputStream().use { output ->
            val buffer = ByteArray(65536)
            while (true) {
                val read = input.read(buffer)
                if (read < 0) break
                count += read
                require(count <= size) { "Download exceeds the advertised size." }
                digest.update(buffer, 0, read); output.write(buffer, 0, read)
                progress(count.toFloat() / size)
            }
        }
        require(count == size && hex(digest.digest()) == expected) { "Update integrity check failed. Download it again." }
    }

    private fun InputStream.readBytesBounded(limit: Int): ByteArray {
        val output = java.io.ByteArrayOutputStream()
        val buffer = ByteArray(8192)
        while (true) {
            val read = read(buffer); if (read < 0) break
            require(output.size() + read <= limit) { "Update response is too large." }
            output.write(buffer, 0, read)
        }
        return output.toByteArray()
    }

    private fun connect(address: String): HttpURLConnection {
        var next = address
        repeat(6) {
            val url = URI(next)
            require(url.scheme == "https" && url.userInfo == null && url.port in listOf(-1, 443) &&
                url.host in listOf("github.com", "api.github.com", "release-assets.githubusercontent.com", "objects.githubusercontent.com")) {
                "Untrusted update download redirect."
            }
            val connection = url.toURL().openConnection() as HttpURLConnection
            connection.connectTimeout = 15000; connection.readTimeout = 30000
            connection.instanceFollowRedirects = false
            connection.setRequestProperty("User-Agent", "Overport-Galaxy-XR-Updater")
            if (connection.responseCode !in listOf(301, 302, 303, 307, 308)) return connection
            val location = connection.getHeaderField("Location")
            connection.disconnect()
            require(!location.isNullOrBlank()) { "Missing update redirect." }
            next = url.resolve(location).toString()
        }
        error("Too many update redirects.")
    }

    fun sha256(file: File): String = file.inputStream().use { input ->
        val digest = MessageDigest.getInstance("SHA-256"); val buffer = ByteArray(65536)
        while (true) { val read = input.read(buffer); if (read < 0) break; digest.update(buffer, 0, read) }
        hex(digest.digest())
    }
    private fun hex(bytes: ByteArray) = bytes.joinToString("") { "%02x".format(it) }
}
