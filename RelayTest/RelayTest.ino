// ============================================================
// 继电器测试程序（与 smart-farm 无关，测完可删）
// 作用：8 脚先输出低电平 1 秒，再输出高电平 1 秒，循环
// 观察：继电器在哪个阶段"咔哒"吸合 / LED 亮
//   - 低电平阶段动作  -> 低电平触发（保持 RELAY_ACTIVE_LOW = true）
//   - 高电平阶段动作  -> 高电平触发（改成 RELAY_ACTIVE_LOW = false）
//   - 两个阶段都不动  -> 硬件问题，检查接线（见串口提示）
// ============================================================
#define RELAY_PIN 8

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(13, OUTPUT);               // 板载 LED 同步测试
  Serial.begin(9600);
  delay(200);
  Serial.println("===== 继电器测试开始 =====");
  Serial.println("每个状态持续 1 秒，注意听「咔哒」声、看继电器上的 LED");
}

void loop() {
  digitalWrite(RELAY_PIN, LOW);          // 先试低电平
  digitalWrite(13, HIGH);                // 板载 LED 亮
  Serial.println(">> 现在是【低电平 LOW 】：继电器动了吗？");
  delay(1000);

  digitalWrite(RELAY_PIN, HIGH);         // 再试高电平
  digitalWrite(13, LOW);                 // 板载 LED 灭
  Serial.println(">> 现在是【高电平 HIGH】：继电器动了吗？");
  delay(1000);
}
