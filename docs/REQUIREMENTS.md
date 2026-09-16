# 需求文档 — sticky-mood（me.status）

状态：**需求已由用户提供（2026-09-14）**，含 8 页 UI 设计稿。
本文档记录确认的需求、与硬件物理约束的逐条对照、以及仍待拍板的技术决策。
设计稿原图：用户微信临时目录
`.../xwechat_files/wxid_uawwl3o9ynbj22_99a9/temp/RWTemp/2026-09/1162ac00ca42622dae8ede926488dc43.png`
（建议拷进 `docs/design/` 存档，见文末"待办"）。

---

## 1. 产品意图

> 希望大家能有一些远离手机的时刻。我现在用 iPhone 健康记录每日心境，
> 用"每日提醒"这类软件做快捷的 speech-to-text 笔记。但每次拿起手机就容易被分散注意力，
> 切到别的应用或去干别的事。周围很多人有同样的困扰。
> 所以想用电子墨水屏做一个记录心态、心情的软件：想记录的时候拿出 Sticky，
> 更沉静、更凭直觉地记录自己的心境和每日总结。

由此推出的设计原则（我理解的产品灵魂，写代码时会守）：

- **拿起即记，记完即走。** 交互步骤要少到不需要思考。
- **不制造新的注意力陷阱。** 没有信息流、没有红点、没有"再看一眼"的理由。
- **墨水屏的慢是特性不是缺陷。** 不追求即时反馈，追求"记完就放下"。

## 2. 已确认的交互决议

| # | 决议 | 日期 |
|---|---|---|
| 唤醒 | 按 AI 键点亮屏幕，之后全程触摸操作 | 2026-09-14 |
| 方向 | 竖屏 480×800（与现有代码一致，零返工） | 2026-09-14 |

## 2b. 已确认的技术决议（第二轮，2026-09-14）

| # | 决议 | 备注 |
|---|---|---|
| D1 | 语音转文字：**用我们自己的 Qwen API key（BYOK）**，`qwen3-asr-flash` 一次性同步调用，音频走 base64 | ✅ **gating 项已消除**。不需要碰 Seeed 闭源管线。接口契约已核实，见 `docs/FEASIBILITY.md` §3。待用户提供 API key + WorkspaceId |
| D2 | Wi-Fi 凭据 / API key：**先写死在编译配置**，产品化再做配网 | |
| D6a | 字体：**用户自行找字体包放本地**（`assets/fonts/`） | 英文三款已到位（Manrope / Playpen Sans / Plus Jakarta Sans，均 SIL OFL 1.1）。中文已定为思源柔黑 GenSenRounded2 TC（OFL 1.1，GB2312 子集 M/B），苹方因授权不能嵌入分发，见 `docs/ASSET-SPEC.md` §4 |
| D6b | 猫插画：**按 `docs/ASSET-SPEC.md` 出图** | ✅ 已交付：11 猫 + 12 图标 1-bit 成品在 `assets/1bit/`，管线在 `tools/art/` |
| 工具链 | 安装 ESP-IDF v5.4 并编译 bring-up | ✅ **已完成**：app 镜像 293 KB（含 Wi-Fi/mbedTLS/lwIP），DIRAM 21%。构建命令与踩坑记录见 `README.md` |

## 2c. 第三轮决议（2026-09-14）

| # | 决议 | 备注 |
|---|---|---|
| D7 | **SD 卡先假设可行**，不因未验证而停手 | 不确定项（卡检测、供电使能、SPI 时钟、FAT32、与面板共用 SPI2 如何串行化）已整理进 `docs/QUESTIONS-FOR-SEEED.md` §A |
| D8 | **Lily（Sticky 负责人）= 本项目技术顾问** | 硬件/固件疑问走问题清单转交；固件写好可发给她，她愿意借真机做测试 |
| D9 | **用户手上没有实体机器**（缺货卖光） | 所有真机验证（S1b–S6、S8）走借测，一次窗口尽量全覆盖，见 `docs/QUESTIONS-FOR-SEEED.md` §I |
| D10 | App 状态**不依赖 RTC 保持内存**，改存 NVS/SD | 因为"释放电源锁存 = 整轨掉电"，RTC 内存是否还有电未确认（问题 C4）。按保守方案做，即使 RTC 域有电也只是少省一次读写 |

## 2d. 第四轮决议（2026-09-15）

| # | 决议 | 备注 |
|---|---|---|
| D11 | **前端先行的三层验证流程** | ① 网页像素级预览：`main/ui`+`main/pages` 零 ESP-IDF 依赖，host 编译同一份 C++ 渲染代码，`bash tools/sim/build_sim.sh` 产出可点击的 `build/sim/index.html`；② host 逻辑测试；③ 真机只验硬件事实（借测，固件经 GitHub 公开 repo + CI 预编译 bin 交给 Lily） |
| D12 | **回到 Home = 放弃本次 check-in**，mood/energy/intention 归零 | 流程内 back（Intention→Energy→Mood）保留已选项；只有回到 Home 才重置。否则模拟器可达状态会带着半成品选择爆炸到上千个，且产品语义上"改天再填"也不该残留半份记录 |

**功能可行度分析的权威版本是 `docs/FEASIBILITY.md`**，含逐项判定、容量/功耗预算、
spike 清单与工作量估算。本文件只记录需求与决议。

## 3. 页面清单（8 页，来自设计稿）

| # | 页面 | 内容 | 交互 |
|---|---|---|---|
| 1 | **Home** | 品牌头 `me.status` + 右上日期时间；猫插画；标题 "How are you today?" + 副标题 "A moment for yourself"（2026-09-15 改，原 "Good morning, Winnie." 弃用）；深色主按钮 **CHECK IN**；次按钮 **Hold to Talk**（麦克风图标+文字居中，与 Completed 同款）；底部 "A SMALLER SCROLL. A BRIGHTER LIFE." | 点 CHECK IN 进流程；**长按 AI 键**进语音记录 |
| 2 | **Mood** (1/3) | 进度条 1/3；"How are you feeling today?"；5 只猫脸 **Rough / Low / Okay / Good / Great**（3+2 网格）；Back；提示 "Tap a cat to continue." | 点一只猫即进入下一页 |
| 3 | **Energy** (2/3) | 进度条 2/3；"How's your energy today?"；趴着的猫 + 电池图标；**1–5 五个方块**（实心=已选） | 点方块选 1–5 |
| 4 | **Intention** (3/3) | 进度条 3/3；"What do you need today?"；列表 **Focus / Create / Rest / Move / Connect**（带图标，选中项深色反白）；Back + **Done** | 单选，Done 提交 |
| 5 | **Completed** | 猫 + 星星；"All set! Here's your status for today."；状态卡三行 **Feeling / Energy / Today I need**；一句引言（如 "Make something fun today."）；底部次按钮 **Hold to Talk**（2026-09-15 加） | **此页即今日状态页，待机后持续显示**；点 Hold to Talk 直接进语音记录 |
| 6 | **Voice Note (Listening)** | "Listening..."；猫 + 麦克风 + 声波；"Hold the AI button and speak naturally."；示例 "Today was a good day... I learned something new..."；右上 × 取消 | 长按 AI 键说话，松开结束 |
| 7 | **Voice Note (Saved)** | 猫读书 + 心；"Thanks! Your note is saved."；笔记卡片（**中文正文** + 时间戳 SEP 12 10:21）；按钮 **View All Notes →** | 跳转 Notes |
| 8 | **Notes** | "< My Notes" + 右上 ⋯；笔记卡片列表（时间戳 + 中文正文）；深色按钮 **+ New Voice Note** | 滚动/翻页看历史 |

品牌文案（页脚）：`me.status — A KINDER DAY STARTS HERE.` / `Check in. Breathe. Be you.`

## 4. 数据与存储（用户明确要求）

- 每天的心情记录（mood + energy + intention）→ **存 SD 卡**
- 每一条语音笔记（转写后的文字）→ **存 SD 卡**
- 历史：可查看**最近三个月**的笔记
- 语音：长按 AI 键说话 → **AI 转文字** → 存 SD 卡
- 完成 check-in 后状态页**在设备待机时继续显示**（利用墨水屏断电保持图像的特性）

## 5. 视觉风格

- **圆润字体**
- **猫猫插画**（线稿风格，天然适合 1-bit）
- 轻松、可爱、圆润
- UI 文案为**英文**（设计稿如此）；笔记正文为用户口语，**中文**

---

## 6. 与硬件约束的逐条对照

| 需求 | 硬件现实 | 结论 |
|---|---|---|
| 状态页待机后持续显示 | 墨水屏断电后图像保留，deep sleep 前全刷即可 | ✅ **零风险，完美契合**。渲染 Completed → 全刷 → 停触摸轮询 → 屏休眠 → deep sleep |
| 数据存 SD 卡 | SD 与屏幕**共用 SPI2**，必须按片选严格串行化；且官方 registry **没有任何公开示例用过这张 SD 卡**，时序/上拉未验证 | ⚠️ **关键路径**。这是硬需求里风险最高的一条，必须最先验证，不能留到后期 |
| 语音转文字 | ESP32-S3 **无法在本地做中文 STT**，必须走云端（Wi-Fi 上传音频） | ✅ **已拍板 D1**：用自己的 Qwen key（`qwen3-asr-flash`，base64 音频）。录音 + 上传是两大耗电点，只在提交笔记时短暂开 Wi-Fi |
| 笔记正文显示中文 | 自带字体只有 5×7 ASCII。UI 英文可用圆润英文字体子集，但笔记卡片要显示**任意中文** | ✅ 可行：GB2312 点阵子集烧进 flash。估算 6763 字 × 24px 高 1-bit ≈ 0.5 MB，32 MB flash 完全够。需要建一条字体生成管线 |
| 长按 AI 键说话 | AI 键（GPIO 4）同时是电源/唤醒键 | ✅ 可行：需区分短按（唤醒/确认）与长按（录音）。已 vendored Espressif `iot_button`，原生支持长按事件 |
| 录音 | PDM 麦克风引脚有文档，但**无公开的采样率/抽取配置**，未验证 | ⚠️ 需早期验证（Phase 0） |
| 右上日期时间（SEP 12 08:30） | 有 PCF8563 RTC；Wi-Fi 可做 SNTP 校时 | ✅ 必做，且与需求无关（心情记录必须有可靠时间戳） |
| 三个月历史 | SD 按天存文件；Notes 页分页渲染 | ✅ 可行。翻页用局部刷新 + 每 20 次一次全刷防残影 |
| 猫插画 + 圆润字体 | 需要把插画转 1-bit 位图、把圆润字体栅格化到用到的字号 | ✅ 可行，工作量在资产制作而非代码（待决策 D6） |
| 多点/手势 | 设计稿只需要 tap，不需要滑动手势 | ✅ 简化：Notes 页翻页用屏幕上的按钮或上下键，不依赖滑动 |

## 7. 待拍板的技术决策

### D1. 语音转文字 —— ✅ 已拍板（2026-09-14）

**用我们自己的 Qwen API key（BYOK）**：设备录 WAV → HTTPS 一次性同步调用
`qwen3-asr-flash`（音频以 base64 data URI 直接放在请求体里，不需要先上传到公网 URL）→ 拿中文转写。

原来的选项 b（复用 Seeed 官方 AI 管线）因此不再需要评估 —— 官方固件闭源这件事**不再是阻塞项**。
已核实的接口契约、体积/延迟估算、以及 `SttClient` 可换后端的架构，见 `docs/FEASIBILITY.md` §3。

仍需用户自己定的：**百炼侧的数据留存政策**（心情日记是高度私密内容，音频会离开设备），
以及 API key 和 WorkspaceId。

### D2. Wi-Fi 凭据怎么进设备？

候选：Seeedash App 配网 / BLE 配网 / SmartConfig / 设备开热点的 Web 配网页。
D1 既然是 BYOK，配网就完全由我们自己实现，不受 Seeed 体系约束。
**本期按决议 D2 先编译期写死**（Wi-Fi SSID/密码 + Qwen key），配网留到产品化阶段再做。

### D3. 用户名 "Winnie" 从哪来？

我的默认：固件里一个可编译期配置的名字 + 后续加一个设置页。你也可以要求配网时输入。

### D4. SD 上的数据格式（我的默认，可改）

```
/notes/YYYY-MM-DD.jsonl     每行一条笔记 {ts, text}
/status/YYYY-MM-DD.json     当天 check-in {ts, mood, energy, intention}
```

JSON 行格式便于追加写（SD 磨损友好）和按天过滤三个月。

### D5. 三个月之后

我的默认：**保留在 SD，只是 Notes 页不显示**（滚动窗口只影响显示，不删数据）。

### D6. 猫插画和字体从哪来？（部分已到位）

**英文字体：✅ 已到位**，在 `assets/fonts/`，三款都是 SIL OFL 1.1，允许嵌入和再分发：

| 字体 | 特点 | 适合 |
|---|---|---|
| Manrope | x-height 高、笔画均匀，小字号最稳 | 正文 / 笔记卡片 |
| Plus Jakarta Sans | 比 Manrope 略宽、更中性 | 正文备选 |
| Playpen Sans | 圆润手写感，最贴合"可爱"基调；笔画偏粗 | 标题 / 心情词，14–16 px 下容易糊 |

**中文字体：✅ 已定为思源柔黑 GenSenRounded2 TC（v2.100，SIL OFL 1.1）。**
苹方是 Apple 专有字体不能嵌入分发（结论不变），改用 OFL 的思源柔黑；
上游无 SC 包但 TC 版实测简体码位齐全、UI 字符串零缺字，1-bit 16/18 px 下 M 字重最稳。
GB2312 子集 M/B 两字重已入库 `assets/fonts/gensen-rounded/`，实测数据见该目录 README。

**猫插画 + 图标：✅ 已交付。** 11 只猫 + 12 个图标全部按 `docs/ASSET-SPEC.md`
的目标尺寸做成 1-bit 成品，在 `assets/1bit/`；线稿源在 `assets/src/`，
生成管线在 `tools/art/`（postprocess.py / gen_icons.py / contact_sheet.py），
验收拼版 `assets/preview-contact-sheet.png`。合计 23 KB。

## 8. 实施顺序

**Phase 0 — 风险验证（与 UI 无关，最先做）**
1. SD 卡读写，且与屏幕 SPI2 串行化不冲突
2. PDM 麦克风录音到 SD
3. Wi-Fi 连接 + SNTP 校时 + PCF8563 RTC 保持
4. deep sleep + GPIO 4 唤醒 + RTC 保持内存 magic 标记
5. 装 ESP-IDF v5.4，把现有 bring-up 编译烧录一遍（**目前代码一次都没编译过**）

**Phase 1 — 资产与渲染框架**
6. 1-bit 资产管线（插画 → 位图）
7. 圆润英文字体子集 + GB2312 中文点阵字库
8. 页面框架：8 页路由、进度条、返回、状态栏（品牌头 + 日期时间）

**Phase 2 — check-in 主流程**
9. Home → Mood → Energy → Intention → Completed
10. 写 SD（`status/`）→ 全刷 → deep sleep 保持状态页

**Phase 3 — 语音笔记**
11. 长按 AI 键录音 → Listening 页
12. STT（按 D1 决议）→ Saved 页 → 写 SD（`notes/`）

**Phase 4 — 历史**
13. Notes 页：三个月过滤、分页、翻页防残影

## 待办

已完成：

- [x] 把设计稿原图拷进 `docs/design/mockup-8pages.png` 存档
- [x] D1（Qwen BYOK）、D2（凭据写死）、D7（SD 先假设可行）、D8（Lily 顾问）、D9（无真机）、D10（状态不赌 RTC 内存）
- [x] 装 ESP-IDF v5.4 并编译 bring-up（镜像 293 KB）
- [x] 整理给 Seeed 的问题清单 `docs/QUESTIONS-FOR-SEEED.md`

待用户：

- [x] **中文字体换一个**（苹方不能嵌入分发）—— 已定思源柔黑 GenSenRounded2 TC，见 §7 D6
- [x] **猫插画**按 `docs/ASSET-SPEC.md` 出图 —— 已用 ImageGen 线稿 + `tools/art/` 转 1-bit 交付
- [ ] **Qwen API key + WorkspaceId + 地域**（S7 可以立刻在电脑上端到端验证，不需要真机）
- [ ] 把 `docs/QUESTIONS-FOR-SEEED.md` 转给 Lily，尤其是 §C（电源时序）和 §A（SD）

待硬件（借测）：

- [ ] S1b–S6、S8：烧录验证、按键极性、SD 并发、PDM 录音、掉电恢复、SNTP/RTC、语音全链路
