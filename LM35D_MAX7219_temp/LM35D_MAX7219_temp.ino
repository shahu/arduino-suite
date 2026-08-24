// ==================================================================
//  LM35D 温度计 + MAX7219 4合1 点阵模块（Arduino UNO）
// ------------------------------------------------------------------
//  接线：
//    LM35D   : VCC -> 5V | VOUT -> A0 | GND -> GND
//    MAX7219 : VCC -> 5V | GND -> GND | DIN -> D11 | CS -> D10 | CLK -> D13
//  功能：每 3 秒读取一次温度，显示格式如 "25.6°"（4块8×8屏，每块一个字）
//  依赖：Arduino IDE 库管理器安装 "LedControl"（作者 Eberhard Fahle）
// ==================================================================
#include <LedControl.h>
#include <avr/pgmspace.h>

// ---------- 引脚 ----------
#define LM35_PIN A0
#define DIN_PIN  11
#define CLK_PIN  13
#define CS_PIN   10
#define DEVICES  4

// ---------- 刷新周期 ----------
#define REFRESH_MS 3000UL      // 每 3 秒刷新一次

// ==================================================================
//  ★ 方向修正开关：不同批次模块的排布方向可能不同。
//    烧录后如果显示不对，按现象修改下面的 true/false（改完重新上传）：
//      - 左右顺序颠倒（显示成 °6.52）  → REVERSE_MODULE_ORDER 改成 true
//      - 字符左右镜像                  → MIRROR_HORIZONTAL    改成 true
//      - 字符上下颠倒                  → MIRROR_VERTICAL      改成 true
// ==================================================================
#define REVERSE_MODULE_ORDER true
#define MIRROR_HORIZONTAL    false
#define MIRROR_VERTICAL      false

// ---------- 显示选项 ----------
#define SHOW_DEGREE  true    // 最后一位显示 ° 符号；false 则留空
#define SERIAL_DEBUG true    // 串口打印温度，方便核对
#define BRIGHTNESS   3       // 亮度 0~15，觉得暗可调大

// ---------- 温度测量选项 ----------
// 用内部 1.1V 基准：分辨率约 0.11°C/格，且不受 5V 电源纹波影响，
// 显示波动会小很多（LM35D 最高输出 1.0V，不会超量程）。
// 注意：每颗芯片内部基准有 ±10% 个体差异，首次使用请对照标准温度计
// 用下面的 CAL_OFFSET / CAL_SCALE 校准一次。
#define USE_INTERNAL_REF true
#define VREF (USE_INTERNAL_REF ? 1.1 : 5.0)

#define SAMPLES       32   // 每次读取的采样次数，越大越稳（响应略慢）
#define CAL_OFFSET    0.0  // 校准：整体偏高就填负数，例 -0.8；偏低填正数
#define CAL_SCALE     1.0  // 校准：高低端都偏时微调比例，例 0.97 / 1.03
#define SMOOTH_FACTOR 0.3  // 显示平滑：0=关闭，0.1~0.5；越小越稳但反应越慢

LedControl lc(DIN_PIN, CLK_PIN, CS_PIN, DEVICES);   // (DIN, CLK, CS, 设备数)

// 5x7 字模：每个字符 5 字节 = 5 列，bit0 = 最上面一行
// 索引：0~9 = 数字，10 = 空格，11 = 度符号 °
const byte FONT[][5] PROGMEM = {
  {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
  {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
  {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
  {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
  {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
  {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
  {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
  {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
  {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
  {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
  {0x00, 0x00, 0x00, 0x00, 0x00}, // 空格 (10)
  {0x00, 0x04, 0x0A, 0x04, 0x00}  // ° (11)
};
#define GLYPH_SPACE 10
#define GLYPH_DEG   11

float tempC = 0.0;
byte  curGlyph[4] = {GLYPH_SPACE, GLYPH_SPACE, GLYPH_SPACE, GLYPH_SPACE};
bool  curDP[4]    = {false, false, false, false};

// 行列方向修正
inline int ROW(int r) { return MIRROR_VERTICAL   ? (7 - r) : r; }
inline int COL(int c) { return MIRROR_HORIZONTAL ? (7 - c) : c; }

// 在一块屏上画一个字符（glyph = 字模索引，dp = 是否画小数点）
void drawChar(int device, byte glyph, bool dp) {
  for (int c = 0; c < 5; c++) {
    byte colData = pgm_read_byte(&FONT[glyph][c]);
    for (int r = 0; r < 7; r++) {
      if (colData & (1 << r)) {
        lc.setLed(device, ROW(r), COL(c), true);
      }
    }
  }
  // 小数点画在字模右侧空白区(第7列)：字模只占0~4列，
  // 原来画在(6,4)会和数字'2'/'5'的右下角笔画重叠，导致有时看不到小数点
  if (dp) lc.setLed(device, ROW(6), COL(6), true);
}

// 重绘全部 4 块屏（字符 i 显示在第 i 个位置）
void redraw(byte glyph[4], bool dp[4]) {
  for (int i = 0; i < 4; i++) {
    int device = REVERSE_MODULE_ORDER ? (3 - i) : i;
    lc.clearDisplay(device);
    drawChar(device, glyph[i], dp[i]);
    curGlyph[i] = glyph[i];
    curDP[i]    = dp[i];
  }
}

// 读温度：多次采样取平均，抑制噪声
float readTempC() {
  analogRead(LM35_PIN);               // 第一次读数丢弃（基准切换后需要稳定）
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(LM35_PIN);
    delay(2);
  }
  float t = sum / (float)SAMPLES * VREF / 1023.0 * 100.0;   // LM35D: 10mV/°C
  return t * CAL_SCALE + CAL_OFFSET;
}

// 把温度转换成 4 个字符并（在内容变化时）刷新
void updateDisplay() {
  float t = tempC;
  if (t < 0.0) t = 0.0;                    // LM35D 量程 0~100°C，负值按 0 显示

  byte glyph[4];
  bool dp[4] = {false, false, false, false};

  int whole = (int)t;                          // 整数部分
  int tenth = (int)((t - whole) * 10.0 + 0.5); // 十分位（四舍五入）
  if (tenth >= 10) { whole++; tenth = 0; }     // 进位

  if (whole >= 100) {                          // 100°C：显示 "100°"
    glyph[0] = 1; glyph[1] = 0; glyph[2] = 0;
    glyph[3] = SHOW_DEGREE ? GLYPH_DEG : GLYPH_SPACE;
  } else {                                     // 例 25.6 → "25.6°"
    glyph[0] = whole / 10;
    glyph[1] = whole % 10;
    glyph[2] = tenth;
    glyph[3] = SHOW_DEGREE ? GLYPH_DEG : GLYPH_SPACE;
    dp[1] = true;                              // 个位后面加小数点
  }

  // 内容没变化就不重绘，避免每 3 秒闪一下
  bool changed = false;
  for (int i = 0; i < 4; i++) {
    if (glyph[i] != curGlyph[i] || dp[i] != curDP[i]) changed = true;
  }
  if (changed) redraw(glyph, dp);

#if SERIAL_DEBUG
  Serial.print(F("Temp: "));
  Serial.print(tempC);
  Serial.println(F(" C"));
#endif
}

void setup() {
  Serial.begin(9600);
#if USE_INTERNAL_REF
  analogReference(INTERNAL);   // 1.1V 内部基准，更抗 USB 供电波动
#endif
  for (int i = 0; i < DEVICES; i++) {
    lc.shutdown(i, false);     // 唤醒
    lc.setIntensity(i, BRIGHTNESS);
    lc.clearDisplay(i);
  }

  // 开机先显示 0.0°
  byte initGlyph[4] = {0, 0, 0, GLYPH_DEG};
  bool initDP[4]    = {false, true, false, false};
  redraw(initGlyph, initDP);
}

void loop() {
  static unsigned long lastRefresh = 0;
  static bool firstReading = true;
  if (millis() - lastRefresh >= REFRESH_MS) {   // 每 3 秒刷新一次
    lastRefresh = millis();
    float t = readTempC();                      // 读取温度（多次采样平均）
    if (firstReading || SMOOTH_FACTOR <= 0) {   // 第一次直接取，避免从 0 慢慢爬升
      tempC = t;
      firstReading = false;
    } else {
      tempC += (t - tempC) * SMOOTH_FACTOR;     // 指数平滑，抑制跳动
    }
    updateDisplay();                            // 内容变化时才重绘
  }
}
