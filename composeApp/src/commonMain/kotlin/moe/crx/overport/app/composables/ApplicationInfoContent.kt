package moe.crx.overport.app.composables

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.unit.dp
import moe.crx.overport.app.util.PatchLocalizer.localizedPatch
import moe.crx.overport.patches.*
import moe.crx.overport.galaxyxr.GalaxyXrPatches
import moe.crx.overport.patching.Patch
import moe.crx.overport.patching.PatchStore
import org.jetbrains.compose.resources.stringResource
import org.jetbrains.compose.ui.tooling.preview.Preview
import overportapp.composeapp.generated.resources.*

@OptIn(ExperimentalLayoutApi::class)
@Composable
@Preview
fun ApplicationInfoContent(
    applicationName: String,
    applicationPackage: String,
    applicationVersion: String,
    applicationIcon: ImageBitmap? = null,
    onCancel: () -> Unit = {},
    onConfirm: (Map<String, List<String>>) -> Unit = {}
) {
    val enabledPatches = remember(applicationPackage) {
        mutableStateMapOf(
            *GalaxyXrPatches.recommended().entries.map { it.key to it.value }.toTypedArray()
        )
    }

    var patchIconArgument by remember(applicationPackage) {
        mutableStateOf("icon")
    }
    var textureSize by remember(applicationPackage) { mutableStateOf("3552x3840") }
    var maximumPerformance by remember(applicationPackage) { mutableStateOf(true) }
    var handTracking by remember(applicationPackage) { mutableStateOf(true) }
    var openDispatcher by remember(applicationPackage) { mutableStateOf(true) }
    var advanced by remember(applicationPackage) { mutableStateOf(false) }
    // These patches define the target and are configured through the controls below.
    val managed = setOf(PATCH_COPY_LIBRARIES, PATCH_GENERATE_CONFIG, PATCH_VR_METADATA, PATCH_GALAXYXR_UNITY_GLES,
        PATCH_GALAXYXR_SWAPCHAIN, PATCH_GALAXYXR_DISPATCHER, PATCH_GALAXYXR_PERFORMANCE, PATCH_TEXTURE_SIZE)

    fun togglePatch(patch: Patch) {
        val checked = enabledPatches.containsKey(patch.name)

        if (checked) {
            enabledPatches.remove(patch.name)
        } else {
            enabledPatches[patch.name] = listOf()
        }
    }

    Column(
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        FlowRow(
            modifier = Modifier.fillMaxWidth().padding(8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            ApplicationInfo(
                applicationName,
                applicationPackage,
                applicationVersion,
                applicationIcon,
                !enabledPatches.containsKey(PATCH_REPLACE_ICON_LABEL.name),
                patchIconArgument != "icon",
            )
        }

        Box(modifier = Modifier.weight(1f)) {
            LazyColumn {
                item {
                    GalaxyXrOptions(textureSize, { textureSize = it }, maximumPerformance, { maximumPerformance = it },
                        handTracking, { handTracking = it }, openDispatcher, { openDispatcher = it })
                    TextButton(onClick = { advanced = !advanced }, modifier = Modifier.padding(horizontal = 8.dp)) {
                        Text(stringResource(if (advanced) Res.string.hide_advanced else Res.string.show_advanced))
                    }
                }
                if (advanced) items(PatchStore.all().filter { it !in managed }) {
                    ListItem(
                        headlineContent = {
                            Text(localizedPatch(it))
                        },
                        supportingContent = {
                            if (!it.isRecommended) {
                                Text(stringResource(Res.string.use_at_your_risk))
                            }
                        },
                        trailingContent = {
                            Checkbox(enabledPatches.containsKey(it.name), { _ ->
                                togglePatch(it)
                            })
                        },
                        modifier = Modifier.clickable {
                            togglePatch(it)
                        }
                    )
                    if (PATCH_REPLACE_ICON_LABEL == it) {
                        FadeVisibility(enabledPatches.containsKey(it.name)) {
                            ListItem(
                                headlineContent = {
                                    Text(stringResource(Res.string.replace_icon_with_icon))
                                },
                                trailingContent = {
                                    RadioButton(patchIconArgument == "icon", {
                                        patchIconArgument = "icon"
                                    })
                                },
                                modifier = Modifier.padding(24.dp, 0.dp, 0.dp, 0.dp).clickable {
                                    patchIconArgument = "icon"
                                }
                            )
                        }
                        FadeVisibility(enabledPatches.containsKey(it.name)) {
                            ListItem(
                                headlineContent = {
                                    Text(stringResource(Res.string.replace_icon_with_cover))
                                },
                                trailingContent = {
                                    RadioButton(patchIconArgument == "cover", {
                                        patchIconArgument = "cover"
                                    })
                                },
                                modifier = Modifier.padding(24.dp, 0.dp, 0.dp, 0.dp).clickable {
                                    patchIconArgument = "cover"
                                }
                            )
                        }
                    }
                }
            }
        }
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Button(
                modifier = Modifier.weight(1f),
                onClick = onCancel
            ) {
                Text(stringResource(Res.string.action_cancel))
            }
            Button(
                modifier = Modifier.weight(1f),
                onClick = {
                    if (enabledPatches.containsKey(PATCH_REPLACE_ICON_LABEL.name)) {
                        enabledPatches[PATCH_REPLACE_ICON_LABEL.name] = listOf(patchIconArgument)
                    }
                    onConfirm(enabledPatches.toMutableMap().apply {
                        this[PATCH_TEXTURE_SIZE.name] = listOf(textureSize)
                        // Arguments: performance, foveation (app: CLI-only experiment), hands.
                        this[PATCH_GALAXYXR_PERFORMANCE.name] = listOf(if (maximumPerformance) "max" else "app", "app", if (handTracking) "auto" else "off")
                        GalaxyXrPatches.selectDispatcher(this, if (openDispatcher) GalaxyXrPatches.DISPATCHER_OPEN else GalaxyXrPatches.DISPATCHER_OVERPORT)
                    })
                }
            ) {
                Text(stringResource(Res.string.save_patched_apk))
            }
        }
    }
}
