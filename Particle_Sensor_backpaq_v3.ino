//// BackpAQ    V0.1                                /////
//// (c) 2019 Sustainable Silicon Valley
//// Written by A. L. Clark 
/*
MIT License

Copyright (c) 2019 BackpAQ Labs LLC and Sustainable Silicon Valley

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE
*/
#include "config.h" // config file
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <DoubleResetDetector.h>  //https://github.com/datacute/DoubleResetDetector
#include <BlynkSimpleEsp8266.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>  
#include <Adafruit_BME280.h>
#define LENG 31  //0x42 + 31 bytes equal to 32 bytes  
#define TIMEOUT 10000 //Microseconds to wait for PMS do wakeup 
#define BME280_ADDRESS (0x76) // 76 or 77 ?
#define LENG 31   //0x42 + 31 bytes equal to 32 bytes
#define MSG_LENGTH 31   //0x42 + 31 bytes equal 
#define DEBUG_PRINTLN(x)  Serial.println(x)
#define DEBUG_PRINT(x)  Serial.print(x)
#define BATT_LEVEL A0
const int PINPMSGO = D0; // PMS "sleep" pin to save power
/////////////////////////////////////////////////////////////////////////////////
unsigned char buf[LENG];  
char floatString[15];
char buffer[20];
char resultstr[50]; 
int status; 
float p;
float t; // temperature F
float h; // humidity
char tempF[20];
char humid[20]; 
float latI = 0;
float lonI = 0;
int savedPos = 0;
float latISav [500];
float lonISav [500];
char buffr[16];
int airQualityIndex = 0;
String newColor;
String newLabel;
int gaugeValue;
double batteryV = 0.0;
float rssi = 0;
unsigned long previousMillis;
int privateMode = 0;

// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

// Onboard LED I/O pin on NodeMCU board
const int PIN_LED = 2; // D4 on NodeMCU and WeMos. Controls the onboard LED.

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;

//// Create instances ////
Adafruit_SSD1306 OLED(-1); // Init OLED display
Adafruit_BME280 bme; // initialize BME temp/humidity sensor   
WidgetMap myMap(V16); // set up GPS Map widget on Blynk 
WidgetLED led24(V24); // indicate Wifi connected
BlynkTimer timer;
//////////////////////////
// Blynk Events 
//////////////////////////
 // This function is called whenever the assigned Button changes state
BLYNK_WRITE(V29) {  // V29 is the Virtual Pin assigned to the Button Widget
    if (param.asInt()) {  // User desires private data mode, no update to Thingspeak
       Serial.print("Privacy Mode on, ");
       privateMode = 1;
    } else {  
       Serial.print("Privacy Mode off, ");
       privateMode = 0;
    }
  }
 // This function is called whenever "erase markers" button is pressed
BLYNK_WRITE(V40) {
  if (param.asInt()) {
    myMap.clear(); // erase any marker(s)
    Serial.print("Map GPS markers erased!");
  }
}
// This function is called whenever GPS position on smartphone is updated
BLYNK_WRITE(V15) {
  GpsParam gps(param); // fetch GPS position from smartphone

  latI = gps.getLat();
  lonI = gps.getLon(); 

 }
 /////////////////////////////////
 //  Structure for raw sensor data
 ///////////////////////////////// 
 struct dustvalues {  
  unsigned int PM10Val=0; // Byte 4&5  
  unsigned int PM2_5Val=0; // Byte 6&7  
  unsigned int PM01Val=0; // Byte 8&9  
  unsigned int Beyond03=0; // Byte 16&17  
  unsigned int Beyond05=0; // Byte 18&19  
  unsigned int Beyond1=0; // Byte 20&21  
  unsigned int Beyond2_5=0; // Byte 22&23  
  unsigned int Beyond5=0; //Byte 24&25  
  unsigned int Beyond10=0; //Byte 26&27   
 }; 
 struct dustvalues dustvalues1, dustvalues2; 
    
 unsigned char gosleep[]={ 0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73 };  
 unsigned char gowakeup[]={ 0x42, 0x4d, 0xe4, 0x00, 0x01, 0x01, 0x74 };  
 
  // Set value, color of AQI gauge on Blynk gauge ////////
 void calcAQIValue() {
 
    gaugeValue = airQualityIndex;
    
  // assign color according to US AQI standard (modified per https://airnow.gov/index.cfm?action=aqibasics.aqi)
    if (gaugeValue > 300) {
        newColor = AQI_MAROON;
        newLabel = "AQI: HAZARDOUS";
     } else if (gaugeValue > 200) {
         newColor = AQI_PURPLE;
         newLabel = "AQI: VERY UNHEALTHY";
     } else if (gaugeValue > 150) {
         newColor = AQI_RED;
         newLabel = "AQI: UNHEALTHY";
     } else if (gaugeValue > 100) {
         newColor = AQI_ORANGE;
         newLabel = "AQI: UNHEALTHY FOR SOME";
     } else if (gaugeValue > 50) {
         newColor = AQI_YELLOW;
         newLabel = "AQI: MODERATE";
     }
       else {
         newColor = AQI_GREEN;  //"Safe"
         newLabel = "AQI: GOOD";
     }        
  }

  //******************* Following are routines to calculate AQI from PM2.5 concentrations *******************************************
// US AQI formula: https://en.wikipedia.org/wiki/Air_Quality_Index#United_States
  int toAQI(int I_high, int I_low, int C_high, int C_low, int C) {
  return (I_high - I_low) * (C - C_low) / (C_high - C_low) + I_low;
}

// Source: https://gist.github.com/nfjinjing/8d63012c18feea3ed04e
// Based on https://en.wikipedia.org/wiki/Air_Quality_Index#United_States
// Confirmed on http://aqicn.org/faq/2013-09-09/revised-pm25-aqi-breakpoints/fr/  (updated EPA standard, published on December 14th 2012)
// Updated by https://aqs.epa.gov/aqsweb/documents/codetables/aqi_breakpoints.html

int calculate_US_AQI25(float density) {

  int d10 = (int)(density * 10); // to counter documented sensitivity issue
  if (d10 <= 0) {
    return 0;
  } else if (d10 <= 120) { // Good
    return toAQI(50, 0, 120, 0, d10);
  } else if (d10 <= 354) { // Moderate
    return toAQI(100, 51, 354, 121, d10);
  } else if (d10 <= 554) { // Unhealthy for Sensitive Groups
    return toAQI(150, 101, 554, 355, d10);
  } else if (d10 <= 1504) { // Unhealthy
    return toAQI(200, 151, 1504, 555, d10);
  } else if (d10 <= 2504) { // Very Unhealthy
    return toAQI(300, 201, 2504, 1505, d10);
  } else if (d10 <= 3504) { // Hazardous
    return toAQI(400, 301, 3504, 2505, d10);
  } else if (d10 <= 5004) { // Hazardous
    return toAQI(500, 401, 5004, 3505, d10);
  } else if (d10 <= 10000) { // Hazardous
    return toAQI(1000, 501, 10000, 5005, d10);
  } else { // Are you still alive ?
    return 1001;
  }
}
void updateOLED() {
// update display and Blynk

  //DEBUG_PRINTLN("updateOLED");
  OLED.setTextColor(WHITE,BLACK);   
  OLED.clearDisplay(); // 
  OLED.setTextSize(1); // 
  OLED.setCursor(0, 0); 
  OLED.println("   SSV BackpAQ V0.1");
  OLED.setTextSize(1); // 
  
  OLED.print("   PM1.0 = ");
  OLED.print(dustvalues1.PM01Val); // write PM1.0 data
  OLED.println(" ug/m3");
  
  OLED.print("   PM2.5 = ");
  OLED.print(dustvalues1.PM2_5Val); // write PM2.5 data
  OLED.println(" ug/m3");
  
  //OLED.print("   PM10  = ");
  //OLED.print(dustvalues1.PM10Val); // write PM10 data
  //OLED.println(" ug/m3");
 
  OLED.print("   PM2.5 AQI  = ");
  OLED.println(airQualityIndex); // write AQI
  
  OLED.display(); //  display data
  
}
/****************** Send To Thinkspeak  Channel(s)  ***************/
void SendToThingspeak(void) // Send to Thingspeak using Webhook
{ 
 if (!privateMode) { // check for data privacy..."0" = send to Thingspeak, "1" = NOT send
  Serial.println("Writing to Thingspeak.");
  /* Thingspeak Field:        1                   2                   3                  4  5    6     7     8                 */
  Blynk.virtualWrite(V7, dustvalues1.PM01Val, dustvalues1.PM2_5Val, dustvalues1.PM10Val, t, h, latI, lonI, airQualityIndex);  
  delay(1000);
  Blynk.virtualWrite(V47, dustvalues1.Beyond03, dustvalues1.Beyond05, dustvalues1.Beyond1, dustvalues1.Beyond2_5,dustvalues1.Beyond5, dustvalues1.Beyond10, batteryV, rssi);
  delay(1000); 
  Blynk.virtualWrite(V16, 1, latI, lonI, airQualityIndex); // update GPS position from iphone sensor on map 
 }
 // save lat, lon GPS position
  latISav[savedPos] = latI;
  lonISav[savedPos] = lonI;
 // savedPos++; 
  // write breadcrumbs... 
// for (int i=0; i<savedPos; i++) {
   // place saved markers on map
  Blynk.virtualWrite(V16, savedPos+1, latI, lonI, airQualityIndex); // update GPS position from iphone sensor on map     
 // }  
}

 char checkValue(unsigned char *buf, char leng){   
  int receiveSum=0;  
 
  for(int i=0; i<(leng-2); i++){  
   receiveSum=receiveSum+buf[i];  
  }  
  receiveSum=receiveSum + 0x42;  
  Serial.print("Checksum calculated: ");  
  Serial.println(receiveSum);  
  Serial.print("Checksum found: ");  
  Serial.println((buf[leng-2]<<8)+buf[leng-1]);  
    
  if(receiveSum == ((buf[leng-2]<<8)+buf[leng-1])) return 1;  
    
  return 0;  
 }

 unsigned int transmitPM(char HighB, char LowB, unsigned char *buf){  
  unsigned int PMValue;  
  PMValue=((buf[HighB]<<8)+buf[LowB]);  
  return PMValue;  
 } 
////////////////////

void getpms5003(void){  // retrieve data from sensor
unsigned char found=0;  
   
  while (found<2){  
  if(Serial.available()>0){ 
    if(Serial.find(0x42)){  //start to read when detect 0x42 (start character)  
     Serial.println("0x42 found");  
     Serial.readBytes(buf,LENG);  
     if (buf[0] == 0x4d){ //second byte should be 0x4D  
       found++;  
       if(checkValue(buf,LENG)){  
        Serial.println("Checksum okay");  
          
        if(found==1){  
         dustvalues1.PM01Val=transmitPM(3,4,buf);  
         dustvalues1.PM2_5Val=transmitPM(5,6,buf);  
         dustvalues1.PM10Val=transmitPM(7,8,buf);  
         dustvalues1.Beyond03=transmitPM(9,10,buf);
         dustvalues1.Beyond05=transmitPM(11,12,buf);
         dustvalues1.Beyond1=transmitPM(13,14,buf);
         dustvalues1.Beyond2_5=transmitPM(15,16,buf);
         dustvalues1.Beyond5=transmitPM(17,18,buf);
         dustvalues1.Beyond10=transmitPM(19,20,buf);
         sprintf(resultstr,"PM1: %4dug/m3\nPM2.5:%4dug/m3\nPM10: %4dug/m3\n",dustvalues1.PM01Val,dustvalues1.PM2_5Val,dustvalues1.PM10Val);  
         Serial.println(resultstr);  
         Serial.println(dustvalues1.PM01Val);
         Blynk.virtualWrite(V0, dustvalues1.PM01Val);
         Serial.println(dustvalues1.PM2_5Val);
         Blynk.virtualWrite(V1, dustvalues1.PM2_5Val);
         Serial.println(dustvalues1.PM10Val);
         Blynk.virtualWrite(V2, dustvalues1.PM10Val);       
         Serial.println(dustvalues1.Beyond03);
         Blynk.virtualWrite(V34, dustvalues1.Beyond03);
         Serial.println(dustvalues1.Beyond05);
         Blynk.virtualWrite(V35, dustvalues1.Beyond05);
         Serial.println(dustvalues1.Beyond1);
         Blynk.virtualWrite(V36, dustvalues1.Beyond1);
         Serial.println(dustvalues1.Beyond2_5);
         Blynk.virtualWrite(V37, dustvalues1.Beyond2_5);
         Serial.println(dustvalues1.Beyond5);
         Blynk.virtualWrite(V38, dustvalues1.Beyond5);
         Serial.println(dustvalues1.Beyond10);
         Blynk.virtualWrite(V39, dustvalues1.Beyond10);
         Serial.flush();
         //delay(10000);  
         delay(5000); 
        }  
        if(found==2){  
         dustvalues2.PM01Val=transmitPM(3,4,buf);  
         dustvalues2.PM2_5Val=transmitPM(5,6,buf);  
         dustvalues2.PM10Val=transmitPM(7,8,buf);  
         dustvalues1.Beyond03=transmitPM(9,10,buf);
         dustvalues1.Beyond05=transmitPM(11,12,buf);
         dustvalues1.Beyond1=transmitPM(13,14,buf);
         dustvalues1.Beyond2_5=transmitPM(15,16,buf);
         dustvalues1.Beyond5=transmitPM(17,18,buf);
         dustvalues1.Beyond10=transmitPM(19,20,buf);
         sprintf(resultstr,"PM1: %4dug/m3\nPM2.5:%4dug/m3\nPM10: %4dug/m3\n",dustvalues2.PM01Val,dustvalues2.PM2_5Val,dustvalues2.PM10Val);  
         Serial.println(resultstr);  
         Serial.println(dustvalues1.PM01Val);
         Blynk.virtualWrite(V0, dustvalues1.PM01Val);
         Serial.println(dustvalues1.PM2_5Val);
         Blynk.virtualWrite(V1, dustvalues1.PM2_5Val);
         Serial.println(dustvalues1.PM10Val);
         Blynk.virtualWrite(V2, dustvalues1.PM10Val);       
         Serial.println(dustvalues1.Beyond03);
         Blynk.virtualWrite(V34, dustvalues1.Beyond03);
         Serial.println(dustvalues1.Beyond05);
         Blynk.virtualWrite(V35, dustvalues1.Beyond05);
         Serial.println(dustvalues1.Beyond1);
         Blynk.virtualWrite(V36, dustvalues1.Beyond1);
         Serial.println(dustvalues1.Beyond2_5);
         Blynk.virtualWrite(V37, dustvalues1.Beyond2_5);
         Serial.println(dustvalues1.Beyond5);
         Blynk.virtualWrite(V38, dustvalues1.Beyond5);
         Serial.println(dustvalues1.Beyond10);
         Blynk.virtualWrite(V39, dustvalues1.Beyond10);
         Serial.flush(); 
         delay(5000);
        }  
  // Both modes come here     
   // calculate AQI and set colors for Blynk
     airQualityIndex = calculate_US_AQI25(dustvalues1.PM2_5Val);
     calcAQIValue(); // set AQI colors
   // Update colors/labels for AQI gauge on Blynk
     Blynk.setProperty(V6, "color", newColor); // update AQI color  
     Blynk.setProperty(V6, "label", newLabel);  // update AQI label
     Blynk.virtualWrite(V6, gaugeValue);  // update AQI value
   // update sensor battery level
     batteryV = analogRead(BATT_LEVEL) / 209.66; // assumes 3.7V and external 180K resistor btwn A0 and Batt +. For 5V use 220K.
     Blynk.virtualWrite(V21, batteryV);  
   }else{  
       Serial.println("Checksum not okay");  
       Serial.flush();  
       if(found>0)found--;  
       delay(500);  
      }  
     }  
    }  
   }  
  }  
 }  

void getTemps() {
    
     t = bme.readTemperature();
    Serial.print("Temperature = ");
    Serial.print(t); 
    Serial.println(" *C");
    
    // English Units - degrees F
    buffer[0] = '\0';
    dtostrf(t * 1.8 + 32.0, 5, 1, floatString);
    strcat(buffer, floatString);
    strcat(buffer, "F");
    // tempF = buffer;
    Blynk.virtualWrite(V3, buffer ); 

    h = bme.readHumidity();
    Serial.print("Humidity = ");
    Serial.print(h); 
    Serial.println(" %");
    // write to Blynk 
    buffer[0] = '\0';
    dtostrf(h , 6, 1, floatString);
    strcat(buffer, floatString);
    //humid = buffer;
    strcat(buffer, "%");
    Blynk.virtualWrite(V4, buffer ); 
}

void powerOnSensor() {
  //Switch on PMS sensor
 // digitalWrite(PINPMSGO, HIGH);
  //Warm-up
  unsigned long timeout = millis();
  timeout = MIN_WARM_TIME - (millis() - timeout);
  if (timeout > 0) {
    DEBUG_PRINT("sensor warm-up: ");
    DEBUG_PRINTLN(timeout);
    delay(timeout);
  }
}

void powerOffSensor() {
//#ifdef USE_WIFI
 // WiFi.disconnect();
//#endif
  //Switch off PMS sensor
  digitalWrite(PINPMSGO, LOW);

  DEBUG_PRINTLN("going to sleep zzz...");
   delay(SLEEP_TIME);
}

// callback routine for WiFiManager configuration
void configModeCallback (WiFiManager *wifiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP()); 
  Serial.println(wifiManager->getConfigPortalSSID());
  OLED.println("WiFi SSID 'BackpAQ'->"); 
  OLED.display(); 
}
void setup()
{
  Serial.begin(9600);
 // initialize the LED digital pin as an output.
  pinMode(PIN_LED, OUTPUT);
  OLED.begin(SSD1306_SWITCHCAPVCC,0x3C);  // init OLED 
 // display name, version, copyright  
  OLED.clearDisplay();                               // Clear OLED display
  OLED.setTextSize(1);                               // Set OLED text size to small
  OLED.setTextColor(WHITE,BLACK);                    // Set OLED color to White
  OLED.setCursor(0,0);                               // Set cursor to 0,0
  OLED.println("SSV BackpAQ V0.1");  
  OLED.println("(c) 2019 SSV");
  OLED.display();                                     // Update display
  delay(2000); 
  
 Serial.setTimeout(1500); 
 privateMode = 0; 
 led24.off();
 
// WiFiManager is used here to configure Wifi
// Next section handles WiFi logon credentials...
 if (WiFi.SSID()==""){
    Serial.println("We haven't got any access point credentials, so get them now");   
    initialConfig = true;
  }
  // check for double press of Reset Button
  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    initialConfig = true;
  }
  
 if (initialConfig) {
    Serial.println("Starting configuration portal.");
    digitalWrite(PIN_LED, LOW); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.
  //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager; // using Wifi Manager with customer portal capture to scan/prompt for ssid, password
 //set custom ip for portal
    wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    wifiManager.setAPCallback(configModeCallback); // alert local display need to connect to config SSID
 //Start an access point (SSID "BackpAQ") and go into a blocking loop awaiting configuration
   wifiManager.startConfigPortal("BackpAQ");   
 } // initial config
 
  digitalWrite(PIN_LED, HIGH); // Turn led off as we are not in configuration mode.
  WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
  unsigned long startedAt = millis();
  Serial.print("After waiting ");
  int connRes = WiFi.waitForConnectResult();
  float waited = (millis()- startedAt);
  Serial.print(waited/1000);
  Serial.print(" secs in setup() connection result is ");
  Serial.println(connRes);
  if (WiFi.status()!=WL_CONNECTED){
    Serial.println("failed to connect, finishing setup anyway");
    } else{
      Serial.print("local ip: ");
      Serial.println(WiFi.localIP());
   // print the received signal strength:
      rssi = WiFi.RSSI();
      OLED.print("Connected to "); 
      OLED.println(WiFi.SSID());  // display local SSID
      OLED.display();
      }
  
// proceed to connect to Blynk cloud
   Blynk.config(auth); // wifi params arready set by WifiManager
   bool result = Blynk.connect();
  if (result != true)
  {
    DEBUG_PRINTLN("BLYNK Connection Failed");
   // wifiManager.resetSettings();
    ESP.reset();
    delay (5000);
  }
 else // Blynk Connected!
  {
  // we're connected to Blynk!!
   DEBUG_PRINTLN("BLYNK Connected");
  // OLED.println("BLYNK Connected!");  
  } 
// get temp, pressure, humidity
  status = bme.begin(BME280_ADDRESS);  
   if (!status) {
       Serial.println("Could not find a valid BME280 sensor, check wiring!");    
   }
   
// Delay(200); // so sensor can initialize

  Serial.println("Starting up...");
          
  /* get PMS sensor ready...*/
  //pinMode(PINPMSGO, OUTPUT); // Define PMS 5003 pin 
  OLED.println("Sensor warming up ...");
  OLED.display();
  //delay(3000);   
  
  previousMillis = millis();

  DEBUG_PRINTLN("Switching On PMS");
  powerOnSensor(); // wake up!
  DEBUG_PRINTLN("Initialization finished");
  OLED.clearDisplay();
 
Blynk.virtualWrite(V0, dustvalues1.PM01Val);

// set timed functions for Blynk
timer.setInterval(1000L, getpms5003); // get data from PM sensor
timer.setInterval(10000L, getTemps); // get temp,  humidity
timer.setInterval(1050L, updateOLED); // update local display
timer.setInterval(20000L, SendToThingspeak); // upload sensor values to Thingspeak every 20 seconds
} // setup

void loop()
{
  drd.loop(); // monitor double reset clicks
  Blynk.run();
  timer.run(); 
} 
