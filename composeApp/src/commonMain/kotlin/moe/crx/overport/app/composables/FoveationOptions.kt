package moe.crx.overport.app.composables

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import moe.crx.overport.galaxyxr.FoveationSettings
import org.jetbrains.compose.resources.stringResource
import overportapp.composeapp.generated.resources.*
import kotlin.math.round

@Composable
fun FoveationOptions(mode: String, onMode: (String) -> Unit, quality: FoveationSettings, onQuality: (FoveationSettings) -> Unit) {
    var expanded by remember { mutableStateOf(false) }
    val modes = linkedMapOf("app" to stringResource(Res.string.foveation_app), "runtime-eye" to stringResource(Res.string.foveation_runtime_eye))
    Card(Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(stringResource(Res.string.foveation_title), style = MaterialTheme.typography.titleMedium)
            Box {
                OutlinedButton(onClick = { expanded = true }) {
                    Text(modes.getValue(mode))
                }
                DropdownMenu(expanded, onDismissRequest = { expanded = false }) {
                    modes.forEach { (value, label) -> DropdownMenuItem(text = { Text(label) }, onClick = { onMode(value); expanded = false }) }
                }
            }
            if (mode == "app") {
                Text(stringResource(Res.string.foveation_app_quality_hint), style = MaterialTheme.typography.bodySmall)
            }
                Text(stringResource(Res.string.foveation_repatch), style = MaterialTheme.typography.bodySmall)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TextButton(onClick = { onQuality(FoveationSettings(.5f, 2f, 1f)) }) { Text(stringResource(Res.string.foveation_soft)) }
                    TextButton(onClick = { onQuality(FoveationSettings()) }) { Text(stringResource(Res.string.foveation_start)) }
                }
                QualitySlider(stringResource(Res.string.foveation_density), quality.density, .125f..1f, stringResource(Res.string.foveation_density_hint)) { onQuality(quality.copy(density = it)) }
                QualitySlider(stringResource(Res.string.foveation_gain), quality.gain, 0f..32f, stringResource(Res.string.foveation_gain_hint)) { onQuality(quality.copy(gain = it)) }
                QualitySlider(stringResource(Res.string.foveation_area), quality.area, 0f..2f, stringResource(Res.string.foveation_area_hint)) { onQuality(quality.copy(area = it)) }
            if (mode == "runtime-eye") Text(stringResource(Res.string.foveation_runtime_eye_hint))
        }
    }
}

@Composable
private fun QualitySlider(label: String, value: Float, range: ClosedFloatingPointRange<Float>, hint: String, onValue: (Float) -> Unit) {
    var input by remember { mutableStateOf(value.toString()) }
    fun parsed(text: String) = text.trim().replace(',', '.').toFloatOrNull()
    LaunchedEffect(value) {
        if (parsed(input) != value) input = value.toString()
    }
    val number = parsed(input)
    val valid = number != null && number.isFinite() && number in range
    OutlinedTextField(
        value = input,
        onValueChange = { text ->
            input = text
            val next = parsed(text)
            if (next != null && next.isFinite() && next in range) onValue(next)
        },
        label = { Text(label) },
        singleLine = true,
        isError = !valid,
        supportingText = { Text("${range.start} – ${range.endInclusive}") },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
        modifier = Modifier.fillMaxWidth(),
    )
    // Keep the exact supported endpoints (density starts at 0.125), otherwise snap to 0.05.
    Slider(value, onValueChange = { onValue((round(it * 20) / 20).coerceIn(range)) }, valueRange = range)
    Text(hint, style = MaterialTheme.typography.bodySmall)
}
