# Building from source

## Requirements

- Linux, macOS or Windows
- JDK 17+ (Android Studio JBR works)
- Android SDK with build-tools 36.x
- Android NDK `28.0.13004108` (or set `ANDROID_NDK_HOME`)
- Gradle is provided by the wrapper in `hma/`

## 1. Build the manager APK

```bash
cd hma
./gradlew :app:assembleRelease
```

Copy the APK into the module assets:

```bash
cp app/build/outputs/apk/release/app-release.apk \
   zygote/src/main/assets/manager.apk
```

## 2. Build the native module

The Zygisk module embeds the UID engine. On Windows:

```powershell
$env:ANDROID_NDK_HOME = "C:\path\to\ndk\28.0.13004108"
powershell -ExecutionPolicy Bypass -File zygote-runtime/build-native.ps1
```

On Linux/macOS, compile `zygote-runtime/src/main/cpp` with the same source
list and flags used by `build-native.ps1` (C++17, `-shared`, `-fPIC`,
`-static-libstdc++`, `-Wl,-z,max-page-size=16384`, `-Wl,-z,relro,-z,now`).

The script writes the loader libraries to
`zygote-runtime/src/main/jniLibs/<abi>/libzygisk_loader.so`.

## 3. Build the module ZIP

```bash
cd hma
./gradlew :zygote:assembleRelease
```

Output:

```
zygote/build/outputs/magisk/release/HMA-OSS-ZYGISK-<version>-release.zip
```

## 4. (Optional) Rebuild the UID engine standalone

The engine sources are in `uid/`:

```bash
cd uid
python build.py --module-only
```

`build.py` expects the NDK and the vendored `third_party/` tree. The release
build inside HMA uses the same sources but compiles them into
`libzygisk_loader.so`; it does not install a separate UID module.

## Verification

```bash
adb shell su -c 'ls /data/adb/modules'
adb shell su -c 'ls /data/misc/hide_my_applist_*/uid_scope'
adb shell su -c 'cat /data/misc/hide_my_applist_*/uid_scope/<target-uid>'
adb logcat | grep 'UID filter: merged'
```

Expected: only `hma_oss_zygisk`; one `uid_scope/<uid>` file per target;
`merged via prctl` when a target starts.
