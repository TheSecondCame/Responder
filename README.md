# Responder by STA

| 项目 | 内容 |
|---|---|
| 产品名 | Responder（知识竞赛组织系统） |
| 版本 | 2.0 Beta 5 · Build 526。发布分支为 `main`，桌面版源码基线为 `7dfeda0`。 |
| 开发方 | 成都七中科学技术协会（STA） |
| 当前形态 | 后端电脑运行 Win32 蓝白管理窗口，前端电脑通过局域网浏览器主持比赛、展示大屏。 |
| 技术栈 | C++20 · cpp-httplib 0.30.1 · nlohmann/json · Win32 API · Bootstrap 5 前端 |
| 文档日期 | 2026-11-14 |
| 文档定位 | 当前仓库的 `README.md` 与 `开发者交接文档.md` 内容相同，涵盖使用、开发与现场运维。 |

---

## 目录

| 章节 | 内容 | 何时看 |
|---|---|---|
| [0. 五分钟速览](#0-五分钟速览) | 接手必知的五件事 | **先看这个** |
| [1. 交接范围与仓库现状](#1-交接范围与仓库现状) | 两个目录的关系、缺失资产 | 第一次接手 |
| [2. 环境与依赖](#2-环境与依赖) | VS / SDK / 运行库 / 第三方库 | 配环境 |
| [3. 构建与发布](#3-构建与发布) | 编译命令、构建号、部署清单 | 出包 |
| [4. 架构总览](#4-架构总览) | 进程/线程模型、模块分层 | 理解全局 |
| [5. 源码导读](#5-源码导读按文件) | 每个文件干什么、关键行号 | 改代码前 |
| [6. 路由与接口契约](#6-路由与接口契约) | 17 条路由、参数、鉴权响应 | 调接口 |
| [7. 数据模型与文件格式](#7-数据模型与文件格式) | 题目/题库/比赛 JSON 与 txt 格式 | 处理数据 |
| [8. 核心业务流程](#8-核心业务流程) | 登录、比赛、题库运维 | 理清逻辑 |
| [9. 前端](#9-前端) | 资源清单、大屏脚本 | 改页面 |
| [10. 开发指南](#10-开发指南) | 改界面落点、四条铁律、验证步骤 | 动手前 |
| [11. 已知问题与技术债](#11-已知问题与技术债) | P0/P1/P2 分级问题 | **改代码前必读** |
| [12. 现场运维手册](#12-现场运维手册) | 赛前 Checklist、排障表 | 现场支持 |
| [13. 后续路线建议](#13-后续路线建议按性价比排序) | 优先级排序的改进项 | 定计划 |
| [附录 A–D](#附录-a源文件清单) | 文件清单、命令、术语、版本演进 | 速查 |

---

## 0. 五分钟速览

如果你只有五分钟，记住这五件事：

1. 从 [TheSecondCame/Responder](https://github.com/TheSecondCame/Responder) 的 `main` 分支检出源码。本文命令中的 `D:\开发区` 是原开发机工作路径，可替换成自己的检出目录。
2. 仓库提供解决方案、源码、根目录的 `Server.exe` 和 `assets\`。现场题库、比赛记录、桌面偏好与运行日志保存在部署机上，首次部署可导入题库或手动录题。
3. **编译入口**是 `D:\开发区\Responder by STA.slnx`（只含一个工程 `Server`），VS 2026 / 工具集 `v145` / C++20。
4. **产物**是 `x64\Release\Server.exe`，需要**复制到 `D:\开发区\` 根目录**才能与 `assets\`、`data\` 相邻运行。
5. **程序把工作目录设成"exe 所在目录"**（实现见 `main.cpp:16`，给 exe 路径追加 `\..`），所以 exe 必须和 `assets\`、`data\` 同级，**不能单独拷走**。

---

## 1. 交接范围与仓库现状

### 1.1 两个目录的关系（重要）

| 路径 | 内容 | 状态 |
|---|---|---|
| `D:\开发区` | 当前工作副本及本机运行数据。版本历史见 GitHub `main`。 | 开发与现场部署 |
| `D:\Responder` | 最初的工作目录：518 原版源码、525 原始包解压目录 `_525`、原始 `responder最终版.rar`/源码 zip、旧 exe、早期的清理记录 | 📦 **历史留档，勿改** |

> `D:\Responder\README.md` 记录的是早期归档清理过程（目录布局已过时），**不能当作当前说明**。

### 1.2 开发机目录示例（题数与场次数为历史快照）

```
D:\开发区\
├─ Server.exe                       ★ 唯一可执行程序（桌面管理 + 后端）
├─ server.log                       运行日志（追加写入，不轮转）
├─ assets\                          前端静态资源（339 文件，挂载为 /assets）
│   ├─ css\  js\  fonts\  img\  sound\  vendor\
├─ data\
│   ├─ problems\                    现役题库：1038 个文件
│   │   ├─ choice_*.json            488 道选择题
│   │   ├─ completion_*.json        278 道填空题
│   │   ├─ info_*.json              271 道信息题
│   │   └─ problem_list.json        题库索引（含"已用"标记）
│   ├─ desktop.ini                  桌面日志滚动偏好，首次更改设置后写入
│   ├─ problems_backup\             题库操作前的自动备份（同结构）
│   ├─ competitions\                6 场比赛，每场一个 GUID 目录
│   └─ upload_tmp\                  网页/窗口导入题库的临时目录（用完即删）
├─ Server\                          源码（VS 工程）
├─ Responder by STA.slnx            解决方案（只含 Server 一个工程）
├─ 归档\
│   ├─ Problems Generator*.exe      已并入 Server 的旧生成器
│   └─ UI截图\                      界面验收截图（online/offline）
└─ Responder by STA ... 525 Source Code.zip   原始源码包快照
```

### 1.3 缺失的工程资产（接手需补）

| 缺失项 | 影响 | 建议 |
|---|---|---|
| 运行数据备份 | Git 仓库不包含现场题库和比赛结果 | 另行备份部署机的 `data\` |
| 自动化测试 | 改题库逻辑靠手测 | 优先给 `problem_list::convert/merge/delete_duplicates` 加单测 |
| 原始需求/设计文档 | 只有代码和 README | 本文档即最小设计说明 |
| 部署脚本 | 手工复制 exe | 可加 `build.ps1` |

---

## 2. 环境与依赖

| 依赖 | 版本/要求 | 说明 |
|---|---|---|
| Visual Studio | **2026 Community**（本机 `C:\Program Files\Microsoft Visual Studio\18\Community`） | 项目工具集为 **v145**，低于此版本的 VS 需改 `PlatformToolset` |
| Windows SDK | `10.0.26100.0` | `WindowsTargetPlatformVersion=10.0` |
| C++ 标准 | `stdcpp20` | 用到了 `std::atomic`、`std::thread`、`std::streambuf` 重写等 |
| 字符集 | Unicode | 全项目 `/utf-8` 编译，源文件均为 UTF-8 |
| 第三方库 | `httplib.h`（0.30.1）、`json.hpp`（nlohmann） | 均为单头文件，已随源码提交，无需包管理 |
| 系统库 | `ws2_32`、`shell32`、`ole32`、`comsuppw` | 通过 `#pragma comment` + 工程 `AdditionalDependencies` 链接 |

> 运行时**不依赖 .NET、不依赖第三方库**，但**依赖 VC++ 运行库**：
> 实测 exe 动态导入了 `MSVCP140.dll`、`VCRUNTIME140.dll`、`VCRUNTIME140_1.dll`
> 和 UCRT（`api-ms-win-crt-*`）。目标机没装过 VS 时，需先安装
> **Microsoft Visual C++ 2015-2022 Redistributable (x64)**；
> 或者在工程里把 C/C++ → Code Generation → Runtime Library 改成
> **`/MT`（多线程静态）** 后重编译，得到一个真正零依赖的 exe。

---

## 3. 构建与发布

### 3.1 编译

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'D:\开发区\Responder by STA.slnx' /p:Configuration=Release /p:Platform=x64 /m
```

产物：`D:\开发区\x64\Release\Server.exe`（另生成 `.pdb`，可不发布）。

**编译前必读的坑：**

1. **`Server\build_updater.exe` 必须存在**。工程有一个预构建步骤执行
   `"$(SolutionDir)Server\build_updater.exe"`，它会把 `Server\build.h` 的
   `PROGRAM_BUILD` 自增 1。文件缺失会报 `MSB3073 ... 已退出，代码为 9009`。
   若被误删，用源码重建：
   ```powershell
   cmd /c ""C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && ^
     cl /nologo /EHsc /std:c++20 /Fe:D:\开发区\Server\build_updater.exe ^
     /Fo:%TEMP%\bu.obj D:\开发区\Server\build_updater.cpp"
   ```
2. 仅验证改动、需要保留构建号时，可向上述 MSBuild 命令追加 `/p:PreBuildEventUseInBuild=false`。本次桌面版以 x64 Release、Build 526 验证。
3. **不要删除 `日志`/`截图` 之外的构建产物后只提交 exe**：`x64\` 是中间目录，可整体加入 `.gitignore`。
4. 只有 **x64 Release** 经过验证。`x86`、`ARM64` 配置在 vcxproj 里存在但**未验证**，其中 `x86` 还缺 `WIN32` 相关宏的一致处理，使用前需实测。

### 3.2 发布（现场部署清单）

```
发布目录/
├─ Server.exe            ← 从 x64\Release\ 复制
├─ assets\               ← 必须完整（缺了页面无样式/脚本）
└─ data\                 ← 题库与比赛数据
    ├─ problems\         首次运行自动建目录；导入或手动新增后生成索引
    └─ competitions\
```

- `README.md` 可一并带上。
- **目标机若未装过 Visual Studio，需要先装 VC++ 2015-2022 运行库（x64）**，
  否则双击会提示缺 `MSVCP140.dll` / `VCRUNTIME140.dll`。
- **首次在目标机运行会触发 Windows 防火墙提示，必须点"允许"**，否则局域网内其他设备访问不到。
- `server.log` 会在首次启动时自动生成。

---

## 4. 架构总览

### 4.1 进程与线程模型

```
┌──────────────────────────── Server.exe（单进程）────────────────────────────┐
│                                                                             │
│  main.cpp                                                                   │
│    ├─ SetupWorkingDirectory()   工作目录 = exe 所在目录                     │
│    ├─ EnableDpiAwareness()      系统 DPI 感知                                │
│    ├─ logging::setup()          接管 std::cout/cerr → server.log + 窗口     │
│    └─ server_core::initialize() 建目录、加载题库、注册 17 条路由、生成 PIN   │
│                                                                             │
│  ┌─ GUI 主线程（默认模式）────────────┐   ┌─ 服务线程 ─────────────────┐   │
│  │ Win32 消息循环（100ms 轮询）        │   │ httplib::Server::listen()  │   │
│  │  · 刷新状态/日志/存量               │   │  · 线程池 ≥8 个 worker     │   │
│  │  · 按钮：上线/下线/题库操作         │   │  · 每个请求在 worker 中处理 │   │
│  │  · 题库操作另起 detached 线程       │   │                            │   │
│  └────────────────────────────────────┘   └────────────────────────────┘   │
│                                                                             │
│  --console 模式：跳过 GUI，直接在前台 listen()                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**关键并发事实（务必理解）：**

- HTTP 请求由 **≥8 个 worker 线程并发处理**（httplib 默认 `max(8, cores-1)`）。
- 但**每个路由只有一个处理器对象实例**（`Server.cpp:38-61` 的文件级静态对象），
  请求参数 `params`/`body`/`req_form` 是对象成员 —— 两个并发请求命中同一路由时
  会互相覆盖参数。这是**已知竞态**，见 §11-P0-1。
- `problem_bank_mutex` 保护进程内共享题库的桌面读写、浏览器题库操作和比赛抽题。批量操作持锁期间，抽题可能等待。该锁不保护路由处理器的请求参数，也不能协调两个独立进程对同一数据目录的写入。

### 4.2 模块分层

| 层 | 文件 | 职责 |
|---|---|---|
| 入口 | `main.cpp` | 工作目录、DPI、日志、模式分发 |
| 控制 | `server_core.h` / `Server.cpp` | 服务生命周期、路由注册、题库加载 |
| 桌面 UI | `gui.cpp`、`question_editor.rc`、`question_editor_ids.h` | 四栏导航、服务控制、比赛记录、题目浏览与编辑、桌面偏好 |
| 基础设施 | `logging.*`、`functions.*`、`encoding.*` | 日志重定向、目录操作、编码/URI/GUID 工具 |
| 请求框架 | `responder_request.*`、`responder_template.*` | 基类、鉴权、表单解析、HTML 外框 |
| 业务页面 | `main_pages.cpp`、`competition_pages.cpp`、`problem_pages.cpp` | 登录/主页、比赛管理与大屏、题库管理 |
| 领域模型 | `responder_problem.*`、`responder_competition.*` | 题目/题库、比赛/队伍/比分 |
| 前端 | `assets\` | Bootstrap 模板 + 大屏交互脚本 |

---

## 5. 源码导读（按文件）

> 下文旧模块的行号和规模沿用早期交接快照，代码定位请以类名或函数名为准。

### 5.1 入口与控制层

| 文件 | 行数 | 关键点 |
|---|---|---|
| `main.cpp` | 67 | `SetupWorkingDirectory()`:16 · `EnableDpiAwareness()`:28 · 命令行解析 `--console`:44 · `logging::setup(console?0:1)`:54 |
| `server_core.h` | 43 | `PORT = 8087`:14 · `initialize/start/stop/is_running/last_error`:18-31 · `authentic_pin` / `authentic_token` 外部声明:35,38 |
| `Server.cpp` | 190 | 全局 `pl`/`pl_unused`:18 · `authentic_*`:21 · **处理器静态对象**:38-61（内含生命周期警告注释）· `register_handlers()`:64 · `initialize()`:105 · `start()`:142（含绑定失败处理）· `stop/is_running/last_error`:173-189 |

### 5.2 桌面 UI

| 分栏 | 操作 | 维护入口 |
|---|---|---|
| 服务管理 | 上线、下线、打开浏览器，查看本机与局域网地址、轮换后的 PIN 和运行日志 | `ApplyServerState`、`DoStart`、`DoStop`、`AppendLog` |
| 题库管理 | 浏览和搜索题目、编辑或新增单题，或执行转换、合并、去重和备份 | `RefreshBank`、`OpenQuestion`、`SaveQuestion`、`RunProblemOperation` |
| 比赛管理 | 从本机读取名称、状态和队伍数，可手动刷新；服务上线后可打开浏览器管理比赛 | `RefreshMatches` 读取 `data/competitions/list.json` 和各场的 `competition.json` |
| 设置 | 切换日志是否自动滚动，重启后沿用上次选择 | `data/desktop.ini` 的 `[Window]` / `FollowLog`，默认开启 |

主窗口的布局、蓝白配色和字体在 [gui.cpp](Server/gui.cpp) 中维护。侧栏和操作按钮通过 `ButtonInput` 跟踪光标与按压状态，`AnimateButton` 按需启动定时器，`DrawButton` 绘制悬停渐变和按压回弹。支持空格键，关闭 Windows 客户区动画后直接显示最终状态。

题目编辑弹窗使用 [question_editor.rc](Server/question_editor.rc) 定义布局，[question_editor_ids.h](Server/question_editor_ids.h) 定义控件编号。`QuestionDialog` 处理输入和保存。新增或调整字段时，同时检查资源、校验和存盘逻辑。

### 5.3 请求框架

| 文件 | 行数 | 关键点 |
|---|---|---|
| `responder_request.h` | 66 | `responder_request`（基类，含 `params`/`body`/`req_form`）:13 · `responder_page`（只注册 GET）:32 · `responder_post_request`（注册 POST，返回裸字符串）:50 · `responder_post_page`（注册 POST + 模板渲染）:58 |
| `responder_request.cpp` | 229 | `parse_form_urlencoded()`:15（**本次新增**，原版缺失）· `generate_random_token`:56 · Cookie/token 校验:73-105 · 三个 `set_handler`:119/165/199（均含鉴权） |
| `responder_template.cpp` | 172 | `output_html_code()`:40 起：页头/顶栏/面包屑/页脚；顶栏导航含「主页 / 题库管理 / 退出」 |
| `responder_pages.h` | 140 | 全部 17 个页面/处理器类的声明 |

> **新增一个页面的标准动作**：在 `responder_pages.h` 声明类（选对基类）→ 在对应 `.cpp`
> 实现 `init()`（设 `address`/标题/`is_request_login`）与 `generate_page()` →
> 在 `Server.cpp` 的文件级静态区加对象 → 在 `register_handlers()` 里 `set_handler` →
> 在 `Server.vcxproj` 加源文件（仅新文件需要）。

### 5.4 业务页面

| 文件 | 行数 | 关键点 |
|---|---|---|
| `main_pages.cpp` | 291 | 欢迎页:17 · 登录页 `/start`:41 · `login_handler`:112（**PIN 校验 + 轮换**，118 行有"防暴力破解"TODO）· 主页 `/main`:144 · 登出 `/logout`:279 |
| `competition_pages.cpp` | 1544 | 新建:17 · 新建处理:198 · 编辑:236 · 编辑处理:447 · 删除:500 · 详情:571 · **大屏展示页:784** · 计分接口:1203 · 抽题接口:1256 · 结束接口:1499 |
| `problem_pages.cpp` | 393 | 题库管理页 `/main/problems`:108 · 操作处理器:234（convert / merge / dedup / backup 四个 action）· 常量 `PROBLEM_DIR`/`BACKUP_DIR`/`UPLOAD_FILE`:19-23 · `backup_problems()`:86 · `reload_problem_list()`:75 |

### 5.5 领域模型

| 文件 | 行数 | 关键点 |
|---|---|---|
| `responder_problem.h` | 102 | `problem_completion`（基类）:16 · `problem_choice`（+options）:27 · `problem_info`（+info 线索）:35 · `problem_list`（三个 `vector<pair<T,int>>`，int=已用标记）:43 · `delete_duplicates()`:52 · `count_unused()`:54-69 · `convert/merge`:71-72 · `remove_used()`:87 |
| `responder_problem.cpp` | 517 | `delete_duplicates()`:**166-211** · `convert()`:**213**（**304 行调用查重**）· `merge()`:**324**（判重在 383/412/446）· `output_to_folder()`:**486** |
| `responder_competition.h` | 50 | `competition` 类：`teams`（队伍名+按键字符）:26 · `team_scores`:27 · `status` 枚举 `NOT_STARTED/ONGOING/PAUSED/FINISHED`:29-35 · `problems`（本场已用题）:41 · `generate_guid()`:46 |
| `responder_competition.cpp` | 123 | 比赛列表读写:9 · `load/save`:约 20 起 · GUID 生成:约 100 |

### 5.6 基础设施

| 文件 | 行数 | 关键点 |
|---|---|---|
| `logging.h` | 81 | `append/drain/snapshot/clear/setup`:16-31 · `LogStreamBuf`（按行切分的自定义 `streambuf`）:36 |
| `logging.cpp` | 71 | 全局互斥 + 行缓冲 + `server.log`（`ios::app`）+ 隐藏控制台:约 40 |
| `functions.h` | 39 | `RemoveDir`:13 · `CopyDir`（**本次新增**，原在 Generator 里）:14 · `get_ip_list`:15 · `RandomGenerator` 单例:17 |
| `functions.cpp` | 93 | `RemoveDir`:约 13 · `CopyDir`:约 40 · `get_ip_list`:约 72（Winsock，已 `#pragma comment` 链接 `ws2_32`） |
| `encoding.h/.cpp` | 55/430 | Hex / URI / GUID / GB2312↔UTF-8 / CRC32 工具。**注意**：题库读写路径不依赖其中的转码函数 |
| `build.h` | 13 | `PROGRAM_BUILD 526`（编译时自增，见 §3.1） |
| `build_updater.cpp` | 43 | 构建期工具：读 `build.h` 的版本号写回 +1 |

---

## 6. 路由与接口契约

### 6.1 页面路由（GET）

| 路由 | 类 | 需登录 | 说明 |
|---|---|---|---|
| `/` | `welcome_page` | 否 | 欢迎页 |
| `/start` | `start_page` | 否 | PIN 输入页（已登录会跳 `/main`） |
| `/main` | `main_page` | **是** | 比赛列表 + 题库存量概览 + 新建比赛入口 |
| `/main/competition/new` | `new_competition_page` | 是 | 新建表单 |
| `/main/competition/edit` | `competition_edit_page` | 是 | 编辑表单（`?guid=`） |
| `/main/competition/detail` | `competition_detail_page` | 是 | 比赛详情（`?guid=`） |
| `/main/competition/display` | `competition_display_page` | 是 | **大屏/主持页**（`?guid=`） |
| `/main/problems` | `problem_manage_page` | 是 | 题库管理页 |
| `/logout` | `logout_page` | 是 | 清 Cookie 并跳 `/` |

> `responder_page::set_handler` 会自动为路由补一个 `/xxx/` 的 302 重定向。

### 6.2 动作接口（POST）

| 路由 | 处理器 | 参数 | 说明 |
|---|---|---|---|
| `/start` | `login_handler` | `pin` | 校验 6 位 PIN；成功则**轮换 PIN**并下发 50 位 token（JS 写 Cookie） |
| `/main/competition/new` | `new_competition_handler` | `name` `description` `choiceCount` `completionCount` `infoCount` | 创建比赛，返回 JS 跳转 |
| `/main/competition/edit` | `competition_edit_handler` | 同上 + `guid` | 修改基础信息 |
| `/main/competition/delete` | `competition_delete_handler` | `guid` | 删除比赛目录 |
| `/main/competition/display/problem` | `..._problem_handler` | `guid` `type` `complexity` `requestcomplexity` | **抽题**：type 0/1/2；返回 HTML 片段；无可用题返回 `fail : no unused ...` |
| `/main/competition/display/scores` | `..._scores_handler` | `guid` `team` `score` | **改分**：team 为队伍按键字符 |
| `/main/competition/display/enddisplay` | `..._enddisplay_handler` | `guid` `action` | `action=1` 结束比赛（`FINISHED`），否则暂停（`PAUSED`） |
| `/main/problems` | `problem_manage_handler` | `action`=`convert\|merge\|dedup\|backup` + `file`(multipart) | 题库操作，返回结果页 |

> 注意：`/main/competition/display` **没有独立 POST 路由**。比赛从"未开赛"进入"进行中"
> 是在 **GET 渲染大屏页时**完成的（`competition_pages.cpp:787` 设 `is_request_login`，
> 同函数内 `c.competition_status = competition::ONGOING;`）。
> 也就是说，登录后只要打开大屏页，这场比赛就变为进行中。

**鉴权失败响应差异**（排查时有用）：
- GET 页面未登录 → 返回一段清 Cookie + 跳 `/` 的 `<script>`（内容 130 字节左右）。
- POST 未登录 → 纯文本 `fail : unauthorized`。
- 比对方式是 Cookie `token` 与内存 `authentic_token` 全等。

### 6.3 静态资源

`svr.set_mount_point("/assets", "assets")`（`Server.cpp:122`）——直接映射磁盘目录，
**所以发布时必须带 `assets\`**。前端通过 `/assets/...` 引用。

---

## 7. 数据模型与文件格式

> 所有 JSON 均为 **UTF-8 无 BOM**。用 PowerShell 读时注意 `-Encoding UTF8`，
> 否则中文会显示为乱码（文件本身没问题）。

### 7.1 题目文件（`data\problems\{type}_{id}.json`）

```jsonc
// 选择题 choice_1.json
{"answer":"D","complexity":1,"content":"题干","id":1,"options":["A","B","C","D"]}

// 填空题 completion_1.json
{"answer":"3","complexity":2,"content":"题干","id":1}

// 信息题 info_1.json（info 为 4 条线索，逐条展示）
{"answer":"DDoS攻击","complexity":2,"content":"请根据以下线索…","id":1,
 "info":["①…","②…","③…","④…"]}
```

| 字段 | 含义 |
|---|---|
| `id` | 同类型内从 1 连续编号；**= 文件名后缀**，三者必须一致 |
| `complexity` | 难度 1–3（0 视为未定义）；轮答按难度抽题时依赖它 |
| `answer` | 选择题为 `A/B/C/D`；其它题型为文本 |
| `options` | 固定 4 个选项 |
| `info` | 固定 4 条线索 |

### 7.2 题库索引（`data\problems\problem_list.json`）

```json
{"choice_problems":[[1,1],[2,0],…],
 "completion_problems":[[1,0],…],
 "info_problems":[[1,0],…]}
```

每项是 `[题目 id, 已使用标记]`，标记 `0`=未用、`1`=已用（比赛抽过）。
**比赛抽题只从"未用"里选**，这也是主页显示"剩余/总数"的依据。

### 7.3 比赛数据（`data\competitions\`）

```
competitions\
├─ list.json              {"competitions":["<GUID>", …]}
└─ <GUID>\
    ├─ competition.json   基本信息
    ├─ problems.json      本场抽到的题（[id, 1] 形式）
    └─ scores.json        各队比分（**纯文本，每行一个整数，不是 JSON**）
```

`competition.json` 示例：

```json
{"choice_count":45,"completion_count":30,"info_count":0,
 "competition_status":3,"description":"5.11",
 "name":"5.11 高一A组",
 "teams":[["高一8班",65],["高一11班",66],["高一12班",67]]}
```

| 字段 | 说明 |
|---|---|
| `competition_status` | `0` 未开赛 · `1` 进行中 · `2` 暂停 · `3` 已结束 |
| `teams` | `[队伍名, 按键字符 ASCII]`；界面按 `64+i` 自动分配 `A/B/C…` |
| `scores.json` | 行序与 `teams` 一一对应，读取时按行解析 |

### 7.4 导入用的 txt 格式

```
复杂度 类型          ← 两个整数：复杂度 1-3；类型 0=选择 1=填空 2=信息
题干（单行）
选项1 / 线索①
选项2 / 线索②
选项3 / 线索③
选项4 / 线索④
答案
<空行分隔下一条>
```

- 选择题与信息题**必须 4 行**选项/线索；填空题**只有 1 行**答案。
- 复杂度越界会自动修正为 1；解析失败会 `Skipping line` 并继续。
- 文件编码：UTF-8（与库内一致；**不做转码**，字节原样入库）。

---

## 8. 核心业务流程

### 8.1 登录与鉴权

```
启动 → 生成 authentic_token(50 位) + authentic_pin(6 位数字)
     → 控制台/窗口显示 PIN
浏览器 /start 输入 PIN → POST /start?pin=xxxxxx
     ├─ 匹配 → 轮换 PIN（旧 PIN 立即失效）→ 下发 token（JS 写 Cookie）
     └─ 不匹配 → 弹回 /start
后续请求 → 携带 Cookie token → 与内存 token 全等则放行
```

- token 只在启动时生成一次，**不随登录轮换**；PIN 每次登录都轮换。
- `is_request_login` 为 false 的只有 `/`、`/start`。

### 8.2 比赛生命周期

```
新建（NOT_STARTED）→ 详情页点「开始比赛」→ /display?guid=… （置 ONGOING）
   ├─ 抢答模式：按键 A/B/C 抢答 → 显示答案 → 判分
   ├─ 轮答模式：按队伍顺序轮流作答，可按难度抽题
   └─ 结束：/display/enddisplay?action=1 → FINISHED（action 缺省则 PAUSED）
```

大屏页前端行为（`assets\js\competition_display.js`，60 KB）：

- 每秒 `synchronization()` 刷新时钟；
- `IsAutoScoring`、`IsAutoComplexity`、`IsRoundFlowRunning`、`ResponderTimerMode` 控制自动计分、难度一致、轮答流程、计时模式；
- 抽题/计分/结束都通过上表 POST 接口；
- 计时到点触发 `ring.wav` / `timer.wav`（`assets\sound\`）。

### 8.3 题库批量操作

| 入口 | 转换 | 合并 | 查重 | 备份 |
|---|---|---|---|---|
| 桌面窗口 | ✅ | ✅ | ✅ | ✅ |
| 网页 `/main/problems` | ✅ | ✅ | ✅ | ✅ |
| 原命令行 Generator | ⛔ 已移除（exe 留档在 `归档\`） | | | |

**转换（convert）**：读 txt → 内存构建 → **自动去重**（`responder_problem.cpp:304`）→
`RemoveDir(data/problems)` → 重写全部文件，ID 从 1 重排。
**合并（merge）**：读入现有题库 + txt 新题，按「题干 + 选项/线索 + 答案」判重后追加，ID 接续。
**查重（dedup）**：对现有题库去重 + 连续重排 ID。
**备份**：`data/problems → data/problems_backup`（覆盖式）。

桌面和网页的批量修改均先调用备份逻辑。备份目录只保留最近一次副本。

### 8.4 桌面查题与单题保存

1. 在“题库管理”选择题型，再输入关键词。题号按完整数字串匹配；题干、答案、选项及线索按子串匹配，英文忽略大小写。清空搜索保留所选题型。列表下方显示当前结果的未用、已用数量。
2. 双击结果或点击“编辑选中题”。列表用“题型 + 题号”定位记录，同一题号可出现在不同题型中。编辑时题型固定，编号与已用标记保留。“新增题目”允许选择题型，新题按该类型的连续编号追加，标记为未用。
3. 填写题干、难度和答案；选择题需填四个选项，信息题需填四条有序线索。选择题接受旧库的文本答案，单个小写 a–d 保存时转成大写。取消会丢弃未保存的输入。
4. `SaveQuestion` 持锁核对编辑快照，准备新题库和未用题池，并备份现有题库。题目先写同目录临时文件，再替换正式文件；新增题还需最后替换索引。索引替换报错时会尝试删除刚发布的新题。磁盘写入成功后才更新内存，修改可用于后续抽题。整库转换仍走 §8.3 的目录重写流程。

---

## 9. 前端

| 资源 | 说明 |
|---|---|
| `assets\vendor\` | Bootstrap 5 / bootstrap-icons / boxicons / jQuery 3.6.3 / SweetAlert / Notyf / Swiper / AOS / Isotope / Waypoints / GLightbox / PureCounter |
| `assets\css\style.css` | 模板样式（BizLand 改造） |
| `assets\css\competition_display.css` | 大屏页专用（计分卡、计时器） |
| `assets\js\competition_display.js` | **大屏页核心逻辑**（60 KB），与后端接口强耦合 |
| `assets\js\main.js` | 模板滚动/导航等装饰性脚本 |
| `assets\sound\` | `ring.wav`（抢答铃）、`timer.wav`（计时） |
| `assets\img\` | 模板配图（多数与业务无关，可裁剪） |

页面 HTML **不存在于磁盘**——全部由 C++ 字符串拼接生成（见 `*_pages.cpp`），
只有 `assets\` 是静态文件。改样式优先改 CSS；改结构必须改 C++ 重新编译。

---

## 10. 开发指南

### 10.1 改界面的落点

- **桌面窗口**：`gui.cpp` → `LayoutControls()` 改坐标，`theme` 命名空间改配色，
  `ButtonInput` / `AnimateButton` / `DrawButton` 改交互动效，`CreateFonts()` 改字号；尺寸统一走 `S()` 做 DPI 缩放。
- **网页页面**：对应 `*_pages.cpp` 里的 `generate_page()`（HTML 字面量），或 `assets\css`。

### 10.2 必须遵守的四条约定（都是踩过的坑）

1. **处理器对象只能放文件级静态区**（`Server.cpp:38-61`）。
   httplib 的 lambda 用 `this` 捕获对象；放进局部作用域会导致注册函数返回后对象析构，
   请求时访问已释放内存 —— 现象是**连接被重置（ECONNRESET）**，日志被写坏。
2. **POST 且要渲染页面 → 继承 `responder_post_page`**。
   `responder_page::set_handler` 只注册 GET；用它做表单处理器会得到 **404**。
3. **表单提交的参数来自 `params`（已由基类解析）或 `req_form`（multipart）**。
   原版只读查询串，`application/x-www-form-urlencoded` 的表单体取不到值（已修，见
   `responder_request.cpp:15`）。新增处理器时二选一，别自己再解析一遍。
4. **`std::cout` 已被全局接管**（`logging::setup`），输出会进窗口与 `server.log`。
   不要用 `std::cout` 输出二进制/大段数据；调试信息直接用 `std::cout` 即可在窗口看到。

### 10.3 验证改动的最小步骤

```powershell
# 1) 编译
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'D:\开发区\Responder by STA.slnx' /p:Configuration=Release /p:Platform=x64 /m
# 2) 部署
Copy-Item 'D:\开发区\x64\Release\Server.exe' 'D:\开发区\Server.exe' -Force
# 3) 运行（默认 GUI；调试服务逻辑可用 --console 看控制台）
& 'D:\开发区\Server.exe'
# 4) 浏览器 http://127.0.0.1:8087 用窗口 PIN 登录，走一遍改动点
# 5) 看日志
Get-Content 'D:\开发区\server.log' -Tail 50 -Encoding UTF8
```

桌面版回归使用独立临时题库，已验证三种题型的新增、编辑、取消、筛选和重启读取，编辑已用题后保留标记，索引无法替换时撤回新增题。搜索覆盖题号、中文题干、英文选项和答案、信息题线索、空结果、清空及从结果打开编辑器。复查界面时另测侧栏切换、鼠标与空格键反馈、日志偏好重启持久化。

**调试陷阱**：若程序"启动了但页面打不开"，先确认**端口上的是不是你刚启动的那个进程**：

```powershell
Get-NetTCPConnection -State Listen -LocalPort 8087 | Select-Object OwningProcess
Get-Process -Id <PID> | Select-Object Path, StartTime
```

本机曾出现旧实例残留占端口，导致新版静默退出、请求全打到旧进程（旧版程序忽略
`listen()` 返回值，端口占用时**不报错直接退出**；新版本已改为明确报错 + 退出码 1）。

---

## 11. 已知问题与技术债

### P0 —— 影响正确性，建议尽快处理

| # | 问题 | 现象/影响 | 位置 | 建议修法 |
|---|---|---|---|---|
| 1 | **处理器对象共享 + 多线程 = 参数竞态** | 两个请求同时命中同一路由时，`params`/`body` 互相覆盖，可能返回别人的数据或错乱 | `responder_request.h:17-22`；`responder_request.cpp:119/165/199` | 把 `params/body/req_form` 改为**每次请求的局部上下文**，作为参数传给 `generate_page()`；或至少加互斥锁（会串行化，降低并发） |
| 2 | **长时间题库操作会阻塞抽题** | 全库备份或批量写入时会持有题库锁 | `problem_bank_mutex`、`RunProblemOperation`、`SaveQuestion` | 大规模导入和去重宜安排在赛前 |
| 3 | **`delete_duplicates()` 丢失"已使用"标记** | 同一重复组 ≥3 条且"已用"那条在较后位置时，标记随条目删除，该题可能被再次抽到 | `responder_problem.cpp:166-211` | 先扫整组做 `|=` 聚合到保留项，再统一删除；或对整组排序优先保留已用项 |
| 4 | **`convert` 是破坏性操作** | `RemoveDir` 后重写；中途崩溃/断电会留下空题库（可从前置备份恢复） | `responder_problem.cpp:213` 起；备份在 `problem_pages.cpp:86` | 改为写临时目录 + 成功后原子替换（`MoveFileEx` 重命名） |

### P1 —— 影响可用性/可维护性

| # | 问题 | 说明 | 建议 |
|---|---|---|---|
| 5 | 登录无失败限流 | 6 位纯数字 PIN 可爆破；源码 118 行留 TODO | 加失败计数 + 冷却 |
| 6 | 端口/路径硬编码 | `PORT=8087`（`server_core.h:14`）；换端口需改码重编译 | 读命令行 `--port` 或 `config.json` |
| 7 | `server.log` 无限增长 | `ios::app`，已见 19 KB，长期运行会变大 | 按大小轮转或每日切分 |
| 8 | 上传 txt 无校验 | 不限大小/不校验编码，超大文件会占内存 | 限制大小、检查 UTF-8、给出明确错误 |
| 9 | urlencoded 请求体上限 8 KB | httplib `CPPHTTPLIB_FORM_URL_ENCODED_PAYLOAD_MAX_LENGTH=8192`；比赛表单很小，暂无影响 | 留意大表单需求 |
| 10 | 网页版题库管理风格与桌面端不一致 | 一个 Bootstrap 默认风、一个卡片风 | 统一视觉 |
| 11 | `build_updater.exe` 是隐形依赖 | 源码目录必须保留该二进制才能编译（见 §3.1） | 改由 MSBuild 内置任务生成版本号，去掉该 exe |
| 12 | 仅验证 x64 Release | x86 / ARM64 配置未测 | 如需分发先实测 |
| 13 | 发布依赖 VC++ 运行库 | 默认 `/MD`，目标机需装 VC++ 2015-2022 x64 运行库 | 换 `/MT` 静态链接，或随包附带 `VC_redist.x64.exe` |
| 14 | 仓库未包含回归测试脚本 | 窗口行为已用临时题库验证，复测场景见 §10.3 | 优先把单题保存和检索用例纳入自动化 |

### P2 —— 代码质量

| # | 问题 |
|---|---|
| 15 | 页面 HTML 用 C++ 字符串拼接，单文件 1,544 行（`competition_pages.cpp`），可读性差；建议后续抽模板文件并在构建期嵌入 |
| 16 | `encoding.cpp` 大量平台/区域相关 API（`codecvt` 已废弃），且题库路径未使用；可裁剪 |
| 17 | 若干 `size_t → int` 转换告警（C4267），题库/队伍极大时理论溢出 |
| 18 | `_remove_used()` 在循环中 `erase`（O(n²)），题库上千仍可接受 |
| 19 | 多处依赖 `std::cout` 副作用做"日志"，未分级（info/warn/error） |

---

## 12. 现场运维手册

### 12.1 比赛前 Checklist

1. 双击 `Server.exe` → 点「上线」→ 状态变 `● 已上线`。
2. 记下窗口里的 **PIN** 与 **局域网地址**（如 `http://192.168.0.109:8087`）。
3. 用另一台设备/手机试访问局域网地址（**验证防火墙已放行**）。
4. 「打开浏览器」→ 用 PIN 登录 → 确认比赛与题库正常。
5. 观察题库存量是否够用（`剩余` 应 ≥ 本场计划抽题数）。
6. 大屏页按 F11 全屏；确认抢答按键 A/B/C 与队伍数匹配。
7. 演练一次"抢答 → 显示答案 → 判分"。
8. 正式比赛前建议点一次「立即备份」。

### 12.2 排障速查

| 症状 | 排查顺序 |
|---|---|
| 双击无反应/立刻消失 | 1) 端口被占：`Get-NetTCPConnection -State Listen -LocalPort 8087`；2) 看 `server.log` 末尾；3) `x64\Release\` 是否还有旧实例在跑 |
| 页面打不开 | 1) 状态是否 `已上线`；2) 本机用 `127.0.0.1` 试；3) 局域网不通查防火墙；4) 端口是否被别的程序占 |
| 登录后马上被踢回 `/start` | Cookie 未带上（浏览器隐私设置/换端口）；PIN 已轮换需用新 PIN |
| 中文变乱码 | 读 JSON 时编码用错（文件是 UTF-8）；页面乱码则查 `assets` 是否完整 |
| 抽题返回 `fail : no unused …` | 题库该题型已用尽，或难度筛选后无剩余（`competition_pages.cpp:1356` 有降级逻辑） |
| 导入后题数不对 | 查重生效（重复题被跳过/删除）；看 `server.log` 的 `- skipped: duplicate ...` |
| 数据改坏了 | 停机 → 把 `data\problems_backup\` 内容复制回 `data\problems\` → 重启 |

### 12.3 本机环境遗留（交接时需知）

- 旧版 1.0.2 部署在 `C:\Users\20962\Downloads\Responder by STA 1.0.2 Build 230\`（Apache），
  对应服务 `ResponderbySTAWebsite` 现为 **Stopped / Disabled**。若手工启用，它会抢占 **8087**，
  导致本程序无法绑定。
- 工作机屏幕 1440×900 / DPI 100%，界面在此尺寸下验证通过。

---

## 13. 后续路线建议（按性价比排序）

1. 后续在 `main` 基线上维护源码和发布说明，现场运行数据单独备份。
2. 隔离请求上下文，避免多请求复用同一路由实例时共享参数。
3. **题库写入改为"临时目录 + 原子替换"**，消除 convert 的破坏性窗口。
4. **PIN 失败限流 + 端口可配置**（运维友好，改动小）。
5. **补单元测试**：`problem_list` 的 convert/merge/dedup 可脱离网络直接测。
6. **页面模板化**：把 `*_pages.cpp` 里的 HTML 抽到 `assets\templates\`，构建期生成头文件嵌入。
7. **统一前后端视觉**，再考虑题库管理页的批量编辑/预览能力。
8. **（可选）大屏页升级**：WebSocket 替代 1 秒轮询；或改为独立 kiosk 程序。

---

## 附录 A：源文件清单

| 文件 | 字节 | 行数 | 归属层 |
|---|---:|---:|---|
| `main.cpp` | 1,896 | 67 | 入口 |
| `server_core.h` | 1,230 | 43 | 控制 |
| `Server.cpp` | 4,929 | 190 | 控制 |
| `gui.cpp` | 29,181 | 948 | 桌面 UI |
| `logging.h` / `logging.cpp` | 1,870 / 1,391 | 81 / 71 | 基础设施 |
| `functions.h` / `functions.cpp` | 967 / 2,574 | 39 / 93 | 基础设施 |
| `encoding.h` / `encoding.cpp` | 1,558 / 10,015 | 55 / 430 | 基础设施 |
| `responder_request.h` / `.cpp` | 2,906 / 7,393 | 66 / 229 | 请求框架 |
| `responder_template.h` / `.cpp` | 611 / 6,095 | 19 / 172 | 请求框架 |
| `responder_pages.h` | 3,344 | 140 | 请求框架 |
| `main_pages.cpp` | 9,279 | 291 | 业务页面 |
| `competition_pages.cpp` | 55,619 | 1,544 | 业务页面 |
| `problem_pages.cpp` | 13,966 | 393 | 业务页面 |
| `responder_problem.h` / `.cpp` | 2,623 / 16,542 | 102 / 517 | 领域模型 |
| `responder_competition.h` / `.cpp` | 1,203 / 3,115 | 50 / 123 | 领域模型 |
| `build.h` / `build_updater.cpp` | 294 / 1,065 | 13 / 43 | 构建 |
| `httplib.h` | 479,436 | 13,954 | 第三方 |
| `json.hpp` | 944,740 | — | 第三方 |

> 此表记录早期交接时的文件规模。桌面编辑器新增 `question_editor.rc`、`question_editor_ids.h`，当前模块入口见 §5.2。
> 不含 `build_updater.exe`（334,336 字节，构建期工具）。

## 附录 B：命令速查

```powershell
# 编译
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'D:\开发区\Responder by STA.slnx' /p:Configuration=Release /p:Platform=x64 /m

# 部署
Copy-Item 'D:\开发区\x64\Release\Server.exe' 'D:\开发区\Server.exe' -Force

# 运行（GUI / 命令行）
& 'D:\开发区\Server.exe'
& 'D:\开发区\Server.exe' --console

# 看谁占了 8087
Get-NetTCPConnection -State Listen -LocalPort 8087 | ForEach-Object {
  Get-Process -Id $_.OwningProcess | Select-Object Id, ProcessName, Path
}

# 停掉所有实例
Get-Process -Name Server -ErrorAction SilentlyContinue | Stop-Process -Force

# 看日志（注意编码）
Get-Content 'D:\开发区\server.log' -Tail 80 -Encoding UTF8

# 备份 / 回滚题库
Copy-Item 'D:\开发区\data\problems' 'D:\开发区\data\problems_backup' -Recurse -Force   # 备份
Copy-Item 'D:\开发区\data\problems_backup\*' 'D:\开发区\data\problems\' -Force          # 回滚（先停机）
```

## 附录 C：术语表

| 术语 | 含义 |
|---|---|
| 抢答（Responder Flow） | 主持人放题后队伍按键抢答，先按者获答权 |
| 轮答（Round Flow） | 按队伍顺序依次作答，可按"难度一致"抽题 |
| 信息题 | 给 4 条线索逐步揭示的题型，`info_*.json` |
| 复杂度 | 难度 1–3，轮答抽题与"难度一致"设置依赖它 |
| 已用标记 | `problem_list.json` 中每题的第二项，0 未用 / 1 已用 |
| PIN | 6 位数字一次性登录码，每次成功登录后轮换 |
| token | 50 位会话令牌，存 Cookie `token`，与服务端内存比对 |
| 构建号（Build） | `build.h` 中的 `PROGRAM_BUILD`，每次编译自增 |

## 附录 D：版本演进摘要

| 版本 | 关键变化 |
|---|---|
| 1.0.2 Build 230（2024-05） | 旧版 Apache + CGI 站，独立部署，密码认证（已停用，仅留档） |
| 2.0 Beta 5 Build 518 | C++ 服务端雏形；`convert` **无查重**；Console 程序 |
| 2.0 Beta 5 Build 525 | `convert` 内置 `delete_duplicates()`；Generator 新增菜单 3（去重+备份）；难度筛选加降级 |
| 2.0 Beta 5 Build 526（早期桌面版） | **本次改造**：合并为单 exe；新增桌面 GUI（上线/下线 + 题库管理 + 日志）；网页端题库管理页；修复表单体解析、POST 路由、端口独占绑定、启动失败可见化；DPI 感知与卡片式 UI |
| Build 526 · Git `7dfeda0` | 蓝白四栏桌面管理、按钮动效、比赛记录和日志设置；题目列表、筛选、单题新增和编辑、即时搜索；题库访问加锁并更新保存流程。 |

---

**交接联系人**：原开发方 STA（成都七中科学技术协会），仅留 Copyright 声明，无联系方式留存。
