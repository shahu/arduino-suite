/*
 * 光敏电阻模块控制 LED
 *
 * 接线：
 *   光敏模块 VCC -> Arduino 5V
 *   光敏模块 GND -> Arduino GND
 *   光敏模块 AO  -> Arduino A0
 *   LED 阳极(长脚) --220Ω--> Arduino D7
 *   LED 阴极(短脚)        -> Arduino GND
 *
 * 工作原理：
 *   这类模块在光线越暗时，LDR 阻值越大，AO 输出电压越低，
 *   即 A0 模拟读数(0~1023)越小。读数低于阈值时点亮 LED。
 *
 * 阈值校准：
 *   上传后打开串口监视器(波特率 9600)，
 *   分别观察"正常亮度"和"遮住传感器"时的读数，
 *   把 THRESHOLD 设到两者中间偏暗的一侧。
 */

const int LDR_PIN = A0;     // 光敏电阻模块模拟输出
const int LED_PIN = 7;      // LED 连接引脚

const int THRESHOLD  = 500; // 亮度阈值：读数低于此值视为"暗"
const int HYSTERESIS = 20;  // 回差：防止临界亮度时 LED 闪烁

bool ledState = false;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);   // 初始熄灭

  Serial.begin(9600);
  Serial.println(F("LDR + LED 程序启动"));
}

void loop() {
  int light = analogRead(LDR_PIN);   // 读取光照，0~1023

  // 带回差的比较，避免在阈值附近来回抖动
  if (light < THRESHOLD - HYSTERESIS) {
    ledState = true;                 // 环境暗 -> 点亮
  } else if (light > THRESHOLD + HYSTERESIS) {
    ledState = false;                // 环境亮 -> 熄灭
  }

  digitalWrite(LED_PIN, ledState ? HIGH : LOW);

  Serial.print(F("光照读数: "));
  Serial.print(light);
  Serial.print(F("   LED: "));
  Serial.println(ledState ? F("亮") : F("灭"));

  delay(100);                        // 稍作延时，稳定读数
}
