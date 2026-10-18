# zygote-runtime

This is a vendored copy of
[ZygoteLoader](https://github.com/aerath-stuff/ZygoteLoader) at commit
`2a2160f` (the version HMA-OSS previously consumed from Jitpack as
`com.github.aerath-stuff.ZygoteLoader:runtime:2a2160f`).

HMA-OSS substitutes that Jitpack dependency with this project (see the root
`build.gradle.kts`), so the native entry point can be extended without forking
the Gradle plugin.

## What changed compared with upstream

- `src/main/cpp/main.cpp` decides, for every application process, whether the
  package opted into process hiding and prepares the seccomp filter.
- Added `uid_policy.h`, `uid_rule.h`, `uid_hook.*` and `hma_scope.*`, adapted
  from [UID-](https://github.com/longze777/UID-) (Apache-2.0).
- Native libraries are prebuilt into `src/main/jniLibs/<abi>/libzygisk_loader.so`;
  Gradle no longer configures CMake. `build-native.ps1` rebuilds them.
- `CMakeLists.txt` is kept as a reference build description.

The Java side (`src/main/java/com/v7878/zygisk/`) and the installer assets under
`src/main/assets/` are unchanged upstream code.

## Licenses

- ZygoteLoader: see the upstream repository license.
- The UID filter sources adapted from UID- are Apache-2.0; the original license
  text is kept in `third_party/uid/LICENSE`.

## Updating

When upstream ZygoteLoader changes, copy its `runtime/` sources over this
directory again, re-apply the native changes (diff `main.cpp` first), then run
`build-native.ps1` and build the module ZIP.
