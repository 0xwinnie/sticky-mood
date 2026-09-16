# Vendored components

这些组件不是本项目原创，从上游原样拷入，未做修改。保留在此是为了让项目可以独立构建，
不需要额外拉取仓库。

| 组件 | 来源 | 许可证 | 用途 |
|---|---|---|---|
| `seeed_epaper/` | Seeed 官方墨水屏驱动，取自 [reterminal-sticky-playground-registry](https://github.com/Seeed-Projects/reterminal-sticky-playground-registry) `firmwares/sticky-2048/source/components/seeed_epaper` | MIT（registry 根 LICENSE） | SSD1677 / UC8179 面板驱动，全刷/局部刷/4 级灰度 |
| `gt911/` | 同上，`components/gt911` | MIT | GT911 电容触摸，地址自动探测 |
| `bq27220/` | 同上，`components/bq27220` | MIT | BQ27220 电量计（I2C1 @ 0x55） |
| `debug_logging/` | 同上，`components/debug_logging` | MIT | 墨水屏/触摸调试日志开关，见其 `Kconfig` |
| `button/` | [espressif/esp-iot-solution](https://github.com/espressif/esp-iot-solution/tree/master/components/button) v4.1.6 | Apache-2.0（见 `button/license.txt`） | 消抖、长按、组合键 |

`bq27220/` 和 `button/` 目前**已拷入但尚未被 bring-up 代码使用**，
留给状态栏（电量）和按键交互。它们没有出现在 `main/CMakeLists.txt` 的 `REQUIRES` 里，
用到时再加。

## 打开墨水屏/触摸调试日志

```bash
idf.py menuconfig
# Application Debug Logging -> Enable e-paper/touch debug logs
```

## 更新 vendored 组件

```bash
git clone --depth 1 --filter=blob:none --sparse \
  https://github.com/Seeed-Projects/reterminal-sticky-playground-registry.git /tmp/reg
cd /tmp/reg && git sparse-checkout set firmwares/sticky-2048/source
cp -R /tmp/reg/firmwares/sticky-2048/source/components/{seeed_epaper,gt911,bq27220,debug_logging} \
      <本项目>/components/
```
