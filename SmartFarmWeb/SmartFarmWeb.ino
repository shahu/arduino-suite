/*
  SmartFarm WiFi 网页服务器 —— NodeMCU (ESP8266)
  功能：连家里局域网 WiFi（固定 IP）+ 网页；展示传感器数据、调节照明灯亮度。
  与 Arduino UNO 串口(UART0=GPIO1/3)通信（全英文，避免 Uno 侧乱码）：
    Arduino -> NodeMCU : SENSOR,光,暗,灯,土ADC,土湿度,土DO,泵,继电器,距离
    NodeMCU -> Arduino : PING           (心跳，让 Uno 判定 NodeMCU 在线)
                         BRIGHT,0-255   (网页拖亮度)
  烧录：Arduino IDE -> 开发板 "NodeMCU 1.0 (ESP-12E Module)" -> USB 上传
*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ===== 局域网 WiFi（Station 模式，固定 IP）=====
const char* STA_SSID = "Xiaomi_702";
const char* STA_PASS = "husha117";

// 固定 IP（必须和你路由器同一网段；小米路由器默认 192.168.31.x）
IPAddress localIP(192, 168, 31, 200);   // 固定访问地址 http://192.168.31.200
IPAddress gateway(192, 168, 31, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 31, 1);

// ===== AP 兜底（连不上局域网时自动开热点，仍可访问）=====
const char* AP_SSID = "SmartFarm";
const char* AP_PASS = "12345678";   // 至少 8 位

// ===== 传感器状态（Arduino 发来）=====
int sLight = 0;      // 光敏 DO 电平
int sDark = 0;       // 黑暗判定(1=暗)
int sLamp = 0;       // 照明灯(1=开)
int sSoilADC = 0;    // 土壤 ADC 原始值
int sSoil = 0;       // 土壤湿度 %
int sSoilDO = 0;     // 土壤 DO 电平
int sPump = 0;       // 水泵(1=开)
int sRelay = 0;      // 继电器电平
float sDist = -1;    // 距离 cm
int sBright = 100;   // 灯亮度 0-255

ESP8266WebServer server(80);

// 解析 Arduino 发来的 "SENSOR,光,暗,灯,土ADC,土湿度,土DO,泵,继电器,距离"
void parseLine(String line) {
  if (!line.startsWith("SENSOR,")) return;
  String b = line.substring(7);
  int v[9], idx = 0, start = 0;
  for (int i = 0; i <= b.length(); i++) {
    if (i == b.length() || b.charAt(i) == ',') {
      if (idx < 9) v[idx++] = b.substring(start, i).toInt();
      start = i + 1;
    }
  }
  if (idx >= 9) {
    sLight = v[0]; sDark = v[1]; sLamp = v[2];
    sSoilADC = v[3]; sSoil = v[4]; sSoilDO = v[5];
    sPump = v[6]; sRelay = v[7]; sDist = v[8];
  }
}

// ===== 网页内容（展示模块 + 控制模块，手机自适应）=====
const char* ROOT_HTML = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Farm</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{font-family:-apple-system,"Segoe UI",sans-serif;background:#0f172a;color:#e2e8f0;margin:0;padding:14px}
h1{font-size:22px;margin:0 0 4px}
.muted{color:#94a3b8;font-size:13px}
.module{background:#1e293b;border:1px solid #334155;border-radius:14px;padding:16px;margin-top:14px}
.module h2{font-size:16px;margin:0 0 10px}
.row{display:flex;justify-content:space-between;padding:7px 0;border-bottom:1px solid #273549}
.row:last-child{border-bottom:none}
.row .k{color:#94a3b8}
.row .v{font-weight:700;max-width:65%;text-align:right}
.slider{width:100%;height:34px;accent-color:#38bdf8;margin-top:10px}
.sval{text-align:center;font-size:22px;font-weight:700;margin-top:8px}
</style></head><body>
<h1>Smart Farm 🌱</h1>
<div class="muted">Arduino + NodeMCU WiFi monitor &amp; control</div>

<!-- 展示模块：和 Arduino 串口打印一致 -->
<div class="module">
  <h2>📊 传感器数据</h2>
  <div class="row"><span class="k">光敏 DO电平</span><span class="v" id="light">-</span></div>
  <div class="row"><span class="k">环境判定</span><span class="v" id="dark">-</span></div>
  <div class="row"><span class="k">照明灯</span><span class="v" id="lamp">-</span></div>
  <div class="row"><span class="k">土壤 ADC</span><span class="v" id="adc">-</span></div>
  <div class="row"><span class="k">土壤湿度</span><span class="v" id="soil">-</span></div>
  <div class="row"><span class="k">土壤 DO</span><span class="v" id="soildo">-</span></div>
  <div class="row"><span class="k">距离</span><span class="v" id="dist">-</span></div>
  <div class="row"><span class="k">水泵</span><span class="v" id="pump">-</span></div>
  <div class="row"><span class="k">继电器</span><span class="v" id="relay">-</span></div>
  <div class="row"><span class="k">NodeMCU</span><span class="v" id="node">ONLINE</span></div>
</div>

<!-- 控制模块：只控制灯亮度 -->
<div class="module">
  <h2>🎛 控制</h2>
  <div class="muted">照明灯亮度 (0-255)</div>
  <input class="slider" type="range" min="0" max="255" value="100" oninput="setBright(this.value)">
  <div class="sval" id="bv">100</div>
</div>

<script>
async function refresh(){
  try{
    const r=await fetch('/data'); const d=await r.json();
    const soilStatus=(d.soil<30?'过干 DRY':(d.soil>70?'过湿 WET':'适中 OK'));
    document.getElementById('light').textContent='DO='+d.light;
    document.getElementById('dark').textContent =d.dark? '黑暗 DARK':'明亮 BRIGHT';
    document.getElementById('lamp').textContent =d.lamp? '开 ON':'关 OFF';
    document.getElementById('adc').textContent  =d.soiladc;
    document.getElementById('soil').textContent =d.soil+' % ('+soilStatus+')';
    document.getElementById('soildo').textContent=d.soildo;
    document.getElementById('dist').textContent =(d.dist>=0? d.dist+' cm':'--');
    document.getElementById('pump').textContent =d.pump? '开 ON':'关 OFF';
    document.getElementById('relay').textContent='D8='+d.relay;
    document.getElementById('node').textContent='ONLINE';
    document.getElementById('bv').textContent  =d.bright;
  }catch(e){}
}
setInterval(refresh,1000); refresh();
async function setBright(v){
  await fetch('/bright?val='+v);
  document.getElementById('bv').textContent=v;
}
</script></body></html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", ROOT_HTML); }

// 返回传感器 JSON（网页轮询用）
void handleData() {
  String j = "{\"light\":" + String(sLight)
    + ",\"dark\":" + String(sDark)
    + ",\"lamp\":" + String(sLamp)
    + ",\"soiladc\":" + String(sSoilADC)
    + ",\"soil\":" + String(sSoil)
    + ",\"soildo\":" + String(sSoilDO)
    + ",\"pump\":" + String(sPump)
    + ",\"relay\":" + String(sRelay)
    + ",\"dist\":" + String(sDist, 0)
    + ",\"bright\":" + String(sBright) + "}";
  server.send(200, "application/json", j);
}

// 网页亮度滑块：收到值后发命令给 Arduino（英文）
void handleBright() {
  if (server.hasArg("val")) {
    int v = server.arg("val").toInt();
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    sBright = v;
    Serial.print("BRIGHT,"); Serial.println(v);   // 发给 Arduino
  }
  server.send(200, "text/plain", String(sBright));
}

void setup() {
  Serial.begin(9600);             // 与 Arduino 通信（UART0=GPIO1/3）

  // ---- 连局域网 WiFi（固定 IP）----
  WiFi.mode(WIFI_STA);
  WiFi.config(localIP, gateway, subnet, dns);   // 固定 IP
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print("Connecting to "); Serial.println(STA_SSID);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! Fixed IP: http://");
    Serial.println(WiFi.localIP());
  } else {
    // 兜底：15 秒连不上局域网就开热点，手机连 SmartFarm 仍可访问
    Serial.println("STA failed, fallback to AP mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("AP IP: http://");
    Serial.println(WiFi.softAPIP());
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/bright", handleBright);
  server.begin();
}

void loop() {
  // 心跳：每秒发 PING，Arduino 据此判定 NodeMCU 在线
  static unsigned long lastPing = 0;
  if (millis() - lastPing >= 1000) {
    lastPing = millis();
    Serial.println("PING");
  }

  // 非阻塞读 Arduino 发来的传感器数据（逐字节拼行，绝不卡网页服务器）
  static char rxBuf[80];
  static byte rxIdx = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      rxBuf[rxIdx] = '\0';
      rxIdx = 0;
      if (rxBuf[0] != '\0') parseLine(String(rxBuf));
    } else if (c != '\r' && rxIdx < 79) {
      rxBuf[rxIdx++] = c;
    }
  }
  server.handleClient();
}
