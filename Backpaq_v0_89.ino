//// BackpAQ    V0.88                                /////
//// (c) 2020 BackpAQ Labs LLC and Sustainable Silicon Valley
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
#include <FS.h>                   //this lib needs to be first, or it all crashes and burns...
#include "config_89.h" // config file  make config changes here
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <ESP_WiFiManager.h>      // https://github.com/khoih-prog/ESP_WiFiManager   special ESP fork of Tapzu's WiFiManager
#include <DoubleResetDetector.h>  //https://github.com/datacute/DoubleResetDetector
#include <BlynkSimpleEsp8266.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <Adafruit_BME280.h>
#include <widgetRTC.h>
#include <TimeLib.h>
#include "ThingSpeak.h"

const int PINPMSGO = D0; // PMS "sleep" pin to save power
#define LENG 31  //0x42 + 31 bytes equal to 32 bytes  
#define LENG 31   //0x42 + 31 bytes equal to 32 bytes
#define MSG_LENGTH 31   //0x42 + 31 bytes equal 
#define DEBUG_PRINTLN(x)  Serial.println(x)
#define DEBUG_PRINT(x)  Serial.print(x)
unsigned char buf[LENG];
char floatString[15]; // for temp/humidity string conversion
char buffer[20];
char pmLabel[20];
String pmLabel1;
char blynk_token[34] = "YOUR_BLYNK_TOKEN"; // will be filled in at WiFi config time and stored in eeprom
char thingspeak_A_channel[8] = "891066";
unsigned long thingspeak_A_channel_i;
unsigned long thingspeak_B_channel_i;
char thingspeak_B_channel[8] = "759545";
char thingspeak_A_key[20] = "A9L9601U1FROH6BE";
//const char * thingspeak_A_key = "A9L9601U1FROH6BE";
char thingspeak_B_key[20] = "6C1HZQR08LSTF5AG";
char backpaq_mobile_str[5];
char backpaq_mobile_string [5];
//const char * thingspeak_B_key = "6C1HZQR08LSTF5AG";
char resultstr[50]; // for PM Sensor
int status;
float p; // pressure (not used here)
float t; // temperature F
float h; // humidity
char tempF[20];
char humid[20];
float latI = 0;
float lonI = 0;
int savedPos = 0; // index for GPS position
float latISav [500];
float lonISav [500];
char buffr[16];
int airQualityIndex = 0;
String newColor;
String newLabel;
int gaugeValue;
float batteryV = 0.0;
float rssi = 0;
String currentTime;
String currentDate;
String timeHack;
unsigned long previousMillis;
int privateMode = 0;
bool not_Connected = false;
bool display_PM25 = false;
bool display_PM10 = false;
bool display_AQI = false;
bool display_TD = false;

//flag for saving data
bool shouldSaveConfig = false;

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10
// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

// Onboard LED I/O pin on NodeMCU board
const int PIN_LED = 2; // D4 on NodeMCU and WeMos. Controls the onboard LED.

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;
bool noBlynk;
bool thingspeakWebhook = true;
bool backpaq_mobile = true; // default mode for BackpAQ is mobile

//// Create constructors ////
Adafruit_SSD1306 OLED(-1); // Init OLED display
Adafruit_BME280 bme; // initialize BME temp/humidity sensor
WidgetMap myMap(V16); // set up GPS Map widget on Blynk
WidgetLED led24(V24); // indicate Wifi connected
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS); // used to detect "double press of ESP Reset button"
BlynkTimer timer; // Blynk timer
WidgetRTC rtc; // initialize realtime clock
WiFiClient  client;

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
// This function is called whenever "erase markers" button is pressed
BLYNK_WRITE(V40) {
  if (param.asInt()) {
    myMap.clear(); // erase any marker(s)
    Serial.print("Map GPS markers erased!");
  }
}
BLYNK_CONNECTED() {
  rtc.begin(); // sync time on Blynk connect
}
// This function is called whenever GPS position on smartphone is updated
BLYNK_WRITE(V15) {
  GpsParam gps(param); // fetch GPS position from smartphone

  latI = gps.getLat();
  lonI = gps.getLon();

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
struct dustvalues dustvalues1, dustvalues2;
// send commands to PMS - not currently used
unsigned char gosleep[] = { 0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73 };
unsigned char gowakeup[] = { 0x42, 0x4d, 0xe4, 0x00, 0x01, 0x01, 0x74 };
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
  } else {
    newColor = AQI_GREEN;  //"Safe"
    newLabel = "AQI: GOOD";
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

void updateOLED() {
  // update local display

  //DEBUG_PRINTLN("updateOLED");
  OLED.setTextColor(WHITE, BLACK);
  OLED.clearDisplay(); //
  OLED.setTextSize(1); //
  OLED.setCursor(0, 0); // reset cursor to origin
  OLED.print("   SSV BackpAQ ");
  OLED.println(version); // version from config.h
  OLED.setTextSize(1); // smallest size

  OLED.print("   PM1.0 = ");
  OLED.print(dustvalues1.PM01Val_atm); // write PM1.0 data
  OLED.println(" ug/m3");

  OLED.print("   PM2.5 = ");
  OLED.print(dustvalues1.PM2_5Val_atm); // write PM2.5 data
  OLED.println(" ug/m3");

  // Not displayed due to limitations of OLED display
  //OLED.print("   PM10  = ");
  //OLED.print(dustvalues1.PM10Val_atm); // write PM10 data
  //OLED.println(" ug/m3");

  OLED.print("   PM2.5 AQI  = ");
  OLED.println(airQualityIndex); // write AQI

  OLED.display(); //  display data

}

/****************** Send To Thinkspeak  Channel(s)  ***************/
void SendToThingspeakWF(void) // Send to Thingspeak using native web client
{

  Serial.println("Writing to Thingspeak, native.");

  ThingSpeak.begin(client);  // Initialize ThingSpeak

  // Update the first channel
  // set the fields with the values
  ThingSpeak.setField(1, dustvalues1.PM01Val_atm);
  ThingSpeak.setField(2, dustvalues1.PM2_5Val_atm);
  ThingSpeak.setField(3, dustvalues1.PM10Val_atm);
  ThingSpeak.setField(4, t);
  ThingSpeak.setField(5, h);
  ThingSpeak.setField(6, latI);
  ThingSpeak.setField(7, lonI);
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

  // Update the second channel
  // set the fields with the values
  ThingSpeak.setField(1, dustvalues1.Beyond03);
  ThingSpeak.setField(2, dustvalues1.Beyond05);
  ThingSpeak.setField(3, dustvalues1.Beyond1);
  ThingSpeak.setField(4, dustvalues1.Beyond2_5);
  ThingSpeak.setField(5, dustvalues1.Beyond5);
  ThingSpeak.setField(6, dustvalues1.Beyond10);
  ThingSpeak.setField(7, version);
  ThingSpeak.setField(8, timeHack);

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
}

void SendToThingspeakWH(void) // Send to Thingspeak using Webhook
{
  Serial.println("Writing to Thingspeak, webhook.");
  /* Channel 1: Thingspeak Field:        1            2                      3                       4  5  6     7     8                 */
  Blynk.virtualWrite(V7, dustvalues1.PM01Val_atm, dustvalues1.PM2_5Val_atm, dustvalues1.PM10Val_atm, t, h, latI, lonI, airQualityIndex);
  Serial.println("Thingspeak channel 1 update successful.");
  delay(1000);
  /* Channel 2 Thingspeak Field:        1             2                   3                     4                    5                    6                   7       8      */
  Blynk.virtualWrite(V47, dustvalues1.Beyond03, dustvalues1.Beyond05, dustvalues1.Beyond1, dustvalues1.Beyond2_5, dustvalues1.Beyond5, dustvalues1.Beyond10, version, timeHack);
  Serial.println("Thingspeak channel 2 update successful.");
  delay(1000);

}

void SendToThingspeak() // send data to Thingspeak cloud, channels defined in config.h tab
{
  if (!privateMode) { // check for data privacy..."0" = send to Thingspeak, "1" = NOT send
   //if (thingspeakWebhook || backpaq_mobile) // if user selects smartphone mode or checks "mobile" in config...
     if (backpaq_mobile) // if user checks "mobile" in config...
      {
      SendToThingspeakWH(); // send data to Thingspeak using smartphone/Blynk WebHook
      } else
       {
        SendToThingspeakWF(); // send data to Thingspeak using WiFi and API call
       }
  }
}

void mapGPS()
{

  // ---------------------------------------------------------------------------------------------
  // Display tracks on BackpAQ MAP page using GPS position and PM values
  // ---------------------------------------------------------------------------------------------

  sprintf(pmLabel, "PM2.5:%4dug/m3\n", dustvalues1.PM2_5Val_atm); // use PM2.5 value
  Blynk.virtualWrite(V16, savedPos, latI, lonI, pmLabel, newColor); // update marker position=GPS, color=PMAQI (marker color hack for iOS)

  // Blynk.virtualWrite(V16, savedPos, latI, lonI, currentDate+ " " +currentTime, newColor); // timestamp

  // save lat, lon GPS position
  savedPos++; // increment marker number
  latISav[savedPos] = latI; // save pos
  lonISav[savedPos] = lonI;

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
char checkValue(unsigned char *buf, char leng) {
  int receiveSum = 0;

  for (int i = 0; i < (leng - 2); i++) {
    receiveSum = receiveSum + buf[i];
  }
  receiveSum = receiveSum + 0x42;
  // receiveSum = receiveSum & 0xFFFF;
  Serial.print("Checksum calculated: ");
  Serial.println(receiveSum);
  Serial.print("Checksum found: ");
  Serial.println((buf[leng - 2] << 8) + buf[leng - 1]);

  if (receiveSum == ((buf[leng - 2] << 8) + buf[leng - 1])) return 1;

  return 0;
}

uint16_t transmitPM(char HighB, char LowB, unsigned char *buf) {
  uint16_t PMValue;
  PMValue = ((buf[HighB] << 8) + buf[LowB]);
  return PMValue;
}
////////////////////
///// PMS7003 in active mode, which is the default after powered on, constantly sends data with the latest measurement (in intervals of 200~2000ms)
///////////////////
void getpms5003(void) { // retrieve data frames from sensor
  unsigned char found = 0;
  // Note: this routine is very sensitive to timing and I/O . Though not elegantly written it works with the Plantower sensor. Change at your own risk.
  while (found < 2) {
    if (Serial.available() > 0) {
      if (Serial.find(0x42)) { //start to read when detect 0x42 (start character)
        Serial.println("0x42 found");
        Serial.readBytes(buf, LENG);
        if (buf[0] == 0x4d) { //second byte should be 0x4D
          found++;
          if (checkValue(buf, LENG)) {
            Serial.println("Checksum okay");
            // transmit PM values
            if (found == 1) {
              dustvalues1.PM01Val_cf1 = transmitPM(3, 4, buf); // CF1 data Bytes 4-9
              dustvalues1.PM2_5Val_cf1 = transmitPM(5, 6, buf);
              dustvalues1.PM10Val_cf1 = transmitPM(7, 8, buf);
              dustvalues1.PM01Val_atm = transmitPM(9, 10, buf); // ATM data Bytes 10-15
              dustvalues1.PM2_5Val_atm = transmitPM(11, 12, buf);
              dustvalues1.PM10Val_atm = transmitPM(13, 14, buf);
              dustvalues1.Beyond03 = transmitPM(15, 16, buf);
              dustvalues1.Beyond05 = transmitPM(17, 18, buf);
              dustvalues1.Beyond1 = transmitPM(19, 20, buf);
              dustvalues1.Beyond2_5 = transmitPM(21, 22, buf);
              dustvalues1.Beyond5 = transmitPM(23, 24, buf);
              dustvalues1.Beyond10 = transmitPM(25, 26, buf);
              // write to console
              Serial.println();
              Serial.println("-----------------1---------------------");
              Serial.println("Concentration Units (standard)");
              Serial.print("PM 1.0: "); Serial.print(dustvalues1.PM01Val_cf1);
              Serial.print("\t\tPM 2.5: "); Serial.print(dustvalues1.PM2_5Val_cf1);
              Serial.print("\t\tPM 10: "); Serial.println(dustvalues1.PM10Val_cf1);
              Serial.println("---------------------------------------");
              Serial.println("Concentration Units (environmental)");
              Serial.print("PM 1.0: "); Serial.print(dustvalues1.PM01Val_atm);
              Serial.print("\t\tPM 2.5: "); Serial.print(dustvalues1.PM2_5Val_atm);
              Serial.print("\t\tPM 10: "); Serial.println(dustvalues1.PM10Val_atm);
              Serial.println("---------------------------------------");
              Serial.print("Particles > 0.3um / 0.1L air:"); Serial.println(dustvalues1.Beyond03);
              Serial.print("Particles > 0.5um / 0.1L air:"); Serial.println(dustvalues1.Beyond05);
              Serial.print("Particles > 1.0um / 0.1L air:"); Serial.println(dustvalues1.Beyond1);
              Serial.print("Particles > 2.5um / 0.1L air:"); Serial.println(dustvalues1.Beyond2_5);
              Serial.print("Particles > 5.0um / 0.1L air:"); Serial.println(dustvalues1.Beyond5);
              Serial.print("Particles > 10.0 um / 0.1L air:"); Serial.println(dustvalues1.Beyond10);
              Serial.println("-----------------1--------------------");
              Serial.flush();
              // write to Blynk
              Blynk.virtualWrite(V0, dustvalues1.PM01Val_atm); // using ATM values
              Blynk.virtualWrite(V1, dustvalues1.PM2_5Val_atm);
              Blynk.virtualWrite(V2, dustvalues1.PM10Val_atm);
              Blynk.virtualWrite(V34, dustvalues1.Beyond03);
              Blynk.virtualWrite(V35, dustvalues1.Beyond05);
              Blynk.virtualWrite(V36, dustvalues1.Beyond1);
              Blynk.virtualWrite(V37, dustvalues1.Beyond2_5);
              Blynk.virtualWrite(V38, dustvalues1.Beyond5);
              Blynk.virtualWrite(V39, dustvalues1.Beyond10);

              delay(pmSampleTime);
            }
            if (found == 2) {
              dustvalues2.PM01Val_cf1 = transmitPM(3, 4, buf);
              dustvalues2.PM2_5Val_cf1 = transmitPM(5, 6, buf);
              dustvalues2.PM10Val_cf1 = transmitPM(7, 8, buf);
              dustvalues1.PM01Val_atm = transmitPM(9, 10, buf);
              dustvalues1.PM2_5Val_atm = transmitPM(11, 12, buf);
              dustvalues1.PM10Val_atm = transmitPM(13, 14, buf);
              dustvalues1.Beyond03 = transmitPM(15, 16, buf);
              dustvalues1.Beyond05 = transmitPM(17, 18, buf);
              dustvalues1.Beyond1 = transmitPM(19, 20, buf);
              dustvalues1.Beyond2_5 = transmitPM(21, 22, buf);
              dustvalues1.Beyond5 = transmitPM(23, 24, buf);
              dustvalues1.Beyond10 = transmitPM(25, 26, buf);
              // write to console
              Serial.println();
              Serial.println("-------------------2------------------");
              Serial.println("Concentration Units (standard)");
              Serial.print("PM 1.0: "); Serial.print(dustvalues2.PM01Val_cf1);
              Serial.print("\t\tPM 2.5: "); Serial.print(dustvalues2.PM2_5Val_cf1);
              Serial.print("\t\tPM 10: "); Serial.println(dustvalues2.PM10Val_cf1);
              Serial.println("---------------------------------------");
              Serial.println("Concentration Units (environmental)");
              Serial.print("PM 1.0: "); Serial.print(dustvalues1.PM01Val_atm);
              Serial.print("\t\tPM 2.5: "); Serial.print(dustvalues1.PM2_5Val_atm);
              Serial.print("\t\tPM 10: "); Serial.println(dustvalues1.PM10Val_atm);
              Serial.println("---------------------------------------");
              Serial.print("Particles > 0.3um / 0.1L air:"); Serial.println(dustvalues1.Beyond03);
              Serial.print("Particles > 0.5um / 0.1L air:"); Serial.println(dustvalues1.Beyond05);
              Serial.print("Particles > 1.0um / 0.1L air:"); Serial.println(dustvalues1.Beyond1);
              Serial.print("Particles > 2.5um / 0.1L air:"); Serial.println(dustvalues1.Beyond2_5);
              Serial.print("Particles > 5.0um / 0.1L air:"); Serial.println(dustvalues1.Beyond5);
              Serial.print("Particles > 10.0 um / 0.1L air:"); Serial.println(dustvalues1.Beyond10);
              Serial.println("------------------2--------------------");
              Serial.flush();
              // write to Blynk
              Blynk.virtualWrite(V0, dustvalues1.PM01Val_atm);  // using ATM values
              Blynk.virtualWrite(V1, dustvalues1.PM2_5Val_atm);
              Blynk.virtualWrite(V2, dustvalues1.PM10Val_atm);
              Blynk.virtualWrite(V34, dustvalues1.Beyond03);
              Blynk.virtualWrite(V35, dustvalues1.Beyond05);
              Blynk.virtualWrite(V36, dustvalues1.Beyond1);
              Blynk.virtualWrite(V37, dustvalues1.Beyond2_5);
              Blynk.virtualWrite(V38, dustvalues1.Beyond5);
              Blynk.virtualWrite(V39, dustvalues1.Beyond10);

              delay(pmSampleTime);
            }

            // calculate AQI and set colors for Blynk
            airQualityIndex = calculate_US_AQI25(dustvalues1.PM2_5Val_atm); // use PM2.5 ATM for AQI calculation
            calcAQIValue(); // set AQI colors

            // Update colors/labels for AQI gauge on Blynk
            Blynk.setProperty(V6, "color", newColor); // update AQI color
            Blynk.setProperty(V6, "label", newLabel);  // update AQI label
            Blynk.virtualWrite(V6, gaugeValue);  // update AQI value

            // Get battery-voltage
            int rawA0 = analogRead(BATT_LEVEL);
            batteryV = analogRead(BATT_LEVEL) / 197.5; // assumes 3.7V battery and external 180K resistor btwn A0 and Batt +. May have to adjust for specific batteries, loads.
            Blynk.virtualWrite(V21, batteryV); // update battery level indicator
            //Blynk.virtualWrite(V49, rawA0); // for debug

          }  else {  // bad checksum...
            Serial.println("Checksum not okay");
            Serial.flush();
            if (found > 0)found--;
            delay(500);
          }
        }
      }
    }
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
  digitalWrite(PINPMSGO, LOW);

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
  Serial.begin(9600);

  // initialize the LED digital pin as an output.
  pinMode(PIN_LED, OUTPUT);
  OLED.begin(SSD1306_SWITCHCAPVCC, 0x3C); // init OLED
  // display name, version, copyright
  OLED.clearDisplay();                               // Clear OLED display
  OLED.setTextSize(1);                               // Set OLED text size to small
  OLED.setTextColor(WHITE, BLACK);                   // Set OLED color to White
  OLED.setCursor(0, 0);                              // Set cursor to 0,0
  OLED.print("SSV BackpAQ ");
  OLED.println(version);
  // OLED.println("(c) 2020 SSV");                    // Choose to not display for now
  OLED.display();                                     // Update display
  delay(2000);

  Serial.setTimeout(1500);
  privateMode = 0;
  noBlynk = false;
  led24.off();

  //-----------------------------------------------------------------
  // ESP WiFiManager is used here to configure Wifi
  // We use SPIFFS to store config info in flash
  //-----------------------------------------------------------------

 //clean FS, uncomment for formatting flash
 //SPIFFS.format();
 // ------------------------------------------------------------------
 // Open and read configuration parms from FS json
 // ------------------------------------------------------------------
  Serial.println("mounting FS...");

  // set up SPIFFS file system and initialize for our Blynk token
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
          
        // Read parameters   
          strcpy(blynk_token,json["blynk_token"]);
          strcpy(thingspeak_A_channel, json["thingspeak_A_channel"]);
          thingspeak_A_channel_i = atoi(thingspeak_A_channel);
          strcpy(thingspeak_A_key,json["thingspeak_A_key"]);
          strcpy(thingspeak_B_channel,json["thingspeak_B_channel"]);
          thingspeak_B_channel_i = atoi(thingspeak_B_channel);
          strcpy(thingspeak_B_key,json["thingspeak_B_key"]);
          backpaq_mobile = json["backpaq_mode"];
          
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
    if (backpaq_mobile)
    {
      strcat(customhtml, " checked");
    }
  ESP_WMParameter p_backpaq_mode("bckpq_mode", "Check for BackpAQ mobile", "T", 2, customhtml, WFM_LABEL_AFTER);
 
  // create custom text for all parameters
  ESP_WMParameter custom_blynk_token("Blynk", "Blynk auth token", blynk_token, 34); // Define additional parameter for config portal
  ESP_WMParameter custom_thingspeak_A_channel("ThingspeakAC", "Thingspeak Channel A", thingspeak_A_channel, 8); // Define additional parameter for config portal 
  ESP_WMParameter custom_thingspeak_A_key("ThingspeakAK", "Thingspeak API Write Key", thingspeak_A_key, 20); // Define additional parameter for config portal
  ESP_WMParameter custom_thingspeak_B_channel("ThingspeakBC", "Thingspeak Channel B", thingspeak_B_channel, 8); // Define additional parameter for config portal
  ESP_WMParameter custom_thingspeak_B_key("ThingspeakBK", "Thingspeak API Write Key", thingspeak_B_key, 20); // Define additional parameter for config portal
  ESP_WMParameter p_hint("<small>*Hint: if you want to reuse the currently active WiFi credentials, leave SSID and Password fields empty</small>");
  ESP_WMParameter p_space("<hr noshade>"); 

  // test for existing logon credentials...
  if (WiFi.SSID() == "") {
    Serial.println("We haven't got any access point credentials, so get them now");
    initialConfig = true;
  }
  // Also, check for double press of Reset Button...used here by user to request reset of stored AP creds
  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    initialConfig = true;
  }

  if (initialConfig) {
    Serial.println("Starting configuration portal.");
    digitalWrite(PIN_LED, LOW); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.

    //Local intialization. Once its business is done, there is no need to keep it around
    ESP_WiFiManager ESP_wifiManager; // using Wifi Manager with customer portal capture to scan/prompt for ssid, password

   // ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 48), IPAddress(10,0,1, 1), IPAddress(255, 255, 255, 0)); // ESP version does this automatically
    ESP_wifiManager.setAPCallback(configModeCallback); // alert local display of need to connect to config SSID
    ESP_wifiManager.setSaveConfigCallback(saveConfigCallback); //set config save notify callback

    //Start an access point (SSID "BackpAQ") and go into a blocking loop awaiting configuration
    ESP_wifiManager.setConfigPortalTimeout(300); // allow 5 minutes to get configured
    
    // Set custom parameters in config portal
    ESP_wifiManager.addParameter(&p_hint); // wifi hint
    ESP_wifiManager.addParameter(&p_space); // horiz rule
    ESP_wifiManager.addParameter(&custom_blynk_token); // Blynk auth token
    ESP_wifiManager.addParameter(&p_space); // horiz rule
    ESP_wifiManager.addParameter(&p_backpaq_mode); // Mobile or stationary mode
    ESP_wifiManager.addParameter(&p_space); // horiz rule
    ESP_wifiManager.addParameter(&custom_thingspeak_A_channel); // Thingspeak A Channel
    ESP_wifiManager.addParameter(&custom_thingspeak_A_key); // Write API Key
    ESP_wifiManager.addParameter(&custom_thingspeak_B_channel); // Thingspeak B Channel
    ESP_wifiManager.addParameter(&custom_thingspeak_B_key); // Write API Key
    
    // Start it up....
    ESP_wifiManager.startConfigPortal("BackpAQ");
  } // initial config

  digitalWrite(PIN_LED, HIGH); // Turn led off as we are not in configuration mode.
  WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
  unsigned long startedAt = millis();
  Serial.print("After waiting ");
  int connRes = WiFi.waitForConnectResult();
  float waited = (millis() - startedAt);
  Serial.print(waited / 1000);
  Serial.print(" secs in setup() connection result is ");
  Serial.println(connRes);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("failed to connect, finishing setup anyway");
    not_Connected = true;
    OLED.println("Local mode no WiFi");
    OLED.display();
  } else
  { // connected!
    Serial.print("local ip: ");
    Serial.println(WiFi.localIP());

    //read updated parameters from Config Portal

    strcpy(blynk_token, custom_blynk_token.getValue());
    strcpy(thingspeak_A_channel, custom_thingspeak_A_channel.getValue());
    strcpy(thingspeak_A_key, custom_thingspeak_A_key.getValue());
    strcpy(thingspeak_B_channel, custom_thingspeak_B_channel.getValue());
    strcpy(thingspeak_B_key, custom_thingspeak_B_key.getValue());
   
    backpaq_mobile = (strncmp(p_backpaq_mode.getValue(), "T", 1) == 0); // fetch value from config checkbox, 1 = mobile, 0 = stationary
    Serial.print("Mobile (1) or stationary 0): ");
    Serial.println(backpaq_mobile); 
    // -----------------------------------------------------------------------------------------
    //save the custom parameters to FS
    // -----------------------------------------------------------------------------------------
    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      
     // JSONize parameters
      json["blynk_token"] = blynk_token;
      json["thingspeak_A_channel"] = thingspeak_A_channel;
      json["thingspeak_A_key"] = thingspeak_A_key;
      json["thingspeak_B_channel"] = thingspeak_B_channel;
      json["thingspeak_B_key"] = thingspeak_B_key;
      json["backpaq_mode"] = backpaq_mobile;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }
      
      json.prettyPrintTo(Serial); // make pretty for debug
      json.printTo(Serial);
      json.printTo(configFile); // write to config "file"
      configFile.close();

    } // should save

    // Ok, connected...
    OLED.print("Connect: ");
    OLED.println(WiFi.SSID().substring(0, 10)); // display selected SSID, 1st 10 chars
    OLED.display();

  } // connected

  if (!not_Connected) { // if we have a WiFi connection...
    // proceed to connect to Blynk cloud
    //  Blynk.config(auth); // use auth from Config.h
    Blynk.config(blynk_token); // use Blynk token set via WifiManager
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
      setSyncInterval(10 * 60); //sync RTC every 10 minutes
      DEBUG_PRINTLN("BLYNK Connected");
      // OLED.println("BLYNK Connected!");
    }
  } // if(!not_..

  // get temp,  humidity
  status = bme.begin(0X76);  // this is the address for THIS sensor. Your mileage may vary (try 0x77).
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check address.");
  }

  Serial.println("Starting up...");

  /* get PMS sensor ready...*/
  //pinMode(PINPMSGO, OUTPUT); // Define PMS 5003 pin
  OLED.println("Sensor warming up ...");
  OLED.display();

  previousMillis = millis();

  DEBUG_PRINTLN("Switching On PMS");
  powerOnSensor(); // wake up!
  DEBUG_PRINTLN("Initialization finished");
  OLED.clearDisplay();

  Blynk.virtualWrite(V0, dustvalues1.PM01Val_atm); //

  //-------------------------------------------------------------------------
  // set timed functions for Blynk
  // Apparently important to stagger the times so as not to stack up timer
  //-------------------------------------------------------------------------
  timer.setInterval(1000L, getpms5003); // get data from PM sensor
  timer.setInterval(10000L, getTemps); // get temp,  humidity
  timer.setInterval(1080L, getTimeDate); // get current time, date from RTC
  timer.setInterval(11000L, mapGPS); // draw GPS position on Map screen every 11 seconds
  timer.setInterval(5000L, updateWiFiSignalStrength); // get and update WiFi signal strength
  timer.setInterval(1050L, updateOLED); // update local display
  timer.setInterval(thingspeak_Send_Interval, SendToThingspeak); // update sensor values to Thingspeak every Send_Interval seconds

} // setup

void loop() // keep loop lean & clean!!
{
  drd.loop(); // monitor double reset clicks
  Blynk.run();
  timer.run();
}
