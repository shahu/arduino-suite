// RGB_Blink.ino
// 红、绿、黄三个 LED 分别接在引脚 7、6、5，依次循环闪烁

const int redPin    = 7;  // 红色 LED 接引脚 7
const int greenPin  = 6;  // 绿色 LED 接引脚 6
const int yellowPin = 5;  // 黄色 LED 接引脚 5

const int onDelay  = 3000;  // 每个 LED 点亮的时间（毫秒）
const int offDelay = 500;   // 每个 LED 熄灭的时间（毫秒）

void setup() {
  // 将三个引脚都设置为输出模式
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(yellowPin, OUTPUT);
}

void loop() {
  // 红灯：亮 3 秒，灭 0.5 秒
  digitalWrite(redPin, HIGH);
  delay(onDelay);
  digitalWrite(redPin, LOW);
  delay(offDelay);

  // 绿灯：亮 3 秒，灭 0.5 秒
  digitalWrite(greenPin, HIGH);
  delay(onDelay);
  digitalWrite(greenPin, LOW);
  delay(offDelay);

  // 黄灯：亮 3 秒，灭 0.5 秒
  digitalWrite(yellowPin, HIGH);
  delay(onDelay);
  digitalWrite(yellowPin, LOW);
  delay(offDelay);
}
