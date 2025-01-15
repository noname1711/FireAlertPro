#include <DHT.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#define DHTPIN 32
#define DHTTYPE DHT22
#define MQ2PIN 33
#define BUZZER_PIN 4
#define LED_PIN 2
#define LED_BUILTIN_PIN 5
#define BUTTON_PIN 12
#define WAKEUP_TIME 20

DHT dht(DHTPIN, DHTTYPE);
float mq2Value = 0;
unsigned long previousMillis = 0;
int readingId = 1;

bool inLightSleep = true;
unsigned long activeStartTime = 0;
const unsigned long ACTIVE_DURATION = 5 * 60 * 1000;

const char* ssid = "TP-LINK_17FA";
const char* password = "99990000";

const char* host = "esp32hungle";
const char* serverUrl = "http://45.117.179.18:5000/data";

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

LiquidCrystal_I2C lcd(0x27, 16, 2);

WebServer server(80);

bool otaInProgress = false;

const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='DC143C' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>Welcome ESP32 Hung Le</b></font></center>"
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
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='hunghamhoc' && form.pwd.value=='hungle1711')"   //pass 
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')"
    "}"
    "}"
"</script>";

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

volatile bool alarmState = false;
volatile bool buttonPressed = false;
unsigned long lastButtonPress = 0;

void IRAM_ATTR handleButtonPress() {
  if (millis() - lastButtonPress > 10000) {  // 2 lần bấm cách 10s
    buttonPressed = true;
    lastButtonPress = millis();
    portENTER_CRITICAL_ISR(&mux);
    portEXIT_CRITICAL_ISR(&mux);
  }
}

void setup() {
  Serial.begin(115200);
  
  dht.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, RISING);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_BUILTIN_PIN, OUTPUT);

  analogSetPinAttenuation(MQ2PIN, ADC_11db);
  srand(millis());

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Welcome...");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
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

  if (!MDNS.begin(host)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  
  server.on("/", HTTP_GET, []() {
    otaInProgress = true;
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    otaInProgress = true;
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    otaInProgress = true;
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      otaInProgress = false;
      if (Update.end(true)) {
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

  esp_sleep_enable_timer_wakeup(WAKEUP_TIME * 1000000);
}

float calculatePPM(float analogValue, float R0, float ratioCleanAir) {
  float voltage = analogValue * (5.0 / 1023.0);
  float RS = (5.0 - voltage) / voltage;
  float ratio = RS / R0;
  return ratio / ratioCleanAir;
}

void triggerAlarm(bool ButtonState) {
  if (ButtonState) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(LED_BUILTIN_PIN, HIGH);
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("!!! ALERT !!!");
    Serial.println("Triggering alarm...");
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(LED_BUILTIN_PIN, LOW);
    Serial.println("Turning off alarm...");
  }
}

void loop() {
  server.handleClient();
  delay(1);

  unsigned long currentMillis = millis();

  bool state = false;
  bool abnormal = false;

  if (inLightSleep && !state && !abnormal && !otaInProgress) {
    delay(5000);
    Serial.println("Entering light sleep...");
    lcd.clear();
    lcd.print("Light sleep...");
    delay(500);

    esp_sleep_enable_timer_wakeup(WAKEUP_TIME * 1000000);
    esp_light_sleep_start();

    Serial.println("Woke up from light sleep!");
    previousMillis = millis();
    inLightSleep = false;
  }

  if (buttonPressed) {
    buttonPressed = false;
    alarmState = !alarmState;
    triggerAlarm(alarmState);
    Serial.println("Button pressed, alarm state changed");
  }

  if (currentMillis - previousMillis >= 30000) {
    previousMillis = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    mq2Value = analogRead(MQ2PIN) / 1023.0 * 3.3;

    float R0 = 10.0;
    float ratioCleanAir = 9.83;
    float lpg_ppm = calculatePPM(mq2Value, R0, ratioCleanAir);

    if (lpg_ppm > 1000 || t > 40.0 || h < 20.0) {
      state = true;
      abnormal = true; 
      digitalWrite(BUZZER_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(LED_BUILTIN_PIN, HIGH);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("!!! ALERT !!!");
    } else if (lpg_ppm > 500 || t > 35.0 || (h >= 20.0 && h < 29.0)) {
      abnormal = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("T: ");
      lcd.print(t, 2);
      lcd.setCursor(0, 1);
      lcd.print("H: ");
      lcd.print(h, 2);
      lcd.print(" G: ");
      lcd.print(lpg_ppm, 2);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      digitalWrite(LED_BUILTIN_PIN, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("T: ");
      lcd.print(t, 2);
      lcd.setCursor(0, 1);
      lcd.print("H: ");
      lcd.print(h, 2);
      lcd.print(" G: ");
      lcd.print(lpg_ppm, 2);
    }

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      String jsonPayload = "{";
      jsonPayload += "\"rooms\": {";
      jsonPayload += "\"P.901\": {";
      jsonPayload += "\"readings\": {";
      jsonPayload += "\"" + String(readingId) + "\": {";
      jsonPayload += "\"temperature\": " + String(t, 2) + ",";
      jsonPayload += "\"humidity\": " + String(h, 2) + ",";
      jsonPayload += "\"gas\": " + String(lpg_ppm, 2) + ",";
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

    if (state) {
      Serial.println("In alert mode. Monitoring until safe...");
      unsigned long alertStartMillis = millis();
      while (state) {
        t = dht.readTemperature();
        h = dht.readHumidity();
        mq2Value = analogRead(MQ2PIN) / 1023.0 * 3.3;
        float lpg_ppm = calculatePPM(mq2Value, R0, ratioCleanAir);
        
        state = (lpg_ppm > 1000 || t > 40.0 || h < 20.0);
        if (state) {
          digitalWrite(BUZZER_PIN, HIGH);
          digitalWrite(LED_PIN, HIGH);
          digitalWrite(LED_BUILTIN_PIN, HIGH);
        } else {
          if (millis() - alertStartMillis >= 30000) {
            digitalWrite(BUZZER_PIN, LOW);
            digitalWrite(LED_PIN, LOW);
            digitalWrite(LED_BUILTIN_PIN, LOW);
            Serial.println("Conditions safe. Returning to light sleep...");
            inLightSleep = true;
          } else {
            state = true;
          }
        }
        delay(1000);
      }
        
    } else if (abnormal) {
      Serial.println("Abnormal conditions detected. Monitoring for 5 minutes...");
      unsigned long startActive = millis();
      while (millis() - startActive < 300000) {
        t = dht.readTemperature();
        h = dht.readHumidity();
        mq2Value = analogRead(MQ2PIN) / 1023.0 * 3.3;
        float lpg_ppm = calculatePPM(mq2Value, R0, ratioCleanAir);
        
        if (lpg_ppm > 1000 || t > 40.0 || h < 20.0) {
          state = true;
          break;
        }
        delay(1000);
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