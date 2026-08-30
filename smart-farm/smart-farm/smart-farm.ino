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
// 【超声波测距模块（HC-SR04）】
//   VCC ──► 5V
//   GND ──► GND
//   TRIG ──► D11      // 控制端
//   ECHO ──► D10      // 接收端
//
// 【无源蜂鸣器模块（3 针：VCC/GND/I/O）】
//   VCC ──► 5V
//   GND ──► GND
//   I/O ──► D9      // 信号端，用 tone() 改变频率奏乐
//
// 【NodeMCU ESP8266（WiFi 网页服务器）串口通信】
//   NodeMCU TX(GPIO1) ──直连──► Arduino 引脚 4 (RX)          // 收
//   Arduino 引脚 12 (TX) ──[1kΩ/2kΩ分压]──► NodeMCU RX(GPIO3) // 发
//   Arduino GND ──── 共地 ──── NodeMCU 扩展板 GND
//   NodeMCU 由底座 6~24V 输入(7V 电池)供电；烧录用 USB，烧完拔掉
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
#include <SoftwareSerial.h>      // 与 NodeMCU 通信

// ===== 引脚定义 =====
#define LIGHT_DO_PIN     3    // 光敏模块 DO（数字量，阈值由蓝色电位器决定）
#define SOIL_SENSE_PIN   A0   // 土壤湿度 AO（模拟量）
#define SOIL_DO_PIN      2    // 土壤湿度 DO（数字量，阈值由蓝色电位器决定）
#define NEOPIXEL_PIN     6    // WS2812 灯环 DI（数据输入）
#define NUM_LEDS         16   // 灯板 LED 数量（16 位）
#define LAMP_BRIGHTNESS  60   // 亮度 0-255，16 颗较费电，过高会拉垮 5V
#define RELAY_PIN        8    // 继电器 IN，控制水泵
#define DRY_LED_PIN      13   // 板载 LED（Uno/Nano 为 D13）：土壤干燥时点亮提醒
#define TRIG_PIN         11   // 超声波 HC-SR04 TRIG（控制端）
#define ECHO_PIN         10   // 超声波 HC-SR04 ECHO（接收端）
#define BUZZER_PIN       9    // 无源蜂鸣器（奏乐）

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

#define UPDATE_DELAY     5000UL  // 土壤/泵/LCD/串口刷新间隔（5 秒）
#define LIGHT_CHECK_MS   250UL   // 光照/灯刷新间隔（越小响应越快）
#define DEBOUNCE_MS      400UL   // 光照判定需稳定多少毫秒才切换（防抖）
#define DIST_THRESHOLD_CM    30    // 小于此距离(cm)视为有人靠近
#define ULTRASOUND_CHECK_MS  300   // 测距间隔(ms)
#define RADAR_COOLDOWN_MS    3000  // 两次触发乐曲的最小间隔(ms)

LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

bool lampOn = false;
bool pumpOn = false;
int screen = 0;   // LCD 轮替屏号：0=欢迎 1=土壤 2=泵 3=光照 4=距离 5=NodeMCU
float lastDist = -1;   // 最近一次测得的距离(cm)，-1=无回波

// ---- 与 NodeMCU 的软件串口（引脚 4=收, 引脚 12=发）----
// 协议：Arduino→NodeMCU : SENSOR,光,暗,灯,土,泵,距离
//       NodeMCU→Arduino : BRIGHT,亮度值(0-255)
SoftwareSerial es(4, 12);
int lampBrightness = LAMP_BRIGHTNESS;   // 可由网页调节的灯亮度
unsigned long lastNodeMsg = 0;   // 最近一次从 NodeMCU 收到数据的时刻
bool nodeOnline = false;         // NodeMCU 是否在线
#define NODE_TIMEOUT_MS 4000UL   // 超过此毫秒没收到 NodeMCU 数据则视为离线

void setRelay(bool on) {
  bool active = RELAY_ACTIVE_LOW ? LOW : HIGH;   // 继电器触发电平
  digitalWrite(RELAY_PIN, on ? active : !active);
}

// 点亮/熄灭 WS2812 灯板（on=间隔点亮 8 颗白光, off=灭）
void setLamp(bool on) {
  if (on) {
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i % 2 == 0)   // 间隔点亮：每 2 颗点 1 颗（0,2,4...共 8 颗），省电流
        strip.setPixelColor(i, strip.Color(255, 255, 255));  // 白光，可改 RGB
      else
        strip.setPixelColor(i, 0);
    }
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

// ===== 接近检测：乐曲《生日快乐歌》（非阻塞播放）=====
// 频率(Hz)，每行对应一句
const int melodyFreq[] = {
  392,392,440,392,523,494,            // 祝你生日快乐 (Happy birthday to you)
  392,392,440,392,587,523,            // 祝你生日快乐
  392,392,784,659,523,494,440,        // 祝你生日快乐 [名字]
  698,698,659,523,587,523             // 祝你生日快乐
};
// 时值(ms)：附点八分+十六分 / 四分 / 四分 / 四分 / 二分
const int melodyDur[] = {
  375,125,500,500,500,1000,
  375,125,500,500,500,1000,
  375,125,500,500,500,500,1000,
  375,125,500,500,500,1000
};
const int MELODY_LEN = 25;
int noteIdx = 0;
unsigned long noteStart = 0;
bool playing = false;

void startSong() {
  noteIdx = 0;
  noteStart = millis();
  playing = true;
  tone(BUZZER_PIN, melodyFreq[0]);
}

// 每圈调用，按拍长自动推进音符；播完停止（不阻塞其他逻辑）
void updateSong() {
  if (!playing) return;
  if (millis() - noteStart >= melodyDur[noteIdx]) {
    noteStart = millis();
    noteIdx++;
    if (noteIdx >= MELODY_LEN) {
      noTone(BUZZER_PIN);
      playing = false;
    } else {
      tone(BUZZER_PIN, melodyFreq[noteIdx]);
    }
  }
}

// HC-SR04 测距（返回 cm；-1 = 超时无回波）
float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  if (dur == 0) return -1;
  return dur / 58.0;   // 厘米
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
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);   // 模块要求先拉低 TRIG
  strip.begin();
  strip.setBrightness(lampBrightness);
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
  Serial.println(F("Smart Farm ready"));
  es.begin(9600);            // 与 NodeMCU 通信（引脚4收/引脚12发）
}

void loop() {
  static bool dark = false;              // 防抖后的稳定黑暗判定（默认关灯）
  static int lightLevel = 0;             // 最近一次光敏 DO 电平
  static bool rawStable = false;         // 最近一次原始判定
  static unsigned long msRawChanged = 0; // 原始判定最近变化时刻
  static unsigned long lastLightCheck = 0;
  static unsigned long lastSoilUpdate = 0;
  unsigned long ms = millis();

  // ---- 接收 NodeMCU 数据（非阻塞 + 固定缓冲，省内存）----
  static char rxBuf[48];
  static byte rxIdx = 0;
  while (es.available()) {
    char c = es.read();
    if (c == '\n') {
      rxBuf[rxIdx] = '\0';
      rxIdx = 0;
      if (rxBuf[0] != '\0') {
        Serial.print(F("[NodeMCU] ")); Serial.println(rxBuf);
        lastNodeMsg = millis();                               // 记录收到时间，用于在线判定
        if (strncmp(rxBuf, "BRIGHT,", 7) == 0) {
          int v = atoi(rxBuf + 7);
          if (v < 0) v = 0;
          if (v > 255) v = 255;
          lampBrightness = v;
          strip.setBrightness(lampBrightness);   // 立即使灯板按新亮度
        }
      }
    } else if (c != '\r' && rxIdx < 47) {
      rxBuf[rxIdx++] = c;
    }
  }

  // ---- 超声波测距 & 接近报警：每 ULTRASOUND_CHECK_MS 测一次 ----
  static unsigned long lastUltraCheck = 0;
  static unsigned long lastTrigger = 0;
  static bool personNear = false;   // 防抖后的"人在旁"状态
  static byte nearCount = 0;        // 连续"近"的计数（防毛刺误触发）
  if (ms - lastUltraCheck >= ULTRASOUND_CHECK_MS) {
    lastUltraCheck = ms;
    float dist = readDistance();
    lastDist = dist;
    bool nearNow = (dist > 0 && dist < DIST_THRESHOLD_CM);
    if (nearNow) nearCount++; else nearCount = 0;
    bool nearStable = (nearCount >= 3);   // 连续 3 次(约0.9秒)都近才算，单次毛刺不算
    // 边沿触发：只在"从远到近"这一下播放，避免人在旁边时每 3 秒重播导致音乐不断
    if (nearStable && !personNear && (ms - lastTrigger > RADAR_COOLDOWN_MS)) {
      lastTrigger = ms;
      startSong();
      Serial.print(F("有人靠近! 距离=")); Serial.print(dist); Serial.println(F(" cm"));
    }
    personNear = nearStable;
  }
  updateSong();   // 让乐曲逐音符推进（非阻塞，不卡灯/水泵）

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
    const __FlashStringHelper* doStatus = doDry ? F("干燥") : F("湿润");
    const __FlashStringHelper* status = moisture < DRY_PCT ? F("过干")
                                       : moisture > WET_PCT ? F("过湿") : F("适中");

    bool dry = (moisture < DRY_PCT);
    pumpOn = dry;
    setRelay(pumpOn);
    digitalWrite(DRY_LED_PIN, dry ? HIGH : LOW);

    // ---- 把传感器数据发给 NodeMCU（网页显示用，9 个字段）----
    es.print("SENSOR,");
    es.print(lightLevel); es.print(',');               // 光敏 DO
    es.print(dark ? 1 : 0); es.print(',');             // 黑暗判定
    es.print(lampOn ? 1 : 0); es.print(',');           // 照明灯
    es.print(soil); es.print(',');                     // 土壤 ADC
    es.print(moisture); es.print(',');                 // 土壤湿度%
    es.print(doLevel); es.print(',');                  // 土壤 DO
    es.print(pumpOn ? 1 : 0); es.print(',');           // 水泵
    es.print(digitalRead(RELAY_PIN)); es.print(',');   // 继电器电平
    es.println(lastDist >= 0 ? (int)lastDist : -1);    // 距离

    // ---- NodeMCU 在线判定：超过 NODE_TIMEOUT_MS 没收到数据则离线 ----
    nodeOnline = (ms - lastNodeMsg) < NODE_TIMEOUT_MS;

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
      case 4:   // 屏 5：距离(cm)
        lcd.setCursor(0, 0); lcd.print("Distance        ");
        lcd.setCursor(0, 1);
        if (lastDist >= 0) {
          lcd.print("   "); lcd.print((int)lastDist); lcd.print(" cm");
        } else {
          lcd.print("   -- cm");
        }
        lcd.print("       ");
        break;
      case 5:   // 屏 6：NodeMCU 在线状态
        lcd.setCursor(0, 0); lcd.print("NodeMCU         ");
        lcd.setCursor(0, 1);
        lcd.print(nodeOnline ? "  ONLINE " : "  OFFLINE");
        lcd.print("       ");
        break;
    }

    // ---- 串口：一次性打印全部传感器状态（每 5 秒）----
    Serial.println(F("=============================================="));
    Serial.print(F("[Smart Farm]  LCD屏#")); Serial.println(screen);

    Serial.println(F("-- 光敏传感器 (LDR, DO--D3) ---"));
    Serial.print(F("   DO电平   : ")); Serial.println(lightLevel);
    Serial.print(F("   判定     : ")); Serial.println(dark ? F("黑暗 DARK") : F("明亮 BRIGHT"));
    Serial.print(F("   照明灯   : ")); Serial.println(lampOn ? F("ON") : F("OFF"));

    Serial.println(F("-- 土壤湿度传感器 (AO--A0, DO--D2) ---"));
    Serial.print(F("   ADC原始值 : ")); Serial.println(soil);
    Serial.print(F("   输出电压  : ")); Serial.print(voltage, 2); Serial.println(F(" V"));
    Serial.print(F("   土壤湿度  : ")); Serial.print(moisture); Serial.println(F(" %"));
    Serial.print(F("   DO电平   : ")); Serial.println(doLevel);
    Serial.print(F("   DO状态   : ")); Serial.println(doStatus);
    Serial.print(F("   综合状态  : ")); Serial.println(status);

    Serial.println(F("-- 超声波测距 (HC-SR04, TRIG--D11, ECHO--D10) ---"));
    if (lastDist >= 0) { Serial.print(F("   距离      : ")); Serial.print(lastDist); Serial.println(F(" cm")); }
    else               { Serial.println(F("   距离      : -- cm (无回波)")); }

    Serial.println(F("-- 水泵 / 继电器 (D8) ---"));
    Serial.print(F("   泵状态    : ")); Serial.println(pumpOn ? F("ON 抽水") : F("OFF 停止"));
    Serial.print(F("   继电器电平: ")); Serial.println(digitalRead(RELAY_PIN));

    Serial.println(F("-- NodeMCU (ESP8266) ---"));
    Serial.print(F("   在线状态  : ")); Serial.println(nodeOnline ? F("ONLINE") : F("OFFLINE"));

    Serial.println(F("=============================================="));

    screen = (screen + 1) % 6;   // 下一屏（下个周期显示）
  }
}