# Responder（桌面管理版）

在 Windows 上双击 Server.exe，使用左侧导航管理服务、题库、比赛和设置。题库管理支持按题型筛选，通过题号、题干、答案、选项或线索即时搜索，以及直接新增和编辑题目。首次使用可从本机导入题库。

## 从源码编译

环境：Visual Studio Community 2026（平台工具集 **v145**）、Windows SDK 10.0、C++20。

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'D:\开发区\Responder by STA.slnx' /p:Configuration=Release /p:Platform=x64 /m
```

- `Server\build_updater.exe` 是**编译期预构建工具**（编译时把 `build.h` 版本号 +1），
  必须存在才能编译，但不参与运行。
- 程序为 Windows 桌面子系统：图形模式下不创建控制台窗口；`--console` 模式会优先附着父控制台（无父控制台时新建控制台窗口）并同步输出运行日志。
- 产物默认动态链接 VC++ 运行库：**目标机若没装过 Visual Studio，需先安装
  Microsoft Visual C++ 2015-2022 Redistributable (x64)**，否则会提示缺少
  `MSVCP140.dll` / `VCRUNTIME140.dll`。

> 面向开发者的完整说明（架构、接口、数据格式、已知问题、运维手册）见
> [`开发者交接文档.md`](开发者交接文档.md)。

## 打包部署

完成 Release x64 编译后，在仓库根目录运行 `./publish.ps1`。脚本检查本地资源引用和 12 MB 体积上限，生成 `Responder-x64.zip`。解压后保留 `Server.exe` 与 `assets` 的相对位置即可启动。

## 源码结构（关键文件）

| 文件 | 职责 |
|---|---|
| `main.cpp` | 入口：切换工作目录、初始化日志、按参数进 GUI 或命令行模式 |
| `gui.cpp` | Win32 桌面窗口：上线/下线控制、题库管理按钮、日志显示 |
| `logging.h/.cpp` | 全局日志：接管 `std::cout`，同时写入 `server.log` 与窗口 |
| `server_core.h` / `Server.cpp` | HTTP 服务核心：路由注册、题库加载、启动/停止/状态查询 |
| `problem_pages.cpp` | 网页版题库管理页（浏览器内的同一套功能） |
| `responder_*.{h,cpp}` | 页面框架、请求基类、比赛与题目模型 |
| `httplib.h` / `json.hpp` | 第三方单头库 |

