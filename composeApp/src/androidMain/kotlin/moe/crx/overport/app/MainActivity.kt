package moe.crx.overport.app

import android.annotation.SuppressLint
import android.graphics.Color
import android.os.Bundle
import android.provider.OpenableColumns
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.launch
import moe.crx.overport.app.composables.AppContent
import moe.crx.overport.app.model.MainViewModel
import java.io.InputStream
import java.io.OutputStream
import java.io.File

class MainActivity : ComponentActivity() {
    companion object {
        const val APK_MIMETYPE = "application/vnd.android.package-archive"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setTheme(R.style.Theme_Overport)
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.dark(Color.TRANSPARENT),
        )
        setContent {
            ActivityContent(this)
        }
    }

    @SuppressLint("Recycle")
    @Composable
    private fun ActivityContent(activity: MainActivity) {
        // Signing identity must survive Android cache cleanup.
        val viewModel: MainViewModel = viewModel { MainViewModel(File(activity.filesDir, "workspace").apply { mkdirs() }, BitmapIconResizer) }
        val openFlow = remember { MutableSharedFlow<Pair<String, InputStream>?>() }
        val saveFlow = remember { MutableSharedFlow<OutputStream?>() }
        val scope = rememberCoroutineScope()

        val openApkFile = rememberLauncherForActivityResult(
            ActivityResultContracts.OpenDocument()
        ) { uri ->
            if (uri == null) {
                scope.launch {
                    openFlow.emit(null)
                }
                return@rememberLauncherForActivityResult
            }

            val stream = activity.contentResolver.openInputStream(uri) ?: return@rememberLauncherForActivityResult
            val fileName = activity.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use {
                if (it.moveToFirst()) it.getString(0) else null
            }?.substringAfterLast('/') ?: "application.apk"

            scope.launch {
                openFlow.emit(fileName to stream)
            }
        }

        val saveApkFile = rememberLauncherForActivityResult(
            ActivityResultContracts.CreateDocument(APK_MIMETYPE)
        ) { uri ->
            if (uri == null) {
                scope.launch {
                    saveFlow.emit(null)
                }
                return@rememberLauncherForActivityResult
            }

            val stream = activity.contentResolver.openOutputStream(uri) ?: return@rememberLauncherForActivityResult

            scope.launch {
                saveFlow.emit(stream)
            }
        }

        AppContent(
            viewModel = viewModel,
            openFile = { openApkFile.launch(arrayOf(APK_MIMETYPE)) },
            saveFile = { name -> saveApkFile.launch(name) },
            openFlow = openFlow,
            saveFlow = saveFlow,
        )
    }
}
