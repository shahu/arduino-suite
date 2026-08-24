// HelloWorld.ino
// 在引脚 13 连接 LED（Arduino 板载 LED 也位于引脚 13）

void setup() {
  pinMode(13, OUTPUT);   // 设置引脚 13 为输出模式（控制 LED）
  Serial.begin(9600);    // 初始化串口通信，波特率 9600
}

void loop() {
  // 检查串口缓冲区是否收到数据
  if (Serial.available() > 0) {
    char command = Serial.read();  // 读取一个字符指令

    // 如果收到的指令是字符 R
    if (command == 'R') {
      Serial.println("Hello World");  // 通过串口输出 Hello World 字符串

      // 同时在引脚 13 的 LED 上闪烁一次作为反馈
      digitalWrite(13, HIGH);
      delay(3000);
      digitalWrite(13, LOW);
    }
  }
}
