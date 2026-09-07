plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
}

android {
    namespace = "com.brazilmr"
    compileSdk = 34
    ndkVersion = "26.1.10909125"

    defaultConfig {
        applicationId = "com.brazilmr"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "1.0.0"

        externalNativeBuild {
            cmake {
                cppFlags += listOf(
                    "-std=c++17",
                    "-O3",
                    "-fvisibility=hidden",
                    "-fno-exceptions",
                    "-fvisibility-inlines-hidden",
                    "-ffunction-sections",
                    "-fdata-sections"
                )
                arguments += listOf(
                    "-DANDROID_STL=c++_static",
                    // ^ STL estático: o pré-compilado libc++_shared.so do NDK r26
                    //   é alinhado a 4 KB e quebraria em dispositivos de 16 KB.
                    "-DCMAKE_BUILD_TYPE=Release",
                    // LuaJIT nativo é opcional (requer checkout do subtree em third_party/luajit).
                    // O engine Lua padrão é LuaJ (JVM), 100% funcional sem NDK extra.
                    "-DBRAZILMR_BUILD_LUAJIT=OFF"
                )
            }
        }
        ndk {
            // BrazilMR roda em ARM64 (todos os VR Box modernos) com fallback armeabi-v7a.
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            isShrinkResources = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        debug {
            isJniDebuggable = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }

    // Não há UI 2D: sem ViewBinding, sem Compose, sem XML layouts.
    buildFeatures {
        viewBinding = false
        buildConfig = true
    }

    packaging {
        jniLibs.useLegacyPackaging = false
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.annotation)
    implementation(libs.kotlinx.coroutines.android)

    // ARCore — backend 6DoF primário (SLAM). Declarado "optional" no manifest:
    // sem ARCore o BrasilMR cai para o backend de CV próprio (odometria visual).
    implementation(libs.google.ar.core)

    // MediaPipe Tasks Vision — backend de hand tracking (landmarks).
    // Se o modelo hand_landmarker.task não estiver em assets/ (tools/fetch_models.sh),
    // o runtime degrada para apontador de cabeça (0DoF).
    implementation(libs.mediapipe.task.vision)

    // LuaJ — engine Lua da BrazilMR VR SDK (LuaJIT nativo opcional via CMake).
    implementation(libs.luaj.jse)

    testImplementation(libs.junit)
}
