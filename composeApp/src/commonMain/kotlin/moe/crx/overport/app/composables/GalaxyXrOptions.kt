package moe.crx.overport.app.composables

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.jetbrains.compose.resources.stringResource
import overportapp.composeapp.generated.resources.*

@Composable
fun GalaxyXrOptions(
    textureSize: String,
    onTextureSize: (String) -> Unit,
    maximumPerformance: Boolean,
    onMaximumPerformance: (Boolean) -> Unit,
    handTracking: Boolean,
    onHandTracking: (Boolean) -> Unit,
    openDispatcher: Boolean,
    onOpenDispatcher: (Boolean) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    val sizes = linkedMapOf(
        "3552x3840" to stringResource(Res.string.resolution_panel),
        "2784x3240" to "2784 × 3240",
        "2320x2700" to "2320 × 2700",
        "native" to stringResource(Res.string.resolution_runtime),
    )
    Card(modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Samsung Galaxy XR", style = MaterialTheme.typography.titleLarge)
            Text(stringResource(Res.string.galaxy_compatibility), style = MaterialTheme.typography.bodyMedium)
            Text(stringResource(Res.string.resolution_per_eye), style = MaterialTheme.typography.labelLarge)
            Box {
                OutlinedButton(onClick = { expanded = true }, modifier = Modifier.fillMaxWidth()) {
                    Text(sizes.getValue(textureSize))
                }
                DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                    sizes.forEach { (value, label) ->
                        DropdownMenuItem(text = { Text(label) }, onClick = { onTextureSize(value); expanded = false })
                    }
                }
            }
            Text(stringResource(Res.string.resolution_hint), style = MaterialTheme.typography.bodySmall)
            ListItem(
                headlineContent = { Text(stringResource(Res.string.maximum_performance)) },
                supportingContent = { Text(stringResource(if (maximumPerformance) Res.string.performance_max_hint else Res.string.performance_app_hint)) },
                trailingContent = { Switch(checked = maximumPerformance, onCheckedChange = onMaximumPerformance) },
                colors = ListItemDefaults.colors(containerColor = MaterialTheme.colorScheme.surfaceContainerHighest),
            )
            ListItem(
                headlineContent = { Text(stringResource(Res.string.hand_tracking_permission)) },
                supportingContent = { Text(stringResource(if (handTracking) Res.string.hand_tracking_auto_hint else Res.string.hand_tracking_off_hint)) },
                trailingContent = { Switch(checked = handTracking, onCheckedChange = onHandTracking) },
                colors = ListItemDefaults.colors(containerColor = MaterialTheme.colorScheme.surfaceContainerHighest),
            )
            ListItem(
                headlineContent = { Text(stringResource(Res.string.open_dispatcher)) },
                supportingContent = { Text(stringResource(if (openDispatcher) Res.string.open_dispatcher_on_hint else Res.string.open_dispatcher_off_hint)) },
                trailingContent = { Switch(checked = openDispatcher, onCheckedChange = onOpenDispatcher) },
                colors = ListItemDefaults.colors(containerColor = MaterialTheme.colorScheme.surfaceContainerHighest),
            )
        }
    }
}
