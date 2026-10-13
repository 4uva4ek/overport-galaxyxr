package moe.crx.overport.app.update

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.io.File
import java.nio.file.Files
import java.util.Base64
import java.util.zip.ZipInputStream

class WindowsAppUpdater(private val exit: () -> Unit) : AppUpdater {
    override val platform = "windows-x64.zip"
    override val downloadDirectory = File(System.getenv("LOCALAPPDATA") ?: System.getProperty("user.home"), "OverportGalaxyXR/updates")
    override val installLabel = "Update and restart"
    override val installDescription = "Overport will close and restart. A backup of the previous version is kept beside the app. Your workspace and signing key are kept."

    override suspend fun install(file: File, update: AppUpdate): String {
        withContext(Dispatchers.IO) {
            require(System.getProperty("os.name").startsWith("Windows")) { "Automatic installation is available on Windows only." }
            val launcher = System.getProperty("jpackage.app-path")?.let(::File)?.canonicalFile
                ?: error("Run the packaged OverportGalaxyXR.exe to install updates.")
            val target = launcher.parentFile
            require(launcher.name == "OverportGalaxyXR.exe" && launcher.isFile && File(target, "app").isDirectory &&
                File(target, "runtime/bin/server/jvm.dll").isFile) { "Cannot identify the portable installation." }
            val stage = Files.createTempDirectory(target.parentFile.toPath(), ".OverportGalaxyXR-update-").toFile()
            extract(file, stage)
            val script = File(stage, "update.ps1")
            script.writeBytes(checkNotNull(javaClass.getResourceAsStream("/update-windows.ps1")).use { it.readBytes() })
            val ready = File(stage, "ready")
            val log = File(downloadDirectory, "last-install.txt")
            fun quote(value: String) = "'" + value.replace("'", "''") + "'"
            val command = "& ${quote(script.path)} -Target ${quote(target.path)} -Stage ${quote(stage.path)} " +
                "-ParentProcessId ${ProcessHandle.current().pid()} -Log ${quote(log.path)}"
            val encoded = Base64.getEncoder().encodeToString(command.toByteArray(Charsets.UTF_16LE))
            val powershell = File(System.getenv("SystemRoot"), "System32/WindowsPowerShell/v1.0/powershell.exe")
            val process = ProcessBuilder(powershell.path, "-NoProfile", "-NonInteractive", "-WindowStyle", "Hidden",
                "-ExecutionPolicy", "Bypass", "-EncodedCommand", encoded)
                .redirectErrorStream(true).redirectOutput(File(stage, "helper.log")).start()
            var attempts = 0
            while (!ready.isFile && process.isAlive && attempts++ < 100) delay(100)
            check(ready.isFile && process.isAlive) { "Could not start the update helper. Current app is unchanged. See ${stage.path}." }
        }
        exit()
        return "Restarting…"
    }

    companion object {
        internal fun extract(zip: File, directory: File) {
            val root = directory.canonicalFile.toPath()
            var total = 0L
            var count = 0
            ZipInputStream(zip.inputStream().buffered()).use { input ->
                while (true) {
                    val entry = input.nextEntry ?: break
                    require(++count <= 30000) { "Too many update files." }
                    val name = entry.name.replace('\\', '/')
                    require(name.startsWith("OverportGalaxyXR/") && !name.contains(':') &&
                        name.split('/').none { it == ".." || it.endsWith('.') || it.endsWith(' ') }) { "Invalid update archive path." }
                    val out = File(directory, name).canonicalFile
                    require(out.toPath().startsWith(root) && out.toPath() != root) { "Update archive escapes its directory." }
                    if (entry.isDirectory) check(out.mkdirs() || out.isDirectory)
                    else {
                        check(out.parentFile.mkdirs() || out.parentFile.isDirectory)
                        check(out.createNewFile()) { "Duplicate file in update archive." }
                        out.outputStream().use { output ->
                            val buffer = ByteArray(65536)
                            while (true) {
                                val read = input.read(buffer)
                                if (read < 0) break
                                total += read
                                require(total <= 2L * 1024 * 1024 * 1024) { "Update archive is too large." }
                                output.write(buffer, 0, read)
                            }
                        }
                    }
                }
            }
            val app = File(directory, "OverportGalaxyXR")
            require(File(app, "OverportGalaxyXR.exe").isFile && File(app, "runtime/bin/server/jvm.dll").isFile &&
                File(app, "app/OverportGalaxyXR.cfg").isFile) { "Incomplete Windows update." }
        }
    }
}
