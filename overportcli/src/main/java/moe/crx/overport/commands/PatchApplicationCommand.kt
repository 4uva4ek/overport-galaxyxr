package moe.crx.overport.commands

import kotlinx.coroutines.cancelAndJoin
import moe.crx.overport.cli.CliCommand
import moe.crx.overport.galaxyxr.GalaxyXrPatches
import moe.crx.overport.patches.PLATFORM_ICON_RESIZER
import moe.crx.overport.patches.PATCH_TEXTURE_SIZE
import moe.crx.overport.patches.PATCH_VERBOSE_RUNTIME
import moe.crx.overport.patches.PATCH_GALAXYXR_PERFORMANCE
import moe.crx.overport.patches.TextureSize
import moe.crx.overport.patching.PatchStore
import moe.crx.overport.patching.PatcherContext
import moe.crx.overport.utils.DesktopUtil.defaultWorkspace
import moe.crx.overport.utils.ImageIOIconResizer
import moe.crx.overport.utils.NameFormatter.DEFAULT_FORMAT
import moe.crx.overport.utils.PrintUtil.drawProgressBar
import moe.crx.overport.utils.PrintUtil.drawSpinner
import moe.crx.overport.utils.PrintUtil.updateLine
import moe.crx.overport.versions.VersionManager
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream

object PatchApplicationCommand : CliCommand() {
    override fun name() = "patch"
    override fun description() = "Patch provided APK file."

    override fun printHelp(): Boolean {
        println("usage: overport patch --input=<path> [--output=<path>]")
        println("    [--output-name=<format>] [--workspace=<path>] [--patches=<names>] [--version=<value>]")
        println()
        println("--input=<path>")
        println("Specifies input APK file to patch.")
        println()
        println("--output=<path>")
        println("Specifies output APK file directory path.")
        println("If not specified, uses same path as input file.")
        println("If specified path is a file, uses it's parent directory.")
        println()
        println("--output-name=<format>")
        println("Specifies output APK file name format string.")
        println("If not specified, uses name of input file but with \"output\" suffix appended to it.")
        println("See more information about name format below.")
        println()
        println("--workspace=<path>")
        println("Specifies workspace directory path.")
        println("If not specified, uses default one in user's home directory.")
        println()
        println("--patches=<value>")
        println("Specifies patches to apply. Value should be a semicolon-separated list of patch names.")
        println("If not specified, uses every recommended patch available. Use \"overport patches\" to see available patches.")
        println("You can also specify arguments for a specific patch using <patch name>=<comma-separated arguments>.")
        println()
        println("--target=<value>")
        println("Specifies target platform preset. Available targets: ${GalaxyXrPatches.TARGET}.")
        println("Uses the recommended patch set for that target")
        println("(unless --patches is also specified, which takes precedence).")
        println("Galaxy XR defaults to 3552x3840 per eye; --texture-size overrides this.")
        println()
        println("--version=<value>")
        println("Specifies which overport version to use for patching.")
        println("Value also could be \"experimental\" or \"latest\".")
        println("Defaults to 3.4.3-aa54c3f for Galaxy XR, latest otherwise. Use \"overport install\" to list versions.")
        println()
        println("--texture-size=WIDTHxHEIGHT | native")
        println("Override the per-eye texture recommendation in this APK (64..4096 per dimension).")
        println("Omit to use the target default; native clears the override. Games may apply their own scale.")
        println("--performance=max | app (Galaxy XR only): default max requests CPU/GPU BOOST and supported 90 Hz.")
        println("Use app to restore application-controlled performance and refresh rate.")
        println("--foveation=eye | app (Galaxy XR only): experimental eye tracking for application foveation profiles; default app.")
        println("--hands=auto | on | off (Galaxy XR only): request the Android XR hand tracking permission at launch.")
        println("Default auto requests it only when the game declares Quest hand tracking; on forces it, off never asks.")
        println("--hand-mesh=fix | runtime (Galaxy XR only): fix (default) rebuilds the incomplete runtime hand skeleton for OVRPlugin.")
        println("--hand-frames=meta | runtime (Galaxy XR only): meta (default) re-rolls the wrist and little-metacarpal joint frames")
        println("to the Meta runtime conventions that Quest hand rigs expect; runtime keeps the Galaxy XR frames.")
        println("--hand-bind=edited | rebuild (Galaxy XR only): edited (default) uses the hand-tuned bind table built into the adapter;")
        println("rebuild derives the bind pose from the runtime data only.")
        println("--dispatcher=open | overport (Galaxy XR only): open (default) packages the open OpenXR dispatcher")
        println("(openxr-dispatcher-open ${moe.crx.overport.galaxyxr.OpenDispatcher.VERSION}) as libopenxr_loader.so;")
        println("overport keeps the closed Overport dispatcher with the hash-guarded swapchain abort fix.")
        println("--verbose-runtime enables runtime diagnostics in this APK without changing device properties.")
        println("These explicit options are applied after --patches or the selected target preset.")
        println()
        println("About file name format:")
        println("File name format allows to specify pattern for automatic file name generation.")
        println("Default file name format is $DEFAULT_FORMAT.")
        println("Available format tags:")
        println("{package} - application package name")
        println("{label} - application label")
        println("{code} - application version code")
        println("{version} - application version name")
        println("{overport} - overport version used for patching")
        println("{filename} - input file name without extension")
        println("{ext} - input file extension")
        println("{filenameext} - input file name with extension")
        return true
    }

    private data class ParsedArguments(
        var inputFile: String? = null,
        var outputFile: String? = null,
        var workspace: String? = null,
        var patches: Map<String, List<String>>? = null,
        var nameFormat: String? = null,
        var overportVersion: String? = null,
        var target: String? = null,
        var textureSize: String? = null,
        var verboseRuntime: Boolean = false,
        var performance: String? = null,
        var foveation: String? = null,
        var hands: String? = null,
        var handMesh: String? = null,
        var handFrames: String? = null,
        var handBind: String? = null,
        var dispatcher: String? = null,
    )

    private fun scanArguments(args: List<String>): ParsedArguments {
        val parsed = ParsedArguments()

        val associated = args.associate {
            it.substringBefore('=') to it.substringAfter('=', "")
        }

        associated.forEach { (key, value) ->
            when (key) {
                "--input" -> parsed.inputFile = value
                "--output" -> parsed.outputFile = value
                "--workspace" -> parsed.workspace = value
                "--patches" -> parsed.patches = value.split(';').map { splitted -> splitted.trim() }.associate {
                    it.substringBefore('=') to it.substringAfter('=', "").split(',').map { splitted -> splitted.trim() }
                }

                "--output-name" -> parsed.nameFormat = value
                "--version" -> parsed.overportVersion = value
                "--target" -> parsed.target = value
                "--texture-size" -> parsed.textureSize = value
                "--verbose-runtime" -> parsed.verboseRuntime = true
                "--performance" -> parsed.performance = value
                "--foveation" -> parsed.foveation = value
                "--hands" -> parsed.hands = value
                "--hand-mesh" -> parsed.handMesh = value
                "--hand-frames" -> parsed.handFrames = value
                "--hand-bind" -> parsed.handBind = value
                "--dispatcher" -> parsed.dispatcher = value
            }
        }

        return parsed
    }

    override suspend fun execute(args: List<String>): Boolean {
        val parsed = scanArguments(args)

        val inputFile = parsed.inputFile?.let { File(it) } ?: return false
        if (parsed.foveation != null && (parsed.target != GalaxyXrPatches.TARGET || parsed.foveation !in listOf("app", "eye"))) {
            println("--foveation requires --target=galaxyxr and a value of eye or app.")
            return false
        }
        if (parsed.performance != null && (parsed.target != GalaxyXrPatches.TARGET || parsed.performance !in listOf("max", "app"))) {
            println("--performance requires --target=galaxyxr and a value of max or app.")
            return false
        }
        if (parsed.hands != null && (parsed.target != GalaxyXrPatches.TARGET || parsed.hands !in listOf("auto", "on", "off"))) {
            println("--hands requires --target=galaxyxr and a value of auto, on or off.")
            return false
        }
        if (parsed.handMesh != null && (parsed.target != GalaxyXrPatches.TARGET || parsed.handMesh !in listOf("fix", "runtime"))) {
            println("--hand-mesh requires --target=galaxyxr and a value of fix or runtime.")
            return false
        }
        if (parsed.handFrames != null && (parsed.target != GalaxyXrPatches.TARGET || parsed.handFrames !in listOf("meta", "runtime"))) {
            println("--hand-frames requires --target=galaxyxr and a value of meta or runtime.")
            return false
        }
        if (parsed.handBind != null && (parsed.target != GalaxyXrPatches.TARGET || parsed.handBind !in listOf("rebuild", "edited"))) {
            println("--hand-bind requires --target=galaxyxr and a value of edited or rebuild.")
            return false
        }
        if (parsed.dispatcher != null && (parsed.target != GalaxyXrPatches.TARGET || parsed.dispatcher !in GalaxyXrPatches.DISPATCHERS)) {
            println("--dispatcher requires --target=galaxyxr and a value of open or overport.")
            return false
        }

        if (!inputFile.isFile) {
            return false
        }

        parsed.textureSize?.let {
            try {
                TextureSize.parse(it)
            } catch (e: IllegalArgumentException) {
                println(e.message)
                return false
            }
        }

        var outputDir = parsed.outputFile?.let { File(it) } ?: inputFile.parentFile

        if (outputDir.isFile) {
            outputDir = outputDir.parentFile
        }

        val workspace = parsed.workspace?.let { File(it) } ?: defaultWorkspace()

        val patches = (parsed.patches ?: when (parsed.target) {
            GalaxyXrPatches.TARGET -> GalaxyXrPatches.recommended(parsed.dispatcher ?: GalaxyXrPatches.DISPATCHER_OPEN)
            null -> PatchStore.recommended().associate { it.name to listOf() }
            else -> {
                println("Unknown target '${parsed.target}'. Available targets: ${GalaxyXrPatches.TARGET}. Using the default patch set.")
                PatchStore.recommended().associate { it.name to listOf() }
            }
        }).toMutableMap()
        parsed.textureSize?.let { patches[PATCH_TEXTURE_SIZE.name] = listOf(it) }
        if (parsed.verboseRuntime) patches[PATCH_VERBOSE_RUNTIME.name] = emptyList()
        if (parsed.performance != null || parsed.foveation != null || parsed.hands != null || parsed.handMesh != null || parsed.handFrames != null || parsed.handBind != null) {
            patches[PATCH_GALAXYXR_PERFORMANCE.name] = listOf(parsed.performance ?: "max", parsed.foveation ?: "app", parsed.hands ?: "auto",
                parsed.handMesh ?: "fix", parsed.handFrames ?: "meta", parsed.handBind ?: "edited")
        }
        parsed.dispatcher?.let { GalaxyXrPatches.selectDispatcher(patches, it) }

        val nameFormat = parsed.nameFormat ?: DEFAULT_FORMAT

        val version = parsed.overportVersion ?: if (parsed.target == GalaxyXrPatches.TARGET) GalaxyXrPatches.DEFAULT_RUNTIME else "latest"

        println("overport cli utility, version ${VersionManager.VERSION} (${VersionManager.DISTRIBUTION})")
        println("Input APK: ${inputFile.absolutePath}")
        println("Output directory: ${outputDir.absolutePath}")
        println("Workspace directory: ${workspace.absolutePath}")

        PLATFORM_ICON_RESIZER = ImageIOIconResizer
        val patcher = PatcherContext(version, workspace, inputFile.name, nameFormat)

        val waitingForLibraries = updateLine {
            drawSpinner()
            print("Preparing libraries...")
        }
        patcher.checkout()
        waitingForLibraries.cancelAndJoin()

        parsed.target?.let {
            println("Target platform: $it")
        }
        println("Using overport version: ${patcher.overportVersion()}")

        val waitingForDecompilation = updateLine {
            drawSpinner()
            print("Decompiling APK...")
        }
        FileInputStream(inputFile).use { fis ->
            patcher.prepare(fis)
        }
        waitingForDecompilation.cancelAndJoin()

        println("Decompiled app: ${patcher.appName()}, package ${patcher.appPackage()}, version ${patcher.appVersionName()}")

        val totalPatches = patches.size
        var appliedPatches = 0
        var currentPatch = ""
        val waitingForPatching = updateLine {
            drawSpinner()
            drawProgressBar(appliedPatches.toLong(), totalPatches.toLong())
            print("Applying patch $currentPatch... ")
            print("($appliedPatches / $totalPatches)")
        }
        patcher.patch(patches) { index, patch ->
            appliedPatches = index
            currentPatch = patch.name
        }
        waitingForPatching.cancelAndJoin()

        outputDir.mkdirs()

        val outputFile = File(outputDir, patcher.outputFileName())

        println("Output file: ${outputFile.absolutePath}")

        val waitingForCompilation = updateLine {
            drawSpinner()
            print("Compiling APK... ")
        }
        FileOutputStream(outputFile).use { fos ->
            patcher.export(fos)
        }
        waitingForCompilation.cancelAndJoin()

        val waitingForCleaning = updateLine {
            drawSpinner()
            print("Cleaning up... ")
        }
        patcher.cleanup()
        waitingForCleaning.cancelAndJoin()

        println("Patching successful.")

        return true
    }
}
