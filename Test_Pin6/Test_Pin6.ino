// Test_Pin6.ino
// 单独测试引脚 6 上的 LED，让它一直闪烁
// 如需测试其他引脚，只需修改下面 pinToTest 的数字

const int pinToTest = 6;  // 要测试的引脚编号

void setup() {
  pinMode(pinToTest, OUTPUT);  // 设置该引脚为输出模式
}

void loop() {
  digitalWrite(pinToTest, HIGH);  // 点亮 LED
  delay(3000);                     // 亮 500 毫秒
  digitalWrite(pinToTest, LOW);   // 熄灭 LED
  delay(500);                     // 灭 500 毫秒
}
