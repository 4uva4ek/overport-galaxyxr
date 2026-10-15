package moe.crx.overport.app.update

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.awt.Desktop
import java.io.File

internal fun desktopUpdatePlatform(os: String, arch: String): String {
    val x64 = arch.lowercase() in listOf("amd64", "x86_64")
    return when {
        os.startsWith("Windows") && x64 -> "windows-x64.zip"
        os == "Linux" && x64 -> "linux-x64.tar.gz"
        os == "Mac OS X" && x64 -> "macos-x64.dmg"
        os == "Mac OS X" && arch.lowercase() in listOf("aarch64", "arm64") -> "macos-arm64.dmg"
        else -> error("Unsupported desktop platform: $os ($arch)")
    }
}

fun createDesktopAppUpdater(exit: () -> Unit): AppUpdater {
    val platform = desktopUpdatePlatform(System.getProperty("os.name"), System.getProperty("os.arch"))
    return if (platform == "windows-x64.zip") WindowsAppUpdater(exit) else ManualDesktopAppUpdater(platform)
}

private class ManualDesktopAppUpdater(override val platform: String) : AppUpdater {
    override val downloadDirectory = File(System.getProperty("user.home"), "overport/updates")
    override val installLabel = "Download update"
    override val installDescription = if (platform.startsWith("macos-"))
        "Download the disk image, then quit Overport and replace the app in Applications. Your workspace and signing key are kept."
    else
        "Download the archive, then quit Overport and extract it into a new folder. Your workspace and signing key are kept."

    override suspend fun install(file: File, update: AppUpdate): String = withContext(Dispatchers.IO) {
        require(file.name == update.asset.name && file.isFile && UpdateClient.sha256(file) == update.sha256) {
            "Update integrity check failed. Download it again."
        }
        // Opening the folder is optional: minimal Linux desktops may have no file manager.
        runCatching {
            if (Desktop.isDesktopSupported() && Desktop.getDesktop().isSupported(Desktop.Action.OPEN)) {
                Desktop.getDesktop().open(file.parentFile)
            }
        }
        "Downloaded to ${file.absolutePath}. Quit Overport and install the new version manually."
    }
}
