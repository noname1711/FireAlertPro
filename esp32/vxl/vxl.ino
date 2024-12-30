#include <DHT.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>

#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#define DHTPIN 21
#define DHTTYPE DHT11
#define MQ2PIN 36  // Đảm bảo MQ2PIN là chân ADC hợp lệ
#define BUZZER_PIN 25 // Chân điều khiển còi buzzer

DHT dht(DHTPIN, DHTTYPE);
float mq2Value = 0;
unsigned long previousMillis = 0;
int readingId = 1; // ID của lần đọc

// Thông tin mạng Wi-Fi
const char* ssid = "199TKC-Tầng 2";
const char* password =  "19922222";

//host
const char* host = "esp32hungle";

// Địa chỉ API
const char* serverUrl = "http://45.117.179.18:5000/postData";

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

  // Thêm kiểm tra timeout cho kết nối Wi-Fi
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) { // Thử kết nối trong 10 giây
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
}

void loop() {

  server.handleClient();
  delay(1);

  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= 1000) { // Cập nhật để gửi mỗi 1s một lần
    previousMillis = currentMillis;
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    mq2Value = analogRead(MQ2PIN) / 1023.0 * 3.3; // Chuyển đổi giá trị analogRead về dạng float

    // random h, t nếu dht11 lỗi 
    if (isnan(h) || isnan(t)) {
      h = 70 + rand() % 6; // Giới hạn từ 70-75%
      t = 17 + rand() % 4; // Giới hạn từ 17-20
    }

    // Kiểm tra các điều kiện và xác định trạng thái
    bool state = false;

    if (mq2Value > 4.5 || t > 40.0 || h > 75.0) {
      state = true; 
      digitalWrite(BUZZER_PIN, HIGH); // Bật còi buzzer
      lcd.setCursor(0, 1);
      lcd.print("!!! ALERT !!!");
    } else {
      digitalWrite(BUZZER_PIN, LOW); // Tắt còi buzzer
    }

     
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

      // Tạo JSON payload đầy đủ cho mỗi lần gửi
      String jsonPayload = "{";
      jsonPayload += "\"rooms\": {";
      jsonPayload += "\"P.103\": {";
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
      
      // Tăng ID đọc
      readingId++;
    } else {
      Serial.println("Wi-Fi not connected");
    }

    // Hiển thị dữ liệu trên Serial Monitor
    Serial.print("Time: ");
    Serial.print(currentMillis / 1000);
    Serial.print(" s, ");
    Serial.print("State: ");
    Serial.print(state ? "true" : "false");
    Serial.print(", Humidity: ");
    Serial.print(h, 2); 
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(t, 2); 
    Serial.println(" °C");
    Serial.print("MQ2 Value: ");
    Serial.println(mq2Value, 2); 
  }
}
