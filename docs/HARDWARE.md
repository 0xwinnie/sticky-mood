# reTerminal Sticky — 硬件参考

本项目针对 Seeed **reTerminal Sticky**（3.97" 磁吸墨水屏）。以下信息来自官方硬件文档，
并与 Playground Registry 中一个已在量产硬件上运行的 app 的 `pin_config.h` 交叉核对过。

**来源**
- [官方硬件概览](https://www.seeedstudio.com/sticky/docs/en/device-guide/hardware-overview/)
- [Seeed-Projects/reterminal-sticky-playground-registry](https://github.com/Seeed-Projects/reterminal-sticky-playground-registry)
- 官方文档：[显示刷新与低功耗](https://www.seeedstudio.com/sticky/docs/en/device-guide/esp-refresh/)、[ESP-IDF 基础](https://www.seeedstudio.com/sticky/docs/en/device-guide/esp-basics/)、[页面与外设](https://www.seeedstudio.com/sticky/docs/en/device-guide/esp-pages/)

## 核心规格

| 项目 | 参数 |
|---|---|
| SoC | ESP32-S3R8，双核 Xtensa LX7，最高 240 MHz |
| PSRAM | 8 MB，octal SPI，80 MHz |
| Flash | 32 MB QSPI |
| 屏幕 | 3.97" 墨水屏，**SSD1677** 控制器，原生 **800×480 横屏** |
| 显示模式 | 1-bit 黑白；4 级灰度（黑/深灰/浅灰/白） |
| 触摸 | **GT911** 电容触摸，挂在 I2C0 |
| 存储 | microSD，走 SPI（与屏幕共用 SPI2） |
| 传感器 | SHT40 温湿度、LSM6DS3TR-C 六轴 IMU、PCF8563 RTC |
| 音频输入 | PDM 麦克风 |
| 音频输出 | 蜂鸣器（PWM） |
| 无线 | Wi-Fi 4 仅 2.4 GHz；蓝牙 5.0 LE |
| 电池 | 750 mAh 锂电，BQ27220 电量计，BQ25616 充电管理 |
| 充电 | USB-C，5 V / 1 A |
| 按键 | AI/电源、上（上一页）、下（下一页）、凹陷式复位（CHIP_PU） |
| 尺寸/重量 | 106 × 65.5 × 7.3 mm，70 g |
| 防护 | **IP40**（防尘，不防溅） |
| 安装 | 四角 N52 磁铁 + 磁吸环配件 |

厂商标称续航：典型待机约 7 天；静态内容 + 省电模式可达"数月"。

## 完整 GPIO 映射

见 `main/pin_config.h`（与下表一致）。

### 墨水屏（SPI2）

| 信号 | GPIO |
|---|---|
| MOSI | 14 |
| SCK / CLK | 13 |
| MISO | 12 |
| CS | 15 |
| DC | 16 |
| RST | 17 |
| BUSY | 18 |
| EN（面板供电使能） | 47 |

已知可用的 SPI 参数：`SPI2_HOST`、mode 0、10 MHz、`busy_level = 1`、`enable_level = 1`、
`busy_timeout_ms = 10000`、`reset_low_ms = 10`、`reset_high_ms = 10`。

### microSD（与屏幕共用 SPI2）

| 信号 | GPIO |
|---|---|
| CS | 8 |
| MISO / SCK / MOSI | 12 / 13 / 14 |

卡和面板共用一条 SPI 总线，只靠片选区分。**必须串行化访问**：面板传输或 busy 等待期间
绝不能发起 SD 传输。

### 触摸 GT911（I2C0）

| 信号 | GPIO |
|---|---|
| SCL | 2 |
| SDA | 3 |
| INT | 21 |
| RST | 41 |
| EN | 42 |

GT911 支持两个地址（`0x5D` / `0x14`），取决于上电时 INT 的电平；驱动会自行探测。

### 传感器总线（I2C1）

| 信号 | GPIO |
|---|---|
| SCL | 0 |
| SDA | 1 |
| INT（见下方歧义） | 7 |

挂载设备：SHT40、LSM6DS3TR-C、PCF8563、BQ27220（地址 `0x55`）。

### 按键

| 按键 | GPIO | 备注 |
|---|---|---|
| AI / 电源 | 4 | 同时是 deep sleep 的 EXT1 唤醒源（低电平唤醒） |
| 上 / 上一页 | 5 | |
| 下 / 下一页 | 6 | |
| 复位 | CHIP_PU | 物理针孔，不占 GPIO |

### 电源控制

| 信号 | GPIO | 备注 |
|---|---|---|
| POWER_HOLD | 45 | 必须拉高才能锁存主电源轨 |
| POWER_LOCK | 46 | 脉冲一次以提交 hold/release |
| EN_BAT_CHGn | 39 | 充电使能，**低有效** |
| CHARGE_STATE | 40 | 充电状态输入 |
| EXTERNAL_POWER | 9 | 接 USB/外部电源时为高 |

### 其他

| 功能 | GPIO |
|---|---|
| PDM 麦克风 CLK / DATA | 19 / 20 |
| 蜂鸣器（LEDC PWM） | 48 |
| USB 串口 TX / RX | 43 / 44 |

## 总线拓扑

```
SPI2 ──┬── SSD1677 墨水屏   (CS 15, DC 16, RST 17, BUSY 18, EN 47)
       └── microSD          (CS 8)

I2C0 ───── GT911 触摸       (SCL 2,  SDA 3,  INT 21, RST 41, EN 42)

I2C1 ──┬── SHT40            温湿度
       ├── LSM6DS3TR-C      六轴 IMU
       ├── PCF8563          RTC
       └── BQ27220 @0x55    电量计
                            (SCL 0, SDA 1)
```

共享总线必须**只创建一次**（放在 board 层），然后把句柄传给各驱动。两个驱动各自调用
`i2c_new_master_bus()` 是常见故障源。

## 电源自锁（必做）

设备不是"通电即常开"。主电源轨由一个锁存电路控制，**固件负责这个锁存**：

- 保持开机：`POWER_HOLD`(45) 拉高，然后 `POWER_LOCK`(46) 做一次 低→高→低 的短脉冲。
- 关机：`POWER_HOLD` 拉低，再做同样的脉冲。

如果固件没有拉高 hold，松开电源键后设备会立刻掉电，表现为"黑屏"或"卡在重启循环"。
实现见 `main/board/power.cpp`，必须是 `app_main()` 的第一件事。

充电由 BQ25616 管理：GPIO 39（低有效）可禁止充电，GPIO 40 读充电状态，GPIO 9 读 USB 在位。
电量、电压、电流通过 I2C1 上的 BQ27220 读取。

## 屏幕方向：最需要先定的决定

面板原生横屏 800×480，但产品两种朝向都有人用，公开参考实现因此不一致：

| 实现 | 逻辑画布 | 旋转 | 备注 |
|---|---|---|---|
| 官方 dashboard demo | 800×480 横屏，原点左上 | 显示层做 180° | 页面直接按横屏绘制 |
| Registry app `sticky-2048` | 480×800 竖屏 | 270°，且 `mirror_x = true` | 竖屏游戏布局 |

两者都不算"唯一正确"。**选定一个方向后，旋转只在像素写入这一个地方实现，并对触摸坐标
做匹配的变换。** 混用约定会得到"显示正常但触摸是旋转/镜像的"。

本项目当前采用**竖屏 480×800（270° + mirror_x）**，因为这是在量产硬件上验证过的组合，
见 `main/hardware/sticky_display.cpp` 与 `main/hardware/sticky_touch.cpp`。
方向仍是待确认的需求项，见 `docs/REQUIREMENTS.md`。

## 帧缓冲

- 格式 `SEEED_EPAPER_PIXEL_FORMAT_MONO1_MSB`：每像素 1 bit，MSB 在前。
- **置位为白，清零为黑。** 清成白屏是 `memset(0xFF)`。
- 行跨度 `800 / 8` = **100 字节**；总大小 `100 × 480` = **48000 字节**。
- 分配在 PSRAM，失败时回退内部 RAM。
- `spi_bus_config_t.max_transfer_sz` 必须设为整个缓冲区大小，否则驱动会拆分或拒绝传输。

## 刷新模式

| 模式 | 适用 | 代价 |
|---|---|---|
| `GRAY4` 全刷 | 真正用到灰阶的画面（照片、首页） | 最慢，闪烁最明显 |
| `FULL` 黑白全刷 | 纯黑白画面：文字、传感器、电量、笔记、休眠画面 | 慢，整屏闪一次 |
| `PARTIAL` 局部刷 | 小幅增量变化，比如状态栏的分钟 | 快，不闪 |

按页面内容自适应选择，不要硬编码：只有黑白像素的页面用 `GRAY4` 是白白付灰阶代价。

`sticky_display_refresh_partial()` 语义要注意：它**不接受矩形参数**，而是提交整帧，
控制器自动放过未变化的像素。所以"局部"的含义是"因为大部分帧没变所以便宜"，
不是"我只更新了一个子矩形"。

由此推出局部刷新的正确姿势 —— 因为你替换了什么就必须先擦掉什么：

1. 先用白色填充旧内容区域；
2. 再画新内容；
3. 最后调用局部刷新。

跳过第 1 步会让旧字形和新字形按位或叠在一起。

## 残影（ghosting）

- 局部刷新会累积残影。周期性强制全刷 —— registry 的游戏每 20 次局部刷新做一次全刷，
  这是个合理默认值。
- 页面切换时、进入 deep sleep 前，都要做一次全刷，保证留存画面干净。
- 面板休眠、设备断电后画面依然保留。**这是特性**：先渲染好你希望用户看到的画面，再睡。

## 性能与功耗

- 全刷耗时以秒计且阻塞。**绝不要**在回调、ISR 或持锁状态下调用。
- 面板有 EN 脚（GPIO 47），更新之间给面板断电是设备能做到数月续航的一部分原因。
- 10 MHz SPI 是已知可用的时钟；瓶颈不在传输，在面板波形。
- 要认真对待 BUSY（GPIO 18，有效电平 1）和 10 秒超时。BUSY 卡住通常意味着面板 EN
  没拉高，或复位时序太短。

## Deep sleep 与唤醒

Sticky 的休眠方式比较特殊：**它不会从中断处恢复**。唤醒后芯片重新进入 `app_main()`，
所以状态必须走带外通道保存。

官方 demo 的顺序：

1. 渲染一张"deep sleep"页面并做**黑白全刷**（断电后面板继续显示这张图）；
2. 停止触摸轮询；
3. 让墨水屏控制器进入休眠（`seeed_epaper_panel_sleep()`）；
4. 把 AppState 存进 RTC 保持内存，并打一个 magic 标记（`0x53544943` = `"STIC"`），
   以便区分冷启动和唤醒；
5. 配置 **GPIO 4 低电平** EXT1 唤醒（AI/电源键），进入 deep sleep。

开机时检查 magic：匹配则恢复保存的页面并用合适的刷新模式重绘，否则走冷启动。

## 已知歧义（设计前需实测确认）

- **GPIO 7**：官方硬件文档标注为 LSM6DS3TR-C IMU 中断，而 registry app 源码里作为
  BQ27220 电量计中断（`PIN_BFG_INT`）。两个器件都在 I2C1 上。GPIO 7 的归属视为
  **未验证**，要设计之前请用示波器确认，或一次只测一个中断。
- **microSD**：文档写的是 SPI 模式，但 registry 里没有公开示例真正用过 SD 卡，
  时序和上拉行为**未验证**。
- **PDM 麦克风**：引脚有文档，但没有公开的采样率/抽取率配置，麦克风初始化视为**未验证**。
- **按键极性**：本项目假定低有效 + 内部上拉，需在硬件上确认。
