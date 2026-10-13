package moe.crx.overport.app.update

import androidx.compose.runtime.*
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import moe.crx.overport.versions.VersionManager

class UpdateController(val installer: AppUpdater) {
    var available by mutableStateOf<AppUpdate?>(null); private set
    var busy by mutableStateOf(false); private set
    var checking by mutableStateOf(false); private set
    var status by mutableStateOf("Updates are checked automatically."); private set
    var progress by mutableStateOf<Float?>(null); private set

    suspend fun check() {
        if (checking || busy) return
        checking = true
        try {
            available = withContext(Dispatchers.IO) { UpdateClient.latest(VersionManager.DISTRIBUTION_VERSION, installer.platform) }
            status = available?.let { "Version ${it.version} is available." } ?: "You have the latest version."
        } catch (e: CancellationException) { throw e }
        catch (e: Exception) { status = e.message ?: "Could not check for updates." }
        finally { checking = false }
    }

    suspend fun install() {
        val update = available ?: return
        if (busy || checking) return
        busy = true; progress = 0f; status = "Downloading ${update.version}…"
        try {
            val file = withContext(Dispatchers.IO) {
                UpdateClient.download(update, installer.downloadDirectory) { progress = it }
            }
            progress = null; status = "Preparing installation…"
            status = installer.install(file, update)
        } catch (e: CancellationException) { throw e }
        catch (e: Exception) { status = e.message ?: "Update failed. Your current installation is unchanged." }
        finally { progress = null; busy = false }
    }
}
