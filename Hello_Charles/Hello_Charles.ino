#include <LiquidCrystal.h>

// Hello_Charles.ino
// 在 0.96 寸 SSD1306 OLED 显示屏（I2C 接口）上显示 "Hello Charles!"
//
// 接线（Arduino Uno / Nano）：
//   OLED VCC  -> 5V
//   OLED GND  -> GND
//   OLED SDA  -> A4
//   OLED SCL  -> A5
//
// 需要安装的库（Arduino IDE：项目 -> 加载库 -> 管理库）：
//   1. Adafruit SSD1306
//   2. Adafruit GFX
//   3. Adafruit BusIO（会自动安装）

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128   // OLED 宽度（像素）
#define SCREEN_HEIGHT 64    // OLED 高度（像素）

// I2C 引脚由 Wire 库自动使用板子的默认 I2C 引脚（Uno/Nano 为 A4、A5）
// OLED_RESET 传 -1：OLED 的复位引脚与 Arduino 复位共用
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(9600);

  // 初始化 OLED，0x3C 是常见的 I2C 地址（若显示异常可改成 0x3D 试试）
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 初始化失败，请检查接线！"));
    while (true)
      ;  // 初始化失败则停止运行
  }

  display.clearDisplay();  // 清空屏幕

  display.setTextSize(2);              // 字号放大 2 倍
  display.setTextColor(SSD1306_WHITE); // 白色文字
  display.setCursor(10, 20);           // 设置起始坐标（左、上）
  display.println(F("Hello"));         // 第一行
  display.println(F("Charles!"));      // 第二行

  display.display();  // 把内容真正显示到屏幕上
}

void loop() {
  // 本程序只需显示一次，不需要循环内容
}
