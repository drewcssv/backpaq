//// ---------- Version for Blynk IOT -------------------
//// BackpAQ    V0.99G-IOT (for AdaFruit Feather Huzzah ESP32 and Sensirion SCD4X, MEMS I2S Sound Sensor + onboard GPS                              /////
//// (c) 2021, 2022 BackpAQ Labs LLC and Sustainable Silicon Valley
//// BackpAQ Personal Air Quality Monitor
//// Written by A. L. Clark, BackpAQ Labs
/*
  MIT License

  Copyright (c) 2021, 2022 BackpAQ Labs LLC and Sustainable Silicon Valley

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

#define USE_SPIFFS  // Using SPIFFS

#include <FS.h>
#ifdef USE_LittleFS
//#define SPIFFS LITTLEFS
#include <LITTLEFS.h>
#else
#include <SPIFFS.h> // This version ued LITTLEFS
#endif

// include these libs for noise sampling via I2S
//#include <driver/i2s.h> // needs to be here before config!
//#include "arduinoFFT.h" // note special version!

#include "config_90.h" // config file  make config changes here

//-------------------------------------------------------------------------------------------------------------
// NOTE: This version is for BLYNK IOT with WiFi provisioning
//-------------------------------------------------------------------------------------------------------------

#define BLYNK_TEMPLATE_ID "TMPLFxu7Ig6o"
#define BLYNK_DEVICE_NAME "BackpAQV3"
//#define BLYNK_AUTH_TOKEN "2d1FFkrD6b3HMkqpiB7CZbhjPuxHdCmT"

#define BLYNK_FIRMWARE_VERSION        "0.3.2" // starting new versioning with V3

#define BLYNK_PRINT Serial
#define BLYNK_DEBUG

#define APP_DEBUG

// Board is Adafruit Feather Huzzah32  ESP32

#include "BlynkEdgent.h"

#ifdef ESP32
#include <esp_wifi.h>

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

#define GPS_BackpAQ 1           // Use GPS in BackpAQ device

#ifdef ESP32
//#include <BlynkSimpleEsp32.h>
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#else
#include <BlynkSimpleEsp8266.h>
#endif

// includes for Arduino and Adafruit libraries
#include <EEPROM.h>
#include <Adafruit_GFX.h>        // https://github.com/adafruit/Adafruit-GFX-Library
//#include <Adafruit_SSD1306.h>  //https://github.com/adafruit/Adafruit_SSD1306
#include <Adafruit_SH110X.h>     // https://github.com/adafruit/Adafruit_SH110x
#include <SensirionI2CScd4x.h>   // https://github.com/Sensirion/arduino-i2c-scd4x
#include <Arduino.h>
#include "ArduinoJson.h"          // Needs V5 (in script folder)
#include <widgetRTC.h>
#include <TimeLib.h>              //https://github.com/PaulStoffregen/Time
#include "ThingSpeak.h"           // Thingspeak Arduino library
#include "Adafruit_PM25AQI.h"     // https://github.com/adafruit/Adafruit_PM25AQI
#include "LinearRegression.h"     // Linear Regression Library
// includes and variables for Sound
#include "soundsensor.h"
#include "measurement.h"
// libraries for GPS
#include <SparkFun_u-blox_GNSS_Arduino_Library.h> //http://librarymanager/All#SparkFun_u-blox_GNSS
SFE_UBLOX_GNSS myGNSS;
#include <MicroNMEA.h>            //http://librarymanager/All#MicroNMEA
char nmeaBuffer[100];
MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));
// OLED
#include "oled.h"
static Oled oled;
// Sound
static char deveui[32];
static int cycleTime = CYCLETIME;
static bool ttnOk = false;
// task semaphores
bool audioRequest = false;
bool audioReady = false;
// payloadbuffer
unsigned char payload[80];
int payloadLength = 0;

// Global variables
//const int PINPMSGO = D0; // PMS "sleep" pin to save power (on WeMos)
#define LENG 31  //0x42 + 31 bytes equal to 32 bytes  
#define LENG 31   //0x42 + 31 bytes equal to 32 bytes
#define MSG_LENGTH 31   //0x42 + 31 bytes equal 
#define DEBUG_PRINTLN(x)  Serial.println(x)
#define DEBUG_PRINT(x)  Serial.print(x)

// SSID and PW for Router
String Router_SSID;
String Router_Pass;

// Buffers
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

// Temp, humidity vars
float p; // pressure (not used here)
float senTemp; // temperature F
float senHumid; // humidity
char tempF[20];
char humid[20];
//float temperature;
// float humidity;

// CO2, TVOC vars
 uint16_t co2;
float eCO2;
float TVOC;
uint16_t TVOC_base;
uint16_t eCO2_base;
bool noSGP = false;

// LR vars
int tv_counter = 0;
float values[2];        // define variables
int counter = 0;        // counting values for regression
int forecast = 0;       // in time[min] will be reach the next threshold
double correlation;

// GPS vars
float latI = 0;
float lonI = 0;
double latID = 0;
double lonID = 0;
float gpsSpeed = 0;
float lastKnownLat = 0;
float lastKnownLon = 0;
float default_lat = 0;
float default_lon = 0;
int markerNum = 0; // index for GPS position, incremented for each location
int track_id = 0;
float latISav [500]; // array to save tracks
float lonISav [500];

// Track vars
String trackName = "no name";
String trackStartTime = "";
String trackEndTime = "";
bool amTracking = false;
String track_comment = "";
String track_comment_array[500];
char buffr[16];
int airQualityIndex = 0;
String newColor;
String iconURL = "";

// Battery voltage, power vars
#define batteryPin 35
//#define batteryPin A13
#define battery_K ((2) * (3300.0) / 4096.0)
String batteryColor;
String newLabel;
int gaugeValue;
float batteryV = 0.0;

// Misc global variables
float rssi = 0;
String currentTime;
String currentDate;
String timeHack;
//unsigned long previousMillis;
int privateMode = 0;
// display vars
bool not_Connected = false;
bool display_PM25 = false;
bool display_PM10 = false;
bool display_AQI = false;
bool display_TD = false;
String conversion = "NONE";
int convFactor = 1;

//flag for saving data
bool shouldSaveConfig = false;

// Sound measurement variables
String currLoudnessDBA; // holds currrent dB(A) average noise value
String currLoudnessDBC; // holds currrent dB(C) average noise value
String currLoudnessDBZ; // holds currrent dB(Z) average noise value
String currSpectrumA;
String currSpectrumC;
String currSpectrumZ;

// Onboard LED I/O pin on NodeMCU board
const int PIN_LED = 2; // D4 on NodeMCU and WeMos. Controls the onboard LED.

// Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
bool initialConfig = false;
bool noBlynk;

// set up "traffic light" parameters
String trafficLight = "green";
int greenLevel = 0;     // threshold - enterng green level
int yellowLevel = 500;  // threshold - entering yellow level
int redLevel = 1000;    // threshold - entering red level

#define ADC_PIN 13
#define CONV_FACTOR 4.0
//#define READS 20
// These defines must be put before #include <ESP_DoubleResetDetector.h>
// to select where to store DoubleResetDetector's variable.
// For ESP32, You must select one to be true (EEPROM or SPIFFS/LITTLEFS)
// For ESP8266, You must select one to be true (RTC, EEPROM or SPIFFS)
// Otherwise, library will use default EEPROM storage
#define ESP_DRD_USE_EEPROM      false
#define ESP_DRD_USE_SPIFFS      true    //false

#define DOUBLERESETDETECTOR_DEBUG       true  //false

//#include <ESP_DoubleResetDetector.h>      //https://github.com/khoih-prog/ESP_DoubleResetDetector

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 7

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

LinearRegression lr;    // Linear Regression: define lr object

// create soundsensor
static SoundSensor soundSensor;

// Start I2C  with both ESP3Kit ports (for Heltec only)
//TwoWire BMEWire =  TwoWire(1);

// Declaration for an OLED display connected to I2C (SDA, SCL pins) and uses pin 16 on ESP32 for reset
#define OLED_RESET     16 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SH1107 OLED = Adafruit_SH1107(64, 128, &Wire); // OLED display
//---------------------------------------------------------------------------------------------------------
// Initialize sensors
Adafruit_PM25AQI aqi = Adafruit_PM25AQI(); // PM sensor library

// Sensirion SGD41X Sensor
SensirionI2CScd4x scd4x;

// Blynk widgets
WidgetMap myMap(V7); // set up Map widget on Blynk
WidgetLED led2(V91); // indicate tracking started
WidgetTerminal myTerm(V70); //Terminal widget for entering comments on Map
BlynkTimer timer; // Blynk timer
WidgetRTC rtc; // initialize realtime clock

WiFiClient  client1; // initialize WiFi client

////--------------------------------------------------------------------------------------------------------------------
// Blynk Mobile App Events and Handlers ----
// The following are functions that are invoked upon request from the BackpAQ device
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
// This function is called whenever the assigned Button changes state [[ not currently being used ]]
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
   // myMap.clear(); // erase any marker(s)
   Blynk.virtualWrite(V5, "clr"); // V5 is the Map virtual pin
    Serial.print("Map GPS markers erased!");
  }
}
// This function is called whenever Tracking Start/End is started/stopped
BLYNK_WRITE(V90) {
  if (param.asInt()) {
    Serial.println("Tracking Started at " + timeHack);
    blinkLedWidget();  // blink LED
    trackName = trackName + ">"; // append start
    if (amTracking) { // if already tracking...
      amTracking = false;
    }

  } else {
    Serial.println("Tracking Stopped at " + timeHack);
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
  Serial.println("Track name is " + trackName);
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

// When BackpAQ device connected
BLYNK_CONNECTED() {
  rtc.begin(); // sync time on Blynk connect
  setSyncInterval(1 * 60);

 // Send requests for sensor metadata (Note: metadata values are unique to each BackpAQ sensor)
  Blynk.sendInternal("meta", "get", "Sensor Name");
  Blynk.sendInternal("meta", "get", "Sensor Type");
  Blynk.sendInternal("meta", "get", "Channel A Number");  
  Blynk.sendInternal("meta", "get", "Channel A Key");  
  Blynk.sendInternal("meta", "get", "Channel B Number");  
  Blynk.sendInternal("meta", "get", "Channel B Key");
  Blynk.sendInternal("meta", "get", "Channel C Number");  
  Blynk.sendInternal("meta", "get", "Channel C Key");
  Blynk.sendInternal("meta", "get", "Default GPS Position");
  Blynk.sendInternal("meta", "get", "Backpaq Mode");
}

// Retrieve sensor metadata
// Fires when Blynk is initiated

BLYNK_WRITE(InternalPinMETA) {
    String field = param[0].asStr();
    
    if (field == "Sensor Name")
    {
      String value = param[1].asStr();
      Serial.print("Sensor Name = ");
      Serial.println(value);
    }  
    if (field == "Sensor Type")
    {
      String value = param[1].asStr();      
      Serial.print("Sensor Type = ");
      Serial.println(value);        
    }
    if (field == "Channel A Number")
    {
      thingspeak_A_channel_i = param[1].asInt();      
      Serial.print("Channel A Number = ");
      Serial.println(thingspeak_A_channel_i);    
    }
     if (field == "Channel B Number")
    {
      thingspeak_B_channel_i = param[1].asInt();      
      Serial.print("Channel B Number = ");
      Serial.println(thingspeak_B_channel_i);   
    }
     if (field == "Channel C Number")
    {
      thingspeak_C_channel_i = param[1].asInt();      
      Serial.print("Channel C Number = ");
      Serial.println(thingspeak_C_channel_i);   
    }
     if (field == "Channel A Key")
    {
      thingspeak_A_key_s = param[1].asStr();      
      Serial.print("Channel A Key = ");
      Serial.println(thingspeak_A_key_s);   
    }
     if (field == "Channel B Key")
    {
      thingspeak_B_key_s = param[1].asStr();      
      Serial.print("Channel B Key = ");
      Serial.println(thingspeak_B_key_s);   
    }
     if (field == "Channel C Key")
    {
      thingspeak_C_key_s = param[1].asStr();      
      Serial.print("Channel C Key = ");
      Serial.println(thingspeak_C_key_s);    
    }
     if (field == "Default GPS Position")
    {
      String default_gps_pos = param[1].asStr();      
      Serial.print("Default GPS Position = ");
      Serial.println(default_gps_pos); 
    }
     if (field == "Backpaq Mode")
    {
      String value = param[1].asStr();      
      Serial.print("BackpAQ Mode = ");
      Serial.println(value);
      backpaq_mobile = 1; // for now
    }
}

#ifdef GPS_SmartPhone
//This function is called whenever GPS position on smartphone is updated
BLYNK_WRITE(V15) {
  GpsParam gps(param); // fetch GPS position from smartphone

  latI = gps.getLat();
  lonI = gps.getLon();
  gpsSpeed = gps.getSpeed();

  if (latI == 0) { // in case Blynk not active or GPS not in range...
    latI = lastKnownLat; // assign last known pos
    lonI = lastKnownLon;
  } else {
    lastKnownLat = latI; // store pos as last known pos
    lastKnownLon = lonI;
  }
} 
#endif
// Add comments to track data file on Thingspeak
BLYNK_WRITE(V70) {
  track_comment = "";
  track_id++;
  myTerm.clear(); // clear the pipes
  track_comment = param.asStr(); // fetch new comments from MAP page
  if (track_comment != "") {
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
    
  } else if (amTracking) {
    led2.on();
    
  } else {
    led2.off();
  }
}
void printUint16Hex(uint16_t value) {
  Serial.print(value < 4096 ? "0" : "");
  Serial.print(value < 256 ? "0" : "");
  Serial.print(value < 16 ? "0" : "");
  Serial.print(value, HEX);
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
  Serial.print("Serial: 0x");
  printUint16Hex(serial0);
  printUint16Hex(serial1);
  printUint16Hex(serial2);
  Serial.println();
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
//--------------------------------------------------------------------------------------
//Handle sound measurements. Called from Blynk Timer.
void getSounds( ) {

  // oled.begin(deveui);

  // Weighting lists
  static float aweighting[] = A_WEIGHTING;
  static float cweighting[] = C_WEIGHTING;
  static float zweighting[] = Z_WEIGHTING;

  // measurement buffers
  static Measurement aMeasurement( aweighting);
  static Measurement cMeasurement( cweighting);
  static Measurement zMeasurement( zweighting);

  soundSensor.offset( MIC_OFFSET);   // set microphone dependent correction in dB
  soundSensor.begin();

  // main loop task 0
  /* while( true){ */
  // read chunk from MEMS mic and perform FFT, and sum energy in octave bins
  float* energy = soundSensor.readSamples();

  // update
  aMeasurement.update( energy);
  cMeasurement.update( energy);
  zMeasurement.update( energy);

  // calculate averages and compose ThingSpeak message
  if ( audioRequest) {
    audioRequest = false; // if first time else...

    aMeasurement.calculate();
    cMeasurement.calculate();
    zMeasurement.calculate();

    // debug info, normally commented out
    aMeasurement.print();
    cMeasurement.print();
    zMeasurement.print();

    // oled.showValues( aMeasurement, cMeasurement, zMeasurement, ttnOk);

    composeMessage( aMeasurement, cMeasurement, zMeasurement); // compose and send data to ThingSpeak

    //Serial.write(payload, payloadLength);

    // reset counters etc.
    aMeasurement.reset();
    cMeasurement.reset();
    zMeasurement.reset();
    
  //  printf("end compose message core=%d\n", xPortGetCoreID());
    audioReady = true;    // signal that audio report is ready
  }
  audioRequest = true; // ensure one pass per time segment
}
// compose ThingSpeak payload message
static bool composeMessage( Measurement& la, Measurement& lc, Measurement& lz) {
/*
  // find max value to compress values [0 .. max] in an unsigned byte from [0 .. 255]
  float max = ( la.max > lc.max) ? la.max : lc.max;
  max = ( lz.max > max) ? lz.max : max;
  float c = 255.0 / max;
  int i = 0;
  payload[ i++] = round(max);   // save this constant as first byte in the message

  payload[ i++] = round(c * la.min);
  payload[ i++] = round(c * la.max);
  payload[ i++] = round(c * la.avg);

  payload[ i++] = round(c * lc.min);
  payload[ i++] = round(c * lc.max);
  payload[ i++] = round(c * lc.avg);

  payload[ i++] = round(c * lz.min);
  payload[ i++] = round(c * lz.max);
  payload[ i++] = round(c * lz.avg);

  for ( int j = 0; j < OCTAVES; j++) {
    payload[ i++] = round(c * lz.spectrum[j]);
  }

  payloadLength = i;
  printf( "messagelength=%d\n", payloadLength);

  if ( payloadLength > 51)   // max TTN message length
    printf( "message too big, length=%d\n", payloadLength);
*/
 // Build spectrum array

     String energyStringA = ""; String energyA = "";
     String energyStringC = ""; String energyC = "";
     String energyStringZ = ""; String energyZ = "";
     String energyStringStart = "["; String energyStringEnd = "]";
  
 // Loop through spectrum for each weighting  
     for (int i = 0; i < OCTAVES; i++) {
       energyA += String(la.spectrum[i]) + ",";
       energyC += String(lc.spectrum[i]) + ",";
       energyZ += String(lz.spectrum[i]) + ",";
      }
 // package for AQView in JSON form       
    energyStringA = energyStringStart + energyA + energyStringEnd;
    energyStringC = energyStringStart + energyC + energyStringEnd;
    energyStringZ = energyStringStart + energyZ + energyStringEnd;
    
   // DEBUG ONLY
    Serial.println("SpectrumA: " + energyStringA);
    Serial.println("SpectrumC: " + energyStringC);
    Serial.println("SpectrumZ: " + energyStringZ);
    
    static char outstrA[10]; static char outstrC[10]; static char outstrZ[10];
// strings
    dtostrf(la.avg, 6, 2, outstrA);
    dtostrf(lc.avg, 6, 2, outstrC);
    dtostrf(lz.avg, 6, 2, outstrZ);
    
// package to send to ThingSpeak
    currLoudnessDBA = outstrA; // update OLED, Blynk value is avg dB(A)
    currLoudnessDBC = outstrC; // update OLED, Blynk value is avg dB(C)
    currLoudnessDBZ = outstrZ; // update OLED, Blynk value is avg dB(Z)
    currSpectrumA = energyStringA;
    currSpectrumC = energyStringC;
    currSpectrumZ = energyStringZ;
    
   // DEBUG ONLY
    Serial.print("dB(A) average: "); Serial.println(outstrA);
    Serial.print("dB(C) average: "); Serial.println(outstrC);
    Serial.print("dB(Z) average: "); Serial.println(outstrZ);
      
 // Finally, update Blynk with sound values

    Blynk.virtualWrite(V25, la.avg);  // Update Blynk with current average Noise dB(A)
    Blynk.virtualWrite(V26, lc.avg);  // Update Blynk with current average Noise dB(C)
    Blynk.virtualWrite(V27, lz.avg);  // Update Blynk with current average Noise dB(Z)  
   
    Blynk.virtualWrite(V28, energyA); // write "A" spectrum to Blynk (array of sound levels)
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
// Particulates
  OLED.println("  ");
  OLED.print("   PM1.0 = ");
  OLED.print(dustvalues1.PM01Val_cf1); // write PM1.0 data
  OLED.println(" ug/m3");

  OLED.print("   PM2.5 = ");
  OLED.print(dustvalues1.PM2_5Val_cf1); // write PM2.5 data
  OLED.println(" ug/m3");

  OLED.print("   PM10  = ");
  OLED.print(dustvalues1.PM10Val_cf1); // write PM10 data
  OLED.println(" ug/m3");

  //OLED.print("   PM2.5 AQI  = ");
  //OLED.println(airQualityIndex); // write AQI
// Gas Phase
  OLED.print("   CO2 = ");
  OLED.print(co2);
  OLED.println(" ppm");

 // Sound Levels
  OLED.print("   Noise = ");
  OLED.print(currLoudnessDBA.toInt());
  OLED.println(" dB(A)");

  OLED.display(); //  display data on OLED
}
/***------------------------------------------------------------------------- */
/**************************** Send To Thinkspeak  *****************************/
/***------------------------------------------------------------------------- */
void SendToThingspeakWF(void) // Send to Thingspeak using web services call
{

  Serial.println("Writing to Thingspeak...");

  ThingSpeak.begin(client1);  // Initialize ThingSpeak API endpoint

  String def_latlon = default_gps_position;

  if (default_gps_position == "") {    // use GPS position from config since we don't have smartphone GPS avail

    String def_latlon = default_gps_position;
    // Parse Lat-lon string
    int comma = def_latlon.indexOf(",", 0);
    String default_lat_str = def_latlon.substring(0, comma - 1); // extract lat
    String default_lon_str = def_latlon.substring(comma + 1, 18); // extract lon
    
    // convert to float
    default_lat  = default_lat_str.toFloat();
    default_lon = default_lon_str.toFloat();
    
    if (backpaq_mobile == 0) { // if fixed position
      latI = default_lat; // use default lan, lon from config
      lonI = default_lon;
    }
    Serial.print("Lat: ");  Serial.print(latI, 7);
    Serial.print(", Lon: ");  Serial.println(lonI, 7);

  } else {   // we have Lat/Lon from GPS
    //Serial.print("Lat: ");  Serial.print(latI, 7);
    //Serial.print(", Lon: ");  Serial.println(lonI, 7);
    Serial.println("Using GPS position from BackpAQ GPS");
  }

  if ((latI == 0.00000)  || (lonI == 0.00000)) {
    Serial.println("No GPS position found...exiting before sending to ThingSpeak.");
    Blynk.notify("No GPS position found from device {DEVICE_NAME} ");
    return;  // exit if no GPS position, don't send to ThingSpeak
  }

  // OK, we have a GPS position...

    Serial.print("Latitude: ");  Serial.print(latI, 7);
    Serial.print(", Longitude: ");  Serial.println(lonI, 7);

  // -------------------------------------------------------
  // CHANNEL 1 dataframe: Update the first channel with PM1, PM2.5, PM10 + temp, humidity, lat, lon, CO2
  //--------------------------------------------------------

  // Using "US EPA" correction factor for PM2.5 data

  //   dustvalues1.PM01Val_cf1 = 0.534 * dustvalues1.PM01Val_cf1 - .0844 * senHumid + 5.604;
  //  dustvalues1.PM2_5Val_cf1 = 0.534 * dustvalues1.PM2_5Val_cf1 - .0844 * senHumid + 5.604;
  //   dustvalues1.PM10Val_cf1 = 0.534 * dustvalues1.PM10Val_cf1 - .0844 * senHumid + 5.604;

  ThingSpeak.setField(1, dustvalues1.PM01Val_cf1);
  ThingSpeak.setField(2, dustvalues1.PM2_5Val_cf1);
  ThingSpeak.setField(3, dustvalues1.PM10Val_cf1);
  ThingSpeak.setField(4, senTemp);
  ThingSpeak.setField(5, senHumid);
  ThingSpeak.setField(6, latI); //
  ThingSpeak.setField(7, lonI); //
  //ThingSpeak.setField(8, airQualityIndex);
  ThingSpeak.setField(8, co2);
  ThingSpeak.setStatus(currSpectrumA); // sneaky!
  ThingSpeak.setElevation(currLoudnessDBA.toFloat()); // sneaky!
  ThingSpeak.setLatitude(currLoudnessDBC.toFloat()); //
  ThingSpeak.setLongitude(currLoudnessDBZ.toFloat()); //

  // write to the ThingSpeak channel

  //  char thingspeak_A_key[20] = "xxxxxxxxxxxxxxxx";

 
  thingspeak_A_key_c = thingspeak_A_key_s.c_str();
  
  //thingspeak_A_key_c = thingspeak_A_key; // cast to const char *
  int x = ThingSpeak.writeFields(thingspeak_A_channel_i, thingspeak_A_key_c);
  if (x == 200) {
    Serial.println("Thingspeak channel 1 update successful.");
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }

  delay(2000); // take a breath...
  // ----------------------------------------------------------
  // CHANNEL 2 dataframe: Update the second channel
  // set the fields with the concentration values + noise in Db(A) and CO2
  // ----------------------------------------------------------
  ThingSpeak.setField(1, timeHack);
  ThingSpeak.setField(2, dustvalues1.Beyond05);
  ThingSpeak.setField(3, dustvalues1.Beyond1);
  ThingSpeak.setField(4, dustvalues1.Beyond2_5);
  ThingSpeak.setField(5, dustvalues1.Beyond5);
  ThingSpeak.setField(6, dustvalues1.Beyond10);
  ThingSpeak.setField(7, currLoudnessDBA);
  ThingSpeak.setField(8, co2);

  // write to the ThingSpeak channel
 // thingspeak_B_key_c = thingspeak_B_key; // cast to const char *
  thingspeak_B_key_c = thingspeak_B_key_s.c_str();
  int xx = ThingSpeak.writeFields(thingspeak_B_channel_i, thingspeak_B_key_c);
  if (xx == 200) {
    Serial.println("Thingspeak channel 2 update successful.");
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(xx));
  }

  delay(2000);
  //--------------------------------------------------------------------
  // CHANNEL 3 dataframe: Write track, lat, lon, PM, CO2 & comment data to Thingspeak
  // Set the fields with the values so we can build a geoJSON file
  // -------------------------------------------------------------------
  String names = String(userID) + "," + trackName;
  //Serial.println("names = " + names);
  ThingSpeak.setField(1, userID); // BackpAQ user
  ThingSpeak.setField(2, latI); // GPS pos
  ThingSpeak.setField(3, lonI);
  ThingSpeak.setField(4, dustvalues1.PM2_5Val_cf1); // PM2.5
  ThingSpeak.setField(5, trackName); // track name
  ThingSpeak.setField(6, iconURL);  // URL for PM-color icon
  // ThingSpeak.setField(7, TVOC); //  TVOC value (if activated)
  ThingSpeak.setField(7, co2); //  CO2 value (if activated)
  ThingSpeak.setField(8, track_comment_array[track_id]); // attach any comments
  ThingSpeak.setStatus(currSpectrumA); // sneaky!
  ThingSpeak.setElevation(currLoudnessDBA.toFloat()); // sneaky!
  ThingSpeak.setLatitude(currLoudnessDBC.toFloat()); //
  ThingSpeak.setLongitude(currLoudnessDBZ.toFloat()); //

  // write to the ThingSpeak channel
  thingspeak_C_key_c = thingspeak_C_key_s.c_str();
 // thingspeak_C_key_c = thingspeak_C_key; // cast to const char
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
  Blynk.virtualWrite(V7, dustvalues1.PM01Val_atm, dustvalues1.PM2_5Val_atm, dustvalues1.PM10Val_atm, senTemp, senHumid, latI, lonI, airQualityIndex);
  Serial.println("Thingspeak channel 1 update successful.");
  delay(1000);
  /* Channel 2 Thingspeak Field:        1             2                   3                     4                    5                    6                   7       8      */
  Blynk.virtualWrite(V47, timeHack, dustvalues1.Beyond05, dustvalues1.Beyond1, dustvalues1.Beyond2_5, dustvalues1.Beyond5, dustvalues1.Beyond10, TVOC, eCO2);
  Serial.println("Thingspeak channel 2 update successful.");
  delay(1000);

}

void SendToThingspeak() // send data to Thingspeak cloud, check first for privacy mode
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
  // Display tracks on Blynk MAP page using GPS position and PM values
  // Lat and Lon are also stored in ThingSpeak via another function
  // ---------------------------------------------------------------------------------------------
 
  sprintf(pmLabel, "PM2.5:%4dug/m3\n", dustvalues1.PM2_5Val_atm); // use PM2.5 value for marker label

 Blynk.virtualWrite(V5, markerNum, latID, lonID, pmLabel, newColor); // update marker position=GPS, color=PMAQI (marker color hack for iOS)
 // Blynk.virtualWrite(V7, lonID, latID); // what happened to the other functions?
 //   Blynk.virtualWrite(V7, 1, lonI, latI, "tag");


// WidgetMap myMap(V4);
int Index = 1;
//double latitude = (gps.location.lat());
//double longitude = (gps.location.lng());

 //myMap.location(markerNum, latID, lonID, pmLabel);
 //myMap.location(markerNum, lonID, latID, pmLabel);
  
  markerNum++; // increment marker number
  latISav[markerNum] = latI; // save pos
  lonISav[markerNum] = lonI;

}

// get GPS position from BackpAQ GPS
void getGPS() {

  String line1, line2;
  
// Using Sparkfun GPS module
  myGNSS.checkUblox(); //See if new data is available. Process bytes as they come in.

  if(nmea.isValid() == true)
  {
    long latitude_mdeg = nmea.getLatitude();
    long longitude_mdeg = nmea.getLongitude();

    latI = latitude_mdeg / 1000000.;
    lonI = longitude_mdeg / 1000000.;
    
    latID = latitude_mdeg/1000000.;
    lonID = longitude_mdeg/1000000.;

    line1 = String("lat: ") + String(latI, 6);
    line2 = String("lng: ") + String(lonI, 6);
    
    mapGPS(); //update location on Blynk map

   // Update Blynk
    Blynk.virtualWrite(V12, latID);
    Blynk.virtualWrite(V13, lonID);

    nmea.clear(); // Clear the MicroNMEA storage to make sure we are getting fresh data
  }
  else
  {
    /*Serial.println("Waiting for fresh data");*/
  }

}
//This function gets called from the SparkFun u-blox Arduino Library
//As each NMEA character comes in you can specify what to do with it
//Useful for passing to other libraries like tinyGPS, MicroNMEA, or even
//a buffer, radio, etc.
void SFE_UBLOX_GNSS::processNMEA(char incoming)
{
  //Take the incoming char from the u-blox I2C port and pass it on to the MicroNMEA lib
  //for sentence cracking
  nmea.process(incoming);
}

// get, format current date and time from smartphone via Blynk RTC
void getTimeDate()
{
  currentTime = String(hour()) + ":" + minute() + ":" + second();
  currentDate = String(day()) + " " + month() + " " + year();
  /* Serial.print("Current time: ");
    Serial.println(currentTime);
    Serial.print("Current date: ");
    Serial.println(currentDate); */

  timeHack = currentDate + " " + currentTime;

}

// get PM data from Plantower sensor
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
  dustvalues1.PM2_5Val_atm = data.pm25_standard;
  dustvalues1.PM10Val_atm = data.pm100_standard;

  dustvalues1.PM01Val_cf1 = data.pm10_env;
  dustvalues1.PM2_5Val_cf1 = data.pm25_env;
  dustvalues1.PM10Val_cf1 = data.pm100_env;

  dustvalues1.Beyond03 = data.particles_03um;
  dustvalues1.Beyond05 = data.particles_05um;
  dustvalues1.Beyond1 =  data.particles_10um;
  dustvalues1.Beyond2_5 = data.particles_25um;
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
      dustvalues1.PM01Val_cf1 = 0.534 * dustvalues1.PM01Val_cf1 - .0844 * senHumid + 5.604;
      dustvalues1.PM2_5Val_cf1 = 0.534 * dustvalues1.PM2_5Val_cf1 - .0844 * senHumid + 5.604;
      dustvalues1.PM10Val_cf1 = 0.534 * dustvalues1.PM10Val_cf1 - .0844 * senHumid + 5.604;
      break;
    case 3: // LRAPA
      Serial.println("LRAPA selected");
      dustvalues1.PM01Val_cf1 = 0.5 * dustvalues1.PM01Val_cf1 - .68;
      dustvalues1.PM2_5Val_cf1 = 0.5 * dustvalues1.PM2_5Val_cf1 - .68;
      dustvalues1.PM10Val_cf1 = 0.5 * dustvalues1.PM10Val_cf1 - .68;

      break;
    case 4: // AQndU
      Serial.println("AQandU selected");
      dustvalues1.PM01Val_cf1 = 0.778 * dustvalues1.PM01Val_cf1 + 2.65;
      dustvalues1.PM2_5Val_cf1 = 0.778 * dustvalues1.PM2_5Val_cf1 + 2.65;
      dustvalues1.PM10Val_cf1 = 0.778 * dustvalues1.PM10Val_cf1 + 2.65;

      break;
    default:
      Serial.println("Unknown item selected");
  } // switch

 // write particulate values to Blynk
  Blynk.setProperty(V1, "label", "PM2.5 (" + conversion + ")"); // update PM2.5 label
  Blynk.virtualWrite(V0, dustvalues1.PM01Val_cf1);  // using cf1 values
  Blynk.virtualWrite(V1, dustvalues1.PM2_5Val_cf1);
  Blynk.virtualWrite(V2, dustvalues1.PM10Val_cf1);
  Blynk.virtualWrite(V34, dustvalues1.Beyond03);
  Blynk.virtualWrite(V35, dustvalues1.Beyond05);
  Blynk.virtualWrite(V36, dustvalues1.Beyond1);
  Blynk.virtualWrite(V37, dustvalues1.Beyond2_5);
  Blynk.virtualWrite(V38, dustvalues1.Beyond5);
  Blynk.virtualWrite(V39, dustvalues1.Beyond10);

  // calculate AQI and set colors for Blynk
  airQualityIndex = calculate_US_AQI25(dustvalues1.PM2_5Val_cf1); // use PM2.5 ATM for AQI calculation
  calcAQIValue(); // set AQI colors

  // Update colors/labels for AQI gauge on Blynk
  Blynk.setProperty(V6, "color", newColor); // update AQI color
  Blynk.setProperty(V6, "label", newLabel);  // update AQI label
  Blynk.virtualWrite(V6, gaugeValue);  // update AQI value

  doCO2Prediction(); // do LR prediction for CO2 (BETA!)
}

// get CO2, temp and humidity values 
void get_SCD4X() {

 uint16_t error;

  char errorMessage[256];

  // Read Measurement
 uint16_t co22; // co2 is a global var
 float temperature;
 float humidity;
  const float dCorr = 5.0; // est. temp correction due to interior case heating
  error = scd4x.readMeasurement(co22, temperature, humidity);
  if (error) {
    Serial.print("Error trying to execute readMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else if (co22 == 0) {
    Serial.println("Invalid sample detected, skipping.");
  } else 
  { // good to go!
    Serial.print("Co2:");
    Serial.print(co22);
    Serial.print("\t");   
    Serial.print("Temperature:");
    Serial.print(temperature * 1.8 + 32);
   // Serial.print(temperature * 175.0 / 65536.0 - 45.0);
    Serial.print("\t");
    Serial.print("Humidity:");
   // Serial.println(humidity * 100.0 / 65536.0);
    Serial.println(humidity);
    senTemp = temperature * 1.8 + 32; // to deg F
    senHumid = humidity;
    co2 = co22;

   // write values to Blynk app
    Blynk.virtualWrite(V31, co2); // CO2 in PPM
    Blynk.virtualWrite(V3, temperature * 1.8 + 32 - dCorr); // send F temp to Blynk
    Blynk.virtualWrite(V4, humidity ); // send humidity to Blynk
  }

}

void doCO2Prediction() {
// Experimental function to attempt to predict CO2 from 10 recent values
// calc linear regression for x data points using Least Squares

  counter ++;
  lr.Data(co2);

  if (counter > 9) {
    // calculate Linear Regression of last 10 data points

    Serial.print(lr.Samples()); Serial.println(" Point Linear Regression Calculation...");
    Serial.print("Correlation: "); Serial.println(lr.Correlation());
    Serial.print("Values: "); lr.Parameters(values); Serial.print("Y = "); Serial.print(values[0], 4); Serial.print("*X + "); Serial.println(values[1], 4);
    Serial.print("Values: "); lr.Parameters(values); Serial.print(" a = "); Serial.print(values[0], 4); Serial.print(" b = "); Serial.println(values[1], 4);
    Serial.print("Degree(°): "); Serial.print(57.2957795 * atan(values[0]), 2); Serial.println(""); // convert rad to deg
    Serial.print("Time(s): "); Serial.print((1000 - values[1]) / values[0] * 5, 2); Serial.println("");

    lr.Samples();
    correlation = lr.Correlation();
    lr.Parameters(values);

    // calc forecast for each signal "phase": time until next "color"

    if (co2 < 800) trafficLight = "green";
    else if (co2 > 800 && co2 < 1000) trafficLight = "yellow";
    else trafficLight = "red";

    if (trafficLight == "green") {
      forecast = int((yellowLevel - values[1]) / values[0] * 5 / 60); // forecast time until 800 ppm, steps of 5s, in minutes
      Serial.println(yellowLevel); //
    }
    if (trafficLight == "yellow") {
      forecast = int((redLevel - values[1]) / values[0] * 5 / 60); // forecast time until 1000 ppm, steps of 5s, in minutes
      Serial.println(redLevel); //
    }
    Serial.print(correlation); //
    Serial.print(": y = "); Serial.print(values[0], 2); Serial.print("*x + "); Serial.println(values[1], 2); //
    Serial.println(trafficLight); //
    Serial.print("Time(min): "); Serial.print(forecast); Serial.println("");

    Blynk.virtualWrite(V45, values[0], 2);
    Blynk.virtualWrite(V46, values[1], 2);
    Blynk.virtualWrite(V47, correlation);
    Blynk.virtualWrite(V48, forecast);

    // Reset
    lr.Reset();
    counter = 0;
  }

  // if (correlation > 0.4 && forecast < 99) { // we've got a stable forecast and time < 99 min
  //   canvas.print(String(forecast));
  // } else {
  //   canvas.print("--");
}

// Retrieve battery voltage from ESP on BackpAQ device, send to Blynk/SmartPhone
// Note: cannot do this when WiFi is operating due to ADC1 conflict
void getBatteryVoltsBackpAQ() {

  bool batteryIsCritical = false;
  int int_battery = 0;
  int pct_battery = 0;
  const int MAX_ANALOG_VAL = 4095;
  const float MAX_BATTERY_VOLTAGE = 4.2; // Max LiPoly voltage of a 3.7 battery is 4.2
  pinMode(batteryPin, INPUT);

  int_battery = (int)(battery_K * (float)analogRead(batteryPin));

  int raw_battery = analogRead(batteryPin);
  float flt_battery = (raw_battery/4095.0);
  flt_battery *= 2;    // we divided by 2, so multiply back
  flt_battery *= 1.1; // reference voltage
  flt_battery *= 3.3;  // Multiply by 3.3V, our reference voltage
  Serial.print("VBat: " ); Serial.println(flt_battery);
  float batteryFraction = flt_battery / MAX_BATTERY_VOLTAGE;
  int batteryPct = batteryFraction * 100;
  Serial.println((String)"Raw:" + raw_battery + " Voltage:" + flt_battery + "V Percent: " + (batteryFraction * 100) + "%");
 
  // set battery indicator color based on voltage

  if (batteryPct >= 80) {
    batteryColor = "#009966"; // Green
  } else if (batteryPct > 70 && batteryPct < 80) {
    batteryColor = "#ffde33"; // Yellow
  } else if (batteryPct > 40 && batteryPct < 70) {
    batteryColor = "#ff9933"; // Orange  
  } else { // battery below 40%
    batteryColor = "#cc0033";  // Red
    Blynk.notify("Battery getting low...suggest you recharge soon.");
    batteryIsCritical = true;
  }

  // send to Blynk
   
  Blynk.virtualWrite(V20, flt_battery); // update battery level (%) indicator
  Blynk.setProperty(V21, "color", batteryColor) ;  // update battery color
  Blynk.virtualWrite(V21, batteryPct); // update battery level (%) indicator
  //Blynk.virtualWrite(V49, int_battery); // for debug

  // test for low battery...
  /* if (batteryIsCritical) {
     Blynk.notify("Battery level critical, shutting down...time to charge!");
    goToDeepSleep(); // go to sleep
    } */
}
void initGPS()
 {
 // set up GPS
if (myGNSS.begin() == false)
  {
    Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring."));
    //while (1);
  }
  myGNSS.setI2COutput(COM_TYPE_UBX | COM_TYPE_NMEA); //Set the I2C port to output both NMEA and UBX messages
  myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT); //Save (only) the communications port settings to flash and BBR
  myGNSS.setProcessNMEAMask(SFE_UBLOX_FILTER_NMEA_ALL); // Make sure the library is passing all NMEA messages to processNMEA
  myGNSS.setProcessNMEAMask(SFE_UBLOX_FILTER_NMEA_GGA); // Or, we can be kind to MicroNMEA and _only_ pass the GGA messages to it
 } 
void goToDeepSleep()
{
  Serial.println("Going to sleep...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  //btStop();

  // adc_power_off();
  esp_wifi_stop();
  // esp_bt_controller_disable();

  // Configure the timer to wake us up!
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME * 60L * 1000000L);

  // Go to sleep! Zzzz
  Serial.println("Going to sleep!");
  esp_deep_sleep_start();
  delay(2000); // make sure we go to sleep
}
 void initCO2()
 {

  uint16_t error;
  char errorMessage[256];
  
  scd4x.begin(Wire); // start the SCD4X sensor

  // stop potentially previously started measurement
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
    Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }
  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;
  error = scd4x.getSerialNumber(serial0, serial1, serial2);
  if (error) {
    Serial.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    printSerialNumber(serial0, serial1, serial2);
  }
   // Start CO2 Measurement
  error = scd4x.startPeriodicMeasurement();
  if (error) {
    Serial.print("Error trying to execute startPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }
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

//char * boolstring( _Bool b ) { return b ? true_a : false_a; }

void setup()
{

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  
  Serial.begin(115200); // console

  delay(100);

  BlynkEdgent.begin();

  // Start OLED with Heltec ESP32Kit default pins
  // Wire.begin(4, 15); // HELTEC: scl 15 sda 4 (this assigns I2C pins for other sensors as well)
  // OLED.begin(SSD1306_SWITCHCAPVCC, 0x3C, &Wire); // HELTEC init OLED
  OLED.begin(0x3C, true); // init OLED
  
  // display name, version, copyright
  OLED.clearDisplay();
  OLED.setRotation(1);                               // rotate screen to fit new case (NOTE: THIS IS FOR NEW 94H VERSION ONLY)
  OLED.setTextSize(1);                               // Set OLED text size to small
  OLED.setTextColor(WHITE, BLACK);                   // Set OLED color to White
  OLED.setCursor(0, 0);                              // Set cursor to 0,0
  OLED.print("SSV BackpAQ ");
  OLED.println(version);
  // OLED.println("(c) 2022 SSV");                   // Choose to not display for now
  OLED.display();                                    // Update display
  delay(2000);

  if (!Blynk.connected()) {
    Serial.print("Blynk not connected!");
  }

  // OK, we're connected to Blynk!!

  DEBUG_PRINTLN("BLYNK Connected");
  OLED.println("BLYNK Connected!");

  Serial.println("Starting up...");

  Wire.begin(); // crank up I2C Bus

  initGPS(); // Initialize GPS sensor
  OLED.println("GPS Initialized");
  OLED.display();
 
 // Set up PM sensor
 // if (! aqi.begin_I2C(&BMEWire)) {      // HELTEC:  connect to the PMS sensor over I2C
 if (! aqi.begin_I2C(&Wire)) {      // connect to the PMS sensor over I2C
    //if (! aqi.begin_UART(&Serial1)) { // HELTEC connect to the sensor over hardware serial
    //if (! aqi.begin_UART(&pmSerial)) { // HELTEC connect to the sensor over software serial
    Serial.println("Could not find PM sensor!");
  } else {
    Serial.println("PM25 found!");  // ok we're good
  }

  initCO2(); // Initialize CO2 sensor

  // initialize the LED digital pin as an output.
  pinMode(PIN_LED, OUTPUT);

  Serial.setTimeout(1500);
  privateMode = 0;
  noBlynk = false;

  // update OLED display with connection info...

  OLED.print("Connect to: ");
  OLED.println(configStore.wifiSSID); // from Blynk Edgent
  OLED.display();

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

  //-------------------------------------------------------------------------
  // Set timed functions for Blynk
  // Apparently important to stagger the times so as not to stack up timer
  // Note: 10 intervals/timer. If need to add more, get another timer
  //-------------------------------------------------------------------------
  timer.setInterval(10000L,getAQIData); // get data from PM sensor
  timer.setInterval(5000L, get_SCD4X); // get CO2, temp & humidity from data SCD4x
  timer.setInterval(1010L, getTimeDate); // get current time, date from RTC
  timer.setInterval(2000L, getGPS); // get GPS coords.
  timer.setInterval(3100L, mapGPS); // update GPS position on Map screen 
 // timer.setInterval(60000L, updateWiFiSignalStrength); // get and update WiFi signal strength
  timer.setInterval(1050L, updateOLED); // update local display
  timer.setInterval(30000L, getBatteryVoltsBackpAQ); // get battery level from BackpAQ 
  timer.setInterval(thingspeak_Send_Interval, SendToThingspeak); // update sensor values to Thingspeak every Send_Interval seconds
  timer.setInterval(thingspeak_Send_Interval2, getSounds); // get noise values from sound sensor
  timer.setInterval(1500L,check_status); // Heartbeat
  timer.setInterval(800L, blinkLedWidget);

} // setup

void loop() // keep loop lean & clean!!
{
//  Blynk.run();
  BlynkEdgent.run();
  timer.run();
}
