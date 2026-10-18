package moe.crx.overport.app.composables

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import moe.crx.overport.galaxyxr.FoveationSettings
import org.jetbrains.compose.resources.stringResource
import overportapp.composeapp.generated.resources.*
import kotlin.math.round

@Composable
fun FoveationOptions(mode: String, onMode: (String) -> Unit, quality: FoveationSettings, onQuality: (FoveationSettings) -> Unit) {
    var expanded by remember { mutableStateOf(false) }
    var debugExpanded by remember { mutableStateOf(false) }
    val modes = linkedMapOf("app" to stringResource(Res.string.foveation_app), "runtime-eye" to stringResource(Res.string.foveation_runtime_eye))
    val debugModes = linkedMapOf("qcom" to stringResource(Res.string.foveation_gaze), "qcom-debug" to stringResource(Res.string.foveation_debug))
    Card(Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(stringResource(Res.string.foveation_title), style = MaterialTheme.typography.titleMedium)
            Box {
                OutlinedButton(onClick = { expanded = true }) {
                    Text(modes[mode] ?: "${stringResource(Res.string.foveation_debug_section)}: ${debugModes.getValue(mode)}")
                }
                DropdownMenu(expanded, onDismissRequest = { expanded = false }) {
                    modes.forEach { (value, label) -> DropdownMenuItem(text = { Text(label) }, onClick = { onMode(value); expanded = false }) }
                }
            }
            TextButton(onClick = { debugExpanded = !debugExpanded }) {
                Text(stringResource(if (debugExpanded) Res.string.foveation_debug_hide else Res.string.foveation_debug_section))
            }
            if (debugExpanded) {
                Text(stringResource(Res.string.foveation_debug_explanation), style = MaterialTheme.typography.bodySmall)
                debugModes.forEach { (value, label) ->
                    FilterChip(selected = mode == value, onClick = { onMode(value) }, label = { Text(label) })
                }
            }
            if (mode == "qcom" || mode == "runtime-eye") {
                Text(stringResource(Res.string.foveation_repatch), style = MaterialTheme.typography.bodySmall)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TextButton(onClick = { onQuality(FoveationSettings(.5f, 2f, 1f)) }) { Text(stringResource(Res.string.foveation_soft)) }
                    TextButton(onClick = { onQuality(FoveationSettings()) }) { Text(stringResource(Res.string.foveation_start)) }
                }
                QualitySlider(stringResource(Res.string.foveation_density), quality.density, .125f..1f, stringResource(Res.string.foveation_density_hint)) { onQuality(quality.copy(density = it)) }
                QualitySlider(stringResource(Res.string.foveation_gain), quality.gain, 0f..32f, stringResource(Res.string.foveation_gain_hint)) { onQuality(quality.copy(gain = it)) }
                QualitySlider(stringResource(Res.string.foveation_area), quality.area, 0f..2f, stringResource(Res.string.foveation_area_hint)) { onQuality(quality.copy(area = it)) }
            }
            if (mode == "runtime-eye") Text(stringResource(Res.string.foveation_runtime_eye_hint))
            if (mode == "qcom-debug") Text(stringResource(Res.string.foveation_debug_hint))
        }
    }
}

@Composable
private fun QualitySlider(label: String, value: Float, range: ClosedFloatingPointRange<Float>, hint: String, onValue: (Float) -> Unit) {
    Text("$label: $value", style = MaterialTheme.typography.labelLarge)
    Slider(value, onValueChange = { onValue((round(it * 1000) / 1000).coerceIn(range)) }, valueRange = range)
    Text(hint, style = MaterialTheme.typography.bodySmall)
}
