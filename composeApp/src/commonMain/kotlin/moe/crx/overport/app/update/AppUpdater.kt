package moe.crx.overport.app.update

import java.io.File

interface AppUpdater {
    val platform: String
    val downloadDirectory: File
    val installLabel: String
    val installDescription: String
    suspend fun install(file: File, update: AppUpdate): String
}
