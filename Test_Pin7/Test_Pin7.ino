// Test_Pin7.ino
// 单独测试引脚 7 上的红色 LED，让它一直闪烁

const int redPin = 7;  // 红色 LED 接引脚 7

void setup() {
  pinMode(redPin, OUTPUT);  // 设置引脚 7 为输出模式
}

void loop() {
  digitalWrite(redPin, HIGH);  // 点亮红灯
  delay(3000);                  // 亮 500 毫秒
  digitalWrite(redPin, LOW);   // 熄灭红灯
  delay(500);                  // 灭 500 毫秒
}
