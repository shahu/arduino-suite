// ============================================================
// 串口测试 - NodeMCU (ESP8266)
// 1) 每秒主动发 "PING_NODEMCU" —— 验证 NodeMCU→Uno 方向
// 2) 收到什么回 "[ECHO] xxx" —— 验证 Uno→NodeMCU 方向
// 用 USB 烧录，烧完拔掉 USB，靠 7V 电池运行。
// ============================================================
void setup() {
  Serial.begin(9600);   // UART0 = GPIO1/3，接 Arduino
}

void loop() {
  Serial.println("PING_NODEMCU");   // 主动发，测试 NodeMCU→Uno

  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    Serial.println("[ECHO] " + s);  // 回显收到的，测试 Uno→NodeMCU
  }
  delay(1000);
}
