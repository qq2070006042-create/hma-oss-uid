# Builds the Zygisk runtime native library for every supported ABI.
# Run this after changing anything under src/main/cpp. Gradle packages the
# produced files from src/main/jniLibs.
param(
    [string]$Ndk = $env:ANDROID_NDK_HOME,
    [string]$Api = "29"
)

$ErrorActionPreference = "Stop"

if (-not $Ndk) {
    $sdk = if ($env:ANDROID_SDK_ROOT) { $env:ANDROID_SDK_ROOT }
           elseif ($env:ANDROID_HOME) { $env:ANDROID_HOME }
           else { Join-Path $env:LOCALAPPDATA "Android\Sdk" }
    $candidate = Get-ChildItem (Join-Path $sdk "ndk") -Directory -ErrorAction SilentlyContinue |
        Sort-Object { [version]($_.Name -replace '-.*$', '') } -Descending |
        Select-Object -First 1
    if (-not $candidate) { throw "Android NDK not found. Set ANDROID_NDK_HOME." }
    $Ndk = $candidate.FullName
}

$bin = Join-Path $Ndk "toolchains\llvm\prebuilt\windows-x86_64\bin"
if (-not (Test-Path $bin)) { throw "Unsupported NDK layout: $Ndk" }

$source = Join-Path $PSScriptRoot "src\main\cpp"
$output = Join-Path $PSScriptRoot "src\main\jniLibs"

$targets = [ordered]@{
    "arm64-v8a"   = "aarch64-linux-android$Api-clang++.cmd"
    "armeabi-v7a" = "armv7a-linux-androideabi$Api-clang++.cmd"
    "x86"         = "i686-linux-android$Api-clang++.cmd"
    "x86_64"      = "x86_64-linux-android$Api-clang++.cmd"
}

foreach ($abi in $targets.Keys) {
    $compiler = Join-Path $bin $targets[$abi]
    if (-not (Test-Path $compiler)) { throw "Compiler not found: $compiler" }

    $destination = Join-Path $output $abi
    New-Item -ItemType Directory -Force $destination | Out-Null

    $arguments = @(
        "--target=$($targets[$abi] -replace '-clang\+\+\.cmd$', '')",
        "-shared", "-fPIC", "-O2", "-std=c++17",
        "-fno-threadsafe-statics",
        "-fvisibility=hidden", "-fvisibility-inlines-hidden",
        "-Wall", "-Wextra", "-Werror=format",
        "-I", $source,
        "-static-libstdc++",
        "-Wl,--hash-style=both", "-Wl,-z,max-page-size=16384", "-Wl,-z,relro,-z,now",
        "-Wl,-exclude-libs,ALL", "-Wl,--gc-sections",
        (Join-Path $source "atexit.cpp"),
        (Join-Path $source "main.cpp"),
        (Join-Path $source "dex.cpp"),
        (Join-Path $source "operators.cpp"),
        (Join-Path $source "uid_hook.cpp"),
        (Join-Path $source "uid_broker.cpp"),
        "-llog",
        "-o", (Join-Path $destination "libzygisk_loader.so")
    )

    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed for $abi" }
    Write-Host "Built $abi"
}


