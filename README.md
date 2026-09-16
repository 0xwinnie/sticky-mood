# sticky-mood

Seeed **reTerminal Sticky**（3.97" 磁吸墨水屏，ESP32-S3）上的**触屏心情/状态记录** App 固件。

> **当前状态：硬件 bring-up，尚无任何心情记录逻辑。**
> 现在编译出来的是一个验证屏：显示触摸坐标、手势、命中了哪个测试框、按键计数。
> 目的是在写任何业务逻辑之前，先把"工具链 + 墨水屏 + 触摸变换 + 按键"这条最有风险的链路跑通。
> 产品需求还是开放问题，见 [`docs/REQUIREMENTS.md`](docs/REQUIREMENTS.md)。

## 硬件

| | |
|---|---|
| SoC | ESP32-S3R8，双核 LX7 @ 240 MHz |
| 内存 | 8 MB octal PSRAM / 32 MB QSPI flash |
| 屏幕 | 3.97" 墨水屏，SSD1677，原生 800×480 横屏，1-bit 黑白 + 4 级灰度 |
| 触摸 | GT911 电容触摸（I2C0） |
| 按键 | AI/电源（GPIO 4）、上（5）、下（6） |
| 其他 | SHT40 温湿度、LSM6DS3TR-C IMU、PCF8563 RTC、PDM 麦克风、蜂鸣器、microSD |
| 电源 | 750 mAh，BQ27220 电量计，BQ25616 充电，USB-C |

完整引脚映射、总线拓扑、电源自锁电路、刷新策略和已知歧义见 [`docs/HARDWARE.md`](docs/HARDWARE.md)。

## 目录结构

```
sticky-mood/
  CMakeLists.txt          ESP-IDF 顶层工程
  sdkconfig.defaults      板级事实（32MB flash、octal PSRAM 等），别改错
  components/             vendored 驱动，见 components/VENDORED.md
    seeed_epaper/           SSD1677 面板驱动（Seeed 官方）
    gt911/                  电容触摸
    bq27220/                电量计（已拷入，bring-up 未用）
    button/                 Espressif iot_button（已拷入，bring-up 未用）
    debug_logging/          墨水屏/触摸调试日志开关
  main/
    main.cpp              主循环：触摸/AI 键 -> 状态机 -> 渲染 -> 刷新策略
    pin_config.h          全部 GPIO 定义
    board/power.cpp       电源自锁（app_main 第一件事）
    hardware/
      sticky_display.cpp  面板 + PSRAM 帧缓冲 + 旋转（唯一的旋转实现处）+ blit_ui
      sticky_touch.cpp    GT911 轮询任务 + 手势分类 + 匹配的坐标变换
    ui/                   零 ESP-IDF 依赖的 UI 核心（host 模拟器编译同一份代码）
      framebuffer.cpp     1-bit 480x800 画布
      bitmap_font.cpp     点阵字库渲染（utf8 -> 字形 blit）
      art.cpp             1-bit 精灵渲染
      resources.cpp       字面/精灵名字表
      layout.h            网格常量 + 字面枚举
    pages/
      pages.cpp           8 页渲染 + 热区 + 状态机（纯 C++）
    app/
      app_state.h         AppState（page/mood/energy/intention/...）
      device_resources.cpp  从固件内嵌 blob 装载 Resources
  assets/
    fonts/                字体源文件（不烧进固件，只用来生成点阵字库），见 assets/fonts/README.md
      manrope/              OFL 1.1
      playpen-sans/         OFL 1.1
      plus-jakarta-sans/    OFL 1.1
      gensen-rounded/       中文：思源柔黑 GenSenRounded2 TC，OFL 1.1，GB2312 子集 M/B
    src/                  猫插画高清线稿源（AI 出图，人工验收过）
    1bit/                 23 个 1-bit 成品（11 猫 + 12 图标），进固件的就是这些
    preview-contact-sheet.png  全部资产的验收拼版
  tools/
    art/                  资产管线：postprocess.py（线稿→1-bit）/ gen_icons.py / contact_sheet.py
    font/                 make_font.py：字体 -> 点阵 blob（build/font_data）
    sim/                  host 模拟器 + 网页可点击预览（build/sim/index.html）
    build_assets.sh       生成固件要内嵌的 blob（idf.py build 前先跑）
  docs/
    HARDWARE.md           硬件参考（GPIO、总线拓扑、电源自锁、刷新策略、已知歧义）
    REQUIREMENTS.md       需求与决议（权威需求来源）
    FEASIBILITY.md        功能可行度分析（判定表、容量/功耗预算、spike 清单）
    ASSET-SPEC.md         猫插画 / 图标 / 字体的交付规格
    QUESTIONS-FOR-SEEED.md  给 Seeed（Lily）的问题清单 + 借测计划
    design/mockup-8pages.png  设计稿存档
```

架构遵循官方推荐的分层：

```
数据:  Touch/AI 键 -> pages::on_tap -> AppState -> pages::render -> FrameBuffer -> blit_ui -> Display
输入:  Button/Touch -> AppEvent -> handle -> Page/Display
```

两条硬规则：

- **页面只读 AppState，绝不直接碰硬件。**
- **ISR 和驱动回调只投递事件。** 全刷耗时以秒计，在回调里刷面板会触发看门狗复位。

## 环境准备

本机已装好 **ESP-IDF v5.4**（`~/esp/esp-idf`，target esp32s3），本仓库的 bring-up 已编译通过。

每个新 shell 里激活工具链：

```bash
. ~/esp/esp-idf/export.sh
idf.py --version   # 应显示 ESP-IDF v5.4.x
```

v5.4 是官方 demo 和 registry CI 都声明的版本，别随意升。

换机器重装时注意本机网络的两个坑（GitHub 大文件传输经常被 reset）：

1. **主仓库从 gitee 镜像拉**：`git clone -b v5.4 https://gitee.com/EspressifSystems/esp-idf.git`，
   然后 `./install.sh esp32s3`。
2. **子模块必须改回 GitHub**：gitee 镜像的 `.gitmodules` 指向 gitee，而镜像的子模块仓库要账号，
   会报 `could not read Username for 'https://gitee.com'`。用一次性的 URL 重写（不改 git config）：

   ```bash
   cd ~/esp/esp-idf
   git -c url."https://github.com/".insteadOf="https://gitee.com/" \
       submodule update --init <path>
   ```

   不要加 `--depth 1`：第三方仓库（cJSON、micro-ecc、Unity、CMock、spiffs、protobuf-c）
   拒绝按 SHA 浅取（`upload-pack: not our ref`），全量克隆反而能成功。

   编译必需的子模块：`mbedtls/mbedtls`、`lwip/lwip`、`esp_wifi/lib`、`esp_phy/lib`、
   `esp_coex/lib`、`heap/tlsf`、`json/cJSON`、`mqtt/esp-mqtt`、`unity/unity`、`cmock/CMock`、
   `spiffs/spiffs`、`protobuf-c/protobuf-c`、`bootloader/.../micro-ecc`。
   蓝牙和 OpenThread 的库体积大且本项目不用，没拉 —— 因此构建时要带
   `IDF_SKIP_CHECK_SUBMODULES=1`（见下）。

## 构建与烧录

```bash
export IDF_SKIP_CHECK_SUBMODULES=1   # 没拉 BT / OpenThread 子模块，跳过 cmake 的全量子模块检查
idf.py set-target esp32s3            # 只需第一次
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

bring-up 实测产物：app 镜像 **约 293 KB**（Flash Code 154 KB + Flash Data 58 KB，含 Wi-Fi 协议栈），
DIRAM 用了 21%。字体和猫插画的预算对照见 `docs/FEASIBILITY.md`。

macOS 上端口形如 `/dev/cu.usbmodem*`；Linux 是 `/dev/ttyUSB0` 或 `/dev/ttyACM0`；Windows 是 `COM5`。

要发布到 Playground，构建时显式带版本号，让产物和声明的 release 一致：

```bash
idf.py -D PROJECT_VER=1.0.0 build
```

## 两个必知的坑

**1. 电源自锁。** 设备不是"通电即常开"，主电源轨由锁存电路控制，固件负责拉高
`POWER_HOLD`(45) 并脉冲 `POWER_LOCK`(46)。`board::power_on_hold()` 必须是 `app_main()`
的第一句，否则松开电源键后设备立刻掉电，表现为黑屏或卡在重启循环。

**2. 新增 .cpp 要加进 `main/CMakeLists.txt` 的 `SRCS`。** 忘加的话链接照样通过，
代码静默不执行 —— 这是"我的页面怎么不刷新"最常见的原因。

更多故障对照表见 `docs/HARDWARE.md`。

## 屏幕方向

当前是**竖屏 480×800**（原生 800×480 旋转 270° + `mirror_x = true`），
因为这是在量产硬件上验证过、且显示与触摸变换互相匹配的组合。

旋转只实现在 `StickyDisplay::draw_pixel()` 一处，触摸的反向变换在
`StickyTouch::read_touch()` 一处。**改一个必须同时改另一个**，否则会出现
"显示正常但触摸错位"。方向仍是待确认的需求项（`docs/REQUIREMENTS.md` Q5）。

## 发布到 Sticky Playground

社区固件走 [Seeed-Projects/reterminal-sticky-playground-registry](https://github.com/Seeed-Projects/reterminal-sticky-playground-registry)：
在 `firmwares/<id>/` 下提交 `firmware.json` + `README.md` + `assets/preview.jpg` +
可构建的 `source/`，CI 用 ESP-IDF 构建并打包，合并后发布 Release，
用户在 [Firmware 页面](https://www.seeedstudio.com/sticky/playground/firmware/) 用浏览器直接烧录。

Registry 支持 ESP-IDF / PlatformIO / Arduino 三种构建方式，本项目用 ESP-IDF。

## 许可证

MIT，见 [LICENSE](LICENSE)。`components/` 下的第三方代码保留其各自许可证，
见 [components/VENDORED.md](components/VENDORED.md)。
