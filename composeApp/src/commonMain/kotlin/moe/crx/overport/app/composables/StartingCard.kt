package moe.crx.overport.app.composables

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.Park
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.dp
import moe.crx.overport.app.AndroidPlatform
import moe.crx.overport.app.getPlatform
import moe.crx.overport.app.util.ModifierUtil.rounded
import org.jetbrains.compose.resources.stringResource
import org.jetbrains.compose.ui.tooling.preview.Preview
import overportapp.composeapp.generated.resources.Res
import overportapp.composeapp.generated.resources.decoding_apk
import overportapp.composeapp.generated.resources.or_drag_and_drop_it
import overportapp.composeapp.generated.resources.select_apk_to_begin
import overportapp.composeapp.generated.resources.galaxy_start_hint
import java.util.*

val isWinter = Calendar.getInstance().get(Calendar.MONTH).let { it == 0 || it == 1 || it == 11 }

@Composable
@Preview
fun StartingCard(
    showLoading: Boolean = false,
    currentString: String? = null,
    onCardClick: () -> Unit = {}
) {
    val strokeColor = LocalContentColor.current

    Card(
        modifier = Modifier
            .padding(16.dp)
            .fillMaxSize()
            .drawBehind {
                if (!showLoading) {
                    drawRoundRect(
                        color = strokeColor,
                        style = Stroke(
                            width = 4.dp.toPx(),
                            pathEffect = PathEffect.dashPathEffect(
                                floatArrayOf(
                                    16.dp.toPx(),
                                    16.dp.toPx()
                                ), 16.dp.toPx()
                            )
                        ),
                        cornerRadius = CornerRadius(32.dp.toPx())
                    )
                    if (isWinter) {
                        drawRoundRect(
                            color = Color(0xFFEF5350),
                            style = Stroke(
                                width = 4.dp.toPx(),
                                pathEffect = PathEffect.dashPathEffect(
                                    floatArrayOf(
                                        16.dp.toPx(),
                                        16.dp.toPx()
                                    ), 0f
                                )
                            ),
                            cornerRadius = CornerRadius(32.dp.toPx())
                        )
                    }
                }
            }
            .rounded(32.dp)
            .clickable(!showLoading, onClick = onCardClick),
        colors = CardDefaults.cardColors(
            containerColor = Color.Transparent
        )
    ) {
        Column(
            modifier = Modifier
                .padding(16.dp)
                .fillMaxSize(),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            if (!showLoading) {
                Icon(
                    if (isWinter) {
                        Icons.Default.Park
                    } else {
                        Icons.Default.Download
                    },
                    contentDescription = null,
                    modifier = Modifier
                        .padding(16.dp)
                        .width(64.dp)
                        .height(64.dp)
                )
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(stringResource(Res.string.galaxy_start_hint), style = MaterialTheme.typography.titleMedium)
                    Spacer(Modifier.height(12.dp))
                    Text(
                        text = stringResource(Res.string.select_apk_to_begin)
                    )
                    if (getPlatform() !is AndroidPlatform) {
                        Text(
                            text = stringResource(Res.string.or_drag_and_drop_it)
                        )
                    }
                }
            } else {
                BreathingIcon(
                    modifier = Modifier
                        .padding(16.dp)
                        .rotate(30f)
                        .width(96.dp)
                        .height(96.dp)
                )
                Text(
                    text = currentString ?: stringResource(Res.string.decoding_apk)
                )
            }
        }
    }
}
