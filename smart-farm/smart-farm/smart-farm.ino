// ============================================================
// Smart Farm v1.1 —— 最终接线表（对照实物逐条检查）
// ============================================================
// 【电源】
//   Arduino 用 USB 供电即可。水泵必须用独立电源（3V 泵 =
//   2 节 5 号碱性电池，或 3 节镍氢充电电池），不能吃 Arduino 5V。
//
// 【土壤湿度模块（LM393）】
//   VCC ──► 5V
//   GND ──► GND
//   AO  ──► A0      // 模拟量
//   DO  ──► D2      // 数字量（蓝色电位器调阈值）
//
// 【光敏模块（3 针：VCC/GND/DO，LM393 数字量）】
//   VCC ──► 5V
//   GND ──► GND
//   DO  ──► D3      // 数字量（蓝色电位器调阈值）
//
// 【继电器模块（SRD-05VDC，跳线帽插在 H 侧 = 高电平触发）】
//   VCC ──► 5V
//   GND ──► GND
//   IN  ──► D8
//   负载侧：COM ──► 电池/电源正极
//           NO  ──► 水泵正极（红线）
//           电源负极 ──► 水泵负极（黑线）直连，不经继电器
//
// 【水泵（3V）】
//   电池盒 + ──► 继电器 COM
//   继电器 NO ──► 泵+（红）
//   电池盒 - ──► 泵-（黑）直连
//
// 【照明灯（WS2812 16 位 RGB 幻彩灯板，内置全彩驱动）】
//   DI  ──► D6      // 数据输入（信号），注意方向别接成 DO
//   5V  ──► 5V
//   GND ──► GND
//   DO  ──► 空（不接；仅串联多块灯板时才接下一块的 DI）
//   ⚠ 16 颗全亮电流大：建议给 5V 单独供电（负极共地），或降低亮度
//
// 【I2C LCD（0x27）】
//   VCC ──► 5V
//   GND ──► GND
//   SDA ──► A4
//   SCL ──► A5
//
// 【板载 LED】D13 自动使用（土壤干燥时亮），无需接线
// ============================================================
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>   // 需要安装 Adafruit NeoPixel 库（库管理器搜索 NeoPixel）

// ===== 引脚定义 =====
#define LIGHT_DO_PIN     3    // 光敏模块 DO（数字量，阈值由蓝色电位器决定）
#define SOIL_SENSE_PIN   A0   // 土壤湿度 AO（模拟量）
#define SOIL_DO_PIN      2    // 土壤湿度 DO（数字量，阈值由蓝色电位器决定）
#define NEOPIXEL_PIN     6    // WS2812 灯环 DI（数据输入）
#define NUM_LEDS         16   // 灯板 LED 数量（16 位）
#define LAMP_BRIGHTNESS  100  // 亮度 0-255，16 颗较费电，过高会拉垮 5V
#define RELAY_PIN        8    // 继电器 IN，控制水泵
#define DRY_LED_PIN      13   // 板载 LED（Uno/Nano 为 D13）：土壤干燥时点亮提醒

// ===== 标定值：先用串口观察 ADC，再填入实际的干/湿读数 =====
#define DRY_VALUE  800    // 完全干燥时的 ADC（干 -> 数值高）
#define WET_VALUE  300    // 完全湿润时的 ADC（湿 -> 数值低）

// ===== 阈值与判定 =====
#define DRY_PCT          30   // 湿度低于 30% -> 过干，启动水泵
#define WET_PCT          70   // 湿度高于 70% -> 过湿

// ===== 方向/触发：不同模块可能相反，按串口读数改 =====
// 光敏校准：看串口「光敏DO」：
//   用手遮住(天黑)时 DO=0、不遮(天亮)时 DO=1  -> 保持 DARK_IS_LOW true
//   用手遮住(天黑)时 DO=1、不遮(天亮)时 DO=0  -> 改成 DARK_IS_LOW false
//   遮住/不遮 DO 都不变（一直亮灯通常 = DO 卡在 0）-> 调光敏模块蓝色电位器
#define DARK_IS_LOW      false  // 实测：天黑(捏住)时 DO=1，天亮时 DO=0，所以用 false
#define DO_ACTIVE_LOW    true  // true：干燥时 DO 输出低电平（常见）
#define RELAY_ACTIVE_LOW false  // 跳线帽在 H 侧 = 高电平触发，所以用 false

#define UPDATE_DELAY     2000UL  // 土壤/泵/LCD/串口刷新间隔
#define LIGHT_CHECK_MS   250UL   // 光照/灯刷新间隔（越小响应越快）
#define DEBOUNCE_MS      400UL   // 光照判定需稳定多少毫秒才切换（防抖）

LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

bool lampOn = false;
bool pumpOn = false;
int screen = 0;   // LCD 轮替屏号：0=欢迎 1=土壤 2=泵 3=光照

void setRelay(bool on) {
  bool active = RELAY_ACTIVE_LOW ? LOW : HIGH;   // 继电器触发电平
  digitalWrite(RELAY_PIN, on ? active : !active);
}

// 点亮/熄灭 WS2812 灯板（on=亮白光, off=灭）
void setLamp(bool on) {
  if (on) {
    for (int i = 0; i < NUM_LEDS; i++)
      strip.setPixelColor(i, strip.Color(255, 255, 255));  // 白光，可改 RGB
  } else {
    for (int i = 0; i < NUM_LEDS; i++)
      strip.setPixelColor(i, 0);   // 全灭
  }
  strip.show();
}

// RGB 色谱生成（0~255 循环，用于彩虹滚动）
uint32_t wheel(byte pos) {
  pos = 255 - pos;
  if (pos < 85)  return strip.Color(255 - pos * 3, 0, pos * 3);
  if (pos < 170) { pos -= 85; return strip.Color(0, pos * 3, 255 - pos * 3); }
  pos -= 170;
  return strip.Color(pos * 3, 255 - pos * 3, 0);
}

void lcdPad(int n) {   // 数字右对齐成 4 位
  if (n < 10)   lcd.print(' ');
  if (n < 100)  lcd.print(' ');
  if (n < 1000) lcd.print(' ');
  lcd.print(n);
}

void lcdPct(int v) {   // 百分比右对齐成 3 位
  if (v < 10)       lcd.print("  ");
  else if (v < 100) lcd.print(' ');
  lcd.print(v);
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(DRY_LED_PIN, OUTPUT);
  pinMode(SOIL_DO_PIN, INPUT);
  pinMode(LIGHT_DO_PIN, INPUT);
  strip.begin();
  strip.setBrightness(LAMP_BRIGHTNESS);
  strip.clear();
  strip.show();

  // ---- 开机全彩滚动自检：16 颗灯彩虹滚动 2 圈（约 3 秒）----
  for (int step = 0; step < 512; step++) {
    for (int i = 0; i < NUM_LEDS; i++)
      strip.setPixelColor(i, wheel((i * 16 + step) & 255));
    strip.show();
    delay(6);
  }
  strip.clear();
  strip.show();
  digitalWrite(DRY_LED_PIN, LOW);
  setRelay(false);            // 初始：灯灭、泵停、干燥灯灭

  // ---- 泵继电器自检：咔哒 3 次（不点灯，灯在彩虹后保持熄灭）----
  setLamp(false);           // 确保彩虹结束后灯是灭的
  for (int i = 0; i < 3; i++) {
    setRelay(true);  delay(300);
    setRelay(false); delay(300);
  }

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Hello Charles!");
  lcd.setCursor(0, 1);
  lcd.print("Smart Farm 1.1 ");
  delay(1500);

  Serial.begin(9600);
  Serial.println("Smart Farm ready");
}

void loop() {
  static bool dark = false;              // 防抖后的稳定黑暗判定（默认关灯）
  static int lightLevel = 0;             // 最近一次光敏 DO 电平
  static bool rawStable = false;         // 最近一次原始判定
  static unsigned long msRawChanged = 0; // 原始判定最近变化时刻
  static unsigned long lastLightCheck = 0;
  static unsigned long lastSoilUpdate = 0;
  unsigned long ms = millis();

  // ---- 光照 & 灯：每 LIGHT_CHECK_MS 判断一次，快速响应手遮挡 ----
  if (ms - lastLightCheck >= LIGHT_CHECK_MS) {
    lastLightCheck = ms;
    int lv = digitalRead(LIGHT_DO_PIN);
    bool nowDark = DARK_IS_LOW ? (lv == LOW) : (lv == HIGH);
    // 防抖：判定值连续 DEBOUNCE_MS 稳定才切换，避免临界抖动误判/灯闪烁
    if (nowDark != rawStable) {
      rawStable = nowDark;
      msRawChanged = ms;
    } else if (ms - msRawChanged >= DEBOUNCE_MS) {
      dark = rawStable;
    }
    lightLevel = lv;         // 供串口/LCD 显示
    lampOn = dark;
    setLamp(lampOn);
  }

  // ---- 土壤 / 泵 / LCD / 串口：每 UPDATE_DELAY 一次 ----
  if (ms - lastSoilUpdate >= UPDATE_DELAY) {
    lastSoilUpdate = ms;
    int soil = analogRead(SOIL_SENSE_PIN);
    int doLevel = digitalRead(SOIL_DO_PIN);
    float voltage = soil * 5.0 / 1023.0;
    int moisture = constrain(map(soil, DRY_VALUE, WET_VALUE, 0, 100), 0, 100);
    bool doDry = DO_ACTIVE_LOW ? (doLevel == LOW) : (doLevel == HIGH);
    const char* doStatus = doDry ? "干燥" : "湿润";
    const char* status = moisture < DRY_PCT ? "过干"
                       : moisture > WET_PCT ? "过湿" : "适中";

    bool dry = (moisture < DRY_PCT);
    pumpOn = dry;
    setRelay(pumpOn);
    digitalWrite(DRY_LED_PIN, dry ? HIGH : LOW);

    // ---- LCD 轮替显示（每屏停留 UPDATE_DELAY 秒）----
    switch (screen) {
      case 0:   // 屏 1：欢迎
        lcd.setCursor(0, 0); lcd.print("Welcome Charles!");
        lcd.setCursor(0, 1); lcd.print("Smart Farm 1.1 ");
        break;
      case 1:   // 屏 2：土壤湿度百分比
        lcd.setCursor(0, 0); lcd.print("Soil Moisture   ");
        lcd.setCursor(0, 1);
        lcd.print("    "); lcdPct(moisture); lcd.print("%");
        lcd.print("        ");
        break;
      case 2:   // 屏 3：泵开关状态
        lcd.setCursor(0, 0); lcd.print("Pump Status     ");
        lcd.setCursor(0, 1);
        lcd.print("     "); lcd.print(pumpOn ? "ON" : "OFF");
        lcd.print("        ");
        break;
      case 3:   // 屏 4：光照亮/暗
        lcd.setCursor(0, 0); lcd.print("Light Status    ");
        lcd.setCursor(0, 1);
        lcd.print(dark ? "   DARK" : "  BRIGHT");
        lcd.print("        ");
        break;
    }

    // ---- 串口：打印土壤 6 项 + 光照 ----
    Serial.print(">>> LCD Screen #"); Serial.println(screen);
    Serial.println("===== 土壤湿度 =====");
    Serial.print("1. ADC 原始值 : "); Serial.println(soil);
    Serial.print("2. 输出电压  : "); Serial.print(voltage, 2); Serial.println(" V");
    Serial.print("3. 土壤湿度  : "); Serial.print(moisture); Serial.println(" %");
    Serial.print("4. DO 电平   : "); Serial.println(doLevel);
    Serial.print("5. DO 状态   : "); Serial.println(doStatus);
    Serial.print("6. 综合状态  : "); Serial.println(status);
    Serial.print("光敏DO="); Serial.print(lightLevel);
    Serial.print(" 判定dark="); Serial.print(dark ? 1 : 0);
    Serial.print(" 灯="); Serial.print(lampOn ? "开" : "关");
    Serial.print(" 泵="); Serial.print(pumpOn ? "开" : "关");
    Serial.print(" (D8电平="); Serial.print(digitalRead(RELAY_PIN)); Serial.println(")");
    Serial.println("------------------------");

    screen = (screen + 1) % 4;   // 下一屏（下个周期显示）
  }
}