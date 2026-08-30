// ============================================================
// 串口回显测试 - Arduino UNO
// 接线：Uno 引脚 12 --[1k/2k分压]--> NodeMCU RX(GPIO3)   (发)
//       Uno 引脚 4  <--直连-- NodeMCU TX(GPIO1)          (收)
// 每 1 秒发 HELLO_UNO，并在硬件串口(USB)监视器打印收到的内容。
// ============================================================
#include <SoftwareSerial.h>
SoftwareSerial es(4, 12);   // RX=4(接NodeMCU TX), TX=12(经1k/2k接NodeMCU RX)

void setup() {
  Serial.begin(9600);        // USB serial monitor
  es.begin(9600);            // SoftwareSerial to NodeMCU
  Serial.println("Uno test: sending HELLO_UNO, waiting for echo...");
}

void loop() {
  es.println("HELLO_UNO");           // send to NodeMCU

  if (es.available()) {
    String s = es.readStringUntil('\n');
    Serial.print("[RX] "); Serial.println(s);   // print to USB monitor
  }
  delay(1000);
}
