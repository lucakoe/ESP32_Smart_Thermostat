
//////////////////////////////////////////////////////////
//OLED
//////////////////////////////////////////////////////////
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

// 0X3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS 0x3C

// Define proper RST_PIN if required.
#define RST_PIN -1

SSD1306AsciiWire oled;
#define OLED_LINES_SCREEN 8
String displayContent[OLED_LINES_SCREEN];
const int REFRESH_INTERVALL = 1000; // in millisecounds
///////////////////////////////////////////////////////////
//WiFi
///////////////////////////////////////////////////////////
#include <WiFiClient.h>
#include <WiFi.h>

const int MAX_TRYS_TO_CONNECT = 4;

const char* defaultHotspotSSID = "Fancy Thermostat";
const char* defaultHotspotPassword = "987654321";

char* defaultWifiSSID = "SSID";
char* defaultWifiPassword = "password";

char* currentWifiSSID = defaultWifiSSID;
char* currentWifiPassword = defaultWifiPassword;

const int MAX_SSID_PASSWORD_LENTGH=30;

const String SSID_INPUT = "customSSID";
const String PASSWORD_INPUT = "customPassword";


//https://github.com/marian-craciunescu/ESP32Ping
#include <ESP32Ping.h>
#include <ping.h>
const char* WEBSITE_TO_PING = "www.google.com";
const int MAX_PING=999;



////////////////////////////////////////////////////////////
//Website
////////////////////////////////////////////////////////////
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
const char landingPage_html[]
PROGMEM = R"rawliteral(
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h1>Type in your Wifi-Credentials</h1>
<form action="/get">
  <label for="customSSID"></label>
  customSSID: <input id="customSSID" name="customSSID" maxlength="100" placeholder ="SSID" required>
  <label for="customPassword"></label>
  customPassword: <input id="customPassword" name="customPassword" maxlength="100" placeholder ="Password">
  <button type="submit" name="action" value="submitwifi">Submit</button>
</form>
</body>

)rawliteral";

const char homePage_html[]
PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>Termostat</h2>
  <p>
    <span class="dht-labels">Temperature</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <span class="dht-labels">Humidity</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">%</sup>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 10000 ) ;
</script>
</html>

)rawliteral";


///////////////////////////////////////////////////////////
//Thermometer
///////////////////////////////////////////////////////////
#include <OneWire.h>
#include <DallasTemperature.h>

// GPIO where the DS18B20 is connected to
const int oneWireBus = 16;     

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);



////////////////////////////////////////////////////////////
void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    setupOLEDDisplay();
    refreshOLED();
    setupWifiConnection(defaultWifiSSID,defaultWifiPassword);
    if(WiFi.status() != WL_CONNECTED){
      setupHotspotLandingPage();
      while(!getLandingPageData());
    }
    
    setupThermo();
    setupHomePage();


}

void loop() {
    String tempOut = "Temperature: "+ String(getTemp())+"°C";
    String pingOut = "Ping: "+ String(getPing());
    displayContent[0]=tempOut;
    displayContent[2]= WEBSITE_TO_PING;
    displayContent[3]=pingOut;
    Serial.println(WiFi.status());
    Serial.println(tempOut);
    Serial.println(WEBSITE_TO_PING);
    Serial.println(pingOut);
    refreshOLED();
    delay(REFRESH_INTERVALL);
}


/////////////////////////////////////////////////////////
void setupOLEDDisplay() {
    Wire.begin();
    Wire.setClock(400000L);

#if RST_PIN >= 0
    oled.begin(&Adafruit128x64, I2C_ADDRESS, RST_PIN);
#else // RST_PIN >= 0
    oled.begin(&Adafruit128x64, I2C_ADDRESS);
#endif // RST_PIN >= 0

    oled.setFont(System5x7);
    oled.clear();
}

// TODO Fix Flickering Bug
void refreshOLED() {
    oled.clear();
    for (int i = 0; i < OLED_LINES_SCREEN; i++) {
        oled.println(displayContent[i]);
    }

}

//Returns true if successfull false if not
bool setLineOLED(int line, String newContent) {
    if (line < OLED_LINES_SCREEN && line >= 0) {
        displayContent[line] = newContent;
        refreshOLED();
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////


void setupWifiConnection(char* wifiSSID,char* wifiPassword) {
    WiFi.begin(wifiSSID, wifiPassword);
    int counting = 0;
    while (WiFi.status() != WL_CONNECTED && (counting != MAX_TRYS_TO_CONNECT)) {
        delay(1000);
        Serial.println("Connecting to WiFi..");
        counting++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Couldn't connect to Wifi");
        WiFi.disconnect(true);
    }

}

void setupWiFiHotSpot() {
    Serial.println("Setting up Hotspot");
    WiFi.softAP(defaultHotspotSSID, defaultHotspotPassword);
    setLineOLED(0, "SSID: " + String(defaultHotspotSSID));
    Serial.println("SSID: " + String(defaultHotspotSSID));
    setLineOLED(1, "Password: " + String(defaultHotspotPassword));
    Serial.println("Password: " + String(defaultHotspotPassword));
    setLineOLED(3, "IP: " + WiFi.softAPIP().toString());
    Serial.println("IP: " + WiFi.softAPIP().toString());
}




////////////////////////////////////////////////////////
void setupHotspotLandingPage() {
    server.on("/hello", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", landingPage_html);
    });

    server.begin();
}

bool getLandingPageData() {
    server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request){
        char* inputSSID="";
        char* inputPassword="";
        // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
        if (request->hasParam(SSID_INPUT) && request->hasParam(PASSWORD_INPUT)) {
            char charBuf[MAX_SSID_PASSWORD_LENTGH];
            request->getParam(SSID_INPUT)->value().toCharArray(charBuf, MAX_SSID_PASSWORD_LENTGH);
            inputSSID = charBuf;
            request->getParam(PASSWORD_INPUT)->value().toCharArray(charBuf, MAX_SSID_PASSWORD_LENTGH);
            inputPassword = charBuf;
            setupWifiConnection(inputSSID, inputPassword);
            if (WiFi.status() == WL_CONNECTED) {
                currentWifiSSID = inputSSID;
                currentWifiPassword = inputPassword;
                return true;
            }

        }

    });
    return false;
}


void setupHomePage() {
    server.on("/hello", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", homePage_html);
    });

    server.begin();
}

// Replaces placeholder with DHT values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return String(getTemp());
  }
  /*
  else if(var == "HUMIDITY"){
    return readDHTHumidity();
  }
  */
  return String();
}



////////////////////////////////////////////////////////

void setupThermo(){
  
  sensors.begin();
}

float getTemp(){
  sensors.requestTemperatures(); 
  return sensors.getTempCByIndex(0);
  }

//////////////////////////////////////////////////////

String getPing(){
  float startTime = millis();
  float pingTime=0;

/*
  if(!Ping.ping(WEBSITE_TO_PING)){
    return "No connection";
  }
  return String(Ping.averageTime());

*/
  
  
  while(!Ping.ping(WEBSITE_TO_PING)&&pingTime<MAX_PING){
    pingTime=startTime-millis();
  }
  if(pingTime<MAX_PING){
    return "No connection";
  }
  return String(pingTime);
  
}
