import org.jetbrains.compose.desktop.application.dsl.TargetFormat
import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import java.util.Properties

val appVersion: String by project
val galaxyXr = Properties().apply { rootProject.file("galaxyxr.properties").inputStream().use { load(it) } }
val galaxyXrVersion = galaxyXr.getProperty("version")

plugins {
    alias(libs.plugins.kotlinMultiplatform)
    alias(libs.plugins.androidApplication)
    alias(libs.plugins.composeMultiplatform)
    alias(libs.plugins.composeCompiler)
    alias(libs.plugins.kotlinSerialization)
    alias(libs.plugins.composeHotReload)
}

kotlin {
    androidTarget {
        compilerOptions {
            jvmTarget.set(JvmTarget.JVM_11)
        }
    }

    jvm("desktop")

    sourceSets {
        val desktopMain by getting

        androidMain.dependencies {
            implementation(compose.preview)
            implementation(libs.androidx.activity.compose)
        }
        commonMain.dependencies {
            implementation(project(":overportcli"))
            implementation(compose.runtime)
            implementation(compose.foundation)
            implementation(compose.material3)
            implementation(compose.ui)
            implementation(compose.components.resources)
            implementation(compose.components.uiToolingPreview)
            implementation(libs.androidx.lifecycle.viewmodel)
            implementation(libs.androidx.lifecycle.runtime.compose)
            implementation(libs.androidx.lifecycle.viewmodel.compose)
            implementation(compose.materialIconsExtended)
            implementation(libs.kotlinx.serialization.json)
        }
        desktopMain.dependencies {
            implementation(compose.desktop.currentOs)
            implementation(libs.kotlinx.coroutines.swing)
            implementation(libs.com.formdev.flatlaf)
        }
        commonTest.dependencies {
            implementation(kotlin("test"))
        }
    }
}

android {
    namespace = "moe.crx.overport.app"
    compileSdk = libs.versions.android.compileSdk.get().toInt()
    buildToolsVersion = "36.0.0"

    defaultConfig {
        applicationId = "moe.crx.overport.galaxyxr"
        minSdk = libs.versions.android.minSdk.get().toInt()
        targetSdk = libs.versions.android.targetSdk.get().toInt()
        versionCode = galaxyXr.getProperty("versionCode").toInt()
        versionName = galaxyXrVersion
    }
    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
            excludes += "META-INF/versions/9/OSGI-INF/MANIFEST.MF"
        }
    }
    signingConfigs {
        if (!System.getenv("GXR_KEYSTORE_FILE").isNullOrBlank()) {
            create("galaxyxr") {
                storeFile = file(System.getenv("GXR_KEYSTORE_FILE"))
                storePassword = System.getenv("GXR_KEYSTORE_PASSWORD")
                keyAlias = "overport-galaxyxr"
                keyPassword = System.getenv("GXR_KEYSTORE_PASSWORD")
            }
        }
    }
    buildTypes {
        release {
            signingConfig = signingConfigs.findByName("galaxyxr")
            isMinifyEnabled = false
            isShrinkResources = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

dependencies {
    debugImplementation(compose.uiTooling)
}

compose.desktop {
    application {
        mainClass = "moe.crx.overport.app.ComposeMainKt"
        // Include the runtime modules used by file dialogs, signing and HTTPS.
        nativeDistributions.includeAllModules = true

        buildTypes {
            release {
                proguard {
                    configurationFiles.from("proguard-rules.pro")
                    isEnabled = false
                }
            }
        }

        nativeDistributions {
            targetFormats(TargetFormat.Msi, TargetFormat.Dmg, TargetFormat.Deb)
            packageName = "OverportGalaxyXR"
            packageVersion = galaxyXrVersion

            windows {
                shortcut = true
                perUserInstall = true
                iconFile.set(file("icons/app-icon.ico"))
            }
            linux {
                shortcut = true
                iconFile.set(file("icons/app-icon.png"))
            }
            macOS {
                // Apple package versions require a positive major; keep release/update versions unchanged.
                packageVersion = galaxyXrVersion.split('.').let { parts ->
                    "${parts[0].toInt() + 1}.${parts[1]}.${parts[2]}"
                }
                bundleID = "moe.crx.overport.galaxyxr"
                dockName = "Overport Galaxy XR"
                iconFile.set(file("icons/app-icon.icns"))
            }
        }
    }
}
