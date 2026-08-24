/*
 * 土壤湿度传感器 完整调试程序 (AO + DO 全参数打印)
 * 适用: 电阻式 FC-28 (YL-69 探头 + YL-38 比较器模块)
 *
 * 接线 (四根线全接):
 *   VCC -> Arduino 5V
 *   GND -> Arduino GND
 *   AO  -> Arduino A0    (模拟量: 0~1023, 干燥时数值高, 湿润时数值低)
 *   DO  -> Arduino D2    (数字量: 0 或 1, 翻转阈值由模块蓝色电位器决定)
 *   (板载 LED D13 无需接线, 干燥时点亮提醒)
 *
 * 打印的全部参数:
 *   1. ADC 原始值     : analogRead(A0), 范围 0~1023
 *   2. 输出电压       : ADC 值换算的电压 (按 5V 参考)
 *   3. 土壤湿度百分比 : 由标定值映射, 0% = 干, 100% = 湿
 *   4. DO 电平        : 0 或 1 (原始数字量)
 *   5. DO 状态        : 湿润 / 干燥 (受电位器阈值控制)
 *   6. 综合状态       : 基于湿度百分比的判断 (过干/适中/过湿)
 *
 * 刷新频率: 每 2 秒更新一次, 想更慢就调大 UPDATE_DELAY
 */

const int AO_PIN = A0;    // 传感器 AO -> A0
const int DO_PIN = 2;     // 传感器 DO -> D2
const int LED_PIN = 13;   // 板载 LED, 干燥时点亮提醒浇水

// ---- 标定值 (默认值仅供参考, 请按实测修改) ----
const int DRY_VALUE = 950;  // 探头悬空(完全干燥)时的 ADC 读数
const int WET_VALUE = 350;  // 探头插进水里(湿润)时的 ADC 读数

// 湿度百分比阈值
const int DRY_THRESHOLD = 30;  // 湿度 <30% 认为过干
const int WET_THRESHOLD = 70;  // 湿度 >70% 认为过湿

// DO 极性: 标准 FC-28 湿润 = LOW, 干燥 = HIGH; 方向相反改成 false
const bool DO_ACTIVE_LOW = true;

// 串口刷新间隔 (毫秒): 500 = 0.5秒, 2000 = 2秒, 5000 = 5秒
const unsigned long UPDATE_DELAY = 2000;

void setup() {
  Serial.begin(9600);
  pinMode(DO_PIN, INPUT_PULLUP);  // DO 内部上拉, 防止悬空读数抖动
  pinMode(LED_PIN, OUTPUT);

  Serial.println(F("========== 土壤湿度传感器 全参数调试 =========="));
  Serial.println(F("接线: VCC->5V  GND->GND  AO->A0  DO->D2"));
  Serial.println(F("说明: ADC 数值越大 = 越干, 越小 = 越湿"));
  Serial.println(F("==============================================="));
}

void loop() {
  // 1. 读取模拟量 AO (A0)
  int adc = analogRead(AO_PIN);
  float voltage = adc * (5.0 / 1023.0);   // 按 5V 参考换算成电压

  // 2. 把 ADC 原始值映射为湿度百分比
  int moisture = constrain(map(adc, DRY_VALUE, WET_VALUE, 0, 100), 0, 100);

  // 3. 读取数字量 DO (D2)
  int doLevel = digitalRead(DO_PIN);
  bool doWet = (doLevel == LOW) ? DO_ACTIVE_LOW : !DO_ACTIVE_LOW;

  // 4. 打印全部参数
  Serial.println(F("---- 当前读数 ----"));
  Serial.print(F("ADC 原始值 : "));
  Serial.println(adc);
  Serial.print(F("输出电压   : "));
  Serial.print(voltage, 2);
  Serial.println(F(" V"));
  Serial.print(F("土壤湿度   : "));
  Serial.print(moisture);
  Serial.println(F(" %"));
  Serial.print(F("DO 电平    : "));
  Serial.println(doLevel);
  Serial.print(F("DO 状态    : "));
  Serial.println(doWet ? F("湿润") : F("干燥"));

  // 5. 基于湿度百分比的综合判断
  Serial.print(F("综合状态   : "));
  if (moisture < DRY_THRESHOLD) {
    Serial.println(F("土壤过干, 需要浇水!"));
  } else if (moisture < WET_THRESHOLD) {
    Serial.println(F("湿度适中"));
  } else {
    Serial.println(F("土壤过湿"));
  }

  // 6. LED 提醒: 土壤干燥时点亮板载 LED
  if (doWet) {
    digitalWrite(LED_PIN, LOW);
  } else {
    digitalWrite(LED_PIN, HIGH);
  }

  Serial.println(F("--------------------"));
  delay(UPDATE_DELAY);  // 等待 2 秒后刷新下一组读数
}
