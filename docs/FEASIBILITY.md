# 功能可行度分析 — sticky-mood（me.status）

日期：2026-09-14。对象：reTerminal Sticky（ESP32-S3R8 / 3.97" 800×480 SSD1677 / GT911 / 750 mAh）。
结论先行：**8 页设计稿里没有不可行的功能**，但有 1 个 gating 未知项（Seeed AI 管线的可复用性）
和 3 个需要真机 spike 的硬件未验证项（SD 卡、PDM 麦克风、按键极性）。
全部风险都有对策，且对策不会推翻现有架构。

评级说明：
- ✅ 可行 —— 软件栈齐备或物理特性天然支持，直接做
- ⚠️ 需验证 —— 路径清楚但有未验证环节，先做 spike 再排期
- 🔶 有条件可行 —— 取决于一个外部决策/外部信息，已给备选方案

---

## 0. 总览判定表

| # | 功能 | 判定 | 关键依据 | 对策 / spike |
|---|---|---|---|---|
| F1 | 状态页待机后持续显示 | ✅ | 墨水屏断电保持图像 | 渲染 → 全刷 → 屏休眠 → 掉电锁存释放 |
| F2 | 8 页触摸流程 + 进度条 + 返回 | ✅ | GT911 驱动已验证；只需 tap，不需手势 | 无需 |
| F3 | 长按 AI 键录音（与唤醒共用一个键） | ✅ | `iot_button` 原生区分短按/长按 | 真机确认长按阈值手感 |
| F4 | 日期时间显示 | ✅ | PCF8563 RTC 在板上；Wi-Fi SNTP 校时 | 需**新写 PCF8563 驱动**（未 vendored，工作量小） |
| F5 | 数据存 SD 卡 | ⚠️ | IDF 有 SPI-SD + FATFS 软件栈；但**本板 SD 时序未验证**，且与屏幕共用 SPI2 | **按用户决定：先假设可行**，不确定项已整理进 `docs/QUESTIONS-FOR-SEEED.md` §A（卡检测/供电使能/SPI 时钟/FAT32/与面板如何串行化）；真机 spike 走借测 |
| F6 | 语音转文字（中文） | ✅ | ESP32-S3 **不能本地做中文 STT**，必须上云。**已决定用用户自己的 Qwen API key**（BYOK），设备直接 HTTPS 打 Qwen ASR，不依赖 Seeed 闭源管线 | 见 §3；`SttClient` 接口仍做成可换后端（便于以后换 provider 或离线降级） |
| F7 | 录音 | ⚠️ | IDF 支持 I2S PDM RX；但本板麦克风采样配置**未验证** | spike：录 16 kHz WAV 并回放/上传验证 |
| F8 | 笔记正文显示中文 | ✅ | GB2312 点阵子集烧 flash，容量充足（见 §4） | 建字体生成管线 |
| F9 | 圆润英文 UI 字体 | ✅ | 开源圆润字体（OFL）栅格化子集，体积极小 | 字体包到位即可 |
| F10 | 猫插画 / 图标 | ✅ | 1-bit 线稿总体积约 23 KB，可忽略 | 按 `docs/ASSET-SPEC.md` 出图 |
| F11 | 三个月历史 + 翻页 | ✅ | SD 按天 JSONL；翻页用局部刷 + 定期全刷 | 无需 |
| F12 | 多人/用户名个性化 | ✅ | 单用户；名字走编译期配置（已定） | 无需 |
| F13 | 续航"拿起来记、记完放下" | ✅ | 会话间彻底掉电锁存，日耗电估算 ~5 mAh（见 §5） | 真机实测校准 |

---

## 1. 与墨水屏物理特性相关的体验判定

这些不是"能不能做"，而是"做出来手感如何"，必须提前对齐预期：

| 交互 | 预期延迟 | 说明 |
|---|---|---|
| 按下电源键 → 看到可读画面 | **≈0 s（图像已在屏上）** | 面板断电仍保持上次画面，唤醒瞬间用户看到的就是上次的状态页；启动在背后进行 |
| 点选（猫脸/能量格/列表项）→ 看到选中态 | 150–400 ms | 局部刷新，不闪屏 |
| 翻页（Home→Mood→…） | 1–2.5 s | 黑白全刷，会闪一次。这是墨水屏的正常手感 |
| 录音结束 → 看到转写文字 | 2–10 s | 上传 + 云端 STT 往返。期间显示 Saving 态 |

**设计含义**：流程里每一次"翻页"都付一次全刷代价。8 页设计稿的 check-in 是 4 次翻页
（Home→Mood→Energy→Intention→Completed），即 4 次全刷 ≈ 5–10 秒总等待，分散在用户思考间隙里，
可接受。**不建议**再增加中间确认页。

另一个含义：**不要做滑动列表**。Notes 页用"上一页/下一页"按钮 + 物理上下键翻页，
每页 3 条卡片，全刷一次。滑动在墨水屏上体验很差且会快速累积残影。

## 2. 逐项技术路径

### F2 / F11 页面与导航
竖屏 480×800 已确认，与现有 `sticky_display.cpp` 一致。页面框架按官方推荐分层：
`pages/` 纯渲染（读 AppState，出画布），`app/` 状态与事件，`hardware/` 驱动。
返回用屏上 Back 按钮 + 物理上/下键做页间移动。

### F3 长按录音 vs 短按唤醒
GPIO 4 一个键承担：冷启动唤醒（EXT1）、唤醒后短按=确认/返回、长按≥600 ms=开始录音、松开=结束。
`iot_button` 提供 `BUTTON_SINGLE_CLICK` / `BUTTON_LONG_PRESS_START` / `BUTTON_LONG_PRESS_UP`。
录音期间屏幕停在 Listening 页，不做刷新（避免刷新和 I2S 抢 CPU）。

### F4 时间
PCF8563（I2C1）走时 + 每次开 Wi-Fi 时 SNTP 校准一次写回 RTC。
PCF8563 精度约 ±1–2 min/月，靠 SNTP 周期性修正足够。
**需新写 PCF8563 驱动**（I2C 读写秒/分/时/日/月/年寄存器，约 150 行），未 vendored。

### F5 SD 卡
软件栈齐备：IDF `sdspi_host` + `esp_vfs_fat`（FAT32）。未验证的是本板硬件行为。
风险点与对策：
- **SPI2 共享**：面板和 SD 卡同总线。对策：一个全局 `spi2_mutex`，面板传输与 SD 传输互斥；
  绝不在面板 busy 等待期间碰 SD。
- **时序/上拉未验证**：spike 先以 10 MHz（与面板同频，避免切频）跑读写测试，失败再降 4 MHz。
- **磨损**：JSONL 追加写 + 每条 `fsync`，按天分文件，顺序写，磨损可忽略。
- **卡不在位**：开机探测失败则降级——记录暂存 NVS 队列，状态栏显示 SD 缺失图标，插卡后回填。

### F6 语音转文字 —— 已决定，见 §3

### F8 / F9 字体
见 §4 与 `docs/ASSET-SPEC.md`。

## 3. 语音转文字：已决定用我们自己的 Qwen API key（BYOK）

**原先的 gating 项已消除。** 用户确认：语音转文字用**自己的 Qwen API key**，
定位是"轻松化的心情记录文本"，不追求逐字准确。Lily 也确认过官方支持填自己的 key。
所以我们**不需要**碰 Seeed 的闭源固件或它的云，设备直接 HTTPS 打阿里云百炼。

### 已核实的接口契约（2026-09-14 查阿里云文档）

用 **`qwen3-asr-flash`**，两种模式都可用，都支持**一次性同步调用**：

| | OpenAI 兼容模式 | DashScope 同步模式 |
|---|---|---|
| 端点 | `https://{WorkspaceId}.cn-beijing.maas.aliyuncs.com/compatible-mode/v1/chat/completions` | `https://{WorkspaceId}.cn-beijing.maas.aliyuncs.com/api/v1/services/aigc/multimodal-generation/generation` |
| 鉴权 | `Authorization: Bearer <DASHSCOPE_API_KEY>` | 同左 |
| 音频 | `messages[0].content[0].input_audio.data` | `input.messages[0].content[0].audio` |

**关键点：音频可以直接传 base64 data URI（`data:audio/wav;base64,...`），不需要先把录音上传到公网 URL。**
这一条决定了整个设计 —— 设备录完 WAV 就能一个请求搞定，不必再搭对象存储。

可选参数：`asr_options.language = "zh"`（含普通话与四川话/闽南语/吴语）、`asr_options.enable_itn`
（把"一百二"规范成"120"）。响应里除了转写文本，还有一个 `annotations[].emotion`
（`happy/sad/neutral/surprised/disgusted/angry/fearful`）—— **这是白送的一个信号**：
可以在用户选完心情后用来交叉校验，或在 Intention 页给个提示词。本期先只记录不干预。

计费按 `usage.seconds`（音频秒数）。

### 设备侧影响

- **体积**：16 kHz / 16 bit / mono = 32 KB/s，60 s 录音 ≈ 1.9 MB，base64 后 ≈ **2.5 MB**。
  8 MB PSRAM 放得下，但**不能一次性拼一个 3 MB 的 JSON 字符串**：
  用 `esp_http_client` 分块 write，边读 SD 上的 WAV 边编码边发。
- **TLS**：需要内置阿里云的根证书（或用 `esp_crt_bundle`）。mbedTLS 已在编译产物里，
  实测镜像 293 KB，加 TLS 客户端不会爆预算。
- **延迟**：同步一次性调用，60 s 音频估 2–6 s。与我们给用户对齐的"录音结束 → 看到文字 2–10 s"一致。
- **不做流式**：实时 ASR（`paraformer-realtime-v2` / WebSocket）能边说边出字，
  但要多维持一条长连接、多一套重连逻辑，而本产品场景是"说完一段再上传"，**不值得**。

### 仍未确认

| 项 | 说明 |
|---|---|
| `WorkspaceId` 与地域 | 端点主机名里要带 Workspace ID；北京 / 新加坡 / 弗吉尼亚三选一，需要用户提供 |
| 单个请求 base64 上限 | 文档提到约 10 MB 量级，2.5 MB 应该在范围内，但要真机跑一次确认不被拒 |
| 数据留存政策 | 见下方隐私提示，需要用户自己在百炼控制台确认 |
| 录音时长上限 | 我们要不要在固件里限死 60 s（也顺便限住流量和电） |

### 架构：接口仍然做成可换后端

```
class SttClient {
    virtual esp_err_t transcribe(const char *wav_path, char *out_text, size_t cap) = 0;
};
// Backend 1: QwenStt        （已定，本期唯一实现）
// Backend 2: NullStt        （无网络/无 key 时的降级：只存音频路径，文字留空）
```

编译期选后端。`NullStt` 不是凑数：设备上 key 写死，一旦过期或断网，
笔记仍要能存下来（音频 + 空文本），不能整条丢掉。

**隐私提示**：心情与日记是高度私密内容，音频会离开设备上传到阿里云。
选哪家云、音频是否留存、要不要开加密存储，需要用户在百炼侧自己确认数据政策 ——
这条我不能代答。

Sources（阿里云官方文档，2026-09-14 查阅）：
- [非实时语音识别 Qwen-ASR API 参考](https://help.aliyun.com/zh/model-studio/qwen-asr-api-reference)
- [qwen3-asr-flash 模型信息](https://help.aliyun.com/zh/model-studio/qwen3-asr-flash)
- [非实时语音识别用户指南](https://help.aliyun.com/zh/model-studio/non-realtime-speech-recognition-user-guide)
- [Paraformer 非实时语音识别 HTTP API](https://help.aliyun.com/zh/model-studio/paraformer-recorded-speech-recognition-restful-api)

## 4. 容量预算（flash / RAM / SD）

### Flash（32 MB，充裕）

| 内容 | 估算 |
|---|---|
| App 二进制 —— bring-up 实测（已含 Wi-Fi + mbedTLS + lwIP + 屏 + 触摸） | **293 KB（实测）** |
| 业务逻辑 + 页面/UI + 录音 + SD 存储 + STT 客户端 | + 0.3–0.6 MB（估） |
| 圆润英文字体 6 个字号（ASCII 子集） | < 50 KB |
| 中文点阵字库 GB2312 子集，16 px + 18 px 两档 | ≈ 0.5 MB |
| 猫插画 + 图标（1-bit） | ≈ 23 KB |
| 合计 | ≈ 1.2–1.5 MB |

实测把原来"2.0–2.6 MB app 二进制"的猜测砍掉了一个数量级：`single_app_large` 的 3 MB app 分区
**足够，不再需要为容量强行做自定义分区表**。
仍建议把中文字库单独放 data 分区（或 SD），理由是"换字体不用重烧固件"，属于可选优化而非必需。
中文只保留 16/18 px 两档（笔记正文与卡片足够），是刻意做的取舍；要更多字号就放 SD。

### RAM
- 内部 SRAM 512 KB：bring-up 实测 DIRAM 用 71 KB / 341 KB（21%），IRAM 与 RTC FAST 宽裕；
  加上 Wi-Fi + TLS 运行时约 150–200 KB 后仍有余量。
- PSRAM 8 MB：帧缓冲 48 KB + 录音缓冲（16 kHz/16 bit/mono = 32 KB/s，60 s ≈ 1.9 MB）+ TLS 缓冲。充裕。

### SD
- 每条笔记 ≈ 0.2–1 KB；每天 status ≈ 0.2 KB。三个月 ≈ < 0.5 MB。容量完全不是问题。

## 5. 功耗预算（估算，需真机校准）

| 状态 | 电流（估） | 时长/次 |
|---|---|---|
| 会话间：释放电源锁存，整轨掉电 | µA 级（仅电量计/RTC） | 占 99% 时间 |
| 活跃 UI（CPU + 屏逻辑） | 80–120 mA | check-in 全程 ≈ 30–60 s |
| 全刷 | +10–30 mA | 1–2.5 s/次 |
| 录音 | +5–10 mA | 用户控制，典型 20–60 s |
| Wi-Fi 上传 + STT | 180–250 mA | 2–6 s |

典型一天：1 次 check-in（4 次全刷）+ 2 条语音笔记 ≈ 活跃 3 分钟 ≈ **4–6 mAh**。
750 mAh → **数月级**，与厂商标称一致。前提是会话间真的掉电锁存，而不是 deep sleep 保轨。
**这决定了 `board::power_off()` 是常态路径，不是异常路径。**

## 6. spike 清单（Phase 0，合计约 4 人日；只有 S7 不需要真机）

| # | spike | 通过标准 | 估时 | 状态 |
|---|---|---|---|---|
| S1 | ESP-IDF v5.4 编译 bring-up | 编译通过、镜像尺寸在预算内 | 0.25 d | ✅ **已完成**（镜像 293 KB，DIRAM 21%） |
| S1b | 烧录 bring-up 到真机 | 屏幕显示验证页，触摸坐标与三个测试框对齐 | 0.25 d | 待借测 |
| S2 | 按键极性 + 短按/长按 | 三键读数符合预期；长按 600 ms 触发 | 0.25 d | 待借测 |
| S3 | SD 卡 SPI 读写，与面板并发 | 10 MHz 读写稳定；与全刷并发不花屏不挂死 | 1 d | 待借测（**先假设可行**，疑点见问题清单 §A） |
| S4 | PDM 麦克风录 16 kHz WAV | 录音可被 Qwen ASR 正确识别为中文 | 0.5 d | 待借测（疑点见问题清单 §B） |
| S5 | 掉电锁存 + 重新上电 + 状态恢复 | 掉电后屏图保留；重新开机恢复到正确页面 | 0.5 d | 待借测。**状态存 NVS/SD，不赌 RTC 保持内存**（见问题清单 C4） |
| S6 | Wi-Fi + SNTP + PCF8563 | 校时写回 RTC，断电重启时间连续 | 0.5 d | 待借测 |
| S7 | Qwen ASR 端到端（**不需要真机**） | 在电脑上用一段样本 WAV 打通 `qwen3-asr-flash`，拿到中文转写 | 0.5 d | 可立刻做：只需用户提供 API key + WorkspaceId |
| S8 | 录音 → 上传 Qwen → 文字写 SD | 端到端 < 10 s，文字落盘可读 | 0.5 d | 待借测（依赖 S3/S4/S7） |

**硬件相关的项目全部依赖借测**（用户手上没有机器，Lily 说固件写好可以借）。
所以借测窗口要一次性覆盖 S1b–S6 + S8，随固件附一份"看什么现象、说明哪条假设错了"的观察清单，
见 `docs/QUESTIONS-FOR-SEEED.md` §I。

S7 是唯一**不需要硬件**的验证项，且它同时验证了 §3 的接口契约，建议先做。

## 7. 工作量估算（spike 之后）

| 阶段 | 内容 | 估时 |
|---|---|---|
| Phase 1 | 资产管线、字体管线（含 GB2312 子集生成）、页面框架/状态栏/导航 | 5 d |
| Phase 2 | Home→Mood→Energy→Intention→Completed 五页 + status 写 SD + 掉电保持 | 4 d |
| Phase 3 | 录音 WAV + SttClient 双后端 + Saved 页 + notes 写 SD | 4 d |
| Phase 4 | Notes 列表、三个月过滤、分页、防残影调优 | 2 d |
| 收尾 | 残影/刷新策略调优、功耗实测、边界情况 | 2 d |
| 合计 | | **≈ 17 人日**（不含 Phase 0 的 3.5 d） |

## 8. 明确不做（本期）

- 滑动/拖拽手势（墨水屏体验差）
- 4 级灰度（设计稿是纯黑白线稿，1-bit 足够；灰阶只会增加刷新时间）
- IMU 手势、温湿度上下文（需求文档未提）
- BLE / 手机 App 同步（SD 即存储，符合"远离手机"意图）
- 多用户
