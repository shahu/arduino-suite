// LCD1602_Hello_Charles.ino（逐行测试版）
// 逐行测试 LCD 的两行是否都能显示
// 上传后打开串口监视器（波特率 9600），按提示逐段观察屏幕

// ============ 接线方式（LCD1602 + I2C 转换板 -> Arduino Uno/Nano）============
//   LCD 模块接口    接到 Arduino 引脚
//   GND          -> GND
//   VCC          -> 5V
//   SDA          -> A4
//   SCL          -> A5
// ---------------------------------------------------------------------------
//   其他板子的 I2C 引脚（SDA / SCL）：
//   Mega 2560    -> 20 / 21
//   Leonardo/Micro -> D2 / D3
//   Uno R4       -> A4 / A5（或靠近 USB 口的专用 I2C 排针）
// ---------------------------------------------------------------------------
//   注意事项：
//   1. VCC 接 5V，不要接 3.3V，否则屏幕可能不亮或对比度不对。
//   2. 屏幕不亮或只有背光没字：先调模块背后的小蓝电位器（对比度），
//      再用 I2C_Scanner 项目扫地址（常见 0x27 或 0x3F），改下面 lcd 的地址。
// ============================================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  delay(100);

  Serial.println("========== LCD1602 逐行测试 ==========");

  // ---- 先扫描 I2C，确认通信 ----
  byte found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("找到设备 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      if (found == 0) found = addr;
    }
  }
  if (found == 0) {
    Serial.println("未找到 I2C 设备！");
    return;
  }

  lcd.init();
  lcd.backlight();

  // ---- 测试 1：第一行数字、第二行字母 ----
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("0123456789ABCDEF");
  lcd.setCursor(0, 1);
  lcd.print("abcdefghijklmnop");
  Serial.println("[T1] 第1行应显示: 0123456789ABCDEF");
  Serial.println("[T1] 第2行应显示: abcdefghijklmnop");
  delay(5000);

  // ---- 测试 2：只写第 2 行（底部）方块 ----
  lcd.clear();
  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++) {
    lcd.write(0xFF);
  }
  Serial.println("[T2] 只写第2行: 底部应显示 16 个方块，顶部应空白");
  delay(5000);

  // ---- 测试 3：只写第 1 行（顶部）方块 ----
  lcd.clear();
  lcd.setCursor(0, 0);
  for (int i = 0; i < 16; i++) {
    lcd.write(0xFF);
  }
  Serial.println("[T3] 只写第1行: 顶部应显示 16 个方块，底部应空白");
  delay(5000);

  // ---- 测试 4：满屏方块 ----
  lcd.clear();
  for (int row = 0; row < 2; row++) {
    lcd.setCursor(0, row);
    for (int col = 0; col < 16; col++) {
      lcd.write(0xFF);
    }
  }
  Serial.println("[T4] 两行都写: 上下两行都应显示方块");
  Serial.println("=========================================");
}

void loop() {
  // 测试只运行一次
}
