plugins {
    id("com.android.library")
}

android {
    namespace = "com.v7878.zygisk"

    defaultConfig {
        consumerProguardFiles("consumer-rules.pro")
    }

    sourceSets {
        getByName("main") {
            // Prebuilt by build-native.ps1 (see README in HIDE_PROCESS.md). The
            // CMake project under src/main/cpp is kept for reference; Gradle
            // does not configure it so builds do not depend on a local CMake
            // toolchain.
            jniLibs.srcDir("src/main/jniLibs")
        }
    }

    buildFeatures {
        buildConfig = true
    }

    buildTypes {
        // The module app runs R8 over the merged program; shrinking this library
        // in isolation would drop the classes its Java entrypoint is loaded by
        // name from native code (com.v7878.zygisk.EntryPoint).
        release {
            isMinifyEnabled = false
        }
    }
}

dependencies {
    implementation(libs.io.github.vova7878.r8annotations)
}
