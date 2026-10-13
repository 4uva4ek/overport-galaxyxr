package moe.crx.overport.app.update

import moe.crx.overport.app.model.GithubRelease
import moe.crx.overport.app.model.ReleaseAsset
import kotlin.test.*
import java.io.File
import java.nio.file.Files
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class UpdateTest {
    private fun release(platform: String, version: String = "0.1.1"): GithubRelease {
        val name = "overport-galaxyxr-$version-$platform"
        return GithubRelease("galaxyxr-v$version", "", "", "", assets = listOf(
            ReleaseAsset(name, "${UpdateClient.REPOSITORY}/releases/download/galaxyxr-v$version/$name", 3, "sha256:" + "0".repeat(64))
        ))
    }

    @Test fun choosesOnlyNewStablePlatformRelease() {
        for (platform in listOf("android.apk", "windows-x64.zip")) {
            assertEquals("0.1.1", UpdateClient.select(release(platform), "0.1.0", platform)?.version)
            assertNull(UpdateClient.select(release(platform), "0.1.1", platform))
            assertNull(UpdateClient.select(release(platform), "0.2.0", platform))
            assertNull(UpdateClient.select(release(platform).copy(prerelease = true), "0.1.0", platform))
            assertNull(UpdateClient.select(release(platform).copy(draft = true), "0.1.0", platform))
            assertFails { UpdateClient.select(release(platform).copy(assets = emptyList()), "0.1.0", platform) }
            val wrong = release(platform).assets.single().copy(url = "https://example.com/app")
            assertFails { UpdateClient.select(release(platform).copy(assets = listOf(wrong)), "0.1.0", platform) }
        }
        assertTrue(UpdateClient.newer("0.10.0", "0.9.9"))
        assertFalse(UpdateClient.newer("0.1.20", "0.2.0"))
    }

    @Test fun rejectsCorruptTruncatedAndOversizeDownloads() {
        val file = File.createTempFile("overport-test", ".bin")
        try {
            val hash = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
            UpdateClient.copyVerified("abc".byteInputStream(), file, 3, hash) {}
            assertEquals("abc", file.readText())
            assertFails { UpdateClient.copyVerified("abd".byteInputStream(), file, 3, hash) {} }
            assertFails { UpdateClient.copyVerified("ab".byteInputStream(), file, 3, hash) {} }
            assertFails { UpdateClient.copyVerified("abcd".byteInputStream(), file, 3, hash) {} }
        } finally { file.delete() }
    }

    @Test fun extractsOnlyCompleteConfinedWindowsPackages() {
        val root = Files.createTempDirectory("overport-archive-test").toFile()
        fun archive(names: List<String>): File {
            val zip = File(root, "update.zip")
            ZipOutputStream(zip.outputStream()).use { out -> names.forEach { name ->
                out.putNextEntry(ZipEntry(name)); out.write(byteArrayOf(1)); out.closeEntry()
            } }
            return zip
        }
        try {
            val files = listOf("OverportGalaxyXR/OverportGalaxyXR.exe", "OverportGalaxyXR/runtime/bin/server/jvm.dll", "OverportGalaxyXR/app/OverportGalaxyXR.cfg")
            WindowsAppUpdater.extract(archive(files), File(root, "ok").apply { mkdir() })
            for ((index, name) in listOf("OverportGalaxyXR/../outside", "OverportGalaxyXR/C:/outside", "elsewhere/file").withIndex()) {
                assertFails { WindowsAppUpdater.extract(archive(listOf(name)), File(root, "bad$index").apply { mkdir() }) }
            }
            assertFails { WindowsAppUpdater.extract(archive(listOf(files.first())), File(root, "incomplete").apply { mkdir() }) }
        } finally { root.deleteRecursively() }
    }
}
