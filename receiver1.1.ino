/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-esp-now-wi-fi-web-server/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.  
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/

#include "esp_wifi.h"
#include <esp_now.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Arduino_JSON.h>
#include <ESP32Servo.h>
#include "pitch.h"
#include <ESP32Time.h>

#ifdef ESP32
  #include <AsyncTCP.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>


bool istimeron = false;
bool lockstatus = false;
bool dis = true;

// Initialize the ESP32Time object with an offset (in seconds)
ESP32Time rtc(0); // Offset in seconds, set to 0 for UTC

int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};


const int output = 2;


const int servoPin = 23; // Servo data pulse pin
const int servoPin1 = 22;

Servo myservo;  // create servo object to control a servo
Servo myservo1;

int pos = 0;  // variable to store the servo angle position

int rssi_display;

// Replace with your network credentials (STATION)
const char* ssid = "petportal";
const char* password = "12345678";


// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
  int id;
  float temp;
  float hum;
  unsigned int readingId;
} struct_message;


// Estructuras para calcular los paquetes, el RSSI, etc
typedef struct {
  unsigned frame_ctrl: 16;
  unsigned duration_id: 16;
  uint8_t addr1[6]; /* receiver address */
  uint8_t addr2[6]; /* sender address */
  uint8_t addr3[6]; /* filtering address */
  unsigned sequence_ctrl: 16;
  uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

struct_message incomingReadings;

JSONVar board;

AsyncWebServer server(80);
AsyncEventSource events("/events");

void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  // All espnow traffic uses action frames which are a subtype of the mgmnt frames so filter out everything else.
  if (type != WIFI_PKT_MGMT)
  return;

  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
  const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

  int rssi = ppkt->rx_ctrl.rssi;
  rssi_display = rssi;
}

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  // Copies the sender mac address to a string
  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
      mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
 
  board["id"] = incomingReadings.id;
  board["temperature"] = incomingReadings.temp;
  board["humidity"] = incomingReadings.hum;
  board["readingId"] = String(incomingReadings.readingId);
  String jsonString = JSON.stringify(board);
  events.send(jsonString.c_str(), "new_readings", millis());


  Serial.printf("Board ID %u: %u bytes\n", incomingReadings.id, len);
  Serial.printf("t value: %4.2f \n", incomingReadings.temp);
  Serial.printf("h value: %4.2f \n", incomingReadings.hum);
  Serial.printf("readingID value: %d \n", incomingReadings.readingId);
  Serial.printf("WIFI RSSI: %i ",rssi_display);
  Serial.println();

  if (dis == true){
    if (rssi_display >= -45)
    {
      myservo.write(0);
      myservo1.write(180);
      delay(6000);
    }
    else if (rssi_display < -45 && rssi_display >= -65)
    {
    }
    else
    {
      myservo.write(90);
      myservo1.write(90);
    }
  }
  else if (istimeron == true)
  {
    myservo.write(90);
    myservo1.write(90);
  }
  else if (lockstatus == true)
  {
    myservo.write(90);
    myservo1.write(90);
  }
  else if (istimeron == false)
  {
    myservo.write(0);
    myservo1.write(180);
  }
  else if (lockstatus == false)
  {
    myservo.write(0);
    myservo1.write(180);
  }
  else
  {
    if (rssi_display >= -40)
    {
      myservo.write(0);
      myservo1.write(180);
    }
    else
    {
      myservo.write(90);
      myservo1.write(90);
    }
  }




  if (rtc.getHour(true) == 9 && rtc.getMinute() == 0) {
  for (int thisNote = 0; thisNote < 8; thisNote++) {
      int noteDuration = 1000 / noteDurations[thisNote];
      tone(26, melody[thisNote], noteDuration);  // Assuming speaker is connected to pin 26
      int pauseBetweenNotes = noteDuration * 1.30;
      delay(pauseBetweenNotes);
      noTone(8);
      }
  }
  Serial.printf("Current time: %02d:%02d:%02d\n", rtc.getHour(true), rtc.getMinute(), rtc.getSecond());
  }

const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Pet Portal with Timer</title>
    <style>
        body {
            background-color: #202c39;
            color: #ffffff;
            text-align: center;
            font-size: 35px;
            padding-top: 40px;
        }
        .row {
            margin-top: 5%;
            display: flex;
            justify-content: space-around;
        }
        button {
            color: #283845;
            flex-basis: 31%;
            padding: 30px 50px;
            background: #F2D492;
            border-radius: 10px;
            margin-bottom: 5%;
            font-size: 20px;
            box-sizing: border-box;
            transition: 0.3s;
            cursor: pointer;
        }
        button.red {
            background-color: red;
        }
        .services {
            width: 80%;
        }
        .timer-container {
            text-align: center;
            background: rgb(18, 18, 18);
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
            margin-top: 20px;
        }
        .time-input {
            margin-bottom: 20px;
        }
        .time-input label {
            margin-right: 10px;
        }
        #countdown {
            font-size: 48px;
            font-weight: bold;
            margin-top: 20px;
        }
        #timerSection {
            display: none;
        }
    </style>
</head>
<body class="services">
    <div>
        <h1>Pet Portal</h1>
    </div>
    <div class="row">
        <button onclick="showSection('collarSection')">COLLAR MENU</button>
        <button onclick="showSection('doorSection')">DOOR MENU</button>
    </div>

    <div id="collarSection" style="display:none;">
        <h2>Collar Menu</h2>
        <div class="row">
            <h3>Temperature Reading From Collar: <br /> <br /></h3><span class="reading"><span id="t1"></span> &deg;F</span><br /> <br /></p>
        </div>
    </div>

    <div id="doorSection" style="display:none;">
        <h2>Door Menu</h2>
        <div class="row">
            <button onclick="showSection('timerSection')">TIMER</button>
            <button id="colorButton" onclick="toggleColor()">DOOR UNLOCKED</button>
        </div>
    </div>

    <div id="timerSection" class="timer-container">
        <h1>Set Timer</h1>
        <div class="time-input">
            <label for="minutes">Minutes:</label>
            <input type="number" id="minutes" min="0" value="0">
            <label for="seconds">Seconds:</label>
            <input type="number" id="seconds" min="0" max="59" value="0">
        </div>
        <button id="start-button">Start Timer</button>
        <div class="countdown-display">
            <span id="countdown">00:00</span>
        </div>
    </div>

    <script>

    let lock = 0;

  if (!!window.EventSource) {
    var source = new EventSource('/events');
 
    source.addEventListener('open', function(e) {
      console.log("Events Connected");
  }, false);
    source.addEventListener('error', function(e) {
      if (e.target.readyState != EventSource.OPEN) {
        console.log("Events Disconnected");
      }
  }, false);
 
    source.addEventListener('message', function(e) {
      console.log("message", e.data);
  }, false);
 
  source.addEventListener('new_readings', function(e) {
    console.log("new_readings", e.data);
    var obj = JSON.parse(e.data);
    document.getElementById("t"+obj.id).innerHTML = obj.temperature.toFixed(2);
    document.getElementById("h"+obj.id).innerHTML = obj.humidity.toFixed(2);
    document.getElementById("rt"+obj.id).innerHTML = obj.readingId;
    document.getElementById("rh"+obj.id).innerHTML = obj.readingId;
  }, false);
  }

  function toggleCheckbox(x){
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/" + x, true);
    xhr.send();
  }

  function timeron(x){
    var timer = new XMLHttpRequest();
    timer.open("GET", "/" + x, true);
    timer.send();
  }

  timeron('timeroff');

        function showSection(sectionId) {
            document.getElementById('collarSection').style.display = 'none';
            document.getElementById('doorSection').style.display = 'none';
            document.getElementById('timerSection').style.display = 'none';
            document.getElementById(sectionId).style.display = 'block';
        }

        function toggleColor() {
            var button = document.getElementById('colorButton');
            if (button.classList.contains('red')) {
                button.classList.remove('red');
                button.textContent = "DOOR UNLOCKED";
                toggleCheckbox('on');
            } else {
                button.classList.add('red');
                button.textContent = "DOOR LOCKED";
                toggleCheckbox('off');
            }
        }

        document.getElementById('start-button').addEventListener('click', function() {
            const minutes = parseInt(document.getElementById('minutes').value);
            const seconds = parseInt(document.getElementById('seconds').value);
            let totalTime = (minutes * 60) + seconds;

            const countdownDisplay = document.getElementById('countdown');
            countdownDisplay.textContent = formatTime(totalTime);

            const interval = setInterval(function() {
                if (totalTime <= 0) {
                    clearInterval(interval);
                    countdownDisplay.textContent = 'Time\'s up!';
                    timeron('timeroff');
                    return;
                }
                totalTime--;
                countdownDisplay.textContent = formatTime(totalTime);
                timeron('timeron');
            }, 1000);
        });

        function formatTime(seconds) {
            const mins = Math.floor(seconds / 60);
            const secs = seconds % 60;
            return `${String(mins).padStart(2, '0')}:${String(secs).padStart(2, '0')}`;
        }
    </script>
</body>
</html>

)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}



void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Set the initial time to the current moment (year, month, day, hour, minute, second)
  rtc.setTime(0, 35, 12, 4, 8, 2024); // Replace with the current date and time

  myservo.attach(servoPin);  // assigns servo pin to the servo object
  myservo1.attach(servoPin1);

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);
 
  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
  Serial.println("Error initializing ESP-NOW");
  return;
  }
 
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send_P(200, "text/html", index_html);
  });
   
  events.onConnect([](AsyncEventSourceClient *client){
  if(client->lastId()){
  Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
  }
  // send event with message "hello!", id current millis
  // and set reconnect delay to 1 second
  client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();


  // Send web page to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Receive an HTTP GET request
  server.on("/on", HTTP_GET, [] (AsyncWebServerRequest *request) {
    lockstatus = false;
    dis = false;
    request->send(200, "text/plain", "ok");
  });

  // Receive an HTTP GET request
  server.on("/off", HTTP_GET, [] (AsyncWebServerRequest *request) {
    lockstatus = true;
    dis = true;
    request->send(200, "text/plain", "ok");
  });

  // this is to check if timer is on or off
  server.on("/timeron", HTTP_GET, [] (AsyncWebServerRequest *request) {
    istimeron = true;
    dis = false;
    request->send(200, "text/plain", "ok");
  });

  // Receive an HTTP GET request
  server.on("/timeroff", HTTP_GET, [] (AsyncWebServerRequest *request) {
    istimeron = false;
    dis = true;
    myservo.write(0);
    myservo1.write(180);
    request->send(200, "text/plain", "ok");
  });
  
  server.onNotFound(notFound);
  server.begin();



}
 
void loop() {  
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 5000;
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
  events.send("ping",NULL,millis());
  lastEventTime = millis();
  }
}
