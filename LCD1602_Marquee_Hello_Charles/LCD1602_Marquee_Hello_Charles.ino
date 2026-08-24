// LCD1602_Marquee_Hello_Charles.ino
// "Hello Charles" 走马灯（文字从右向左循环滚动）
// 上传后即可看到效果，速度可改 SCROLL_DELAY（数字越小越快）

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

// 显示屏 I2C 地址 0x27，16 列 2 行
LiquidCrystal_I2C lcd(0x27, 16, 2);

// 走马灯速度：每滚动一列等待的毫秒数（120 比较顺滑，改小更快、改大更慢）
#define SCROLL_DELAY 120

// 要滚动的文字（末尾加一个空格，让文字能完全滚出屏幕再循环）
const char marqueeText[] = "Hello Charles ";

void setup() {
  Wire.begin();
  lcd.init();       // 初始化 LCD
  lcd.backlight();  // 打开背光

  // ---- 开场：先居中显示欢迎语 2 秒 ----
  lcd.clear();
  lcd.setCursor(4, 0);   // 第 1 行居中："Hello"
  lcd.print("Hello");
  lcd.setCursor(3, 1);   // 第 2 行居中："Charles!"
  lcd.print("Charles!");
  delay(2000);

  lcd.clear();
}

void loop() {
  // 走马灯：文字从屏幕右侧进入，向左滚动，从左侧滚出，然后循环
  scrollMarquee(marqueeText);
}

// 让一段文字从右向左在 LCD 第 1 行滚动一遍
void scrollMarquee(const char* text) {
  int textLen = strlen(text);

  // offset 表示文字相对屏幕左边的位置：
  // 从 16 开始（文字完全在屏幕右边外）逐步减到 -textLen（文字完全滚出左边）
  for (int offset = 16; offset > -textLen; offset--) {
    char line[17];               // 16 个字符 + 结尾 '\0'

    // 把"当前窗口"里能看到的字符填进一行，看不到的位置填空格
    for (int col = 0; col < 16; col++) {
      int idx = offset + col;    // 这一列对应文字的第几个字符
      line[col] = (idx >= 0 && idx < textLen) ? text[idx] : ' ';
    }
    line[16] = '\0';

    // 整行一次写上去（不用 clear()，避免闪烁）
    lcd.setCursor(0, 0);
    lcd.print(line);

    delay(SCROLL_DELAY);
  }
}
