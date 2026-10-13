package moe.crx.overport.app

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.draganddrop.dragAndDropTarget
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.*
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.draganddrop.DragAndDropEvent
import androidx.compose.ui.draganddrop.DragAndDropTarget
import androidx.compose.ui.draganddrop.awtTransferable
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.lifecycle.viewmodel.compose.viewModel
import com.formdev.flatlaf.util.SystemFileChooser
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.launch
import moe.crx.overport.app.composables.AppContent
import moe.crx.overport.app.model.MainViewModel
import moe.crx.overport.app.update.WindowsAppUpdater
import moe.crx.overport.utils.DesktopUtil.defaultWorkspace
import moe.crx.overport.utils.ImageIOIconResizer
import org.jetbrains.compose.resources.painterResource
import org.jetbrains.compose.resources.stringResource
import overportapp.composeapp.generated.resources.Res
import overportapp.composeapp.generated.resources.select_a_file
import overportapp.composeapp.generated.resources.window_icon
import java.awt.Frame
import java.awt.datatransfer.DataFlavor
import java.io.*

@OptIn(ExperimentalFoundationApi::class, ExperimentalComposeUiApi::class)
fun main(args: Array<String>) = application {
    Window(
        onCloseRequest = ::exitApplication,
        title = "Overport Galaxy XR",
        icon = painterResource(Res.drawable.window_icon),
    ) {
        val dataDir = defaultWorkspace()
        dataDir.mkdirs()

        val selectFileString = stringResource(Res.string.select_a_file)
        val viewModel: MainViewModel = viewModel { MainViewModel(dataDir, ImageIOIconResizer) }
        val openFlow = remember { MutableSharedFlow<Pair<String, InputStream>?>() }
        val saveFlow = remember { MutableSharedFlow<OutputStream?>() }
        var lastOpenedDirectory by remember { mutableStateOf("") }
        val scope = rememberCoroutineScope()

        fun openFile(file: File?) {
            if (file == null || !file.isFile) {
                scope.launch {
                    openFlow.emit(null)
                }
                return
            }

            scope.launch {
                lastOpenedDirectory = file.parentFile.absolutePath
                openFlow.emit(file.name to FileInputStream(file))
            }
        }

        val dragAndDropTarget = remember {
            object : DragAndDropTarget {
                override fun onDrop(event: DragAndDropEvent): Boolean {
                    val value: List<File>? = event.awtTransferable
                        .takeIf { it.isDataFlavorSupported(DataFlavor.javaFileListFlavor) }
                        ?.getTransferData(DataFlavor.javaFileListFlavor)
                        ?.let { it as? List<*> }
                        ?.filterIsInstance<File>()

                    if (value?.size == 1) {
                        scope.launch {
                            openFile(value.first())
                        }

                        return true
                    }

                    return false
                }
            }
        }

        LaunchedEffect(args) {
            if (args.isNotEmpty()) {
                openFile(File(args.first()))
            }
        }

        Box(
            modifier = Modifier.fillMaxSize().dragAndDropTarget(
                shouldStartDragAndDrop = { !viewModel.working && !viewModel.isApkLoaded() },
                target = dragAndDropTarget
            )
        ) {
            AppContent(
                appUpdater = remember { WindowsAppUpdater { exitApplication() } },
                viewModel = viewModel,
                openFile = {
                    val fileDialog = SystemFileChooser(selectFileString).apply {
                        currentDirectory = File(lastOpenedDirectory)
                        selectedFile = null
                    }

                    val file = if (fileDialog.showOpenDialog(Frame()) != SystemFileChooser.APPROVE_OPTION) null else {
                        fileDialog.selectedFile
                    }

                    openFile(file)
                },
                saveFile = { name ->
                    val fileDialog = SystemFileChooser(selectFileString).apply {
                        currentDirectory = File(lastOpenedDirectory)
                        selectedFile = File(lastOpenedDirectory, name)
                    }

                    val file = if (fileDialog.showSaveDialog(Frame()) != SystemFileChooser.APPROVE_OPTION) null else {
                        fileDialog.selectedFile
                    }

                    scope.launch {
                        if (file == null) {
                            saveFlow.emit(null)
                        } else {
                            saveFlow.emit(FileOutputStream(file))
                        }
                    }
                },
                openFlow = openFlow,
                saveFlow = saveFlow,
            )
        }
    }
}
