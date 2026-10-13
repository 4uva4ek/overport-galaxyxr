package moe.crx.overport.commands

import moe.crx.overport.cli.CliCommand
import moe.crx.overport.cli.CommandStore.COMMANDS
import moe.crx.overport.utils.PrintUtil.fspace
import moe.crx.overport.utils.PrintUtil.uspace
import moe.crx.overport.versions.VersionManager

object DisplayHelpCommand : CliCommand() {
    override fun name() = "help"
    override fun description() = "Display this message."

    override fun printHelp(): Boolean {
        println("overport cli utility, version ${VersionManager.VERSION} (${VersionManager.DISTRIBUTION})")
        println("usage: overport <command> [args]")
        println()
        println("Available commands:")
        COMMANDS.values.forEach {
            print(fspace())
            print(it.name())
            print(uspace(it, COMMANDS.values))
            println(it.description())
        }
        println()
        println("overport help <command> shows help information for that specific command.")
        println()
        println("For additional info, see: https://github.com/ovrport/app")
        return true
    }

    override suspend fun execute(args: List<String>): Boolean {
        val command = args.firstOrNull()?.let { COMMANDS[it] } ?: run {
            DisplayHelpCommand.printHelp()
            return true
        }

        val hasHelp = command.printHelp()
        if (!hasHelp) {
            printHelp()
        }

        return true
    }
}
