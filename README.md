# KSAT — 纯粹的任务看板

个人/团队任务管理工具。**不带来任何焦虑**：只表示有这样一个任务、任务的内容是什么、属性是什么。

- 界面: Qt5 (Widgets)
- 底层: C++17
- 数据库: SQLite（单文件，无需服务器）
- 跨平台: Windows / Linux
- 语言: English（仅英文）
- 主题: 白色极简 / Apple 风格（仅白色）

## 功能

- 主界面：顶部工具栏左侧为 `Task History`，右侧为 `Filter ▾`、`＋ Add Task`、`Change Password`
  - 任务筛选集中到一个 `Filter ▾` 图标，点击后在图标原位弹出筛选面板，内含 项目 / 负责人 / 状态 三组多选，底部有 `Clear All` 按钮
  - 看板风格极简、逻辑性强，一级界面只保留必要交互
- 两层任务：**大任务 (main task)** 与 **小任务 (sub task)**
  - 大任务：名称（必填，≤30字符）、所属项目、负责人、起始/终止时间、记录、小任务、任务状态；建立后可修改全部内容
  - 大任务卡片**上边沿颜色 = 所属项目颜色**（同项目同色、不同项目不同色、未填项目为白色）；同项目任务按添加顺序并列放置；卡片等高分列、每张都顶到底部
  - 大任务卡片顶部右侧 `⋯` 菜单集中 编辑 / 完成 / 停止 / Open / 删除 五个操作；**Open 始终显示**，用于把被手工 Done/Stopped 的任务重新打开（恢复自动状态）
  - 项目 / 负责人 / 时间范围各占一行（**灰体**，时间仅到日），卡片内 `Notes` 区块与 `Subtasks` 区块各在其顶部有 `+` 新增按钮
  - 记录支持编辑（`✎`）与删除（`✕`，删除前需确认）
  - 小任务按行排列在大任务内部，每条一行；行内显示 [状态点 | 名称 | 状态 | `⋯`]，**记录默认展开**；点击 `⋯` 弹出操作列表（状态在二级列表：Done / Stopped / Open，另有 Add Note、Edit、→ Main task、Delete）；小任务无编辑按钮（编辑入口在弹出列表中）
  - **小任务可建立大任务**：新大任务的项目与名称与小任务一致，小任务上有 `🔗` 链接指向所建大任务，大任务上有「来源」属性指向来源小任务；一个小任务可建立多个大任务；有相关大任务的小任务会被标注
- 记录 (note)：大任务每条记录一行；小任务记录默认展开；数量不限
- 任务状态：未开始 / 进行中 / 完成 / 延期 / 停止
  - 「完成 (Done)」「停止 (Stopped)」手工设置并**记录设置时间**，设置后覆盖任意状态且**不再自动更新**；大任务与小任务均提供 **Open** 手工恢复自动状态
  - 「未开始」「进行中」「延期」根据起止时间**自动判断**
  - 完成、停止的任务自动隐藏（隐藏前需确认："finished task is to be hidden" / "stopped task is to be hidden"；可经状态筛选查看；历史任务中全部可见）
- 历史任务：树形视图（第一层项目 → 第二层任务），显示负责人、当前状态；完成/停止的任务显示完成/停止时间
- 登录 / 修改密码：保留（英文）；首次启动默认密码为 `123`

## 环境要求

| 依赖 | Linux (Ubuntu/Debian) | Windows |
| --- | --- | --- |
| Qt5 (Widgets + Sql) | `sudo apt install qtbase5-dev libqt5sql5-sqlite` | Qt 5.15 (含 SQLite 驱动插件 `qsqlite`) |
| CMake ≥ 3.16 | `sudo apt install cmake` | CMake for Windows |
| 编译器 | g++ ≥ 9 | MinGW-w64 或 MSVC |

> 注意：Qt 的 SQLite 驱动插件（`libqt5sql5-sqlite` / `qsqlite`）必须存在，否则程序会提示无法打开数据库。

## 构建 (Linux)

```bash
bash build.sh              # 或:
# cmake -B build -DCMAKE_BUILD_TYPE=Release
# cmake --build build -j
```

生成 `build/KSAT`。

## 构建 (Windows, MSVC)

```bat
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64
cmake --build build --config Release
```

将 `build\Release\KSAT.exe` 与 Qt 运行库（windeployqt）、`plugins\sqldrivers\qsqlite.dll` 放在同一目录。

## 数据库

SQLite 单文件数据库，无需安装/运行任何服务器。首次启动时自动建库建表。

- 默认数据文件：**KSAT 可执行文件同级目录下的 `ksat.db`**（便携）
- 手动重建可执行 `sqlite3 <路径>/ksat.db < schema.sql`
- 如已在系统配置中设置过 `db/file` 连接参数，则优先使用该路径；正常无需修改

## 目录结构

```
src/
  main.cpp       入口、登录门禁、SQLite 数据库连接
  models.h       数据结构与状态枚举
  database.h/.cpp SQLite 访问、建表、自动状态同步
  dialogs.h/.cpp 新建/编辑大任务与小任务、记录、历史树、多选筛选控件
  taskcard.h/.cpp 看板卡片（上边沿颜色、记录、小任务行、链接）
  mainwindow.h/.cpp 主窗口（工具栏、筛选、看板）
schema.sql       手动建库脚本
```