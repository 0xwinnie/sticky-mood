# 字体源文件

这里的 `.ttf` 是**构建资产，不会烧进固件**。
固件里只放由它们生成的 1-bit 点阵字库（见 `docs/ASSET-SPEC.md`）。
需要具体字重时用 fontTools 的 `varLib.instancer` 从可变字体实例化，不必再下静态字重。

## 英文

| 目录 | 字体 | 授权 | 用途 |
|---|---|---|---|
| `manrope/` | Manrope（可变字重） | SIL OFL 1.1 | 候选：正文/UI，x-height 高，小字号在 1-bit 屏上最稳 |
| `plus-jakarta-sans/` | Plus Jakarta Sans（可变字重） | SIL OFL 1.1 | 候选：正文/UI，比 Manrope 略宽、更中性 |
| `playpen-sans/` | Playpen Sans（可变字重） | SIL OFL 1.1 | 候选：标题/心情词，圆润手写感，最贴合"可爱"基调；笔画粗，14–16 px 下容易糊 |

三个都是 OFL 1.1，**允许嵌入和再分发**，`OFL.txt` 与字体同目录保留即可。
最终选哪个/怎么搭配还没定（要等中文字体定了再一起看整体观感）。

## 中文

**已定：思源柔黑 GenSenRounded2 TC（v2.100，SIL OFL 1.1）**，在 `gensen-rounded/`。
用户最初点名的苹方（PingFang SC）是 Apple 专有字体，不能提取字形嵌入固件、更不能随固件分发
（本项目固件会发给 Seeed 借测，未来还可能发布到 Playground Registry），故弃用。

`gensen-rounded/` 里是 **GB2312 子集**（7544 码位）的 M / B 两个字重 + 授权文件 +
1-bit 实测预览；上游只发 TC 区域包但实测简体码位齐全，细节和覆盖表见该目录 README。
点阵档位：中文 16 / 18 / 24 / 28 px 四档，合计约 1.6 MB。

## 生成命令（待实现）

字体管线属于 Phase 1，脚本会放在 `tools/`，产物输出到 `main/ui/font_data/` 或独立 data 分区镜像。
现在还没有。
