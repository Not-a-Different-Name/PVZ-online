# MOD66 双端编译与完整包 · 2026-10-18

用户要求“编译”。从当前双端工作区构建，保留已有未提交修改，联机版本MOD66、协议1、检查点20；包含关间换位、无尽视角修复及此前累计改动。未提交、推送或覆盖现用安装。

## 完整产物

- [PC完整ZIP](../../dist/PvZ-Online-Build66-Full-20261018-PC.zip)：解压后双击“启动游戏.bat”。包含Release x86程序、游戏资源、中文、bass和微软运行库，不含私人存档。
- [Android完整APK](../../dist/PvZ-Online-Build66-Full-20261018/Android/PvZ-Online-Build66-Full-20261018-arm64.apk)：Android7.0以上、64位ARM；内置完整资源，首次自动准备。
- [双端合包](../../dist/PvZ-Online-Build66-Full-20261018-Both.zip)及[Android说明ZIP](../../dist/PvZ-Online-Build66-Full-20261018-Android.zip)。
- [玩家更新说明](RELEASE-NOTICE-MOD39-66-2026-10-18.md)。交付目录另附累计公告、开发更新日志及逐文件SHA256表。

| 产物 | 字节 | SHA256 |
|---|---:|---|
| PvZ-Online-Build66-Full-20261018-PC.zip | 60,926,009 | `5e2b7cbc400184b1dff719feadf848501e136d769fe6a8b034bd8967d9545300` |
| PvZ-Online-Build66-Full-20261018-Android.zip | 78,907,450 | `bbb8d8b8319c49fb3f573d7e9a0f3e6578ddad4b86bfc55e2648b673a3e0b854` |
| PvZ-Online-Build66-Full-20261018-Both.zip | 139,840,291 | `edb1c48055a05b1d932ca44d965da5cc21101a1daae0c48f686889047792826d` |
| 完整APK | 86,368,395 | `cd5b72f572a10d02d7e0b74ba426c3c8a180d2afad0727f8d1bbef740c702b4a` |

## 本次检查

- PC Release x86、Android API24 arm64原生构建成功；当前源码无新增编译任务，复用已编译且包含本轮修复的产物。Android完整Release APK构建成功；现有Gradle弃用/SDK提示未阻断构建。
- PC从隔离完整副本的中文启动脚本启动，启动时工作目录位于其他目录；标题载入中文资源，窗口800×600客户区1:1截图。测试副本仅调整partner.xml的窗口模式、产品数据目录和注册表键，主程序SHA256与交付一致。读取当前资源及包内运行库；真实玩家userdata未被测试使用。
- Android使用MuMu Android15及独立应用名io.github.pvzonline.full66test20261018，全新安装后自动准备并进入中文标题，不需要手动导入；8个落盘资源逐一SHA256与正式APK及PC资源一致。二次冷启动保留这8个资源的大小和修改时间。
- 正式APK包名io.github.pvzonline.android，versionCode66、版本0.1-online66.c01ab12b-full；v2签名通过，证书与既有MOD63一致。ZIP及全部原生库支持16KB对齐，依赖由包或系统提供；PC附带DLL均为x86。
- 三个ZIP完整性检查通过。PC最终ZIP另行解压后26个文件的SHA256全部匹配。正式APK从隔离测试配置恢复后与提前保留的交付APK完全一致。
- 测试应用已卸载；本轮启动前未运行的模拟器已关闭。现用PC程序仍为MOD63，SHA256维持3e9fe77a8a87e92345cd71881cf7a8d0d76df726de186ac7425afb6a8e6ed7f2；正式Android仍为MOD63，未覆盖安装。

[构建、签名、资源与清单证据](../../android-artifacts/release-build66-20261018/delivery-manifest.json) · [完整性与依赖检查](../../android-artifacts/release-build66-20261018/package-checks.json) · [PC首次启动](../../android-artifacts/release-build66-20261018/pc-first-start.png) · [Android首次启动](../../android-artifacts/release-build66-20261018/android-first-start.png)。

本次是编译、完整打包及首次启动检查。无尽镜头与关间换位专项验收沿用本轮先前记录，见[镜头修复](ENDLESS-CAMERA-FIX-BUILD66-2026-10-18.md)及[关间换位](INTERMISSION-SWAP-BUILD66-2026-10-18.md)。没有新增手机真机触控或六名真人完整长局验收。

## 更新与存档

电脑退出旧游戏后完整解压或覆盖程序文件，保留已有存档；安卓直接覆盖安装，不要卸载旧版。保留既有档案及兼容关间进度，较早版本局中快照不保证兼容，双端草坪快照不可互换。联机请全队统一MOD66。
