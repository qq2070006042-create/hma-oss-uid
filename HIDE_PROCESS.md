# 进程隐藏：HMA 单 ZIP 内置原版 UID 过滤模块

HMA-OSS 的 ZIP 现在**内置原版 UID 过滤模块 `cq-uid-seccomp 0.2.8`**，安装一次即可：

- HMA 模块负责应用列表隐藏、配置和 UI；
- 原版 UID 模块（字节未改）负责 `getpriority(PRIO_USER)` 过滤；
- HMA 的「隐藏运行中的应用」开关决定把哪些目标包写进 UID 的配置。

保留 UID 模块的原始 module id、二进制和 companion，是因为它与春秋检测器由同一作者设计，
检测器认可的是这套原始实现，重写会改变行为。

## 安装

1. 刷 `HMA-OSS-ZYGISK-<version>-release.zip`；
2. 重启；
3. HMA 里给目标应用打开「隐藏运行中的应用（getpriority）」；
4. 等最多 10 秒同步，强制停止并重新打开目标应用。

安装脚本会把内置的 UID 模块部署到：

```
/data/adb/modules/cq_uid_seccomp
```

如果设备上已经装了 UID 模块，安装器会保留现有模块，不覆盖。

## 配置同步

HMA 服务按自身隐藏逻辑写入：

```
/data/misc/hide_my_applist_<随机串>/process_scope
```

HMA 的开机脚本（`hmaoss.sh`）每 10 秒把其中的目标包名同步到：

```
/data/adb/cq_uid_seccomp/config
/data/adb/modules/cq_uid_seccomp/config
```

UID 模块在目标应用启动时按自己的方式安装过滤器；HMA 不参与 seccomp 安装。

## 代码结构

```
zygote/src/main/assets/uid/                 原版 UID 0.2.8 模块文件（未修改）
zygote/src/main/assets/customize.d/45-install-uid-module.sh
                                            安装时部署 UID 模块
zygote/src/main/assets/hmaoss.sh            开机同步 process_scope -> UID config
zygote-runtime/src/main/cpp/main.cpp       仅注入 Java 侧，不含 seccomp 过滤器
zygote/.../HMAService.kt                   writeProcessScope()
app/.../AppSettingsV2Fragment.kt           开关与提示
```

## 构建

```bash
./gradlew :app:assembleRelease
./gradlew :zygote:assembleRelease
```

修改原生代码后：

```powershell
powershell -ExecutionPolicy Bypass -File zygote-runtime/build-native.ps1
```

## 验证

```bash
adb shell su -c 'ls /data/adb/modules'
adb shell su -c 'cat /data/adb/modules/cq_uid_seccomp/config'
adb shell ps -A -o PID,PPID,UID,NAME,ARGS | grep cq_uid_seccomp
```

预期：`cq_uid_seccomp` 模块存在；config 为 `enabled=1` + 目标包名；打开目标应用后出现
`zn-zygisk-companion64 cq_uid_seccomp`。

## 注意

- 目标应用必须重启后过滤器才生效；同步间隔 10 秒。
- 不要重复安装独立的 UID 模块；内置安装器会保留已有模块。
- UID 模块仅 arm64、Android 13+。
- UID 模块来自 UID-（Apache-2.0），许可文件随 `assets/uid/legal` 一起打包。
