/*
 * 光敏电阻 + WS2812B 8位全彩模块 —— 植物灯演示
 *
 * 接线：
 *   光敏模块 VCC -> 5V     GND -> GND     AO -> A0
 *   WS2812B   VCC -> 5V     GND -> GND     DIN -> D6   (DOUT 悬空)
 *
 * 演示逻辑：
 *   环境暗（读数低于阈值）-> 8 颗灯珠发出红+蓝混合的"植物光"，越暗灯越亮
 *   环境亮                  -> 全部熄灭
 *
 * 依赖库：Adafruit NeoPixel
 *   安装：Arduino IDE -> 工具 -> 管理库 -> 搜索 "Adafruit NeoPixel" -> 安装
 */

#include <Adafruit_NeoPixel.h>

#define LDR_PIN   A0
#define DIN_PIN   6
#define NUMPIXELS 8

const int THRESHOLD  = 500;   // 暗/亮阈值：用串口监视器观察读数后校准
const int HYSTERESIS = 20;    // 回差：防止临界亮度时闪烁
const int MIN_BRIGHT = 40;    // 补光亮度下限
const int MAX_BRIGHT = 150;   // 补光亮度上限（8颗全亮电流大，勿设太高）

Adafruit_NeoPixel strip(NUMPIXELS, DIN_PIN, NEO_GRB + NEO_KHZ800);
bool lightOn = false;

void setup() {
  Serial.begin(9600);
  strip.begin();
  strip.clear();
  strip.show();
  Serial.println(F("植物灯演示启动"));
}

void loop() {
  int light = analogRead(LDR_PIN);   // 0~1023，越暗读数越小

  // 带回差的开关判断，避免抖动
  if (light < THRESHOLD - HYSTERESIS) {
    lightOn = true;
  } else if (light > THRESHOLD + HYSTERESIS) {
    lightOn = false;
  }

  if (lightOn) {
    // 读数越小(越暗) -> 灯越亮
    int b = map(light, THRESHOLD - HYSTERESIS, 50, MIN_BRIGHT, MAX_BRIGHT);
    b = constrain(b, MIN_BRIGHT, MAX_BRIGHT);
    strip.setBrightness(b);

    for (int i = 0; i < NUMPIXELS; i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 255)); // 红+蓝 = 植物光(紫红)
    }
  } else {
    strip.clear();   // 环境亮 -> 全部熄灭
  }
  strip.show();

  Serial.print(F("光照: "));
  Serial.print(light);
  Serial.print(F("   状态: "));
  Serial.println(lightOn ? F("开(植物光)") : F("关"));

  delay(100);
}
