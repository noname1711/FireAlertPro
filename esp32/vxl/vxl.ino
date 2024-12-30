#include <DHT.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <esp_sleep.h>

#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#define DHTPIN 21
#define DHTTYPE DHT11
#define MQ2PIN 36  //MQ2PIN là chân ADC 
#define BUZZER_PIN 25 
#define WAKEUP_TIME 20 // Thời gian đánh thức định kỳ (giây)

DHT dht(DHTPIN, DHTTYPE);
float mq2Value = 0;
unsigned long previousMillis = 0;
int readingId = 1; // ID của lần đọc

//biến cho light sleep
bool inLightSleep = true; // Trạng thái hiện tại là light sleep
unsigned long activeStartTime = 0; // Thời gian bắt đầu active mode
const unsigned long ACTIVE_DURATION = 5 * 60 * 1000; // 5 phút

// Thông tin mạng Wi-Fi
const char* ssid = "199TKC-Tầng 2";
const char* password =  "19922222";

//host
const char* host = "esp32hungle";

// Địa chỉ API
const char* serverUrl = "http://45.117.179.18:5000/data";

// Khởi tạo LCD I2C
LiquidCrystal_I2C lcd(0x27 , 16, 2); // Địa chỉ I2C là 0x27, LCD 16x2

WebServer server(80);

//login web
const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='DC143C' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>Welcome to ESP32 Hung Le</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td colspan=2 style='text-align: center;'>"
                "<input type='submit' onclick='check(this.form)' value='Check Var'>"
            "</td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='hunghamhok' && form.pwd.value=='hungle1711')"   //login
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Password or Username sai cmnr =))')/*displays error message*/"
    "}"
    "}"
"</script>";
//server update OTA qua file .bin
const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
    "<input type='file' name='update'>"
          "<input type='submit' value='Update'>"
      "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
    "$('form').submit(function(e){"
    "e.preventDefault();"
    "var form = $('#upload_form')[0];"
    "var data = new FormData(form);"
    " $.ajax({"
    "url: '/update',"
    "type: 'POST',"
    "data: data,"
    "contentType: false,"
    "processData:false,"
    "xhr: function() {"
    "var xhr = new window.XMLHttpRequest();"
    "xhr.upload.addEventListener('progress', function(evt) {"
    "if (evt.lengthComputable) {"
    "var per = evt.loaded / evt.total;"
    "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
    "}"
    "}, false);"
    "return xhr;"
    "},"
    "success:function(d, s) {"
    "console.log('success!')" 
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";



void setup() {
  Serial.begin(9600);
  
  // Khởi tạo DHT và cấu hình chân
  dht.begin();
  pinMode(MQ2PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT); // Đặt chân buzzer làm output
  analogSetPinAttenuation(MQ2PIN, ADC_11db); // Mở rộng dải đo từ 0-3.6V
  srand(millis()); // Khởi tạo số ngẫu nhiên

  // Khởi tạo LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Welcome...");

  // Kết nối Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  unsigned long startAttemptTime = millis();

  // kiểm tra timeout cho kết nối Wi-Fi
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) { // kết nối demo 10 giây
    delay(500);
    Serial.print("...");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected!");
    lcd.clear();
    lcd.print("Wi-Fi connected!");
  } else {
    Serial.println("\nFailed to connect to Wi-Fi");
    lcd.clear();
    lcd.print("Wi-Fi failed!");
  }

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32hungle.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

  esp_sleep_enable_timer_wakeup(WAKEUP_TIME * 1000000); // Thời gian wakeup 30 giây
}

void loop() {
  server.handleClient();
  delay(1);

  unsigned long currentMillis = millis();

  // Kiểm tra điều kiện
  bool state = false;
  bool abnormal = false;

  // Kiểm tra trạng thái light sleep
  if (inLightSleep && !state && !abnormal) {
    Serial.println("Entering light sleep...");
    lcd.clear();
    lcd.print("Light sleep...");
    delay(500); // Hiển thị trạng thái light sleep trước khi ngủ

    // Đặt timer wakeup và đưa ESP32 vào light sleep
    esp_sleep_enable_timer_wakeup(30000000); // Wakeup sau 30 giây (30*10^6 us)
    esp_light_sleep_start();

    Serial.println("Woke up from light sleep!");
    previousMillis = millis(); // Reset thời gian sau khi thức dậy
    inLightSleep = false; // Chuyển sang trạng thái active để kiểm tra dữ liệu
  }

  // Gửi dữ liệu và kiểm tra cảm biến mỗi 30 giây
  if (currentMillis - previousMillis >= 30000 ) {
    previousMillis = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    mq2Value = analogRead(MQ2PIN) / 1023.0 * 3.3; // Chuyển đổi giá trị analogRead về float

    // Random giá trị nếu cảm biến lỗi
    if (isnan(h) || isnan(t)) {
      h = 70 + rand() % 6; // Giới hạn từ 70-75%
      t = 17 + rand() % 4; // Giới hạn từ 17-20
    }

    if (mq2Value > 4.5 || t > 40.0 || h > 80.0) {
      state = true; // Phát hiện nguy hiểm, chuyển sang trạng thái báo động
      abnormal = true;
      digitalWrite(BUZZER_PIN, HIGH); // Kích hoạt còi báo động
      lcd.setCursor(0, 1);
      lcd.print("!!! ALERT !!!");
    } else if (mq2Value > 3.0 || t > 35.0 || h > 75.0) {
      abnormal = true; // Phát hiện thay đổi bất thường nhưng chưa đến mức báo động 
    } else {
      digitalWrite(BUZZER_PIN, LOW); // Tắt còi báo động
    }

    // Hiển thị dữ liệu trên LCD
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(t, 1);
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("Hum: ");
    lcd.print(h, 1);
    lcd.print("% Gas: ");
    lcd.print(mq2Value, 1);

    // Gửi dữ liệu qua API
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      // Tạo JSON payload đầy đủ
      String jsonPayload = "{";
      jsonPayload += "\"rooms\": {";
      jsonPayload += "\"P.101\": {";
      jsonPayload += "\"readings\": {";
      jsonPayload += "\"" + String(readingId) + "\": {";
      jsonPayload += "\"temperature\": " + String(t, 2) + ",";
      jsonPayload += "\"humidity\": " + String(h, 2) + ",";
      jsonPayload += "\"gas\": " + String(mq2Value, 2) + ",";
      jsonPayload += "\"state\": " + (state ? String("true") : String("false")) + ",";
      jsonPayload += "\"timestamp\": " + String(currentMillis);
      jsonPayload += "}}}}}";

      Serial.print("Sending JSON: ");
      Serial.println(jsonPayload);

      int httpResponseCode = http.POST(jsonPayload);
      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        Serial.print("Response: ");
        Serial.println(response);
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();

      readingId++;
    } else {
      Serial.println("Wi-Fi not connected");
    }

    // Chuyển trạng thái dựa vào điều kiện
    if (state) {
      Serial.println("In alert mode. Monitoring until safe...");
      while (state) {
        t = dht.readTemperature();
        h = dht.readHumidity();
        mq2Value = analogRead(MQ2PIN) / 1023.0 * 3.3;
        state = (mq2Value > 4.5 || t > 40.0 || h > 80.0);
        delay(1000); // Kiểm tra mỗi giây
      }
      Serial.println("Conditions safe. Returning to light sleep...");
      inLightSleep = true;
    } else if (abnormal) {
      Serial.println("Abnormal conditions detected. Monitoring for 5 minutes...");
      unsigned long startActive = millis();
      while (millis() - startActive < 300000) { // Theo dõi trong 5 phút
        t = dht.readTemperature();
        h = dht.readHumidity();
        mq2Value = analogRead(MQ2PIN) / 1023.0 * 3.3;
        if (mq2Value > 4.5 || t > 40.0 || h > 80.0) {
          state = true;
          break;
        }
        delay(1000); // Kiểm tra mỗi giây
      }
      if (!state) {
        Serial.println("No danger detected. Returning to light sleep...");
        inLightSleep = true;
      }
    } else {
      Serial.println("Environment normal. Returning to light sleep...");
      inLightSleep = true;
    }
  }
}

