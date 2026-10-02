这个分支只放编译产物，不放源码——每次构建覆盖式更新。

下载（两选一）：
  1) 浏览器打开 https://github.com/Not-a-Different-Name/PVZ-online/blob/builds/SexyAppFramework.exe
     点右上 Download
  2) 打包下载（国内通常更通）：
     https://codeload.github.com/Not-a-Different-Name/PVZ-online/tar.gz/refs/heads/builds

用法：
  SexyAppFramework.exe  覆盖到自己 runtime\ 目录下（覆盖前先关掉游戏，
                        并以 runtime\ 为工作目录启动）
  pvz-relay.exe         要自己开中继服务器时运行它，默认监听 27777

build-info.txt 里有这次的构建时间 / 来源提交 / MOD_BUILD / SHA256，
两边对一下 MOD_BUILD 就知道版本配不配套。
