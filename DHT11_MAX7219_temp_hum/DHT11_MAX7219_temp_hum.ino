// ==================================================================
//  DHT11 温湿度计 + MAX7219 4合1 点阵模块（Arduino UNO）
// ------------------------------------------------------------------
//  接线：
//    DHT11   : VCC -> 5V | GND -> GND | DATA -> D2
//    MAX7219 : VCC -> 5V | GND -> GND | DIN -> D11 | CS -> D10 | CLK -> D13
//  功能：每 2 秒切换一次显示（同时读取一次 DHT11，满足其 ≥1s 间隔要求）：
//          温度  → "25°C"（DHT11 温度分辨率 1°C，只显示整数）
//          湿度  → "46%"
//        上电未读到有效数据前显示 "Err"
//  依赖：库管理器安装 "DHT sensor library"（Adafruit）、
//        "Adafruit Unified Sensor" 和 "LedControl"（Eberhard Fahle）
// ==================================================================
#include <LedControl.h>
#include <avr/pgmspace.h>
#include <DHT.h>

// ---------- 引脚 ----------
#define DHT_PIN  2
#define DHT_TYPE DHT11
#define DIN_PIN  11
#define CLK_PIN  13
#define CS_PIN   10
#define DEVICES  4

// ---------- 时间 ----------
#define ALTERNATE_MS 2000UL   // 每 2 秒切换一次温度/湿度显示

// ==================================================================
//  ★ 方向修正开关：不同批次模块的排布方向可能不同。
//    显示不对时按现象修改（改完重新上传）：
//      - 左右顺序颠倒（如 °C52）  → REVERSE_MODULE_ORDER 改成 true
//      - 字符左右镜像            → MIRROR_HORIZONTAL    改成 true
//      - 字符上下颠倒            → MIRROR_VERTICAL      改成 true
// ==================================================================
#define REVERSE_MODULE_ORDER true
#define MIRROR_HORIZONTAL    false
#define MIRROR_VERTICAL      false

// ---------- 显示选项 ----------
#define SERIAL_DEBUG   true    // 串口打印读数，方便核对
#define BRIGHTNESS     3       // 亮度 0~15
#define SMOOTH_FACTOR  0.5     // 读数平滑 0~1，0=关闭；DHT11 偶尔跳 1~2 格，0.5 较稳

DHT dht(DHT_PIN, DHT_TYPE);
LedControl lc(DIN_PIN, CLK_PIN, CS_PIN, DEVICES);   // (DIN, CLK, CS, 设备数)

// 5x7 字模：每个字符 5 字节 = 5 列，bit0 = 最上面一行
// 索引：0~9 数字，10 空格，11 °，12 %，13 C，14 E，15 r
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
  {0x00, 0x04, 0x0A, 0x04, 0x00}, // ° (11)
  {0x03, 0x13, 0x08, 0x64, 0x60}, // % (12)
  {0x3E, 0x41, 0x41, 0x41, 0x41}, // C (13)
  {0x49, 0x49, 0x49, 0x49, 0x7F}, // E (14)
  {0x00, 0x7C, 0x08, 0x04, 0x0C}  // r (15)
};
#define GLYPH_SPACE 10
#define GLYPH_DEG   11
#define GLYPH_PCT   12
#define GLYPH_C     13
#define GLYPH_E     14
#define GLYPH_R     15

float tempC = 0.0, humPct = 0.0;
bool hasData = false;    // 是否已成功读到过数据
bool showTemp = true;    // true = 显示温度，false = 显示湿度

byte curGlyph[4] = {GLYPH_SPACE, GLYPH_SPACE, GLYPH_SPACE, GLYPH_SPACE};

// 行列方向修正
inline int ROW(int r) { return MIRROR_VERTICAL   ? (7 - r) : r; }
inline int COL(int c) { return MIRROR_HORIZONTAL ? (7 - c) : c; }

// 在一块屏上画一个字符（glyph = 字模索引）
void drawChar(int device, byte glyph) {
  for (int c = 0; c < 5; c++) {
    byte colData = pgm_read_byte(&FONT[glyph][c]);
    for (int r = 0; r < 7; r++) {
      if (colData & (1 << r)) {
        lc.setLed(device, ROW(r), COL(c), true);
      }
    }
  }
}

// 重绘全部 4 块屏（字符 i 显示在第 i 个位置）
void redraw(byte glyph[4]) {
  for (int i = 0; i < 4; i++) {
    int device = REVERSE_MODULE_ORDER ? (3 - i) : i;
    lc.clearDisplay(device);
    drawChar(device, glyph[i]);
    curGlyph[i] = glyph[i];
  }
}

// 读 DHT11；失败时保留上一次数据（DHT11 偶尔读取失败属正常现象）
void readSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
#if SERIAL_DEBUG
    Serial.println(F("DHT read failed"));
#endif
    return;
  }
  if (!hasData || SMOOTH_FACTOR <= 0) {
    tempC = t; humPct = h; hasData = true;   // 第一次直接取
  } else {
    tempC  += (t - tempC)  * SMOOTH_FACTOR;  // 指数平滑
    humPct += (h - humPct) * SMOOTH_FACTOR;
  }
#if SERIAL_DEBUG
  Serial.print(F("Temp: ")); Serial.print((int)round(t));   // 四舍五入取整
  Serial.print(F(" C | Hum: ")); Serial.print((int)round(h));
  Serial.println(F(" %"));
#endif
}

// 按当前状态生成 4 个字符并刷新（内容变化才重绘）
void updateDisplay() {
  byte glyph[4] = {GLYPH_SPACE, GLYPH_SPACE, GLYPH_SPACE, GLYPH_SPACE};

  if (!hasData) {                        // 还没读到有效数据 → Err
    glyph[0] = GLYPH_E; glyph[1] = GLYPH_R; glyph[2] = GLYPH_R;
  } else if (showTemp) {                 // 温度：25°C
    int w = (int)round(tempC);      // 四舍五入取整
    if (w < 0) w = 0;
    if (w > 99) w = 99;
    glyph[0] = w / 10;
    glyph[1] = w % 10;
    glyph[2] = GLYPH_DEG;
    glyph[3] = GLYPH_C;
  } else {                               // 湿度：46%
    int h = (int)round(humPct);     // 四舍五入取整
    if (h < 0) h = 0;
    if (h > 99) h = 99;
    glyph[0] = h / 10;
    glyph[1] = h % 10;
    glyph[2] = GLYPH_PCT;
    glyph[3] = GLYPH_SPACE;
  }

  bool changed = false;
  for (int i = 0; i < 4; i++) {
    if (glyph[i] != curGlyph[i]) changed = true;
  }
  if (changed) redraw(glyph);
}

void setup() {
  Serial.begin(9600);
  dht.begin();
  for (int i = 0; i < DEVICES; i++) {
    lc.shutdown(i, false);     // 唤醒
    lc.setIntensity(i, BRIGHTNESS);
    lc.clearDisplay(i);
  }

  // 开机先显示 Err，读到第一组有效数据后自动消失
  byte initGlyph[4] = {GLYPH_E, GLYPH_R, GLYPH_R, GLYPH_SPACE};
  redraw(initGlyph);
}

void loop() {
  static unsigned long lastSwitch = 0;
  if (millis() - lastSwitch >= ALTERNATE_MS) {
    lastSwitch = millis();
    readSensor();        // 读 DHT11（2 秒一次，满足其最小间隔要求）
    showTemp = !showTemp;   // 温度/湿度交替
    updateDisplay();
  }
}
