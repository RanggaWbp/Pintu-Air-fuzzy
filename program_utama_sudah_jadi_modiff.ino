#include <Wire.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <Fuzzy.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <BlynkSimpleEsp32.h>
#include <ArduinoOTA.h>
#include <Arduino.h>
#include <OneButton.h>
#include <PID_v1.h>

/*
 * pintu terbuka penuh 5cm,tutup penuh 28.56cm
 */
char auth[] = "wePXvZVHRPBKLe08lu24UTSrgDcxgLxw";
unsigned long blynk_upload = 0;
unsigned int blynk_timer = 500;
bool aktifkan_blynk = false;
bool kedip = false;

Fuzzy *fuzzy = new Fuzzy();
float level = 0;
bool force_level1 = false;
bool sedangDiLevel1Sementara = false;
String statusLevel = "Normal"; 
unsigned long waktuMasukLevel10 = 0;
unsigned long waktuMulaiLevel1 = 0;

const unsigned long durasiLevel10 = 720UL * 60 * 1000; // 30 menit
const unsigned long durasiLevel1 = 10UL * 60 * 1000;  // 10 menit


// ======== turbidity ==========
const int turbidity_pin = 33;  // pin ADC ESP32
float turbidity_cal = 0;
float turbVal;
float turbRaw;
float turbBuf[25];
float turbidity_calibration[][2] = {
  {2845, 1.9},
  {2842, 2.1},
  {2830, 3.5},
  {2825, 4.6},
  {2814, 8.0},
  {2682, 26.7},
  {2640, 27.8},
  {2580, 35.5},
  {2400, 55.1},
  {2380, 56.3}
};
const int turbidity_points = sizeof(turbidity_calibration) / sizeof(turbidity_calibration[0]);


// ============ Ph =============
const int pHPin = 32;
int phBuf[10], phTemp;
unsigned long int phAvg;
float phVal;

float ph_calibration[][2] = {
  {1100, 2.50},
  {1280, 2.80},
  {1397, 6.65},
  {1422, 7.75}
};
const int ph_cal_size = sizeof(ph_calibration) / sizeof(ph_calibration[0]);

// ======== TDS & suhu =========
float tdsVal;
const int TdsPin = 35;
const int ONE_WIRE_BUS = 23;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float suhu = 0;

// Data kalibrasi TDS: {ADC, ppm}
float tds_calibration[][2] = {
  {134, 214},
  {726, 560}
};
const int tds_cal_size = sizeof(tds_calibration) / sizeof(tds_calibration[0]);

int tdsBuf[10], temp;
unsigned long int avgValue;

// =========== LCD ============
LiquidCrystal_I2C lcd(0x27, 20, 4);

unsigned long lastSwitch = 0;
int currentPage = 0;
const unsigned long pageInterval = 2000; // 2 detik

// ======== SENSOR ARUS =======
const int sensorIn = 34;      // pin where the OUT pin from sensor is connected on Arduino
int mVperAmp = 66;           // this the 5A version of the ACS712 -use 100 for 20A Module and 66 for 30A Module
int Watt = 0;
double Voltage = 0;
double VRMS = 0;
double AmpsRMS = 0;
float amper = 0;

// ========== TOMBOL ==========

OneButton tkanan(14,false,false);
OneButton tkiri(12,false,false);
OneButton tmode(13,false,false);
// ========= MOTOR ============

const int motor_kanan = 18;
const int motor_kiri = 19;
float jarakTertutup = 28;
float jarakTerbuka = 5.0;
double input, output, setpoint;
double Kp = 25, Ki = 0.0, Kd = 8.0;
PID myPID(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);
String kondisi_motor[4] = {
  "IDLE    ","MEMBUKA..","MENUTUP..","ERROR   "
};
int state_motor = 0;


// ========= ultrasonic =======
const int TRIG_PINTU = 17;
const int ECHO_PINTU = 16;

const int TRIG_AIR_SEBELUM = 4;
const int ECHO_AIR_SEBELUM = 0;

const int TRIG_AIR_SESUDAH = 26;
const int ECHO_AIR_SESUDAH = 25;
float jarakPintu;
int jarakSebelum;
int jarakSesudah;

int modee = 1; //1 auto dengan fuzzy,2 manual
String modee_str = "AUTO";
String status_air;


// ========= Relay ===============
const int relay = 27;
bool relay_off = false;
float batas_cutoff = 8.0; //A

/////////////////////////////////////

float bacaJarak(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long durasi = pulseIn(echoPin, HIGH, 25000);  // timeout 25ms
  float jarak = durasi * 0.0343 / 2.0;
  return jarak;
}
WidgetTerminal terminal(V9);
// ========= Web server =============
WebServer server(80);
// HTML web UI
// HTML web UI dengan Dark Mode
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
 <meta charset="UTF-8">
 <title>Sensor Monitor</title>
 <style>
 * {
   box-sizing: border-box;
 }
 
 body {
   font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
   background: linear-gradient(135deg, #0c0c0c 0%, #1a1a1a 50%, #0f0f0f 100%);
   margin: 0;
   padding: 20px;
   text-align: center;
   min-height: 100vh;
   color: #ffffff;
 }
 
 h1 {
   color: #00d4ff;
   font-size: 2.5em;
   margin-bottom: 30px;
   text-shadow: 0 0 20px rgba(0, 212, 255, 0.5);
   font-weight: 300;
   letter-spacing: 2px;
 }
 
 .container {
   display: grid;
   grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
   gap: 20px;
   max-width: 1200px;
   margin: 20px auto;
   padding: 10px;
 }
 
 .box {
   background: linear-gradient(145deg, #1e1e1e, #2a2a2a);
   border: 1px solid rgba(0, 212, 255, 0.2);
   border-radius: 15px;
   box-shadow: 
     0 8px 32px rgba(0, 0, 0, 0.4),
     inset 0 1px 0 rgba(255, 255, 255, 0.1);
   padding: 25px;
   transition: all 0.3s ease;
   position: relative;
   overflow: hidden;
 }
 
 .box::before {
   content: '';
   position: absolute;
   top: 0;
   left: 0;
   right: 0;
   height: 2px;
   background: linear-gradient(90deg, #00d4ff, #00ff88, #ff6b6b);
   opacity: 0;
   transition: opacity 0.3s ease;
 }
 
 .box:hover {
   transform: translateY(-5px) scale(1.02);
   box-shadow: 
     0 12px 40px rgba(0, 212, 255, 0.3),
     inset 0 1px 0 rgba(255, 255, 255, 0.2);
   border-color: rgba(0, 212, 255, 0.5);
 }
 
 .box:hover::before {
   opacity: 1;
 }
 
 .label {
   font-size: 0.9em;
   color: #a0a0a0;
   margin-bottom: 10px;
   text-transform: uppercase;
   letter-spacing: 1px;
   font-weight: 500;
 }
 
 .value {
   font-size: 1.8em;
   font-weight: 600;
   color: #ffffff;
   text-shadow: 0 2px 10px rgba(0, 212, 255, 0.3);
   margin-top: 5px;
 }
 
 /* Status khusus dengan warna berbeda */
 .status-good { color: #00ff88 !important; }
 .status-warning { color: #ffaa00 !important; }
 .status-danger { color: #ff6b6b !important; }
 
 /* Animasi loading */
 .loading {
   color: #666 !important;
   animation: pulse 1.5s ease-in-out infinite;
 }
 
 @keyframes pulse {
   0%, 100% { opacity: 0.4; }
   50% { opacity: 1; }
 }
 
 /* Responsive design */
 @media (max-width: 768px) {
   .container {
     grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
     gap: 15px;
   }
   
   h1 {
     font-size: 2em;
   }
   
   .box {
     padding: 20px;
   }
   
   .value {
     font-size: 1.5em;
   }
 }
 
 @media (max-width: 480px) {
   .container {
     grid-template-columns: 1fr 1fr;
   }
   
   .box {
     padding: 15px;
   }
   
   .value {
     font-size: 1.3em;
   }
 }
 
 /* Scrollbar untuk browser webkit */
 ::-webkit-scrollbar {
   width: 8px;
 }
 
 ::-webkit-scrollbar-track {
   background: #1a1a1a;
 }
 
 ::-webkit-scrollbar-thumb {
   background: #00d4ff;
   border-radius: 4px;
 }
 
 ::-webkit-scrollbar-thumb:hover {
   background: #00a8cc;
 }
 </style>
 <script>
 function updateValueColor(elementId, value) {
   const element = document.getElementById(elementId);
   
   // Reset classes
   element.classList.remove('status-good', 'status-warning', 'status-danger');
   
   // Atur warna berdasarkan parameter dan nilai
   switch(elementId) {
     case 'ph':
       const phValue = parseFloat(value);
       if (phValue >= 6.5 && phValue <= 8.5) {
         element.classList.add('status-good');
       } else if (phValue >= 6.0 && phValue <= 9.0) {
         element.classList.add('status-warning');
       } else {
         element.classList.add('status-danger');
       }
       break;
       
     case 'status_air':
       if (value.toLowerCase().includes('baik') || value.toLowerCase().includes('normal')) {
         element.classList.add('status-good');
       } else if (value.toLowerCase().includes('sedang')) {
         element.classList.add('status-warning');
       } else {
         element.classList.add('status-danger');
       }
       break;
       
     case 'kondisi_motor':
       if (value.toLowerCase().includes('normal') || value.toLowerCase().includes('baik')) {
         element.classList.add('status-good');
       } else if (value.toLowerCase().includes('warning')) {
         element.classList.add('status-warning');
       } else {
         element.classList.add('status-danger');
       }
       break;
   }
 }
 
 function getData() {
   // Tampilkan loading state
   const elements = ['jarakPintu', 'jarakSebelum', 'jarakSesudah', 'tds', 'ph', 'turbidity', 
                    'levelOutput', 'arus', 'suhu', 'kondisi_motor', 'status_air', 'pwm_motor', 'timer'];
   
   fetch("/data")
     .then(response => response.json())
     .then(data => {
       // Update semua nilai
       document.getElementById("jarakPintu").innerText = data.jarakPintu + " cm";
       document.getElementById("jarakSebelum").innerText = data.jarakSebelum + " %";
       document.getElementById("jarakSesudah").innerText = data.jarakSesudah + " %";
       document.getElementById("tds").innerText = data.tds + " ppm";
       document.getElementById("ph").innerText = data.ph;
       document.getElementById("turbidity").innerText = data.turbidity + " NTU";
       document.getElementById("levelOutput").innerText = data.levelOutput;
       document.getElementById("arus").innerText = data.arus + " A";
       document.getElementById("suhu").innerText = data.suhu + " °C";
       document.getElementById("kondisi_motor").innerText = data.kondisi_motor;
       document.getElementById("status_air").innerText = data.status_air;
       document.getElementById("pwm_motor").innerText = data.pwm_motor;
       document.getElementById("timer").innerText = data.timer;
       
       // Update warna status
       updateValueColor('ph', data.ph);
       updateValueColor('status_air', data.status_air);
       updateValueColor('kondisi_motor', data.kondisi_motor);
       
       // Hapus loading class
       elements.forEach(id => {
         document.getElementById(id).classList.remove('loading');
       });
     })
     .catch(error => {
       console.error('Error fetching data:', error);
       // Tampilkan error state
       elements.forEach(id => {
         const element = document.getElementById(id);
         element.innerText = "Error";
         element.classList.add('status-danger');
       });
     });
 }
 
 setInterval(getData, 1000); // Ambil data tiap 1 detik
 
 window.onload = function() {
   // Set initial loading state
   const elements = ['jarakPintu', 'jarakSebelum', 'jarakSesudah', 'tds', 'ph', 'turbidity', 
                    'levelOutput', 'arus', 'suhu', 'kondisi_motor', 'status_air', 'pwm_motor', 'timer'];
   
   elements.forEach(id => {
     document.getElementById(id).classList.add('loading');
   });
   
   getData(); // Ambil data saat halaman pertama dibuka
 };
 </script>
</head>
<body>
 <h1>🌊 Data Sensor ESP32</h1>
 <div class="container">
   <div class="box">
     <div class="label">🚪 Jarak Pintu</div>
     <div class="value" id="jarakPintu">-</div>
   </div>
   <div class="box">
     <div class="label">📏 Jarak Air Sebelum</div>
     <div class="value" id="jarakSebelum">-</div>
   </div>
   <div class="box">
     <div class="label">📐 Jarak Air Sesudah</div>
     <div class="value" id="jarakSesudah">-</div>
   </div>
   <div class="box">
     <div class="label">⚗️ TDS</div>
     <div class="value" id="tds">-</div>
   </div>
   <div class="box">
     <div class="label">🌡️ Suhu</div>
     <div class="value" id="suhu">-</div>
   </div>
   <div class="box">
     <div class="label">🧪 pH</div>
     <div class="value" id="ph">-</div>
   </div>
   <div class="box">
     <div class="label">🌫️ Turbidity</div>
     <div class="value" id="turbidity">-</div>
   </div>
   <div class="box">
     <div class="label">📊 Output Level</div>
     <div class="value" id="levelOutput">-</div>
   </div>
   <div class="box">
     <div class="label">💧 Status Air</div>
     <div class="value" id="status_air">-</div>
   </div>
   <div class="box">
     <div class="label">⚡ Arus</div>
     <div class="value" id="arus">-</div>
   </div>
   <div class="box">
     <div class="label">⚙️ Status Motor</div>
     <div class="value" id="kondisi_motor">-</div>
   </div>
   <div class="box">
     <div class="label">🔧 PWM Motor</div>
     <div class="value" id="pwm_motor">-</div>
   </div>
   <div class="box">
     <div class="label">⏱️ Timer</div>
     <div class="value" id="timer">-</div>
   </div>
 </div>
</body>
</html>
)rawliteral";

void handleData() {
  String json = "{";
  json += "\"jarakPintu\":" + String(jarakPintu, 1) + ",";
  json += "\"jarakSebelum\":" + String(jarakSebelum) + ",";
  json += "\"jarakSesudah\":" + String(jarakSesudah) + ",";
  json += "\"tds\":" + String(tdsVal) + ",";
  json += "\"suhu\":" + String(suhu) + ",";
  json += "\"ph\":" + String(phVal, 2) + ",";
  json += "\"turbidity\":" + String(turbVal) + ",";
  json += "\"status_air\":\"" + status_air + "\",";
  json += "\"arus\":" + String(amper) + ",";
  json += "\"pwm_motor\":" + String(output) + ",";
  json += "\"kondisi_motor\":\"" + kondisi_motor[state_motor] + "\",";
  json += "\"timer\":\"" + statusLevel + "\",";
  json += "\"levelOutput\":" + String(level);
  json += "}";

  server.send(200, "application/json", json);
}
void handleRoot() {
  server.send_P(200, "text/html", htmlPage);
}

// ========= WIFi manager ==========
struct WiFiProfile {
  String ssid;
  String password;
};
bool online = false;

const int maxProfiles = 10;
WiFiProfile profiles[maxProfiles];
int profileCount = 0;
int currentProfile = 0;
String currentWifi;
unsigned long lastCheck = 0;
const int checkInterval = 15000; // 15 seconds

void tryConnect() {
  if (profileCount == 0) return; // No profiles available
  profiles[currentProfile].ssid.trim();
  profiles[currentProfile].password.trim();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connection failed, trying next profile...");
    WiFi.begin(profiles[currentProfile].ssid.c_str(), profiles[currentProfile].password.c_str());
    currentProfile = (currentProfile + 1) % profileCount; // Move to next profile
    online = false;
  } else {
    Serial.println("Connected to WiFi: " + profiles[currentProfile].ssid);
    currentWifi = profiles[currentProfile].ssid;
    online = true;
    Blynk.begin(auth, "", "","iot.serangkota.go.id",8080);
  }
}

void handleWifi() {
  String html = "<html><head><title>WiFi Manager</title><style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f9; margin-top: 50px; }";
  html += "form { display: inline-block; text-align: left; padding: 20px; background-color: #fff; border-radius: 10px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); }";
  html += "label, input { display: block; margin-bottom: 10px; font-size: 16px; }";
  html += "button { padding: 10px 20px; font-size: 16px; cursor: pointer; background-color: #4CAF50; color: white; border: none; border-radius: 5px; }";
  html += "button:hover { background-color: #45a049; }";
  html += "h1 { color: #333; }";
  html += "ul { list-style-type: none; padding: 0; margin: 20px 0; }";
html += "li { margin: 10px 0; font-size: 18px; display: flex; align-items: center; }";
html += ".delete-btn { background-color: #e74c3c; border: none; color: white; padding: 5px 10px; cursor: pointer; border-radius: 5px; font-size: 14px; margin-left: 10px; }";
html += ".delete-btn:hover { background-color: #c0392b; }";
  html += "</style></head><body>";
  html += "<h1>WiFi Manager</h1><form method='POST' action='/save'>";
  html += "<label>SSID: </label><input type='text' name='ssid' required><br>";
  html += "<label>Password: </label><input type='password' name='password' required><br>";
  html += "<button type='submit'>Save</button></form><h2>Saved Profiles</h2><ul>";
  
  for (int i = 0; i < profileCount; i++) {
    if(i % 2 == 0){
      html +=  " <form method='POST' action='/delete'><input type='hidden' name='index' value='" + String(i) + "'><button type='submit'>" + profiles[i].ssid + "</button></form>";
    }
    else html +=  " <form method='POST' action='/delete'><input type='hidden' name='index' value='" + String(i) + "'><button type='submit'>" + profiles[i].ssid + "</button></form><br>";
  }
  
  html += "</ul></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  if (profileCount >= maxProfiles) {
    server.send(200, "text/plain", "Maximum profiles reached.");
    return;
  }

  String ssid = server.arg("ssid");
  String password = server.arg("password");

  if (ssid.length() > 0 && password.length() > 0) {
    profiles[profileCount].ssid = ssid;
    profiles[profileCount].password = password;
    profileCount++;
    
    saveProfiles(); // Save profiles to LittleFS

    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "Profile saved.");
  } else {
    server.send(400, "text/plain", "Invalid input.");
  }
}

void handleDelete() {
  int index = server.arg("index").toInt();
  if (index < 0 || index >= profileCount) {
    server.send(400, "text/plain", "Invalid index.");
    return;
  }

  for (int i = index; i < profileCount - 1; i++) {
    profiles[i] = profiles[i + 1];
  }
  profileCount--;

  saveProfiles(); // Update profiles in LittleFS
  
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "Profile deleted.");
}

void saveProfiles() {
  File file = LittleFS.open("/profiles.txt", "w");
  if (!file) {
    Serial.println("Failed to open file for writing.");
    return;
  }

  for (int i = 0; i < profileCount; i++) {
    file.println(profiles[i].ssid + "," + profiles[i].password);
  }
  
  file.close();
  Serial.println("Profiles saved.");
}

void loadProfiles() {
  File file = LittleFS.open("/profiles.txt", "r");
  if (!file) {
    Serial.println("No saved profiles found.");
    return;
  }

  profileCount = 0;
  
  while (file.available() && profileCount < maxProfiles) {
    String line = file.readStringUntil('\n');
    int commaIndex = line.indexOf(',');
    if (commaIndex >= 0) {
      profiles[profileCount].ssid = line.substring(0, commaIndex);
      profiles[profileCount].password = line.substring(commaIndex + 1);
      profileCount++;
    }
  }
  for(int i = 0;i < profileCount;i++){
    Serial.println(profiles[i].ssid + "," + profiles[i].password);
  }
  file.close();
  Serial.println("Profiles loaded.");
}
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);  // resolusi ADC ESP32 0-4095
  lcd.init();
  lcd.backlight();
  pinMode(turbidity_pin,INPUT);
  pinMode(pHPin, INPUT);
  pinMode(TdsPin, INPUT);
  pinMode(TRIG_PINTU, OUTPUT);
  pinMode(ECHO_PINTU, INPUT);

  pinMode(TRIG_AIR_SEBELUM, OUTPUT);
  pinMode(ECHO_AIR_SEBELUM, INPUT);

  pinMode(TRIG_AIR_SESUDAH, OUTPUT);
  pinMode(ECHO_AIR_SESUDAH, INPUT);
  pinMode(motor_kanan,OUTPUT);
  pinMode(motor_kiri,OUTPUT);
  pinMode(relay,OUTPUT);
  digitalWrite(relay,LOW);
  digitalWrite(motor_kanan,LOW);
  digitalWrite(motor_kiri,LOW);
  ledcAttachPin(motor_kanan, 0);
  ledcAttachPin(motor_kiri, 1);
  ledcSetup(0, 12000, 8); // 1kHz, 8-bit
  ledcSetup(1, 12000, 8);
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(-150, 150); // Output disesuaikan PWM
  pinMode(2,OUTPUT);
  digitalWrite(2,HIGH);
  tkanan.attachClick([](){
    level++;
    if(level > 10) level = 10;
  });
  tkanan.attachLongPressStart([](){
    ledcWrite(0, 180);
    ledcWrite(1, 0); // Stop 
  });
  tkanan.attachLongPressStop([](){
    ledcWrite(0, 0);
    ledcWrite(1, 0); // Stop 
  });
  tkiri.attachClick([](){
    level--;
    if(level < 1) level = 1;
  });
  tkiri.attachLongPressStart([](){
    ledcWrite(0, 0);
    ledcWrite(1, 180); // Stop 
  });
  tkiri.attachLongPressStop([]{
    ledcWrite(0, 0);
    ledcWrite(1, 0); // Stop 
  });
  tmode.attachClick([](){
    modee++;
    if(modee > 2) modee = 1;
    if(modee == 1) modee_str = "AUTO";
    if(modee == 2) modee_str = "MANUAL";
  });
  tmode.attachLongPressStart([](){
    digitalWrite(relay,LOW);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("motor diaktifkan..");
    delay(1500);
    lcd.clear();
  });
  tmode.attachDoubleClick([](){
    aktifkan_blynk = true;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Blynk aktif..");
    delay(1000);
    lcd.clear();
  });
  sensors.begin();
  // Mode AP
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Monitor");
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  if (!LittleFS.begin()) {
    LittleFS.format();
    Serial.println("Failed to mount LittleFS.");
    return;
  }
  loadProfiles(); 
  ElegantOTA.begin(&server);
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/wifi", HTTP_GET, handleWifi);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/delete", HTTP_POST, handleDelete);
  server.begin();
  ArduinoOTA.setHostname("ESP32-IRIGASI");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nUpdate selesai!");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  int rawADC = analogRead(turbidity_pin);
  turbidity_cal = rawADC * (3.3 / 4095.0);  // konversi ke voltase
    //input 1 tds
    FuzzyInput *TDS = new FuzzyInput(1);
    FuzzySet *tds_rendah = new FuzzySet(0,0,15,25);
    FuzzySet *tds_normal = new FuzzySet(15,20,30,34);
    FuzzySet *tds_tinggi = new FuzzySet(25,35,50,50);
    TDS->addFuzzySet(tds_rendah);
    TDS->addFuzzySet(tds_normal);
    TDS->addFuzzySet(tds_tinggi);
    fuzzy->addFuzzyInput(TDS);
    //input 2 ph
    FuzzyInput *PH = new FuzzyInput(2);
    FuzzySet *ph_asam = new FuzzySet(0,0,5,7);
    FuzzySet *ph_netral = new FuzzySet(6,6,8,8);
    FuzzySet *ph_basa = new FuzzySet(7,9,14,14);
    PH->addFuzzySet(ph_asam);
    PH->addFuzzySet(ph_netral);
    PH->addFuzzySet(ph_basa);
    fuzzy->addFuzzyInput(PH);
    //input 3 turbididy
    FuzzyInput *TURBIDITY = new FuzzyInput(3);
    FuzzySet *turbidity_jernih = new FuzzySet(0,0,300,500);
    FuzzySet *turbidity_keruh = new FuzzySet(300,400,600,680);
    FuzzySet *turbidity_sangat_keruh = new FuzzySet(500,700,1000,1000);
    TURBIDITY->addFuzzySet(turbidity_jernih);
    TURBIDITY->addFuzzySet(turbidity_keruh);
    TURBIDITY->addFuzzySet(turbidity_sangat_keruh);
    fuzzy->addFuzzyInput(TURBIDITY);
    //output 1 level pintu irigasi
    FuzzyOutput *IRIGASI = new FuzzyOutput(1);
    FuzzySet *irigasi_buka = new FuzzySet(0,0,2,4);
    FuzzySet *irigasi_setengah = new FuzzySet(3,4,5,6);
    FuzzySet *irigasi_tutup = new FuzzySet(5,7,10,10);
    IRIGASI->addFuzzySet(irigasi_buka);
    IRIGASI->addFuzzySet(irigasi_setengah);
    IRIGASI->addFuzzySet(irigasi_tutup);
    fuzzy->addFuzzyOutput(IRIGASI);
  
  
  
    //rule 1 If (TDS is rendah) and (pH is Asam) and (Kekeruhan is Jernih) then (Pintu_Irigasi is Buka_Penuh) (1)
    FuzzyRuleAntecedent* tdsIsRendah = new FuzzyRuleAntecedent();tdsIsRendah->joinSingle(tds_rendah);
    FuzzyRuleAntecedent* phIsAsam = new FuzzyRuleAntecedent();
    phIsAsam->joinSingle(ph_asam);
    FuzzyRuleAntecedent* turbidityIsJernih = new FuzzyRuleAntecedent();
    turbidityIsJernih->joinSingle(turbidity_jernih);
    FuzzyRuleAntecedent* tdsAndPh = new FuzzyRuleAntecedent();
    tdsAndPh->joinWithAND(tdsIsRendah, phIsAsam);
    FuzzyRuleAntecedent* fullAntecedent = new FuzzyRuleAntecedent();
    fullAntecedent->joinWithAND(tdsAndPh, turbidityIsJernih);
    FuzzyRuleConsequent* thenPintuBukaPenuh = new FuzzyRuleConsequent();
    thenPintuBukaPenuh->addOutput(irigasi_buka);  // langsung pakai pointer, tidak perlu getFuzzySet()
    FuzzyRule* fuzzyRule1 = new FuzzyRule(1, fullAntecedent, thenPintuBukaPenuh);
    fuzzy->addFuzzyRule(fuzzyRule1);
  
  
    //rule 2
    FuzzyRuleAntecedent* r2_tds_rendah = new FuzzyRuleAntecedent();
    r2_tds_rendah->joinSingle(tds_rendah);
    FuzzyRuleAntecedent* r2_ph_asam = new FuzzyRuleAntecedent();
    r2_ph_asam->joinSingle(ph_asam);
    FuzzyRuleAntecedent* r2_keruh = new FuzzyRuleAntecedent();
    r2_keruh->joinSingle(turbidity_keruh);
    FuzzyRuleAntecedent* r2_tdsAndPh = new FuzzyRuleAntecedent();
    r2_tdsAndPh->joinWithAND(r2_tds_rendah, r2_ph_asam);
    FuzzyRuleAntecedent* r2_full = new FuzzyRuleAntecedent();
    r2_full->joinWithAND(r2_tdsAndPh, r2_keruh);
    FuzzyRuleConsequent* r2_consequent = new FuzzyRuleConsequent();
    r2_consequent->addOutput(irigasi_setengah); // Setengah_Terbuka
    FuzzyRule* rule2 = new FuzzyRule(2, r2_full, r2_consequent);
    fuzzy->addFuzzyRule(rule2);

  // === Rule 3 ===
    FuzzyRuleAntecedent* r3_tds_rendah = new FuzzyRuleAntecedent();
    r3_tds_rendah->joinSingle(tds_rendah);
    FuzzyRuleAntecedent* r3_ph_asam = new FuzzyRuleAntecedent();
    r3_ph_asam->joinSingle(ph_asam);
    FuzzyRuleAntecedent* r3_sangat_keruh = new FuzzyRuleAntecedent();
    r3_sangat_keruh->joinSingle(turbidity_sangat_keruh);
    FuzzyRuleAntecedent* r3_tdsAndPh = new FuzzyRuleAntecedent();
    r3_tdsAndPh->joinWithAND(r3_tds_rendah, r3_ph_asam);
    FuzzyRuleAntecedent* r3_full = new FuzzyRuleAntecedent();
    r3_full->joinWithAND(r3_tdsAndPh, r3_sangat_keruh);
    FuzzyRuleConsequent* r3_consequent = new FuzzyRuleConsequent();
    r3_consequent->addOutput(irigasi_tutup); // Tutup
    FuzzyRule* rule3 = new FuzzyRule(3, r3_full, r3_consequent);
    fuzzy->addFuzzyRule(rule3);
    
    // === Rule 4 ===
    FuzzyRuleAntecedent* r4_tds_rendah = new FuzzyRuleAntecedent();
    r4_tds_rendah->joinSingle(tds_rendah);
    FuzzyRuleAntecedent* r4_ph_netral = new FuzzyRuleAntecedent();
    r4_ph_netral->joinSingle(ph_netral);
    FuzzyRuleAntecedent* r4_jernih = new FuzzyRuleAntecedent();
    r4_jernih->joinSingle(turbidity_jernih);
    FuzzyRuleAntecedent* r4_tdsAndPh = new FuzzyRuleAntecedent();
    r4_tdsAndPh->joinWithAND(r4_tds_rendah, r4_ph_netral);
    FuzzyRuleAntecedent* r4_full = new FuzzyRuleAntecedent();
    r4_full->joinWithAND(r4_tdsAndPh, r4_jernih);
    FuzzyRuleConsequent* r4_consequent = new FuzzyRuleConsequent();
    r4_consequent->addOutput(irigasi_buka); 
    FuzzyRule* rule4 = new FuzzyRule(4, r4_full, r4_consequent);
    fuzzy->addFuzzyRule(rule4);

    // Rule 5: TDS rendah, pH netral, Kekeruhan keruh → irigasi setengah
    FuzzyRuleAntecedent* r5_a = new FuzzyRuleAntecedent(); r5_a->joinSingle(tds_rendah);
    FuzzyRuleAntecedent* r5_b = new FuzzyRuleAntecedent(); r5_b->joinSingle(ph_netral);
    FuzzyRuleAntecedent* r5_c = new FuzzyRuleAntecedent(); r5_c->joinSingle(turbidity_keruh);
    FuzzyRuleAntecedent* r5_ab = new FuzzyRuleAntecedent(); r5_ab->joinWithAND(r5_a, r5_b);
    FuzzyRuleAntecedent* r5_full = new FuzzyRuleAntecedent(); r5_full->joinWithAND(r5_ab, r5_c);
    FuzzyRuleConsequent* r5_then = new FuzzyRuleConsequent(); r5_then->addOutput(irigasi_setengah);
    FuzzyRule* rule5 = new FuzzyRule(5, r5_full, r5_then);
    fuzzy->addFuzzyRule(rule5);
    // Rule 6: TDS rendah, pH netral, Kekeruhan sangat keruh → irigasi tutup
    FuzzyRuleAntecedent* r6_a = new FuzzyRuleAntecedent(); r6_a->joinSingle(tds_rendah);
    FuzzyRuleAntecedent* r6_b = new FuzzyRuleAntecedent(); r6_b->joinSingle(ph_netral);
    FuzzyRuleAntecedent* r6_c = new FuzzyRuleAntecedent(); r6_c->joinSingle(turbidity_sangat_keruh);
    FuzzyRuleAntecedent* r6_ab = new FuzzyRuleAntecedent(); r6_ab->joinWithAND(r6_a, r6_b);
    FuzzyRuleAntecedent* r6_full = new FuzzyRuleAntecedent(); r6_full->joinWithAND(r6_ab, r6_c);
    FuzzyRuleConsequent* r6_then = new FuzzyRuleConsequent(); r6_then->addOutput(irigasi_tutup);
    FuzzyRule* rule6 = new FuzzyRule(6, r6_full, r6_then);
    fuzzy->addFuzzyRule(rule6);
    // Rule 7: TDS rendah, pH basa, Kekeruhan jernih → irigasi buka
    FuzzyRuleAntecedent* r7_a = new FuzzyRuleAntecedent(); r7_a->joinSingle(tds_rendah);
    FuzzyRuleAntecedent* r7_b = new FuzzyRuleAntecedent(); r7_b->joinSingle(ph_basa);
    FuzzyRuleAntecedent* r7_c = new FuzzyRuleAntecedent(); r7_c->joinSingle(turbidity_jernih);
    FuzzyRuleAntecedent* r7_ab = new FuzzyRuleAntecedent(); r7_ab->joinWithAND(r7_a, r7_b);
    FuzzyRuleAntecedent* r7_full = new FuzzyRuleAntecedent(); r7_full->joinWithAND(r7_ab, r7_c);
    FuzzyRuleConsequent* r7_then = new FuzzyRuleConsequent(); r7_then->addOutput(irigasi_buka);
    FuzzyRule* rule7 = new FuzzyRule(7, r7_full, r7_then);
    fuzzy->addFuzzyRule(rule7);
    // Rule 8: TDS rendah, pH basa, Kekeruhan keruh → irigasi setengah
    FuzzyRuleAntecedent* r8_a = new FuzzyRuleAntecedent(); r8_a->joinSingle(tds_rendah);
    FuzzyRuleAntecedent* r8_b = new FuzzyRuleAntecedent(); r8_b->joinSingle(ph_basa);
    FuzzyRuleAntecedent* r8_c = new FuzzyRuleAntecedent(); r8_c->joinSingle(turbidity_keruh);
    FuzzyRuleAntecedent* r8_ab = new FuzzyRuleAntecedent(); r8_ab->joinWithAND(r8_a, r8_b);
    FuzzyRuleAntecedent* r8_full = new FuzzyRuleAntecedent(); r8_full->joinWithAND(r8_ab, r8_c);
    FuzzyRuleConsequent* r8_then = new FuzzyRuleConsequent(); r8_then->addOutput(irigasi_setengah);
    FuzzyRule* rule8 = new FuzzyRule(8, r8_full, r8_then);
    fuzzy->addFuzzyRule(rule8);
    // Rule 9: TDS rendah, pH basa, Kekeruhan sangat keruh → irigasi tutup
    FuzzyRuleAntecedent* r9_a = new FuzzyRuleAntecedent(); r9_a->joinSingle(tds_rendah);
    FuzzyRuleAntecedent* r9_b = new FuzzyRuleAntecedent(); r9_b->joinSingle(ph_basa);
    FuzzyRuleAntecedent* r9_c = new FuzzyRuleAntecedent(); r9_c->joinSingle(turbidity_sangat_keruh);
    FuzzyRuleAntecedent* r9_ab = new FuzzyRuleAntecedent(); r9_ab->joinWithAND(r9_a, r9_b);
    FuzzyRuleAntecedent* r9_full = new FuzzyRuleAntecedent(); r9_full->joinWithAND(r9_ab, r9_c);
    FuzzyRuleConsequent* r9_then = new FuzzyRuleConsequent(); r9_then->addOutput(irigasi_tutup);
    FuzzyRule* rule9 = new FuzzyRule(9, r9_full, r9_then);
    fuzzy->addFuzzyRule(rule9);
    // Rule 10: TDS normal, pH asam, Kekeruhan jernih → irigasi buka
    FuzzyRuleAntecedent* r10_a = new FuzzyRuleAntecedent(); r10_a->joinSingle(tds_normal);
    FuzzyRuleAntecedent* r10_b = new FuzzyRuleAntecedent(); r10_b->joinSingle(ph_asam);
    FuzzyRuleAntecedent* r10_c = new FuzzyRuleAntecedent(); r10_c->joinSingle(turbidity_jernih);
    FuzzyRuleAntecedent* r10_ab = new FuzzyRuleAntecedent(); r10_ab->joinWithAND(r10_a, r10_b);
    FuzzyRuleAntecedent* r10_full = new FuzzyRuleAntecedent(); r10_full->joinWithAND(r10_ab, r10_c);
    FuzzyRuleConsequent* r10_then = new FuzzyRuleConsequent(); r10_then->addOutput(irigasi_buka);
    FuzzyRule* rule10 = new FuzzyRule(10, r10_full, r10_then);
    fuzzy->addFuzzyRule(rule10);
    // Rule 11 TDS Normal, pH Asam, Kekeruhan Keruh → Pintu_Irigasi Setengah_Terbuka
    FuzzyRuleAntecedent* r11_a = new FuzzyRuleAntecedent(); r11_a->joinSingle(tds_normal);
    FuzzyRuleAntecedent* r11_b = new FuzzyRuleAntecedent(); r11_b->joinSingle(ph_asam);
    FuzzyRuleAntecedent* r11_c = new FuzzyRuleAntecedent(); r11_c->joinSingle(turbidity_keruh);
    FuzzyRuleAntecedent* r11_ab = new FuzzyRuleAntecedent(); r11_ab->joinWithAND(r11_a, r11_b);
    FuzzyRuleAntecedent* r11_full = new FuzzyRuleAntecedent(); r11_full->joinWithAND(r11_ab, r11_c);
    FuzzyRuleConsequent* r11_then = new FuzzyRuleConsequent(); r11_then->addOutput(irigasi_setengah);
    FuzzyRule* rule11 = new FuzzyRule(11, r11_full, r11_then);
    fuzzy->addFuzzyRule(rule11);
    // Rule 12 TDS Normal, pH Asam, Kekeruhan Sangat_Keruh → Tutup
    FuzzyRuleAntecedent* r12_a = new FuzzyRuleAntecedent(); r12_a->joinSingle(tds_normal);
    FuzzyRuleAntecedent* r12_b = new FuzzyRuleAntecedent(); r12_b->joinSingle(ph_asam);
    FuzzyRuleAntecedent* r12_c = new FuzzyRuleAntecedent(); r12_c->joinSingle(turbidity_sangat_keruh);
    FuzzyRuleAntecedent* r12_ab = new FuzzyRuleAntecedent(); r12_ab->joinWithAND(r12_a, r12_b);
    FuzzyRuleAntecedent* r12_full = new FuzzyRuleAntecedent(); r12_full->joinWithAND(r12_ab, r12_c);
    FuzzyRuleConsequent* r12_then = new FuzzyRuleConsequent(); r12_then->addOutput(irigasi_tutup);
    FuzzyRule* rule12 = new FuzzyRule(12, r12_full, r12_then);
    fuzzy->addFuzzyRule(rule12);
    // Rule 13 TDS Normal, pH Netral, Kekeruhan Jernih → Buka_Penuh
    FuzzyRuleAntecedent* r13_a = new FuzzyRuleAntecedent(); r13_a->joinSingle(tds_normal);
    FuzzyRuleAntecedent* r13_b = new FuzzyRuleAntecedent(); r13_b->joinSingle(ph_netral);
    FuzzyRuleAntecedent* r13_c = new FuzzyRuleAntecedent(); r13_c->joinSingle(turbidity_jernih);
    FuzzyRuleAntecedent* r13_ab = new FuzzyRuleAntecedent(); r13_ab->joinWithAND(r13_a, r13_b);
    FuzzyRuleAntecedent* r13_full = new FuzzyRuleAntecedent(); r13_full->joinWithAND(r13_ab, r13_c);
    FuzzyRuleConsequent* r13_then = new FuzzyRuleConsequent(); r13_then->addOutput(irigasi_buka);
    FuzzyRule* rule13 = new FuzzyRule(13, r13_full, r13_then);
    fuzzy->addFuzzyRule(rule13);
    // Rule 14 TDS Normal, pH Netral, Kekeruhan Keruh → Setengah_Terbuka
    FuzzyRuleAntecedent* r14_a = new FuzzyRuleAntecedent(); r14_a->joinSingle(tds_normal);
    FuzzyRuleAntecedent* r14_b = new FuzzyRuleAntecedent(); r14_b->joinSingle(ph_netral);
    FuzzyRuleAntecedent* r14_c = new FuzzyRuleAntecedent(); r14_c->joinSingle(turbidity_keruh);
    FuzzyRuleAntecedent* r14_ab = new FuzzyRuleAntecedent(); r14_ab->joinWithAND(r14_a, r14_b);
    FuzzyRuleAntecedent* r14_full = new FuzzyRuleAntecedent(); r14_full->joinWithAND(r14_ab, r14_c);
    FuzzyRuleConsequent* r14_then = new FuzzyRuleConsequent(); r14_then->addOutput(irigasi_setengah);
    FuzzyRule* rule14 = new FuzzyRule(14, r14_full, r14_then);
    fuzzy->addFuzzyRule(rule14);
    // Rule 15 TDS Normal, pH Netral, Kekeruhan Sangat_Keruh → Tutup
    FuzzyRuleAntecedent* r15_a = new FuzzyRuleAntecedent(); r15_a->joinSingle(tds_normal);
    FuzzyRuleAntecedent* r15_b = new FuzzyRuleAntecedent(); r15_b->joinSingle(ph_netral);
    FuzzyRuleAntecedent* r15_c = new FuzzyRuleAntecedent(); r15_c->joinSingle(turbidity_sangat_keruh);
    FuzzyRuleAntecedent* r15_ab = new FuzzyRuleAntecedent(); r15_ab->joinWithAND(r15_a, r15_b);
    FuzzyRuleAntecedent* r15_full = new FuzzyRuleAntecedent(); r15_full->joinWithAND(r15_ab, r15_c);
    FuzzyRuleConsequent* r15_then = new FuzzyRuleConsequent(); r15_then->addOutput(irigasi_tutup);
    FuzzyRule* rule15 = new FuzzyRule(15, r15_full, r15_then);
    fuzzy->addFuzzyRule(rule15);
    //Rule 16 TDS Normal, pH Basa, Kekeruhan Jernih → Buka_Penuh
    FuzzyRuleAntecedent* r16_a = new FuzzyRuleAntecedent(); r16_a->joinSingle(tds_normal);
    FuzzyRuleAntecedent* r16_b = new FuzzyRuleAntecedent(); r16_b->joinSingle(ph_basa);
    FuzzyRuleAntecedent* r16_c = new FuzzyRuleAntecedent(); r16_c->joinSingle(turbidity_jernih);
    FuzzyRuleAntecedent* r16_ab = new FuzzyRuleAntecedent(); r16_ab->joinWithAND(r16_a, r16_b);
    FuzzyRuleAntecedent* r16_full = new FuzzyRuleAntecedent(); r16_full->joinWithAND(r16_ab, r16_c);
    FuzzyRuleConsequent* r16_then = new FuzzyRuleConsequent(); r16_then->addOutput(irigasi_buka);
    FuzzyRule* rule16 = new FuzzyRule(16, r16_full, r16_then);
    fuzzy->addFuzzyRule(rule16);
    //Rule 17 TDS Normal, pH Basa, Kekeruhan Keruh → Setengah_Terbuka
    FuzzyRuleAntecedent* r17_a = new FuzzyRuleAntecedent(); r17_a->joinSingle(tds_normal);
    FuzzyRuleAntecedent* r17_b = new FuzzyRuleAntecedent(); r17_b->joinSingle(ph_basa);
    FuzzyRuleAntecedent* r17_c = new FuzzyRuleAntecedent(); r17_c->joinSingle(turbidity_keruh);
    FuzzyRuleAntecedent* r17_ab = new FuzzyRuleAntecedent(); r17_ab->joinWithAND(r17_a, r17_b);
    FuzzyRuleAntecedent* r17_full = new FuzzyRuleAntecedent(); r17_full->joinWithAND(r17_ab, r17_c);
    FuzzyRuleConsequent* r17_then = new FuzzyRuleConsequent(); r17_then->addOutput(irigasi_setengah);
    FuzzyRule* rule17 = new FuzzyRule(17, r17_full, r17_then);
    fuzzy->addFuzzyRule(rule17);
    //Rule 18 TDS Normal, pH Basa, Kekeruhan Sangat_Keruh → Tutup
    FuzzyRuleAntecedent* r18_a = new FuzzyRuleAntecedent(); r18_a->joinSingle(tds_normal);
    FuzzyRuleAntecedent* r18_b = new FuzzyRuleAntecedent(); r18_b->joinSingle(ph_basa);
    FuzzyRuleAntecedent* r18_c = new FuzzyRuleAntecedent(); r18_c->joinSingle(turbidity_sangat_keruh);
    FuzzyRuleAntecedent* r18_ab = new FuzzyRuleAntecedent(); r18_ab->joinWithAND(r18_a, r18_b);
    FuzzyRuleAntecedent* r18_full = new FuzzyRuleAntecedent(); r18_full->joinWithAND(r18_ab, r18_c);
    FuzzyRuleConsequent* r18_then = new FuzzyRuleConsequent(); r18_then->addOutput(irigasi_tutup);
    FuzzyRule* rule18 = new FuzzyRule(18, r18_full, r18_then);
    fuzzy->addFuzzyRule(rule18);
    //Rule 19 TDS Tinggi, pH Asam, Kekeruhan Jernih → Setengah_Terbuka
    FuzzyRuleAntecedent* r19_a = new FuzzyRuleAntecedent(); r19_a->joinSingle(tds_tinggi);
    FuzzyRuleAntecedent* r19_b = new FuzzyRuleAntecedent(); r19_b->joinSingle(ph_asam);
    FuzzyRuleAntecedent* r19_c = new FuzzyRuleAntecedent(); r19_c->joinSingle(turbidity_jernih);
    FuzzyRuleAntecedent* r19_ab = new FuzzyRuleAntecedent(); r19_ab->joinWithAND(r19_a, r19_b);
    FuzzyRuleAntecedent* r19_full = new FuzzyRuleAntecedent(); r19_full->joinWithAND(r19_ab, r19_c);
    FuzzyRuleConsequent* r19_then = new FuzzyRuleConsequent(); r19_then->addOutput(irigasi_setengah);
    FuzzyRule* rule19 = new FuzzyRule(19, r19_full, r19_then);
    fuzzy->addFuzzyRule(rule19);
    //Rule 20 TDS Tinggi, pH Asam, Kekeruhan Keruh → Tutup
    FuzzyRuleAntecedent* r20_a = new FuzzyRuleAntecedent(); r20_a->joinSingle(tds_tinggi);
    FuzzyRuleAntecedent* r20_b = new FuzzyRuleAntecedent(); r20_b->joinSingle(ph_asam);
    FuzzyRuleAntecedent* r20_c = new FuzzyRuleAntecedent(); r20_c->joinSingle(turbidity_keruh);
    FuzzyRuleAntecedent* r20_ab = new FuzzyRuleAntecedent(); r20_ab->joinWithAND(r20_a, r20_b);
    FuzzyRuleAntecedent* r20_full = new FuzzyRuleAntecedent(); r20_full->joinWithAND(r20_ab, r20_c);
    FuzzyRuleConsequent* r20_then = new FuzzyRuleConsequent(); r20_then->addOutput(irigasi_tutup);
    FuzzyRule* rule20 = new FuzzyRule(20, r20_full, r20_then);
    fuzzy->addFuzzyRule(rule20);
    //Rule 21 TDS Tinggi, pH Asam, Kekeruhan Sangat_Keruh → Tutup 
    FuzzyRuleAntecedent* r21_a = new FuzzyRuleAntecedent(); r21_a->joinSingle(tds_tinggi);
    FuzzyRuleAntecedent* r21_b = new FuzzyRuleAntecedent(); r21_b->joinSingle(ph_asam);
    FuzzyRuleAntecedent* r21_c = new FuzzyRuleAntecedent(); r21_c->joinSingle(turbidity_sangat_keruh);
    FuzzyRuleAntecedent* r21_ab = new FuzzyRuleAntecedent(); r21_ab->joinWithAND(r21_a, r21_b);
    FuzzyRuleAntecedent* r21_full = new FuzzyRuleAntecedent(); r21_full->joinWithAND(r21_ab, r21_c);
    FuzzyRuleConsequent* r21_then = new FuzzyRuleConsequent(); r21_then->addOutput(irigasi_tutup);
    FuzzyRule* rule21 = new FuzzyRule(21, r21_full, r21_then);
    fuzzy->addFuzzyRule(rule21);
    //Rule 22 TDS Tinggi, pH Netral, Kekeruhan Jernih → Setengah_Terbuka
    FuzzyRuleAntecedent* r22_a = new FuzzyRuleAntecedent(); r22_a->joinSingle(tds_tinggi);
    FuzzyRuleAntecedent* r22_b = new FuzzyRuleAntecedent(); r22_b->joinSingle(ph_netral);
    FuzzyRuleAntecedent* r22_c = new FuzzyRuleAntecedent(); r22_c->joinSingle(turbidity_jernih);
    FuzzyRuleAntecedent* r22_ab = new FuzzyRuleAntecedent(); r22_ab->joinWithAND(r22_a, r22_b);
    FuzzyRuleAntecedent* r22_full = new FuzzyRuleAntecedent(); r22_full->joinWithAND(r22_ab, r22_c);
    FuzzyRuleConsequent* r22_then = new FuzzyRuleConsequent(); r22_then->addOutput(irigasi_setengah);
    FuzzyRule* rule22 = new FuzzyRule(22, r22_full, r22_then);
    fuzzy->addFuzzyRule(rule22);
    //Rule 23 TDS Tinggi, pH Netral, Kekeruhan Keruh → Tutup 
    FuzzyRuleAntecedent* r23_a = new FuzzyRuleAntecedent(); r23_a->joinSingle(tds_tinggi);
    FuzzyRuleAntecedent* r23_b = new FuzzyRuleAntecedent(); r23_b->joinSingle(ph_netral);
    FuzzyRuleAntecedent* r23_c = new FuzzyRuleAntecedent(); r23_c->joinSingle(turbidity_keruh);
    FuzzyRuleAntecedent* r23_ab = new FuzzyRuleAntecedent(); r23_ab->joinWithAND(r23_a, r23_b);
    FuzzyRuleAntecedent* r23_full = new FuzzyRuleAntecedent(); r23_full->joinWithAND(r23_ab, r23_c);
    FuzzyRuleConsequent* r23_then = new FuzzyRuleConsequent(); r23_then->addOutput(irigasi_tutup);
    FuzzyRule* rule23 = new FuzzyRule(23, r23_full, r23_then);
    fuzzy->addFuzzyRule(rule23);
    
    //Rule 24 TDS Tinggi, pH Netral, Kekeruhan Sangat_Keruh → Tutup
    FuzzyRuleAntecedent* r24_a = new FuzzyRuleAntecedent(); r24_a->joinSingle(tds_tinggi);
    FuzzyRuleAntecedent* r24_b = new FuzzyRuleAntecedent(); r24_b->joinSingle(ph_netral);
    FuzzyRuleAntecedent* r24_c = new FuzzyRuleAntecedent(); r24_c->joinSingle(turbidity_sangat_keruh);
    FuzzyRuleAntecedent* r24_ab = new FuzzyRuleAntecedent(); r24_ab->joinWithAND(r24_a, r24_b);
    FuzzyRuleAntecedent* r24_full = new FuzzyRuleAntecedent(); r24_full->joinWithAND(r24_ab, r24_c);
    FuzzyRuleConsequent* r24_then = new FuzzyRuleConsequent(); r24_then->addOutput(irigasi_tutup);
    FuzzyRule* rule24 = new FuzzyRule(24, r24_full, r24_then);
    fuzzy->addFuzzyRule(rule24);
    
    //Rule 25 TDS Tinggi, pH Basa, Kekeruhan Jernih → Setengah_Terbuka
    FuzzyRuleAntecedent* r25_a = new FuzzyRuleAntecedent(); r25_a->joinSingle(tds_tinggi);
    FuzzyRuleAntecedent* r25_b = new FuzzyRuleAntecedent(); r25_b->joinSingle(ph_basa);
    FuzzyRuleAntecedent* r25_c = new FuzzyRuleAntecedent(); r25_c->joinSingle(turbidity_jernih);
    FuzzyRuleAntecedent* r25_ab = new FuzzyRuleAntecedent(); r25_ab->joinWithAND(r25_a, r25_b);
    FuzzyRuleAntecedent* r25_full = new FuzzyRuleAntecedent(); r25_full->joinWithAND(r25_ab, r25_c);
    FuzzyRuleConsequent* r25_then = new FuzzyRuleConsequent(); r25_then->addOutput(irigasi_setengah);
    FuzzyRule* rule25 = new FuzzyRule(25, r25_full, r25_then);
    fuzzy->addFuzzyRule(rule25);
    
    //Rule 26 TDS Tinggi, pH Basa, Kekeruhan Keruh → Tutup 
    FuzzyRuleAntecedent* r26_a = new FuzzyRuleAntecedent(); r26_a->joinSingle(tds_tinggi);
    FuzzyRuleAntecedent* r26_b = new FuzzyRuleAntecedent(); r26_b->joinSingle(ph_basa);
    FuzzyRuleAntecedent* r26_c = new FuzzyRuleAntecedent(); r26_c->joinSingle(turbidity_keruh);
    FuzzyRuleAntecedent* r26_ab = new FuzzyRuleAntecedent(); r26_ab->joinWithAND(r26_a, r26_b);
    FuzzyRuleAntecedent* r26_full = new FuzzyRuleAntecedent(); r26_full->joinWithAND(r26_ab, r26_c);
    FuzzyRuleConsequent* r26_then = new FuzzyRuleConsequent(); r26_then->addOutput(irigasi_tutup);
    FuzzyRule* rule26 = new FuzzyRule(26, r26_full, r26_then);
    fuzzy->addFuzzyRule(rule26);
    //Rule 27 TDS Tinggi, pH Basa, Kekeruhan Sangat_Keruh → Tutup
    FuzzyRuleAntecedent* r27_a = new FuzzyRuleAntecedent(); r27_a->joinSingle(tds_tinggi);
    FuzzyRuleAntecedent* r27_b = new FuzzyRuleAntecedent(); r27_b->joinSingle(ph_basa);
    FuzzyRuleAntecedent* r27_c = new FuzzyRuleAntecedent(); r27_c->joinSingle(turbidity_sangat_keruh);
    FuzzyRuleAntecedent* r27_ab = new FuzzyRuleAntecedent(); r27_ab->joinWithAND(r27_a, r27_b);
    FuzzyRuleAntecedent* r27_full = new FuzzyRuleAntecedent(); r27_full->joinWithAND(r27_ab, r27_c);
    FuzzyRuleConsequent* r27_then = new FuzzyRuleConsequent(); r27_then->addOutput(irigasi_tutup);
    FuzzyRule* rule27 = new FuzzyRule(27, r27_full, r27_then);
    fuzzy->addFuzzyRule(rule27);
    // Task 1 di Core 0 (biasanya PRO_CPU)
  xTaskCreatePinnedToCore(
    Task1,        // Fungsi
    "Task1",      // Nama task
    10000,        // Stack size
    NULL,         // Parameter
    1,            // Prioritas
    NULL,         // Handle
    0             // Core 0
  );

  // Task 2 di Core 1 (biasanya APP_CPU)
  xTaskCreatePinnedToCore(
    Task2,
    "Task2",
    10000,
    NULL,
    1,
    NULL,
    1            // Core 1
  );


}
void Task1(void *pvParameters) {
  for (;;) {
    main_loop();
 
  if (currentPage == 0) {
      lcd.setCursor(0, 0);
      lcd.print("Pintu : "); lcd.print(jarakPintu); lcd.print("cm ");
      lcd.setCursor(0, 1);
      lcd.print("Sblm  : "); lcd.print(jarakSebelum); lcd.print("%  ");
      lcd.setCursor(0, 2);
      lcd.print("Stlh  : "); lcd.print(jarakSesudah); lcd.print("%  ");
      lcd.setCursor(0, 3);
      lcd.print("Level : "); lcd.print(level); lcd.print(" " + modee_str);
    } else if (currentPage == 1) {
      lcd.setCursor(0, 0);
      lcd.print("TDS   : "); lcd.print(tdsVal); lcd.print("ppm ");
      lcd.setCursor(0, 1);
      lcd.print("pH    : "); lcd.print(phVal);
      lcd.setCursor(0, 2);
      lcd.print("Turb  : "); lcd.print(turbVal); lcd.print(" NTU ");
      lcd.setCursor(0, 3);
      lcd.print("Arus  : ");
      lcd.print(amper);
      lcd.print("A");
    } else if (currentPage == 2) {
      lcd.setCursor(0, 0);
      lcd.print("Suhu  : "); lcd.print(suhu); lcd.print("C ");
      lcd.setCursor(0, 1);
      lcd.print("Motor : "); lcd.print(kondisi_motor[state_motor]);
      lcd.setCursor(0, 2);
      lcd.print("Air   : "); lcd.print(status_air);
      lcd.setCursor(0, 3);
      lcd.print(statusLevel);
    }
  if (millis() - blynk_upload >= blynk_timer && online){
    blynk_upload  = millis();
    if(Blynk.connected() && aktifkan_blynk){
      Blynk.virtualWrite(V0, jarakPintu);
      Blynk.virtualWrite(V1, jarakSebelum);
      Blynk.virtualWrite(V2, jarakSesudah);
      Blynk.virtualWrite(V3, tdsVal);
      Blynk.virtualWrite(V4, phVal);
      Blynk.virtualWrite(V5, turbVal);
      Blynk.virtualWrite(V6, level);
      Blynk.virtualWrite(V7, amper);
      Blynk.virtualWrite(V8, suhu);
      Blynk.virtualWrite(V10, kondisi_motor[state_motor]);
      Blynk.virtualWrite(V11, status_air);
      Blynk.virtualWrite(V12, statusLevel);
      Blynk.virtualWrite(V13, durasiLevel10);
      Blynk.virtualWrite(V14, durasiLevel1);
      Blynk.virtualWrite(V15, ledcRead(0));
      Blynk.virtualWrite(V16, modee_str);
      terminal.println(output);
      terminal.flush();
    }
    kedip = !kedip;
    digitalWrite(2,kedip);
  }
  if(online && aktifkan_blynk) {
    Blynk.run();
  }
  if (millis() - lastCheck > checkInterval) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      tryConnect();
    }
    else {
      Serial.println("Connected to WiFi: " + profiles[currentProfile].ssid);
      currentWifi = profiles[currentProfile].ssid;
      online = true;
      if(aktifkan_blynk) Blynk.begin(auth, "", "","iot.serangkota.go.id",8080);
    }
  }
  }
}
void fungsi_motor(){
  tkanan.tick();
  tkiri.tick();
  tmode.tick();
  ArduinoOTA.handle(); 
  // Konversi level buka ke jarak target
  
  float levelTerbalik = 10 - level;
  setpoint = jarakTertutup - ((jarakTertutup - jarakTerbuka) / 10.0) * levelTerbalik;
  
  // Update input (feedback dari sensor)
  input = jarakPintu;
 
  // Jalankan PID
  myPID.Compute();
  double error = abs(setpoint - input);
  if (error < 0.5) {  // threshold dead zone, bisa kamu ubah
    output = 0;
  }
  if(!(jarakPintu >= jarakTerbuka - 1 && jarakPintu <= jarakTertutup)){
    state_motor = 3;
  }
    else if(output >= -15 && output <= 15){
    state_motor = 0;
  } else if(output > 15){
    state_motor = 2;
  } else if(output < -15){
    state_motor = 1;
  }
  // Kendali motor berdasarkan output 
  if ( state_motor == 3){
    ledcWrite(0, 0);
    ledcWrite(1, 0); // Stop 
  } else if (output > 0) {
    ledcWrite(0, output); // Motor maju
    ledcWrite(1, 0);
  } else if (output < 0) {
    ledcWrite(0, 0);
    ledcWrite(1, -output); // Motor mundur
  } else {
    ledcWrite(0, 0);
    ledcWrite(1, 0); // Stop
  }
}
void Task2(void *pvParameters) {
  for (;;) {
    fungsi_motor();
    vTaskDelay(1);
 
    if(state_motor == 0) {
       amper = 0;
    }
    else {
      float result;
      int readValue;                // value read from the sensor
      int maxValue = 0;             // store max value here
      int minValue = 4096;          // store min value here ESP32 ADC resolution
      
       uint32_t start_time = millis();
       while((millis()-start_time) < 1000) //sample for 1 Sec
       {
           readValue = analogRead(sensorIn);
           // see if you have a new maxValue
           if (readValue > maxValue) 
           {
               /*record the maximum sensor value*/
               maxValue = readValue;
           }
           if (readValue < minValue) 
           {
               /*record the minimum sensor value*/
               minValue = readValue;
           }
           fungsi_motor();
       }
       
       // Subtract min from max
       result = ((maxValue - minValue) * 3.3)/4096.0; //ESP32 ADC resolution 4096
       Voltage = result;
        VRMS = (Voltage/2.0) *0.707;   //root 2 is 0.707
        AmpsRMS = ((VRMS * 1000)/mVperAmp)-0.7; //0.3 is the error I got for my sensor
        amper = constrain(AmpsRMS,0,30);
        vTaskDelay(1);
    }
    if(amper >= batas_cutoff){
      digitalWrite(relay,HIGH);
    }
    
  }
}
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
float getPhFromADC(int adcValue) {
  for (int i = 0; i < ph_cal_size - 1; i++) {
    float adc1 = ph_calibration[i][0];
    float ph1  = ph_calibration[i][1];
    float adc2 = ph_calibration[i + 1][0];
    float ph2  = ph_calibration[i + 1][1];

    if (adcValue >= adc1 && adcValue <= adc2) {
      float slope = (ph2 - ph1) / (adc2 - adc1);
      return slope * (adcValue - adc1) + ph1;
    }
  }
  if (adcValue < ph_calibration[0][0])
    return ph_calibration[0][1];
  else
    return ph_calibration[ph_cal_size - 1][1];
}

float getTdsFromADC(int adc) {
  for (int i = 0; i < tds_cal_size - 1; i++) {
    float adc1 = tds_calibration[i][0];
    float tds1 = tds_calibration[i][1];
    float adc2 = tds_calibration[i + 1][0];
    float tds2 = tds_calibration[i + 1][1];

    if (adc >= adc1 && adc <= adc2) {
      float slope = (tds2 - tds1) / (adc2 - adc1);
      return slope * (adc - adc1) + tds1;
    }
  }
  if (adc < tds_calibration[0][0])
    return tds_calibration[0][1];
  else
    return tds_calibration[tds_cal_size - 1][1];
}
float getNTUFromADC(int adcValue) {
  for (int i = 0; i < turbidity_points - 1; i++) {
    float adc1 = turbidity_calibration[i][0];
    float ntu1 = turbidity_calibration[i][1];
    float adc2 = turbidity_calibration[i + 1][0];
    float ntu2 = turbidity_calibration[i + 1][1];

    if ((adcValue <= adc1 && adcValue >= adc2) || (adcValue >= adc1 && adcValue <= adc2)) {
      float slope = (ntu2 - ntu1) / (adc2 - adc1);
      return ntu1 + slope * (adcValue - adc1);
    }
  }
  // Jika di luar range, kembalikan nilai default
  return 0;
}
void updateLevelLogika() {
  unsigned long sekarang = millis();

  if (sedangDiLevel1Sementara) {
    if (sekarang - waktuMulaiLevel1 >= durasiLevel1) {
      level = 10;
      force_level1 = false;
      sedangDiLevel1Sementara = false;
      waktuMasukLevel10 = sekarang;
      statusLevel = "Kembali ke Level 10";
      Serial.println("⏫ Kembali ke level 10");
    } else {
      unsigned long sisa = durasiLevel1 - (sekarang - waktuMulaiLevel1);
      statusLevel = "Level 1: " + formatCountdown(sisa);
//      tampilkanCountdown("Level 1 sementara", sisa);
    }
    return;
  }

  if (level == 10) {
    if (waktuMasukLevel10 == 0) {
      waktuMasukLevel10 = sekarang;
      statusLevel = "Mulai Level 10";
      Serial.println("⏳ Masuk level 10, mulai hitung 30 menit");
    } else {
      unsigned long durasi = sekarang - waktuMasukLevel10;
      if (durasi >= durasiLevel10) {
        level = 1;
        force_level1 = true;
        sedangDiLevel1Sementara = true;
        waktuMulaiLevel1 = sekarang;
        waktuMasukLevel10 = 0;
        statusLevel = "Terlalu Lama! Level 1 (10m)";
        Serial.println("⚠️ Level 10 terlalu lama! Pindah ke level 1 selama 10 menit");
      } else {
        unsigned long sisa = durasiLevel10 - durasi;
        statusLevel = "Level 10: " + formatCountdown(sisa);
       // tampilkanCountdown("Level 10 aktif", sisa);
      }
    }
  } else {
    if (waktuMasukLevel10 != 0) {
      waktuMasukLevel10 = 0;
      statusLevel = "- Timer Reset";
      Serial.println("🔄 Keluar dari level 10 sebelum waktunya, timer di-reset");
    } else {
      statusLevel = "";
    }
  }
}
String formatCountdown(unsigned long sisaMillis) {
  unsigned long totalDetik = sisaMillis / 1000;
  unsigned int jam = totalDetik / 3600;
  unsigned int menit = (totalDetik % 3600) / 60;
  unsigned int detik = totalDetik % 60;

  String output = "";
  if (jam > 0) output += String(jam) + "h ";
  if (jam > 0 || menit > 0) output += String(menit) + "m ";
  output += String(detik) + "s";

  return output;
}

void main_loop(){
  for (int i = 0; i < 10; i++) {
    phBuf[i] = analogRead(pHPin);
    turbBuf[i] = analogRead(turbidity_pin);
    vTaskDelay(10);
  }
  for (int i = 0; i < 25; i++) {
    turbBuf[i] = analogRead(turbidity_pin);
    vTaskDelay(10);
  }
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (phBuf[i] > phBuf[j]) {
        phTemp = phBuf[i];
        phBuf[i] = phBuf[j];
        phBuf[j] = phTemp;
      }
    }
  }
  phAvg = 0;
  for (int i = 2; i < 8; i++) phAvg += phBuf[i];
  int adcAverage = phAvg / 6;
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);
  suhu = temperature;
  // Ambil data ADC dan olah rata-rata
  for (int i = 0; i < 10; i++) {
    tdsBuf[i] = analogRead(TdsPin);
    vTaskDelay(5);
  }

  // Sorting data ADC
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (tdsBuf[i] > tdsBuf[j]) {
        temp = tdsBuf[i];
        tdsBuf[i] = tdsBuf[j];
        tdsBuf[j] = temp;
      }
    }
  }

  float tdsAvg = 0;
  for (int i = 2; i < 8; i++) tdsAvg += tdsBuf[i];
  int adcAverage2 = tdsAvg / 6;

  // Hitung nilai TDS tanpa kompensasi
  float tdsRaw = getTdsFromADC(adcAverage2);

  // Hitung rata rata analog turbidity
  for(int i = 0;i < 25;i++){
    turbRaw += turbBuf[i];
  }
  turbRaw = turbRaw/25;
  
  // Hitung nilai final dan masukan ke fuzzy
  
  tdsVal = tdsRaw / (1.0 + 0.02 * (temperature - 25.0));
  phVal = getPhFromADC(adcAverage);
  turbVal = getNTUFromADC(turbRaw);
  
  fuzzy->setInput(1, tdsVal);
  fuzzy->setInput(2, phVal);
  fuzzy->setInput(3, turbVal);
 
  fuzzy->fuzzify();
  if(modee == 1 && force_level1 == false) {
    level = round(fuzzy->defuzzify(1));
  }
  updateLevelLogika();

  if (level <= 3) {
    status_air = "Normal";
  } else if (level <= 7) {
    status_air = "Siaga";
  } else {
    status_air = "Bahaya";
  }
  if(millis() - lastSwitch >= pageInterval){
    lastSwitch = millis();
    currentPage++;
    if (currentPage > 2) currentPage = 0;
    lcd.clear();
  }
  jarakPintu = bacaJarak(TRIG_PINTU, ECHO_PINTU);
  jarakSebelum = map(bacaJarak(TRIG_AIR_SEBELUM, ECHO_AIR_SEBELUM),3,37.25,100,0);
  jarakSebelum = constrain(jarakSebelum,0,100);
  jarakSesudah = map(bacaJarak(TRIG_AIR_SESUDAH, ECHO_AIR_SESUDAH),3,28,100,0);
  jarakSesudah = constrain(jarakSesudah,0,100);
  server.handleClient();
  Serial.print("Pintu: ");
  Serial.print(jarakPintu);
  Serial.print(" % | Air Sebelum: ");
  Serial.print(jarakSebelum);
  Serial.print(" % | Air Sesudah: ");
  Serial.print(jarakSesudah);
  Serial.println(" cm");
  Serial.println("   INPUT    ");
  Serial.print("TDS: ");
  Serial.print(tdsVal);
  Serial.print(" ppm, ");
  Serial.print("pH: ");
  Serial.print(phVal, 2); 
  Serial.print(", ");
  Serial.print("Turbidity: ");
  Serial.print(turbVal);
  Serial.println(" NTU");
  Serial.print("Output Level: ");
  Serial.println(level);
  Serial.print("Arus: ");
  Serial.print(amper,3);
  Serial.println("A");
}
void loop() {

}
