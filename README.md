确保你的wav文件有采样率信息。
# wavCompositorExtended

![License](https://img.shields.io/badge/license-GPLv3-red.svg)
![C++17](https://img.shields.io/badge/C++-17-blue.svg)

`wavCompositorExtended` 是一个功能增强型命令行音频合成工具，支持将多个 `.wav` 文件按时间线、音量增益精确混合，自动处理采样率差异，并输出高质量归一化音频。

> 本项目基于原始 [wavCompositor](https://github.com/Na2Cr2O7/wavableMidi/tree/main/wavCompositor) 修改而来，增加了对文件名含空格、命令行参数、重采样等功能的支持。

---

## 🚀 核心特性

- ✅ **支持空格文件名**：输入文件可包含空格（自动解析）
- ✅ **时间轴混合**：支持毫秒级精度的时间偏移
- ✅ **独立音量控制**：每个音频可设置增益
- ✅ **自动重采样**：支持任意输入采样率 → 目标采样率
- ✅ **立体声输出**：自动适配单/双声道
- ✅ **智能裁剪与归一化**：去静音尾 + 防爆音
- ✅ **命令行友好**：支持 `-o`, `-s`, `-h`
- ✅ **跨平台**：Windows / Linux / macOS

---

## 📦 构建方法

### 依赖

- C++17 编译器
- [`AudioFile.h`](https://github.com/adamstark/AudioFile) — **MIT 许可证**（见下方说明）

## 🛠 使用方式

```bash
wavCompositorExtended <input.txt> [-o output.wav] [-s <sample_rate>] [-h]
```

### 输入文件格式

每三个参数一组：

```text
audio file with spaces.wav 0.0 1.0
another.wav 2.5 0.8
```

支持路径中包含空格、多空格分隔、换行等。

---


> ⚠️ 尽管 `AudioFile.h` 使用 MIT 许可证，**本项目其余代码（main.cpp 等）均以 GPLv3 发布**。MIT 兼容 GPLv3，因此整体分发合法。

---
