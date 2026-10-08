# WSLg Windows 输入法桥接

让 WSLg 里的 Linux 应用能用上 Windows 输入法

[microsoft/wslg#9](https://github.com/microsoft/wslg/issues/9)

本仓库负责自动构建两个成品，
源代码在 [baiiiii/wslg](https://github.com/baiiiii/wslg) and [baiiiii/weston-mirror](https://github.com/baiiiii/weston-mirror)，
产物发布在 [Releases](https://github.com/baiiiii/wslg-build/releases)


| 文件 | 说明 |
|---|---|
| `WSLDVCPlugin.dll` | Windows 侧插件（WSLg 的 DVC 插件，实现文本输入桥） |
| `system_x64.vhd` | WSLg 的 system distro 镜像（内含改造过的 weston） |
| `system_x64_patch.vhd` | weston 功能优化，包含输入法桥接，推荐使用此版本 |

---

## 一、快速开始

### 1. 到 [Releases](https://github.com/baiiiii/wslg-build/releases) 下载：

* `WSLDVCPlugin.dll`
* `system_x64.vhd`

### 2. 替换文件

```text
WSLDVCPlugin.dll   →  C:\Program Files\WSL\WSLDVCPlugin.dll      （覆盖同名文件）
system_x64.vhd     →  例如 C:\system_x64.vhd                     （路径随意，记住它）
```

> 覆盖 DLL 前先执行 `wsl --shutdown`，否则文件被 msrdc.exe 占用。

### 3. 让 WSL 用上这个镜像

编辑 `%USERPROFILE%\.wslconfig`（没有就新建）：

```ini
[wsl2]
systemDistro=C:\system_x64.vhd
```

然后重启：

```powershell
wsl --shutdown
```

---

## 二、图形应用的配置建议（重要）

**总原则**：**应用走 Wayland，输入法就能用 ✓；走 X11，就要靠 XIM ✗（本方案尚未支持）**。

| 应用 | 需要做的 |
|---|---|
| **GTK 3/4**（gedit、Nautilus、GNOME 系） | **默认就走 Wayland ✓ 通常无需配置** |
| **Qt 5/6** | `export QT_QPA_PLATFORM=wayland` |
| **Chromium / Chrome** | 见下方三个开关 |
| **Electron 系**（VS Code、Discord、Obsidian…） | 见下方三个开关 |
| **Firefox** | `export MOZ_ENABLE_WAYLAND=1` |
| **JetBrains IDE** | VmOptions 中添加配置 `-Dawt.toolkit.name=WLToolkit` |
| **Java（Swing/AWT）/ 老 X11 程序** | ✗ **无解**——它们只认 XIM |

### Chromium / Electron 的三个开关

```bash
--ozone-platform=wayland          # 强制用 Wayland 后端，否则走 X11
--enable-wayland-ime              # 启用 Wayland 输入法集成（默认关闭）
--wayland-text-input-version=3    # 选用 zwp_text_input_v3
```

* **第三个开关和本方案正好对口** ✓——桥实现的就是 `zwp_text_input_v3`：
  `set_cursor_rectangle` → 候选窗跟随 ✓；`commit_string` → 中文上屏 ✓。
* **不加这三个开关**，Chromium 走 X11，就只能靠 XIM，**当前不可用**。

```bash
chromium --ozone-platform=wayland --enable-wayland-ime --wayland-text-input-version=3
```

---

## 三、配置文件 `wsltextbridge.cfg`

**位置**：`C:\ProgramData\wsltextbridge.cfg`
**格式**：每行 `键=值`；`#` 之后是注释；**编码必须为 UTF-8**
**布尔值**：`1` / `true` / `on` 为开，**其它任何值（含 `0`、`false`）为关**
**文件不存在**：全部使用下列默认值 —— 而**默认值即推荐的生产配置**，所以平时**不需要创建这个文件**。

| 键 | 类型 | 默认 | 作用 | 建议 |
|---|---|---|---|---|
| `store` | bool | `0` | 提供 `ITextStoreACP` 文本存储，让输入法改走"应用文本存储"路径做组合与查询 | 保持 `0`（置 `1` 会切换到另一条路径，曾观察到候选窗反而不显示） |
| `keyfeed` | bool | `0` | 把按键喂给 TSF 的 `ITfKeystrokeMgr::KeyDown`，让输入法直接"看见"按键 | 保持 `0` |
| `sink` | bool | `1` | 注册 `ITfTextEditSink`，接收文本编辑事件并把提交的文本发往 Linux 侧 | **必须 `1`**（关掉就不上屏） |
| `caret` | bool | `1` | 使用服务端下发的光标边界放置插入符 | 保持 `1`（关掉候选窗就失去锚点） |
| `owner` | bool | `1` | 创建并注册 `ITfContextOwner` —— **候选窗定位的关键** | **必须 `1`**（关掉候选窗会退回屏幕左下角） |
| `geomfix` | int | `0` | 几何补偿实验模式，取值 `20`–`24` 分别针对特定 PDU（`0x030F` / `0x0308`） | 保持 `0`（实验代码） |
| `syncgeom` | int | `0` | 主动发送 `0x0603`(REREGISTRATION_REQUEST) 并订阅 LAYOUT/SELECTION 跟踪，提前预取几何信息 | 保持 `0`；若光标偶尔不同步可试 `1` |
| `verbose` | bool | `0` | 写日志到 `C:\ProgramData\wsltextbridge.log` | 排查时设 `1`，平时 `0` |

### 示例：只开日志

```ini
# C:\ProgramData\wsltextbridge.cfg
verbose=1
```

保存为 **UTF-8**，重启会话即可。日志出现于 `C:\ProgramData\wsltextbridge.log`。
查完直接删掉该 cfg 与日志文件即可 —— **删除后不会再自动生成**。

---

## 四、原理与已知限制

### 原理

WSLg 的图形通道由 `msrdc.exe`（微软 RDP 客户端）与 weston（RDP 服务端）组成。
本方案在两端各加一个组件：

```text
Windows 输入法 (微软拼音 / TextInputHost)
        │  TSF
        ▼
WSLDVCPlugin.dll  ── 私有 DVC 通道（MS-RDPETXT 协议）──▶  weston (rdptext)
        │  实现 ITfContextOwner / ITfTextEditSink 等          │
        │  把光标位置返回给输入法 ⇒ 候选窗跟随光标 ✓           │ 把文本送进 Linux 应用
        ▼                                                      ▼
   Windows 侧                                              Linux 应用 (gedit 等)
```

* 通道名：`WSL::TextBridge::ServerToClient` / `WSL::TextBridge::ClientToServer`
  （weston 与插件两侧必须一致）；
* 通道内的 PDU **逐字节遵循 MS-RDPETXT 协议**，只有通道名是私有的。

### 为什么不直接用规格通道名

MS-RDPETXT 规格点名 `TextInput_ServerToClientDVC` / `TextInput_ClientToServerDVC`，
但实测在 WSLg 会话中**规格通道上没有活着的客户端**：

`rdclientax.dll` 内置的官方客户端（`RemoteTextPlugin`）占着规格名，
却被 `/wslg` 模式的产品门闸强制关闭 ——
`msrdc.exe` 的 `/wslg` 开关会设置 `TS_PROP_CORE_WSLGMODE_ENABLED`，
引擎据此把 `EnableTextProcessingRedirection` 派生为 `0`，
并打印 `Text processing redirection is disabled for this connection.` 后返回 `E_ACCESSDENIED`；

因此本方案改用**私有通道名**，由自己的插件监听，绕开该门闸。

### 已知限制

* 仅支持 WSLg（依赖 `/wslg` 模式下的 hvsocket + 共享内存传输）；
* 需要替换 WSLg 的 system distro 镜像，**Windows 或 WSL 更新后可能需要重新部署**；
* 与微软官方 RDPETXT 客户端不互通（它们是两套独立实现）。

---

## 五、weston 功能优化

[https://github.com/weihanhan/wslg_custom](https://github.com/weihanhan/wslg_custom)

- X11 窗口边框调整
  - 点击左上角标题栏图标可在浅色/深色间切换，默认为深色。
  - 替换了标题栏的图标。
  - 消除左、下、右三处的白色边框。
- `Alt+F12` 在"标题栏模式"和"无边框模式"之间切换，默认为"标题栏模式"。
- `Alt+F11` 在窗口化和最大化之间切换。
- `Alt+鼠标左键` 拖动调整窗口大小。
- `Alt+鼠标右键` 拖动移动窗口。
- 支持从 Windows 剪贴板粘贴图片到 WSLg 窗口。

---

## 许可

见 [LICENSE](LICENSE)。
