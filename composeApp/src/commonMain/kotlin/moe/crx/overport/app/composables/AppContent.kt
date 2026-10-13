package moe.crx.overport.app.composables

import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Build
import androidx.compose.material.icons.filled.CloudDownload
import androidx.compose.material.icons.outlined.Build
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.flow.MutableSharedFlow
import moe.crx.overport.app.model.MainViewModel
import moe.crx.overport.app.theme.OverportTheme
import moe.crx.overport.app.update.AppUpdater
import moe.crx.overport.app.update.UpdateController
import org.jetbrains.compose.resources.painterResource
import org.jetbrains.compose.resources.stringResource
import overportapp.composeapp.generated.resources.Res
import overportapp.composeapp.generated.resources.overport
import overportapp.composeapp.generated.resources.patcher_nav_item
import java.io.InputStream
import java.io.OutputStream

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AppContent(
    appUpdater: AppUpdater,
    viewModel: MainViewModel,
    openFile: () -> Unit,
    saveFile: (String) -> Unit,
    openFlow: MutableSharedFlow<Pair<String, InputStream>?>,
    saveFlow: MutableSharedFlow<OutputStream?>,
) {
    OverportTheme(darkTheme = true) {
        val snackbarHostState = remember { SnackbarHostState() }

        val patcherNavItem =
            BottomNavItem(stringResource(Res.string.patcher_nav_item), Icons.Filled.Build, Icons.Outlined.Build) {
                PatcherScreen(viewModel, snackbarHostState, openFile, saveFile, openFlow, saveFlow)
            }

        val updater = remember(appUpdater) { UpdateController(appUpdater) }
        var showUpdates by remember { mutableStateOf(false) }
        LaunchedEffect(updater) { updater.check() }
        if (showUpdates) UpdateDialog(updater) { showUpdates = false }

        DynamicScaffold(
            snackbarHost = { SnackbarHost(snackbarHostState) },
            topBar = {
                TopAppBar(
                    title = {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Icon(
                                painterResource(Res.drawable.overport),
                                contentDescription = null,
                                modifier = Modifier.size(112.dp, 24.dp)
                            )
                            Spacer(Modifier.width(12.dp))
                            Text("Galaxy XR", style = MaterialTheme.typography.titleMedium)
                        }
                    },
                    actions = {
                        FilledTonalButton(
                            modifier = Modifier.padding(8.dp, 0.dp),
                            enabled = !viewModel.working && !viewModel.isApkLoaded(),
                            onClick = { showUpdates = true }
                        ) {
                            Icon(Icons.Default.CloudDownload, contentDescription = null)
                            Spacer(Modifier.width(8.dp))
                            Text(if (updater.available != null) "Update available" else "Updates")
                        }
                    },
                    colors = TopAppBarDefaults.topAppBarColors(
                        containerColor = Color.Transparent
                    ),
                )
            },
            navigationItems = listOf(patcherNavItem),
            itemsEnabled = !viewModel.working && !viewModel.isApkLoaded()
        )
    }
}
