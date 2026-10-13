package moe.crx.overport.app.composables

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import moe.crx.overport.app.update.UpdateController
import moe.crx.overport.versions.VersionManager

@Composable
fun UpdateDialog(controller: UpdateController, onDismiss: () -> Unit) {
    val scope = rememberCoroutineScope()
    AlertDialog(
        onDismissRequest = { if (!controller.busy) onDismiss() },
        title = { Text("Overport updates") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Installed: ${VersionManager.DISTRIBUTION_VERSION}")
                Text(controller.status)
                if (controller.checking) LinearProgressIndicator(Modifier.fillMaxWidth())
                controller.progress?.let { LinearProgressIndicator(progress = { it }, modifier = Modifier.fillMaxWidth()) }
                if (controller.available != null) Text(controller.installer.installDescription)
                TextButton(onClick = { scope.launch { controller.check() } }, enabled = !controller.busy && !controller.checking) {
                    Text("Check for updates")
                }
            }
        },
        confirmButton = {
            if (controller.available != null) Button(
                enabled = !controller.busy && !controller.checking,
                onClick = { scope.launch { controller.install() } }
            ) { Text(controller.installer.installLabel) }
        },
        dismissButton = { TextButton(onClick = onDismiss, enabled = !controller.busy) { Text("Close") } }
    )
}
