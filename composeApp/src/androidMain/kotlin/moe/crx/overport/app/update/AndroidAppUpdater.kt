package moe.crx.overport.app.update

import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.provider.Settings
import androidx.core.content.FileProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

class AndroidAppUpdater(private val activity: Activity) : AppUpdater {
    override val platform = "android.apk"
    override val downloadDirectory = File(activity.cacheDir, "updates")
    override val installLabel = "Download and install"
    override val installDescription = "Android will ask you to confirm the update. Your settings and signing key are kept."

    override suspend fun install(file: File, update: AppUpdate): String {
        withContext(Dispatchers.IO) {
            val pm = activity.packageManager
            val candidate = pm.getPackageArchiveInfo(file.absolutePath, PackageManager.GET_SIGNING_CERTIFICATES)
                ?: error("The downloaded APK cannot be read.")
            val installed = pm.getPackageInfo(activity.packageName, PackageManager.GET_SIGNING_CERTIFICATES)
            require(candidate.packageName == activity.packageName && candidate.versionName == update.version &&
                candidate.longVersionCode > installed.longVersionCode) { "The APK is not a newer Overport Galaxy XR release." }
            val signatures = candidate.signingInfo?.apkContentsSigners
            val current = installed.signingInfo?.apkContentsSigners
            require(!signatures.isNullOrEmpty() && !current.isNullOrEmpty() && signatures.toSet() == current.toSet()) {
                "This APK uses a different signing key. Your installed app was kept."
            }
        }
        if (!activity.packageManager.canRequestPackageInstalls()) {
            activity.startActivity(Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:${activity.packageName}")))
            return "Allow updates from Overport, return here, then tap Download and install again. The verified APK is cached."
        }
        val uri = FileProvider.getUriForFile(activity, "${activity.packageName}.updates", file)
        activity.startActivity(Intent(Intent.ACTION_INSTALL_PACKAGE).apply {
            data = uri
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        })
        return "Confirm the update in the Android installer. If you cancel, you can try again here."
    }
}
