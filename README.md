# Arduino Suite（Arduino 项目集）

本仓库是作者的 Arduino 学习与项目合集，包含多个传感器驱动示例和主项目 **smart-farm（智慧农场）**。

## 📌 主项目：smart-farm（智慧农场）

基于 Arduino Uno/Nano 的智能花盆：自动感应光照与环境湿度，实现**天黑自动开灯、土壤过干自动浇灌**，并通过 I2C LCD 实时显示状态、串口输出调试信息。

### 功能特性

- 🌙 **自动照明**：光敏传感器检测天黑 → 点亮 WS2812 RGB 灯板（白光）；天亮 → 熄灭
- 💧 **自动浇灌**：土壤湿度低于阈值 → 继电器吸合 → 启动水泵；湿润后自动停机
- 📟 **LCD 轮替显示**（英文）：`Welcome Charles` → `Soil Moisture(%)` → `Pump Status(ON/OFF)` → `Light Status(BRIGHT/DARK)`
- 🔍 **串口调试**：每 2 秒打印土壤 ADC/电压/湿度、DO 电平、干湿状态、光照/泵/灯状态
- ✨ **开机自检**：WS2812 灯板彩虹滚动自检 → 水泵继电器咔哒 3 次 → 进入正常工作

### 硬件清单

| 模块 | 说明 |
|---|---|
| Arduino Uno / Nano | 主控 |
| 土壤湿度模块（LM393） | 模拟 AO + 数字 DO |
| 光敏模块（3 针：VCC/GND/DO） | 数字量，电位器调阈值 |
| 继电器模块（SRD-05VDC） | 跳线帽在 **H 侧 = 高电平触发** |
| WS2812 16 位 RGB 灯板 | 内置全彩驱动，作照明灯 |
| 水泵（3V）+ 2 节 5 号电池 | 独立供电，不经 Arduino |
| I2C LCD1602（地址 0x27） | 显示 |

### 接线表（对照实物逐条检查）

```
【土壤湿度模块】 VCC→5V   GND→GND   AO→A0   DO→D2
【光敏模块】     VCC→5V   GND→GND   DO→D3
【继电器模块】   VCC→5V   GND→GND   IN→D8
                负载侧: COM→电池+   NO→泵+   电池-→泵-
【WS2812 灯板】  5V→5V   GND→GND   DI→D6   DO→空
【I2C LCD】      VCC→5V   GND→GND   SDA→A4  SCL→A5
【板载 LED】     D13(自动)
```

### 依赖库（Arduino IDE → 库管理器）

- `Adafruit NeoPixel`（驱动 WS2812 灯板）
- `LiquidCrystal_I2C`（驱动 I2C LCD）

### 构建 / 烧录

1. 选择开发板：**Arduino Uno** 或 **Arduino Nano**，并选对端口
2. 在库管理器安装上述两个库
3. 打开 `smart-farm/smart-farm/smart-farm.ino`，点击上传
4. 打开串口监视器（波特率 **9600**）查看调试输出

> ⚠️ 提示：WS2812 灯板 16 颗全亮电流较大（接近 1A），若 Arduino 5V 供电导致复位，请给灯板单独供电（负极共地）或降低 `LAMP_BRIGHTNESS`。

### 关键可调参数（`smart-farm.ino` 顶部）

| 宏 | 说明 |
|---|---|
| `DRY_VALUE` / `WET_VALUE` | 干/湿 ADC 标定值（用串口实测后填入） |
| `DRY_PCT` / `WET_PCT` | 过干/过湿阈值百分比 |
| `DARK_IS_LOW` | 光敏触发极性（实测后设 true/false） |
| `RELAY_ACTIVE_LOW` | 继电器触发极性（本模块跳线帽 H 侧 → false） |
| `LAMP_BRIGHTNESS` | 灯板亮度 0-255 |
| `UPDATE_DELAY` / `LIGHT_CHECK_MS` / `DEBOUNCE_MS` | 刷新与防抖间隔 |

---

## 🧪 其他草图（实验/调试）

| 文件夹 | 说明 |
|---|---|
| `LDR_LED` / `LDR_PlantLight` | 光敏控灯实验 |
| `SoilMoisture_Test` | 土壤湿度模块读数测试 |
| `RelayTest` | 继电器触发极性测试 |
| `DHT11_MAX7219_temp_hum` / `LM35D_MAX7219_temp` | 温湿度/温度 + MAX7219 数码管 |
| `RGB_Blink` | RGB LED 闪烁 |
| `I2C_Scanner` / `LCD1602_Hello_Charles` / `LCD1602_Marquee_Hello_Charles` | I2C / LCD 相关测试 |
| `HelloWorld` / `Hello_Charles` | 入门示例 |
| `Test_Pin6` / `Test_Pin7` | 引脚测试 |

## 📄 文档

- `smart-farm-智慧农场.html`：项目说明网页
- `max7219_接线示意图.svg`：MAX7219 接线示意图
- `SG-346_Arduino_知识库.md`：Arduino 知识库笔记
