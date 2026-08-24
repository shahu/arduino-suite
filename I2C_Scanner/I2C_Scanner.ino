// I2C_Scanner.ino
// 扫描 I2C 总线上所有设备，并打印它们的地址到串口监视器
// 用法：上传后打开串口监视器（波特率 9600），会每 5 秒扫描一次

#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
  Serial.println("I2C 扫描开始...");
}

void loop() {
  byte error;
  int found = 0;

  Serial.println("正在扫描 I2C 总线...");

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("找到 I2C 设备，地址 = 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
      found++;
    }
  }

  if (found == 0) {
    Serial.println("没有找到任何 I2C 设备，请检查 SDA/SCL 接线！");
  } else {
    Serial.print("共找到 ");
    Serial.print(found);
    Serial.println(" 个设备");
  }

  delay(5000);  // 5 秒后再扫一次
}
