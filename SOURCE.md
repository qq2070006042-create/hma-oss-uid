# Source release — HMA-OSS + UID filter integration

This archive contains the complete corresponding source for:

- `dist/HMA-OSS-ZYGISK-oss-173-release.zip`
- `dist/HMA-OSS-oss-173-release.apk`

## Upstream projects

| Component | Upstream | License |
|---|---|---|
| HMA-OSS | https://github.com/frknkrc44/HMA-OSS , snapshot commit `1b9f98c` | AGPL-3.0 |
| UID filter | https://github.com/longze777/UID- , version 0.2.8, 作者：泷泽 | Apache-2.0 |
| AndroidVMTools | https://github.com/frknkrc44/AndroidVMTools (bundled under `hma/external/AndroidVMTools`) | MIT |
| local_cxa | https://github.com/5ec1cff/local_cxa_atexit_finalize_impl (bundled under `uid/third_party/local_cxa`) | Apache-2.0 |
| Zygisk API header | https://github.com/topjohnwu/zygisk-module-sample (vendored header) | Apache-2.0 |

The HMA portion is a derivative work of HMA-OSS and is distributed under
**AGPL-3.0**. The UID portion is a derivative work of UID- and remains under
**Apache-2.0**. See `LICENSES/` and the upstream `LICENSE` files.

## What was changed

### HMA side (`hma/`)

- `zygote-runtime/src/main/cpp/` — embedded the UID notification engine into
  the HMA Zygisk module:
  - `uid_hook.cpp` / `uid_hook.hpp`: probe the companion for a target scope,
    hook `prctl` in `libandroid_runtime.so`, merge the UID rule into the
    platform seccomp policy, hand the listener to the companion.
  - `uid_broker.cpp`: UID 0.2.8 broker, changed only in the `getpriority`
    response decision. Scope comes from
    `/data/misc/hide_my_applist_*/uid_scope/<target uid>`.
  - `uid_filter.h`, `uid_notification.h`, `uid_fork_events.h`, `policy.h`:
    UID filter implementation headers.
- `zygote/.../HMAService.kt` — computes, for every app with
  `hideProcess` enabled, the hidden UID list and writes:
  - `<dataDir>/process_scope` (package/user/UID list)
  - `<dataDir>/uid_scope/<target uid>` (hidden UIDs for that target)
  The first write is deferred until system providers are ready.
- `common/.../JsonConfig.kt`, `app/.../AppSettingsV2Fragment.kt`,
  `app/src/main/res/xml/app_settings_v2.xml`, `strings.xml` — per-app
  “hide running apps (getpriority)” setting.
- `zygote/src/main/assets/hmaoss.sh` — no longer writes an external UID
  config; the engine reads HMA's own configuration directory.
- Removed the separate `cq_uid_seccomp` module payload; the engine is inside
  `hma_oss_zygisk`.

### UID side (`uid/`)

- `broker.cpp`:
  - reads the target scope from
    `/data/misc/hide_my_applist_*/uid_scope/<uid>`;
  - replies `N` when the target has no scope (so non-targets are not hooked);
  - returns `ESRCH` only for UIDs in the scope and `CONTINUE` otherwise.
- All other UID files (`module.cpp`, `filter.h`, `notification.h`,
  `fork_events.h`, `policy.h`, `exports.map`) are unchanged from 0.2.8.

## Build

See `BUILDING.md`. The release binaries were built with:

- Android NDK `28.0.13004108`
- JDK from Android Studio JBR
- Gradle wrapper in `hma/`

## Source completeness

The archive excludes only build outputs and the generated manager APK; those
are produced from this source by the documented commands. No proprietary
components are required.
