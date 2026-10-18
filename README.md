# HMA-OSS Zygisk + 选择性 UID 过滤

在 [HMA-OSS](https://github.com/frknkrc44/HMA-OSS) 的 Zygisk 模块中内嵌
[UID-](https://github.com/longze777/UID-) 的 `getpriority(PRIO_USER)` 过滤引擎，
按 HMA 的隐藏列表对目标应用逐个 UID 隐藏，而不是把所有应用一律隐藏。

## 作者

- HMA-OSS：frknkrc44
- UID：泷泽
- 集成与维护：月虹

## 特性

- **单模块**：只安装 `hma_oss_zygisk`，不再有独立的 `cq_uid_seccomp` 模块；
- **按 HMA 列表逐 UID**：HMA 为每个目标应用生成 `uid_scope/<目标 UID>`，
  引擎只对名单内 UID 返回 `ESRCH`，名单外 `CONTINUE`；
- **使用 HMA 自己的配置目录**：scope 写在
  `/data/misc/hide_my_applist_<id>/uid_scope/<目标 UID>`，不创建额外目录；
- **保持 UID 过滤器形态**：规则、Hello 握手、companion、fork/策略跟踪沿用
  UID 0.2.8，只改 broker 的返回判断；
- 安装时若 user 0 已有 HMA App，先卸载再安装模块内置的管理器。

## 安装

1. 刷 `HMA-OSS-ZYGISK-oss-173-release.zip`；
2. 重启；
3. 在 HMA 里给目标应用打开「隐藏运行中的应用（getpriority）」；
4. 等 scope 文件生成（开机后约 15–60 秒），强制停止并重开目标应用；
5. 春秋若显示“检测未完成”，强制停止重开重试；多次后完成且无 seccomp 项即正常。

## 构建

见 [BUILDING.md](BUILDING.md)。

## 源码与许可

- HMA-OSS：AGPL-3.0，衍生部分继续 AGPL-3.0；
- UID：Apache-2.0，修改点见 `patches/uid-broker.patch`；
- AndroidVMTools：MIT；
- local_cxa：Apache-2.0。

许可证全文在 `LICENSES/`，修改说明见 [SOURCE.md](SOURCE.md)。

本项目与 HMA-OSS、UID- 原作者无隶属关系；上游版权归各自作者所有。
