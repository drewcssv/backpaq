//// BackpAQ    V0.93 (for Heltec ESP32)                                /////
//// (c) 2020 BackpAQ Labs LLC and Sustainable Silicon Valley
//// BackpAQ Personal Air Quality Monitor
//// Written by A. L. Clark
/*
  MIT License

  Copyright (c) 2020 BackpAQ Labs LLC and Sustainable Silicon Valley

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
#include <FS.h>  //this lib needs to be first, or it all crashes and burns...https://github.com/esp8266/Arduino
#include "SPIFFS.h" // include for ESP32
#include "config_90.h" // config file  make config changes here
//-------------------------------------------------------------------------------------------------------------
// NOTE: This version is for the Heltec ESP32
//-------------------------------------------------------------------------------------------------------------
#ifdef ESP32
  #include <esp_wifi.h>
  #include <WiFi.h>
  #include <WiFiClient.h>

  #define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

  #define LED_ON      HIGH
  #define LED_OFF     LOW
#else
  #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
  
 //needed for library
  #include <DNSServer.h>
  #include <ESP8266WebServer.h>

  #define ESP_getChipId()   (ESP.getChipId())

  #define LED_ON      LOW
  #define LED_OFF     HIGH
#endif

#include <ESP_WiFiManager.h>      // https://github.com/khoih-prog/ESP_WiFiManager   special ESP fork of Tapzu's WiFiManager

#ifdef ESP32
  #include <BlynkSimpleEsp32.h>
#else
  #include <BlynkSimpleEsp8266.h>
#endif

// includes for Arduino and Adafruit libraries
#include <EEPROM.h>
#include <Adafruit_GFX.h>        // https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h>    //https://github.com/adafruit/Adafruit_SSD1306
#include "Adafruit_SGP30.h"       //https://github.com/adafruit/Adafruit_SGP30
#include <Arduino.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <Adafruit_BME280.h>      // https://github.com/adafruit/Adafruit_BME280_Library
#include <widgetRTC.h>
#include <TimeLib.h>              //https://github.com/PaulStoffregen/Time
#include "ThingSpeak.h"           // Thingspeak Arduino library
#include "Adafruit_PM25AQI.h"     // https://github.com/adafruit/Adafruit_PM25AQI

// Global variables
//const int PINPMSGO = D0; // PMS "sleep" pin to save power (on WeMos)
#define LENG 31  //0x42 + 31 bytes equal to 32 bytes  
#define LENG 31   //0x42 + 31 bytes equal to 32 bytes
#define MSG_LENGTH 31   //0x42 + 31 bytes equal 
#define DEBUG_PRINTLN(x)  Serial.println(x)
#define DEBUG_PRINT(x)  Serial.print(x)
// SSID and PW for your Router
String Router_SSID;
String Router_Pass;
unsigned char buf[LENG];
char floatString[15]; // for temp/humidity string conversion
char buffer[20];
char pmLabel[20];
String pmLabel1;
bool thingspeakWebhook = true;
bool backpaq_mobile = false; // default mode is station
char default_GPS[20];
char default_gps_position[25];
char resultstr[50]; // for PM Sensor
int status;
float p; // pressure (not used here)
float t; // temperature F
float h; // humidity
char tempF[20];
char humid[20];
float eCO2;
float TVOC; 
uint16_t TVOC_base;
uint16_t eCO2_base;
bool noSGP = false;
int tv_counter = 0;
float latI = 0;
float lonI = 0;
float lastKnownLat = 0;
float lastKnownLon = 0;
float default_lat = 0;
float default_lon = 0;
int markerNum = 0; // index for GPS position, incremented for each location
int track_id = 0;
float latISav [500]; // array to save tracks
float lonISav [500];
String trackName = "no name";
String trackStartTime = "";
String trackEndTime = "";
bool amTracking = false;
String track_comment = "";
String track_comment_array[500];
char buffr[16];
int airQualityIndex = 0;
String newColor;
String iconURL="";
String batteryColor;
String newLabel;
int gaugeValue;
float batteryV = 0.0;
float rssi = 0;
String currentTime;
String currentDate;
String timeHack;
//unsigned long previousMillis;
int privateMode = 0;
bool not_Connected = false;
bool display_PM25 = false;
bool display_PM10 = false;
bool display_AQI = false;
bool display_TD = false;
String conversion = "NONE";
int convFactor = 1;
//flag for saving data
bool shouldSaveConfig = false;

// Onboard LED I/O pin on NodeMCU board
const int PIN_LED = 2; // D4 on NodeMCU and WeMos. Controls the onboard LED.

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;
bool noBlynk;

#define ADC_PIN 13
#define CONV_FACTOR 4.0
//#define READS 20
// These defines must be put before #include <ESP_DoubleResetDetector.h>
// to select where to store DoubleResetDetector's variable.
// For ESP32, You must select one to be true (EEPROM or SPIFFS)
// For ESP8266, You must select one to be true (RTC, EEPROM or SPIFFS)
// Otherwise, library will use default EEPROM storage
#define ESP_DRD_USE_EEPROM      false
#define ESP_DRD_USE_SPIFFS      true    //false

#ifdef ESP8266
  #define ESP8266_DRD_USE_RTC     false   //true
#endif

#define DOUBLERESETDETECTOR_DEBUG       true  //false

#include <ESP_DoubleResetDetector.h>      //https://github.com/khoih-prog/ESP_DoubleResetDetector

// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 7

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

//// Create constructors ////
DoubleResetDetector* drd;

// Start I2C  with both ESP3Kit ports   
TwoWire BMEWire =  TwoWire(1);

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins) and uses pin 16 on ESP32 for reset
#define OLED_RESET     16 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 OLED(128, 64, &Wire, OLED_RESET);

Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

// Temperature and Humidity Sensor
Adafruit_BME280 bme; // initialize BME temp/humidity sensor

// TVOC and CO2 Sensor
Adafruit_SGP30 sgp; // Sensirion SGP30 CO2 and TVOC sensor, Adafruit library

// Blynk widgets
WidgetMap myMap(V16); // set up GPS Map widget on Blynk
WidgetLED led2(V91); // indicate tracking started
WidgetTerminal myTerm(V70); //Terminal widget for entering comments on Map 
BlynkTimer timer; // Blynk timer
WidgetRTC rtc; // initialize realtime clock
WiFiClient  client; // initialize WiFi client

////--------------------------------------------------------------------------------------------------------------------
// Blynk Events ----
// The following are object functions that are invoked from the Blynk server after request from the ESP hardware
////--------------------------------------------------------------------------------------------------------------------
// This function is called whenever the assigned Button changes state
BLYNK_WRITE(V29) {  // V29 is the Virtual Pin assigned to the Privacy Button Widget
  if (param.asInt()) {  // User desires private data mode, no update to Thingspeak
    Serial.print("Privacy Mode on, ");
    privateMode = true;
    noBlynk = true; // set if not using Blynk/smartphone app
  } else {
    Serial.print("Privacy Mode off, ");
    privateMode = false;
  }
}
// This function is called whenever the assigned Button changes state
BLYNK_WRITE(V45) {  // V45 is the Virtual Pin assigned to the Thingspeak Send Mode Button Widget
  if (param.asInt()) {  // thingspeakWebhook is *true* for sending to Thingspeak via Smartphone, *false* for sending via this script
    Serial.println("ThingSpeak WebHook is on, ");
    thingspeakWebhook = true;
  } else {
    Serial.println("Thingspeak WebHook =  off, ");
    thingspeakWebhook = false;
  }
}
// This function is called whenever "erase markers" button on Map is pressed
BLYNK_WRITE(V40) {
  if (param.asInt()) {
    myMap.clear(); // erase any marker(s)
    Serial.print("Map GPS markers erased!");
  }
}
// This function is called whenever Tracking Start/End is started/stopped
BLYNK_WRITE(V90) {
  if (param.asInt()) {
    Serial.println("Tracking Started at "+timeHack);
    blinkLedWidget();  // blink LED 
     trackName = trackName + ">"; // append start
    if (amTracking) { // if already tracking...   
       amTracking = false;
    }
  
  } else {
    Serial.println("Tracking Stopped at "+timeHack);
    led2.off();  // set LED to OFF
     trackName = "<" + trackName; // append stop
    if (!amTracking) { // if not tracking...   
        amTracking = true;
    }  
  }
}
// This function is called whenever Track Name is entered
BLYNK_WRITE(V84) {
  trackName = param.asStr(); 
  Serial.println("Track name is "+trackName);     
 }
 
// This function is called whenever "PM Conversion" menu is accessed
BLYNK_WRITE(V60) {
  switch (param.asInt())
  {
    case 1: // Item 1
      Serial.println("None selected");
      convFactor = 1;
      conversion = "No Conv";
      break;
    case 2: // Item 2
      Serial.println("US EPA selected");
      convFactor = 2;
      conversion = "USEPA";
      break;
    case 3: // Item 3
      Serial.println("LRAPA selected");
      convFactor = 3;
      conversion = "LRAPA";
      break;
    case 4: // Item 4
      Serial.println("AQandU selected");
      convFactor = 4;
      conversion = "AQandU";
      break;
    default:
      Serial.println("Unknown item selected");
  }
}

BLYNK_CONNECTED() {
  rtc.begin(); // sync time on Blynk connect
  setSyncInterval(1*60);
}

// This function is called whenever GPS position on smartphone is updated
BLYNK_WRITE(V15) {
  GpsParam gps(param); // fetch GPS position from smartphone

  latI = gps.getLat();
  lonI = gps.getLon();

  if (latI == 0) { // in case Blynk not active or GPS not in range...
    latI = lastKnownLat; // assign last known pos
    lonI = lastKnownLon; 
  } else {
    lastKnownLat = latI; // store pos as last known pos
    lastKnownLon = lonI;
  }
}
// Add comments to track data file on Thingspeak
BLYNK_WRITE(V70) {
  track_comment = "";
  track_id++;
  myTerm.clear(); // clear the pipes
  track_comment = param.asStr(); // fetch new comments from MAP page
  if (track_comment !="") {
     String comment_str = timeHack + "," + String(latI) + "," + String(lonI) + "," + track_comment; // assemble comment string into CSV
     myTerm.println(comment_str); // echo back to terminal 
   // track_comment_array[track_id] = comment_str; // whole comment string
     track_comment_array[track_id] = track_comment; // just the actual comment
    myTerm.flush();
  // myTerm.clear();
  }
}
// function to blink LED on smartphone
void blinkLedWidget()
{
  if (led2.getValue()) {
    led2.off();
    Serial.println("LED on V1: off");
  } else if (amTracking) {
    led2.on();
    Serial.println("LED on V1: on");
  } else {
      led2.off();
    }
}

// get setup for TVOC + CO2 sensor
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}
////////////////////////////////////////////////////////////
//  Structure for raw sensor data from PMS-7003 spec sheet (https://download.kamami.com/p564008-p564008-PMS7003%20series%20data%20manua_English_V2.5.pdf)
///  Data from the Sensor comes in two flavors: Standard Particles or CF-1, bytes 4-9 and Atmospheric Environment, bytes 10-15
////////////////////////////////// /////////////////////////
struct dustvalues {
  uint16_t PM01Val_cf1 = 0; // Byte 4&5 (CF1 Bytes 4-9)
  uint16_t PM2_5Val_cf1 = 0; // Byte 6&7
  uint16_t PM10Val_cf1 = 0; // Byte 8&9
  uint16_t PM01Val_atm = 0; // Byte 10&11  (ATM bytes 10-15)
  uint16_t PM2_5Val_atm = 0; // Byte 12&13
  uint16_t PM10Val_atm = 0; // Byte 14&15
  uint16_t Beyond03 = 0; // Byte 16&17  # Particles
  uint16_t Beyond05 = 0; // Byte 18&19
  uint16_t Beyond1 = 0; // Byte 20&21
  uint16_t Beyond2_5 = 0; // Byte 22&23
  uint16_t Beyond5 = 0; //Byte 24&25
  uint16_t Beyond10 = 0; //Byte 26&27
};
struct dustvalues dustvalues1;

// send commands to PMS - not currently used
unsigned char gosleep[] = { 0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73 };
unsigned char gowakeup[] = { 0x42, 0x4d, 0xe4, 0x00, 0x01, 0x01, 0x74 };

void heartBeatPrint(void)
{
  static int num = 1;

  if (WiFi.status() == WL_CONNECTED)
    Serial.print("H");        // H means connected to WiFi
  else
    Serial.print("F");        // F means not connected to WiFi

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(" ");
  }
}
// Heatbeat function 
void check_status()
{
  static ulong checkstatus_timeout = 0;

#define HEARTBEAT_INTERVAL    10000L
  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((millis() > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = millis() + HEARTBEAT_INTERVAL;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//******************* Following are routines to calculate AQI from PM2.5 concentrations *******************************************

// US AQI formula: https://en.wikipedia.org/wiki/Air_Quality_Index#United_States
int toAQI(int I_high, int I_low, int C_high, int C_low, int C) {
  return (I_high - I_low) * (C - C_low) / (C_high - C_low) + I_low;
}
//// Note: It's important to keep the following EPA AQI breakpoints up to date. See the following references
// Source: https://gist.github.com/nfjinjing/8d63012c18feea3ed04e
// Based on https://en.wikipedia.org/wiki/Air_Quality_Index#United_States
// Updated 12/19 per  https://aqs.epa.gov/aqsweb/documents/codetables/aqi_breakpoints.html

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
// Set value, color of AQI gauge on Blynk gauge ////////
void calcAQIValue() {
  gaugeValue = airQualityIndex;

  // assign color according to US AQI standard (modified per https://airnow.gov/index.cfm?action=aqibasics.aqi)

  if (gaugeValue > 300) {
    newColor = AQI_MAROON;
    newLabel = "AQI: HAZARDOUS";
    iconURL = Purple_Icon_URL;
  } else if (gaugeValue > 200) {
    newColor = AQI_PURPLE;
    newLabel = "AQI: VERY UNHEALTHY";
    iconURL = Purple_Icon_URL;
  } else if (gaugeValue > 150) {
    newColor = AQI_RED;
    newLabel = "AQI: UNHEALTHY";
    iconURL = Red_Icon_URL;
  } else if (gaugeValue > 100) {
    newColor = AQI_ORANGE;
    newLabel = "AQI: UNHEALTHY FOR SOME";
    iconURL = Orange_Icon_URL;
  } else if (gaugeValue > 50) {
    newColor = AQI_YELLOW;
    newLabel = "AQI: MODERATE";
    iconURL = Yellow_Icon_URL;
  } else {
    newColor = AQI_GREEN;  //"Safe"
    newLabel = "AQI: GOOD";
    iconURL = Green_Icon_URL;
  }
}
// Function to convert CF1 to ATM
// Note that the PMS data from the Sensor comes in two flavors: (Standard Particles or CF-1, bytes 4-9)
// and  (Atmospheric Environment, bytes 10-15)
int cf1_to_sat(int cf1)
{
  int sat = -1;

  if (cf1 < 30) sat = cf1;
  if (cf1 > 100) sat = cf1 * 2 / 3;
  if (cf1 >= 30 && cf1 <= 100) sat = 30 + cf1 * (cf1 - 30) / 70 * 2 / 3;
  return sat;
}
////////////////////////////////////  Not currently used  /////////////////////////////////////////////
// Function to calculate NowCast from PM hourly average. Needed as proper input to AQI.
// See https://airnow.zendesk.com/hc/en-us/articles/212303417-How-is-the-NowCast-algorithm-used-to-report-current-air-quality-
// See also: https://www3.epa.gov/airnow/aqicalctest/nowcast.htm
// Method: Enter up to twelve hours of PM2.5 concentrations in ug/m3; the NowCast is calculated below.
// Usage: c = NowPM25(hourly);

int NowPM25(int *hourly)
{
  int hour12Avg = 0;
  int hour4Avg = 0;
  int ratioRecentHour = 0;
  float adjustedhourly[12];
  int i;
  int items = 12;
  int items3 = 0;
  int missing4 = 0;
  int missing12 = 0;
  float adjusted4hour[4];
  float averagearray[12];
  int items4 = 0;
  float max = 0.0;
  float min = 99999.9;
  int range = 0;
  int rateofchange = 0;
  int weightfactor = 0;
  int sumofweightingfactor = 0;
  int sumofdatatimesweightingfactor = 0;
  int nowCast = -999;

  // Find the number of missing values
  //
  missing12 = 0;
  missing4 = 0;
  items3 = 3;
  for (i = 0; i < items3; i++)
  {
    if (hourly[i] = -1)
    {
      missing4++;
    }
  }
  if (missing4 < 2)
  {
    for (i = 0; i < items; i++)
    {
      if (hourly[i] != -1)
      {
        if (hourly[i] < min)
        {
          min = hourly[i];
        }
        if (hourly[i] > max)
        {
          max = hourly[i];
        }
      }
    }
    range = max - min;
    rateofchange = range / max;
    weightfactor = 1 - rateofchange;
    if (weightfactor < .5 )
    {
      weightfactor = .5;
    }
    /// step 4 here.
    for (i = 0; i < items; i++)
    {
      if (hourly[i] != -1)
      {
        sumofdatatimesweightingfactor += hourly[i] * (pow(weightfactor, i));
        sumofweightingfactor += pow(weightfactor, i);
      }
    }
    nowCast = sumofdatatimesweightingfactor / sumofweightingfactor;
    nowCast = floor(10 * nowCast) / 10;
  }
  return (nowCast);
}

// update local OLED display
void updateOLED() {
  
  OLED.setTextColor(WHITE, BLACK);
  OLED.clearDisplay(); //
  OLED.setTextSize(1); //
  OLED.setCursor(0, 0); // reset cursor to origin
  OLED.print("   SSV BackpAQ ");
  OLED.println(version); // version from config.h
  OLED.setTextSize(1); // smallest size
  
  OLED.println("  ");
  OLED.print("   PM1.0 = ");
  OLED.print(dustvalues1.PM01Val_atm); // write PM1.0 data
  OLED.println(" ug/m3");

  OLED.print("   PM2.5 = ");
  OLED.print(dustvalues1.PM2_5Val_atm); // write PM2.5 data
  OLED.println(" ug/m3");
  
  OLED.print("   PM10  = ");
  OLED.print(dustvalues1.PM10Val_atm); // write PM10 data
  OLED.println(" ug/m3");
  
  OLED.print("   PM2.5 AQI  = ");
  OLED.println(airQualityIndex); // write AQI

  OLED.print("   TVOC = ");
  if (noSGP) {
    OLED.print("No Data");
  }
  else {
    OLED.print(TVOC);
    OLED.println(" ppm");
    OLED.print("   CO2 = ");
    OLED.print(eCO2);
    OLED.print(" ppm");
  }
  OLED.display(); //  display data on OLED

}

/****************** Send To Thinkspeak  Channel(s)  ***************/
void SendToThingspeakWF(void) // Send to Thingspeak using native web client
{

  Serial.println("Writing to Thingspeak, native.");

  ThingSpeak.begin(client);  // Initialize ThingSpeak
  
  String def_latlon = default_gps_position;

  if (default_gps_position == "") {
   // use GPS position from config since we don't have smartphone GPS avail

    String def_latlon = default_gps_position; 
   // Parse Lat-lon string
    int comma = def_latlon.indexOf(",", 0);
    String default_lat_str = def_latlon.substring(0,comma-1); // extract lat
    String default_lon_str = def_latlon.substring(comma+1,18); // extract lon
   // convert to float
    default_lat  = default_lat_str.toFloat();
    default_lon = default_lon_str.toFloat();
    if (backpaq_mobile == 0) { // if fixed position
      latI = default_lat; // use default lan, lon from config
      lonI = default_lon;
   }
   Serial.print("Lat: ");  Serial.print(latI, 7);
   Serial.print(", Lon: ");  Serial.println(lonI, 7);
   
  } else {   // have Lat/Lon from smartphone GPS
   //Serial.print("Lat: ");  Serial.print(latI, 7);
   //Serial.print(", Lon: ");  Serial.println(lonI, 7);
  }

  if ((latI == 0.00000)  || (lonI == 0.00000)) {
    Serial.println("No GPS position found...exiting before sending to ThingSpeak.");
    Blynk.notify("No GPS position found from device {DEVICE_NAME} ");
    return;  // exit if no GPS position, don't send to ThingSpeak
  }
  
  // -------------------------------------------------------
  // CHANNEL 1: Update the first channel
  // set the fields with the values
  //--------------------------------------------------------
  ThingSpeak.setField(1, dustvalues1.PM01Val_atm);
  ThingSpeak.setField(2, dustvalues1.PM2_5Val_atm);
  ThingSpeak.setField(3, dustvalues1.PM10Val_atm);
  ThingSpeak.setField(4, t);
  ThingSpeak.setField(5, h);
  ThingSpeak.setField(6, latI); // use lat entered in config
  ThingSpeak.setField(7, lonI); // use lon entered in config
  ThingSpeak.setField(8, airQualityIndex);

  // set the status
  //ThingSpeak.setStatus(myStatus);
  
  // write to the ThingSpeak channel
  thingspeak_A_key_c = thingspeak_A_key; // cast to const char *
  int x = ThingSpeak.writeFields(thingspeak_A_channel_i, thingspeak_A_key_c);
  if (x == 200) {
    Serial.println("Thingspeak channel 1 update successful.");
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }

  delay(5000); // take a breath...
  // ----------------------------------------------------------
  // CHANNEL 2: Update the second channel
  // set the fields with the values
  // ----------------------------------------------------------
  ThingSpeak.setField(1, timeHack);
  ThingSpeak.setField(2, dustvalues1.Beyond05);
  ThingSpeak.setField(3, dustvalues1.Beyond1);
  ThingSpeak.setField(4, dustvalues1.Beyond2_5);
  ThingSpeak.setField(5, dustvalues1.Beyond5);
  ThingSpeak.setField(6, dustvalues1.Beyond10);
  ThingSpeak.setField(7, TVOC);
  ThingSpeak.setField(8, eCO2);

  // set the status
  //ThingSpeak.setStatus(myStatus);

  // write to the ThingSpeak channel
   thingspeak_B_key_c = thingspeak_B_key; // cast to const char *
  int xx = ThingSpeak.writeFields(thingspeak_B_channel_i, thingspeak_B_key_c);
  if (xx == 200) {
    Serial.println("Thingspeak channel 2 update successful.");
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(xx));
  }

  delay(2000);
 //--------------------------------------------------------------------
 // CHANNEL 3: Write track, PM & comment data to Thingspeak 
 // Set the fields with the values so we can build a geoJSON file
 // -------------------------------------------------------------------
  String names = String(userID) + "," + trackName;
  Serial.println("names = " + names);
  ThingSpeak.setField(1, userID); // BackpAQ user 
  ThingSpeak.setField(2, latI); // GPS pos
  ThingSpeak.setField(3, lonI);
  ThingSpeak.setField(4, dustvalues1.PM2_5Val_atm); // PM2.5
  ThingSpeak.setField(5, trackName); // track name
  ThingSpeak.setField(6, iconURL);  // URL for PM-color icon
  ThingSpeak.setField(7, TVOC); //  TVOC value (if activated)
  ThingSpeak.setField(8, track_comment_array[track_id]); // attach any comments 

  // set the status
  //ThingSpeak.setStatus(myStatus);
  
  // write to the ThingSpeak channel
  thingspeak_C_key_c = thingspeak_C_key; // cast to const char 
  Serial.print("Key is "); Serial.println(thingspeak_C_key_c);
  Serial.print("Channel is "); Serial.println(thingspeak_C_channel_i);
  x = ThingSpeak.writeFields(thingspeak_C_channel_i, thingspeak_C_key_c);
  if (x == 200) {
    Serial.println("Thingspeak channel 3 update successful.");
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
}

// Send to Thingspeak using Blynk Webhook from smartphone
void SendToThingspeakWH(void) 
{
  Serial.println("Writing to Thingspeak, webhook.");
  /* Channel 1: Thingspeak Field:        1            2                      3                       4  5  6     7     8                 */
  Blynk.virtualWrite(V7, dustvalues1.PM01Val_atm, dustvalues1.PM2_5Val_atm, dustvalues1.PM10Val_atm, t, h, latI, lonI, airQualityIndex);
  Serial.println("Thingspeak channel 1 update successful.");
  delay(1000);
  /* Channel 2 Thingspeak Field:        1             2                   3                     4                    5                    6                   7       8      */
  Blynk.virtualWrite(V47, timeHack, dustvalues1.Beyond05, dustvalues1.Beyond1, dustvalues1.Beyond2_5, dustvalues1.Beyond5, dustvalues1.Beyond10, TVOC, eCO2);
  Serial.println("Thingspeak channel 2 update successful.");
  delay(1000);

}

void SendToThingspeak() // send data to Thingspeak cloud, channels defined in config.h tab
{
  if (!privateMode) { // check for data privacy..."0" = send to Thingspeak, "1" = NOT send [[ Note: not using WebHook now ]]
   //  if (backpaq_mobile == 1) // fetch value from config checkbox, 1 = mobile, 0 = stationary) // if user selects "mobile" mode in config...
    //  {
    //    SendToThingspeakWF(); // send data to Thingspeak using smartphone/Blynk WebHook
    //  } else
    //   {
        SendToThingspeakWF(); // send data to Thingspeak using WiFi and API call
    //   }
  }
}

void mapGPS()
{

  // ---------------------------------------------------------------------------------------------
  // Display tracks on BackpAQ MAP page using GPS position and PM values
  // Lat and Lon are also stored in Thingspeak via another function
  // ---------------------------------------------------------------------------------------------

  sprintf(pmLabel, "PM2.5:%4dug/m3\n", dustvalues1.PM2_5Val_atm); // use PM2.5 value
  Blynk.virtualWrite(V16, markerNum, latI, lonI, pmLabel, newColor); // update marker position=GPS, color=PMAQI (marker color hack for iOS)

  // Blynk.virtualWrite(V16, markerNum, latI, lonI, currentDate+ " " +currentTime, newColor); // timestamp

  // save lat, lon GPS position
  markerNum++; // increment marker number
  latISav[markerNum] = latI; // save pos
  lonISav[markerNum] = lonI;

  // -----------------------------------------------------------------------------------------------
  // Future capability to store and display tracks
  // write breadcrumbs...
  // for (int i=0; i<savedPos; i++) {
  // place saved markers on map
  // Blynk.virtualWrite(V16, savedPos+1, latI, lonI, airQualityIndex); // update GPS position from iphone sensor on map
  // }
  // -----------------------------------------------------------------------------------------------
}

// get, format current date and time from smartphone via Blynk RTC
void getTimeDate()
{
  currentTime = String(hour()) + ":" + minute() + ":" + second();
  currentDate = String(day()) + " " + month() + " " + year();
  Serial.print("Current time: ");
  Serial.println(currentTime);
  Serial.print("Current date: ");
  Serial.println(currentDate);

  timeHack = currentDate + " " + currentTime;

}

void getAQIData(void) {
  PM25_AQI_Data data;
  
  if (! aqi.read(&data)) {
    Serial.println("Could not read PM sensor");
    delay(500);  // try again in a bit!
    return;
  }
  Serial.println("PM sensor reading success");

  Serial.println();
  Serial.println(F("---------------------------------------"));
  Serial.println(F("Concentration Units (standard)"));
  Serial.println(F("---------------------------------------"));
  Serial.print(F("PM 1.0: ")); Serial.print(data.pm10_standard);
  Serial.print(F("\t\tPM 2.5: ")); Serial.print(data.pm25_standard);
  Serial.print(F("\t\tPM 10: ")); Serial.println(data.pm100_standard);
  Serial.println(F("Concentration Units (environmental)"));
  Serial.println(F("---------------------------------------"));
  Serial.print(F("PM 1.0: ")); Serial.print(data.pm10_env);
  Serial.print(F("\t\tPM 2.5: ")); Serial.print(data.pm25_env);
  Serial.print(F("\t\tPM 10: ")); Serial.println(data.pm100_env);
  Serial.println(F("---------------------------------------"));
  Serial.print(F("Particles > 0.3um / 0.1L air:")); Serial.println(data.particles_03um);
  Serial.print(F("Particles > 0.5um / 0.1L air:")); Serial.println(data.particles_05um);
  Serial.print(F("Particles > 1.0um / 0.1L air:")); Serial.println(data.particles_10um);
  Serial.print(F("Particles > 2.5um / 0.1L air:")); Serial.println(data.particles_25um);
  Serial.print(F("Particles > 5.0um / 0.1L air:")); Serial.println(data.particles_50um);
  Serial.print(F("Particles > 10 um / 0.1L air:")); Serial.println(data.particles_100um);
  Serial.println(F("---------------------------------------"));
  
  dustvalues1.PM01Val_atm = data.pm10_standard;
  dustvalues1.PM2_5Val_atm =data.pm25_standard;
  dustvalues1.PM10Val_atm = data.pm100_standard;
  
  dustvalues1.PM01Val_cf1 = data.pm10_env;
  dustvalues1.PM2_5Val_cf1 =data.pm25_env;
  dustvalues1.PM10Val_cf1 = data.pm100_env;
  
  dustvalues1.Beyond03 = data.particles_03um;
  dustvalues1.Beyond05 = data.particles_05um;
  dustvalues1.Beyond1 =  data.particles_10um;
  dustvalues1.Beyond2_5 =data.particles_25um;
  dustvalues1.Beyond5 =  data.particles_50um;
  dustvalues1.Beyond10 = data.particles_100um;

  //  Three “conversions” to adjust PM2.5 concentrations and corresponding AQI values for woodsmoke, an “AQandU” calibration (0.778 * PA + 2.65)
  //  to a long-term University of Utah study in Salt Lake City and an “LRAPA” calibration (0.5 * PA − 0.68) to a Lane Regional Air
  //  Pollution Agency study of PA sensors. USEPA: PM2.5 (µg/m³) = 0.534 x PA(cf_1) - 0.0844 x RH + 5.604
      switch (convFactor)
               {
                 case 1: // NO conversion
                   Serial.println("None selected");
                   break;
                case 2: // US EPA
                    Serial.println("US EPA selected");
                    dustvalues1.PM01Val_cf1 = 0.534 * dustvalues1.PM01Val_cf1 -.0844 * h + 5.604;
                    dustvalues1.PM2_5Val_cf1 = 0.534 * dustvalues1.PM2_5Val_cf1 - .0844 * h + 5.604;
                    dustvalues1.PM10Val_cf1 = 0.534 * dustvalues1.PM10Val_cf1 - .0844 * h + 5.604;
                    break;
                case 3: // LRAPA
                    Serial.println("LRAPA selected");
                    dustvalues1.PM01Val_cf1 = 0.5 * dustvalues1.PM01Val_cf1 -.68;
                    dustvalues1.PM2_5Val_cf1 = 0.5 * dustvalues1.PM2_5Val_cf1 - .68;
                    dustvalues1.PM10Val_cf1 = 0.5 * dustvalues1.PM10Val_cf1 - .68;
                    dustvalues1.PM01Val_atm = 0.5 * dustvalues1.PM01Val_atm - .68;
                    dustvalues1.PM2_5Val_atm = 0.5 * dustvalues1.PM2_5Val_atm - .68;
                    dustvalues1.PM10Val_atm = 0.5 * dustvalues1.PM10Val_atm - .68;
                    break;
                case 4: // AQndU
                    Serial.println("AQandU selected");
                    dustvalues1.PM01Val_cf1 = 0.778 * dustvalues1.PM01Val_cf1 + 2.65;
                    dustvalues1.PM2_5Val_cf1 = 0.778 * dustvalues1.PM2_5Val_cf1 + 2.65;
                    dustvalues1.PM10Val_cf1 = 0.778 * dustvalues1.PM10Val_cf1 + 2.65;
                    dustvalues1.PM01Val_atm = 0.778 * dustvalues1.PM01Val_atm + 2.65;
                    dustvalues1.PM2_5Val_atm = 0.778 * dustvalues1.PM2_5Val_atm + 2.65;
                    dustvalues1.PM10Val_atm = 0.778 * dustvalues1.PM10Val_atm + 2.65;
                    break;
                default:
                    Serial.println("Unknown item selected");
      } // switch

  // write to Blynk
     Blynk.setProperty(V1, "label", "PM2.5 ("+ conversion + ")");  // update PM2.5 label
     Blynk.virtualWrite(V0, dustvalues1.PM01Val_atm);  // using ATM values
     Blynk.virtualWrite(V1, dustvalues1.PM2_5Val_atm);
     Blynk.virtualWrite(V2, dustvalues1.PM10Val_atm);
     Blynk.virtualWrite(V34, dustvalues1.Beyond03);
     Blynk.virtualWrite(V35, dustvalues1.Beyond05);
     Blynk.virtualWrite(V36, dustvalues1.Beyond1);
     Blynk.virtualWrite(V37, dustvalues1.Beyond2_5);
     Blynk.virtualWrite(V38, dustvalues1.Beyond5);
     Blynk.virtualWrite(V39, dustvalues1.Beyond10);

  // calculate AQI and set colors for Blynk
     airQualityIndex = calculate_US_AQI25(dustvalues1.PM2_5Val_atm); // use PM2.5 ATM for AQI calculation
     calcAQIValue(); // set AQI colors

  // Update colors/labels for AQI gauge on Blynk
      Blynk.setProperty(V6, "color", newColor); // update AQI color
      Blynk.setProperty(V6, "label", newLabel);  // update AQI label
      Blynk.virtualWrite(V6, gaugeValue);  // update AQI value

 //   delay(pmSampleTime);
}

// get TVOC and eCO2 readings from SGP30 sensor
void getTVOC() {

    sgp.setHumidity(getAbsoluteHumidity(t, h)); // set abs humidity for SGP30
 
   if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    return;
  }

   eCO2 = sgp.eCO2;
   TVOC = sgp.TVOC;
     
 // display TVOC, eCO2 values
    Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
    Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");
    
  if (noSGP) { // if no SGP sensor data set values to 0
    TVOC = 0;
    eCO2 = 0;
  } else {    
  // write values to Blynk app   
    Blynk.virtualWrite(V30, TVOC);
    Blynk.virtualWrite(V31, eCO2);
  }
   
 // check to see raw measurements working   
    if (! sgp.IAQmeasureRaw()) {
    Serial.println("Raw Measurement failed");
    return;
  }
  
  // display raw values in log
  Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
  Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol); Serial.println("");
  
  tv_counter++;
  if (tv_counter == 30) {
    tv_counter = 0;

   // This will get the previous baseline from EEPROM
  // But it will only write it to the sensor if there really has been one before
  // On first run this will be empty. This will put the sensor into a special 12 hour baseline
  // finding mode. Also the sensor needs to "burn in". Optimal would be if, on first startup,
  // you let the sensor run for 48 hours. It will store a new baseline every X hours (as set bellow)
  // Make sure the sensor is exposed to outside air multiple times, during the first 48 hours, but especially
  // within the last three hours before turning it off the first time.
  // On next start up the last baseline will be in EEPROM and will be given to the sensor. From then on
  // it will always get a new baseline itself and the code will return it to the sensor in case of a power down.
  // The sensor should be exposed to outside air once a week to make sure the baseline is of good quality.
  
  EEPROM.get(1, TVOC_base);
  EEPROM.get(10, eCO2_base);

  if (eCO2_base != 0) sgp.setIAQBaseline(TVOC_base, eCO2_base); 
  Serial.print("****Baseline values in EEPROM: eCO2: 0x"); Serial.print(eCO2_base, HEX);
  Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
    
 // ensure baseline readings ok
    uint16_t TVOC_base, eCO2_base;
    if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.println("Failed to get baseline readings");
      return;
    }
    Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
    Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
  }

   // Prepare the EEPROMWrite interval
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= EEpromWrite * 3600000) {
    previousMillis = currentMillis; // reset the loop
    sgp.getIAQBaseline(&eCO2_base, &TVOC_base); // get the new baseline
    EEPROM.put(1, TVOC_base); // Write new baselines into EEPROM
    EEPROM.put(10, eCO2_base);
  }
}

void getTemps() {

  t = bme.readTemperature(); // fetch temp, humidity
  Serial.print("Temperature = ");
  Serial.print(t);
  Serial.println(" *C");

  // English Units - degrees F
  buffer[0] = '\0';
  dtostrf(t * 1.8 + 32.0, 5, 1, floatString);
  strcat(buffer, floatString);
  strcat(buffer, "F");
  // tempF = buffer;
  Blynk.virtualWrite(V3, buffer ); // send temp to Blynk

  h = bme.readHumidity();
  Serial.print("Humidity = ");
  Serial.print(h);
  Serial.println(" %");
  // write to Blynk
  buffer[0] = '\0';
  dtostrf(h , 6, 1, floatString);
  strcat(buffer, floatString);
  // humid = buffer;
  strcat(buffer, "%");
  Blynk.virtualWrite(V4, buffer ); // send humidity to Blynk
}

// Retrieve battery voltage from ESP on BackpAQ device, send to Blynk/SmartPhone
void getBatteryVoltsBackpAQ(){
 //Note: Battery voltage read pin changed from GPIO13 to GPI37 per Heltec
   
   adcStart(37);
   while(adcBusy(37));
   Serial.printf("Battery power in GPIO 37: ");
   Serial.println(analogRead(37));
   uint16_t c1  =  analogRead(37)*XS*MUL; // these factors highly processor dependant
   adcEnd(37);

   delay(100);

   adcStart(36);
   while(adcBusy(36));
   Serial.printf("voltage input on GPIO 36: ");
   Serial.println(analogRead(36));
   uint16_t c2  =  analogRead(36)*0.769 + 150;
   adcEnd(36);
   
   batteryV = c1 / 1000.0; // convert mV to V
   
// set battery indicator color
   if (batteryV >= 3.7) {
     batteryColor = "#009966"; // Green
   } else if (batteryV > 3.4 && batteryV < 3.7) {
     batteryColor = "#ffde33"; // Yellow
   } else if (batteryV > 3.0 && batteryV < 3.4) {
     batteryColor = "#ff9933"; // Orange
   } else {
     batteryColor = "#cc0033";  // Red
   }
  // send to Blynk 
   Blynk.setProperty(V21, "color", batteryColor) ;  // update battery color
   Blynk.virtualWrite(V21, batteryV); // update battery level indicator
   //Blynk.virtualWrite(V49, rawA0); // for debug
}
void updateWiFiSignalStrength() {

  rssi = WiFi.RSSI(); // fetch the received WiFi signal strength:
  Serial.print("WiFi signal strength (RSSI): ");
  Serial.print(rssi);
  Serial.println(" DBM");
  Blynk.virtualWrite(V56, rssi); // Post WiFi signal strength on Blynk
}

void powerOnSensor() {  // leaving sensor on for now...power used is small
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
//  digitalWrite(PINPMSGO, LOW);

  DEBUG_PRINTLN("going to sleep zzz...");
  delay(SLEEP_TIME);
}

// callback routine for WiFi configuration
void configModeCallback (ESP_WiFiManager *ESP_wifiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(ESP_wifiManager->getConfigPortalSSID());
  OLED.println("Goto WiFi 'BackpAQ'->");
  OLED.display();
}
//callback notifying us of the need to save WiFi config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//char * boolstring( _Bool b ) { return b ? true_a : false_a; }

void setup()
{
  Serial.begin(115200); // console

   if (! aqi.begin_I2C(&BMEWire)) {      // connect to the PMS sensor over I2C
  //if (! aqi.begin_UART(&Serial1)) { // connect to the sensor over hardware serial
  //if (! aqi.begin_UART(&pmSerial)) { // connect to the sensor over software serial 
    Serial.println("Could not find PM sensor!");
  } else {  Serial.println("PM25 found!"); } // ok we're good
 
 // init temp,  humidity sensor
  BMEWire.begin(14, 27); //  this assigns I2C pins for temp/hum +  PM sensor + TVOC  
  status = bme.begin(0X76, &BMEWire);  // this is the address for THIS sensor. Your mileage may vary (try 0x77 if not working).
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address.");
  }
 
 // start TVOC/CO2 sensor (0x58) using the BMEWire I2C port (they piggyback)
  if(!sgp.begin(&BMEWire)){
    Serial.println("Failed to start TVOC sensor! Please check your wiring.");
    noSGP = true;
   // while(1);
  } else {
   Serial.print("Found SGP30 serial #");
   Serial.print(sgp.serialnumber[0], HEX);
   Serial.print(sgp.serialnumber[1], HEX);
   Serial.println(sgp.serialnumber[2], HEX);
  }
  
  // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate'
  //sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!
  
  // initialize the LED digital pin as an output.
  pinMode(PIN_LED, OUTPUT);
  
   // Start OLED with Heltec ESP32Kit default pins
  Wire.begin(4, 15); // scl 15 sda 4 (this assigns I2C pins for other sensors as well)
  OLED.begin(SSD1306_SWITCHCAPVCC, 0x3C, &Wire); // init OLED
  
  // display name, version, copyright
  OLED.clearDisplay(); 
  OLED.setRotation(2);                               // rotate screen to fit new case (NOTE: THIS IS FOR NEW 94H VERSION ONLY)
  OLED.setTextSize(1);                               // Set OLED text size to small
  OLED.setTextColor(WHITE, BLACK);                   // Set OLED color to White
  OLED.setCursor(0, 0);                              // Set cursor to 0,0
  OLED.print("SSV BackpAQ ");
  OLED.println(version);
  // OLED.println("(c) 2020 SSV");                   // Choose to not display for now
  OLED.display();                                    // Update display
  delay(2000);

  Serial.setTimeout(1500);
  privateMode = 0;
  noBlynk = false;
  
  // set up battery measurment parameters
   analogSetCycles(8);                   // Set number of cycles per sample, default is 8 and provides an optimal result, range is 1 - 255
   analogSetSamples(1);                  // Set number of samples in the range, default is 1, it has an effect on sensitivity has been multiplied
   analogSetClockDiv(1);                 // Set the divider for the ADC clock, default is 1, range is 1 - 255
   analogSetAttenuation(ADC_11db);       // Sets the input attenuation for ALL ADC inputs, default is ADC_11db, range is ADC_0db, ADC_2_5db, ADC_6db, ADC_11db
   analogSetPinAttenuation(36,ADC_11db); // Sets the input attenuation, default is ADC_11db, range is ADC_0db, ADC_2_5db, ADC_6db, ADC_11db
   analogSetPinAttenuation(37,ADC_11db);
                                        // ADC_0db provides no attenuation so IN/OUT = 1 / 1 an input of 3 volts remains at 3 volts before ADC measurement
                                        // ADC_2_5db provides an attenuation so that IN/OUT = 1 / 1.34 an input of 3 volts is reduced to 2.238 volts before ADC measurement
                                        // ADC_6db provides an attenuation so that IN/OUT = 1 / 2 an input of 3 volts is reduced to 1.500 volts before ADC measurement
                                        // ADC_11db provides an attenuation so that IN/OUT = 1 / 3.6 an input of 3 volts is reduced to 0.833 volts before ADC measurement
   
   adcAttachPin(36);
   adcAttachPin(37);

//-----------------------------------------------------------------
// ESP WiFiManager is used here to configure Wifi
// We use SPIFFS to store config info in flash
//-----------------------------------------------------------------

//clean FS, uncomment for formatting flash. Needed any time this code is flashed to new chip or making changes to data structure
//SPIFFS.format();
 // ------------------------------------------------------------------
 // Open and read configuration parms from FS json
 // ------------------------------------------------------------------
  Serial.println("mounting FS...");

  // set up SPIFFS file system and initialize for our custom parameters
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) 
      {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          
        // Read saved parameters   
          strcpy(blynk_token,json["blynk_token"]);
          strcpy(thingspeak_A_channel, json["thingspeak_A_channel"]);
          thingspeak_A_channel_i = atoi(thingspeak_A_channel);
          strcpy(thingspeak_A_key,json["thingspeak_A_key"]);
          strcpy(thingspeak_B_channel,json["thingspeak_B_channel"]);
          thingspeak_B_channel_i = atoi(thingspeak_B_channel);
          strcpy(thingspeak_B_key,json["thingspeak_B_key"]);
          strcpy(thingspeak_C_channel,json["thingspeak_C_channel"]);
          thingspeak_C_channel_i = atoi(thingspeak_C_channel);
          strcpy(thingspeak_C_key,json["thingspeak_C_key"]);
          backpaq_mobile = json["backpaq_mode"]; Serial.print("backpaq_mobile: "); Serial.println(backpaq_mobile);
          strcpy(default_gps_position, json["default_GPS_position"]);
          strcpy(userID,json["userID"]);
          
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

    // Handle Stationary or portable mode - bool parameter visualized using checkbox, so couple of things to note
    // - value is always 'T' for true. When the HTML form is submitted this is the value that will be 
    //   sent as a parameter. When unchecked, nothing will be sent by the HTML standard.
    // - customhtml must be 'type="checkbox"' for obvious reasons. When the default is checked
    //   append 'checked' too
    // - labelplacement parameter is WFM_LABEL_AFTER for checkboxes as label has to be placed after the input field
    
    char customhtml[24] = "type=\"checkbox\"";
    if (backpaq_mobile) // if user saved "mobile mode" ...
    {
      strcat(customhtml, " checked"); //...show "check"
    }
  
  // create custom text for all parameters
  ESP_WMParameter p_backpaq_mode("bckpq_mode", "Run BackpAQ in mobile mode", "T", 2, customhtml, WFM_LABEL_AFTER);
  ESP_WMParameter custom_blynk_token("Blynk", "<small>Blynk auth token</small>", blynk_token, 34); // Define additional parameter for config portal
  ESP_WMParameter custom_userID("userIDD", "<small>BackpAQ Userid</small>", userID, 20); // userid
  ESP_WMParameter custom_gps_position("GPS", "<small>Default GPS Position</small>", default_gps_position, 25); // Enter default GPS position (format: Lat, Lon)
  ESP_WMParameter custom_thingspeak_A_channel("ThingspeakAC", "<small>Thingspeak Channel A</small>", thingspeak_A_channel, 16); // Define additional parameter for config portal 
  ESP_WMParameter custom_thingspeak_A_key("ThingspeakAK", "<small>Thingspeak API Write Key</small>", thingspeak_A_key, 20); // Define additional parameter for config portal
  ESP_WMParameter custom_thingspeak_B_channel("ThingspeakBC", "<small>Thingspeak Channel B</small>", thingspeak_B_channel, 16); // Define additional parameter for config portal
  ESP_WMParameter custom_thingspeak_B_key("ThingspeakBK", "<small>Thingspeak API Write Key</small>", thingspeak_B_key, 20); // Define additional parameter for config portal
  ESP_WMParameter custom_thingspeak_C_channel("ThingspeakCC", "<small>Thingspeak Channel C</small>", thingspeak_C_channel, 16); // Define additional parameter for config portal
  ESP_WMParameter custom_thingspeak_C_key("ThingspeakCK", "<small>Thingspeak API Write Key</small>", thingspeak_C_key, 20); // Define additional parameter for config portal
  ESP_WMParameter p_hint("<small>*Hint: if you want to reuse the currently active WiFi credentials, leave SSID and Password fields empty</small>");
  ESP_WMParameter p_space("<hr noshade>"); 

   
  // Check for double press of Reset Button...used here by user to request reset of stored AP creds
  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  if (drd->detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    initialConfig = true;
  }
    Serial.println("Starting configuration portal.");
    digitalWrite(PIN_LED, LOW); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.

    //Local intialization. Once its business is done, there is no need to keep it around
    ESP_WiFiManager ESP_wifiManager("BackpAQ"); // using Wifi Manager with customer portal capture to scan/prompt for ssid, password, other parms

   // ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 48), IPAddress(10,0,1, 1), IPAddress(255, 255, 255, 0)); // ESP version does this automatically
    ESP_wifiManager.setAPCallback(configModeCallback); // alert local display of need to connect to config SSID
    ESP_wifiManager.setSaveConfigCallback(saveConfigCallback); //set config save notify callback

    //Start an access point (SSID "BackpAQ") and go into a blocking loop awaiting configuration
    //  ESP_wifiManager.setConfigPortalTimeout(300); // allow 5 minutes to get configured
    ESP_wifiManager.setMinimumSignalQuality(-1);
    
    // Set custom parameters in config portal
    ESP_wifiManager.addParameter(&custom_userID); // userid
    ESP_wifiManager.addParameter(&p_space); // horiz rule
    ESP_wifiManager.addParameter(&custom_blynk_token); // Blynk auth token
    ESP_wifiManager.addParameter(&p_space); // horiz rule
    ESP_wifiManager.addParameter(&p_backpaq_mode); // Mobile or stationary mode
    ESP_wifiManager.addParameter(&p_space); // horiz rule
    ESP_wifiManager.addParameter(&custom_gps_position); // Default GPS position
    ESP_wifiManager.addParameter(&p_space); // horiz rule
    ESP_wifiManager.addParameter(&custom_thingspeak_A_channel); // Thingspeak A Channel
    ESP_wifiManager.addParameter(&custom_thingspeak_A_key); // Write API Key
    ESP_wifiManager.addParameter(&custom_thingspeak_B_channel); // Thingspeak B Channel
    ESP_wifiManager.addParameter(&custom_thingspeak_B_key); // Write API Key
    ESP_wifiManager.addParameter(&custom_thingspeak_C_channel); // Thingspeak C Channel
    ESP_wifiManager.addParameter(&custom_thingspeak_C_key); // Write API Key
    
    // Start it up....
    //ESP_wifiManager.startConfigPortal("BackpAQ");
   
  // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();

  //Remove this line if you do not want to see WiFi password printed
  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  if (Router_SSID != "" ) // test for no stored creds or double reset
  {
    ESP_wifiManager.setConfigPortalTimeout(30); //If no access point name has been previously entered disable timeout.
    Serial.println("Got stored Credentials. Timeout 30s");
  }
  else
  {
    Serial.println("No stored Credentials. No timeout");
  }

  String chipID = String(ESP_getChipId(), HEX);
  chipID.toUpperCase();
  
  // SSID and PW for Config Portal
  String AP_SSID = "BackpAQ_" + chipID; // set SSID = BackpAQ + unique chipID
  String AP_PASS = "BackpAQ";

  // Get Router SSID and PASS from EEPROM, then open Config portal AP named "BackpAQ_XXXXXX" and PW "BackpAQ"
  // 1) If got stored Credentials, Config portal timeout is 30s
  // 2) If no stored Credentials, stay in Config portal until get WiFi Credentials
  ESP_wifiManager.autoConnect(AP_SSID.c_str(), AP_PASS.c_str());
  //or use this for Config portal AP named "ESP_XXXXXX" and NULL password
  //ESP_wifiManager.autoConnect();

  //if you get here you have connected to the WiFi
  Serial.print("WiFi connected to ");
  Serial.println(WiFi.localIP());

  //read updated parameters from Config Portal

    strcpy(blynk_token, custom_blynk_token.getValue());
    strcpy(userID, custom_userID.getValue());
    strcpy(thingspeak_A_channel, custom_thingspeak_A_channel.getValue());
    strcpy(thingspeak_A_key, custom_thingspeak_A_key.getValue());
    strcpy(thingspeak_B_channel, custom_thingspeak_B_channel.getValue());
    strcpy(thingspeak_B_key, custom_thingspeak_B_key.getValue());
    strcpy(thingspeak_C_channel, custom_thingspeak_C_channel.getValue());
    strcpy(thingspeak_C_key, custom_thingspeak_C_key.getValue());
    strcpy(default_gps_position, custom_gps_position.getValue());  
    bool ggg = p_backpaq_mode.getValue();
     
    Serial.print("backpaq_mode: "); Serial.println(ggg);
  
    backpaq_mobile = strncmp(p_backpaq_mode.getValue(), "T", 1) == 0; // fetch value from config checkbox, returns "0" if found
    Serial.print("Mobile (1) or stationary 0): ");    Serial.println(backpaq_mobile); 
    
    // -----------------------------------------------------------------------------------------
    //save the custom parameters to FS
    // -----------------------------------------------------------------------------------------
    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      
     // JSONize parameters
      json["blynk_token"] = blynk_token;
      json["userID"] = userID;
      json["thingspeak_A_channel"] = thingspeak_A_channel;
      json["thingspeak_A_key"] = thingspeak_A_key;
      json["thingspeak_B_channel"] = thingspeak_B_channel;
      json["thingspeak_B_key"] = thingspeak_B_key;
      json["thingspeak_C_channel"] = thingspeak_C_channel;
      json["thingspeak_C_key"] = thingspeak_C_key;
      json["backpaq_mode"] = backpaq_mobile;
      json["default_GPS_position"] = default_gps_position;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }
      
      json.prettyPrintTo(Serial); // make pretty for debug
      json.printTo(Serial);
      json.printTo(configFile); // write to config "file" on SPIFFS
      configFile.close();

    } // should save

  // update OLED display with connection info...
    OLED.print("Connect: ");
    OLED.println(WiFi.SSID().substring(0, 10)); // display selected SSID, 1st 10 chars
    OLED.display();

    // proceed to connect to Blynk cloud
   // setSyncInterval(1); //sync RTC every second
  //  rtc.begin();
    Blynk.config(blynk_token, Blynk_server, 80); // use Blynk token set via WifiManager
    bool result = Blynk.connect();
    if (result != true)
    {
      DEBUG_PRINTLN("BLYNK Connection Failed");
      OLED.println("BLYNK Failed");
      //noBlynk = true; // set if not using Blynk/smartphone app
    }
    else // Blynk Connected!
    {
     // we're connected to Blynk!!
      
      DEBUG_PRINTLN("BLYNK Connected");
      OLED.println("BLYNK Connected!");    
    }

  Serial.println("Starting up...");

  /* get PMS sensor ready...*/
  //pinMode(PINPMSGO, OUTPUT); // Define PMS 7003 pin
  OLED.println("Sensor warming up ...");
  OLED.display();
  OLED.setTextSize(1);
  previousMillis = millis();

  DEBUG_PRINTLN("Switching On PMS");
  powerOnSensor(); // wake up!
  DEBUG_PRINTLN("Initialization finished");
  OLED.clearDisplay();

  Blynk.virtualWrite(V0, dustvalues1.PM01Val_atm); // prime pump

  //-------------------------------------------------------------------------
  // set timed functions for Blynk
  // Apparently important to stagger the times so as not to stack up timer
  // Note: 10 intervals/timer. If need to add more, get another timer
  //-------------------------------------------------------------------------
  timer.setInterval(10000L, getAQIData); // get data from PM sensor
  timer.setInterval(20000L, getTemps); // get temp,  humidity
  timer.setInterval(1100L, getTVOC); // get TVOC, CO2 data
  timer.setInterval(1080L, getTimeDate); // get current time, date from RTC
  timer.setInterval(6000L, mapGPS); // draw GPS position on Map screen every 6 seconds
  timer.setInterval(5000L, updateWiFiSignalStrength); // get and update WiFi signal strength
  timer.setInterval(1050L, updateOLED); // update local display
  timer.setInterval(15000L, getBatteryVoltsBackpAQ); // get battery charge from BackpAQ 
  timer.setInterval(thingspeak_Send_Interval, SendToThingspeak); // update sensor values to Thingspeak every Send_Interval seconds
  timer.setInterval(500L, check_status); // Heartbeat
  timer.setInterval(1000L, blinkLedWidget);

} // setup

void loop() // keep loop lean & clean!!
{
  drd->loop(); // monitor double reset clicks
  Blynk.run();
  timer.run();
}
