import com.vanniktech.maven.publish.AndroidSingleVariantLibrary
import com.vanniktech.maven.publish.JavadocJar
import com.vanniktech.maven.publish.SourcesJar

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.maven.publish)
}

java {
    toolchain.languageVersion = JavaLanguageVersion.of(21)
}

android {
    enableKotlin = false

    namespace = "com.v7878.vmtools"
    compileSdk {
        version = release(37)
    }

    defaultConfig {
        minSdk = 26
    }
}

dependencies {
    api(libs.panama.core)
    api(libs.panama.unsafe)
    api(libs.panama.llvm)

    implementation(libs.sun.cleaner)
    implementation(libs.r8.annotations)
}

mavenPublishing {
    publishToMavenCentral(automaticRelease = false)
    configure(
        AndroidSingleVariantLibrary(
            javadocJar = JavadocJar.Empty(),
            sourcesJar = SourcesJar.Sources()
        )
    )

    coordinates(
        groupId = project.group?.toString() ?: "io.github.vova7878",
        artifactId = "AndroidVMTools",
        version = project.version.toString()
    )

    pom {
        name.set("AndroidVMTools")
        // TODO: description.set("")
        inceptionYear.set("2026")
        url.set("https://github.com/vova7878/AndroidVMTools")

        licenses {
            license {
                name.set("MIT")
                url.set("https://opensource.org/license/mit")
                distribution.set("repository")
            }
        }

        developers {
            developer {
                id.set("vova7878")
                name.set("Vladimir Kozelkov")
                url.set("https://github.com/vova7878")
            }
        }

        scm {
            url.set("https://github.com/vova7878/AndroidVMTools")
            connection.set("scm:git:git://github.com/vova7878/AndroidVMTools.git")
            developerConnection.set("scm:git:ssh://git@github.com/vova7878/AndroidVMTools.git")
        }
    }
}
