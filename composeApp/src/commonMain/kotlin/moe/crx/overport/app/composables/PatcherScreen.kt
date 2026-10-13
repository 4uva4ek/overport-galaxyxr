package moe.crx.overport.app.composables

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.ProgressIndicatorDefaults
import androidx.compose.material3.SnackbarHostState
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.launch
import moe.crx.overport.app.model.MainViewModel
import moe.crx.overport.galaxyxr.GalaxyXrPatches
import moe.crx.overport.utils.CantCheckoutException
import org.jetbrains.compose.resources.getString
import overportapp.composeapp.generated.resources.*
import java.io.InputStream
import java.io.OutputStream

@Composable
fun PatcherScreen(
    viewModel: MainViewModel,
    snackbarHostState: SnackbarHostState,
    openFile: () -> Unit,
    saveFile: (String) -> Unit,
    openFlow: MutableSharedFlow<Pair<String, InputStream>?>,
    saveFlow: MutableSharedFlow<OutputStream?>,
) {
    val scope = rememberCoroutineScope()
    var errorMessage by rememberSaveable { mutableStateOf<Pair<String, String>?>(null) }
    var patches by remember { mutableStateOf<Map<String, List<String>>>(mapOf()) }

    val inProgress = viewModel.working || viewModel.isApkLoaded()
    val isPatcherVisible = !viewModel.working && viewModel.isApkLoaded()

    LaunchedEffect(openFlow) {
        openFlow.collect { selected ->
            val (name, stream) = selected ?: return@collect
            if (viewModel.working || viewModel.isApkLoaded()) {
                stream.close()
                return@collect
            }
            try {
                viewModel.checkout(GalaxyXrPatches.DEFAULT_RUNTIME, name)
                stream.use { viewModel.prepare(it) }
            } catch (_: CantCheckoutException) {
                viewModel.cancel()
                errorMessage = getString(Res.string.cant_checkout_title) to getString(Res.string.cant_checkout_message)
            } catch (ex: Exception) {
                viewModel.cancel()
                errorMessage = getString(Res.string.unknown_error) to ex.stackTraceToString()
            } finally {
                stream.close()
            }
        }
    }

    LaunchedEffect(saveFlow) {
        saveFlow.collect { stream ->
            val outputStream = stream ?: return@collect
            try {
                outputStream.use {
                    viewModel.process(patches)
                    viewModel.export(it)
                }
                scope.launch {
                    snackbarHostState.showSnackbar(getString(Res.string.apk_file_exported))
                }
            } catch (ex: Throwable) {
                ex.printStackTrace()
                errorMessage = getString(Res.string.unknown_error) to ex.stackTraceToString()
            } finally {
                viewModel.cancel()
            }
        }
    }

    FadeVisibility(!isPatcherVisible) {
        StartingCard(inProgress, viewModel.currentProgress) {
            openFile()
        }
    }

    FadeVisibility(isPatcherVisible) {
        ApplicationInfoContent(
            viewModel.currentAppName() ?: "",
            viewModel.currentAppPackage() ?: "",
            viewModel.currentAppVersion() ?: "",
            viewModel.currentAppIcon(),
            onCancel = {
                scope.launch {
                    viewModel.cancel()
                }
            },
            onConfirm = {
                patches = it
                saveFile(viewModel.patchedName())
            }
        )
    }

    errorMessage?.let {
        ErrorDialog(it.first, it.second) {
            errorMessage = null
        }
    }

    FadeVisibility(viewModel.currentProgressFloat != null && !isPatcherVisible) {
        val progressAnimation by animateFloatAsState(
            targetValue = viewModel.currentProgressFloat ?: 0f,
            animationSpec = ProgressIndicatorDefaults.ProgressAnimationSpec,
        )
        LinearProgressIndicator(
            { progressAnimation },
            modifier = Modifier.fillMaxWidth(),
            drawStopIndicator = {}
        )
    }
}
