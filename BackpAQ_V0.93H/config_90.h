//// BackpAQ    V0.89                                /////
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
const char* version = "V0.93H"; // for Heltec ESP32 WiFi Kit V2 w/OLED
// Blynk Parameters
char blynk_token[34] = "YOUR_BLYNK_TOKEN"; // will be filled in at WiFi config time and stored in eeprom
char Blynk_server[] = "blynk-cloud.com";  // URL for Blynk Cloud Server
//char auth[] = "xifSaUx1c679vHhDYYAB7KG8I0O8HiFc";  // This is Drew's private token. Insert your Blynk private token "auth" here or use config portal
#define BLYNK_PRINT Serial
#define BLYNK_MAX_READBYTES 2048
#define blynk_map_index 8
//////////////////////////////////////////////////////
///////// Default Wifi connection information  /////// 
////// Not currently used as we're using WiFiManager Config /
//////////////////////////////////////////////////////
char ssid[] = "artemis"; // WiFi SSID
char pass[] = "opaopaopa";  // WiFi Password
//char ssid[] = "Drew's 5s iPhone";
//char pass[] = "esjt938qrh9f0";

/////////////////////////////////////// Particle Sensor Parameters //////////////////////////////////////////
#define MIN_WARM_TIME 35000 //warm-up period requred for sensor to enable fan and prepare air chamber
const int pmSampleTime = 5000; //delay until next PM sample
const int MAX_WIFI_TRY = 4;
#define TIMEOUT 10000 //Microseconds to wait for PMS do wakeup 
const int   SLEEP_TIME = 5 * 60 * 1000; // 5 minutes sleep time
/////////////////////////////////  Misc Sensor Parameters ///////////////////////////////////////////////////
#define BME280_ADDRESS 0x76 // I2C address for temp/humidity sensor
#define BATT_LEVEL A0 // analog pin to measure battery voltage
////////////////////////////////////////// Thingspeak  //////////////////////////////////////////////////////
  int thingspeak_Send_Interval = 40000L; // Send data to Thingspeak roughly every 40 seconds
  const char* ThingSpeak_API_Server = "api.thingspeak.com";

  //  Create two Thingspeak channels with the following fields collection
  //  The names of the field are not important, but the order of the fields should be defined as below
  ////// Replace with your Thingspeak channels and API keys in config portal  ////////////////////////////////////////////////
  
  // BackpAQ PM Data 1:
  //unsigned long thingspeak_A_channel = 891066;
  //char thingspeak_A_channel[8] = "891066";
  char thingspeak_A_channel[10] = "xxxxxxx";
  unsigned long thingspeak_A_channel_i;
 // char thingspeak_A_key[20] = "A9L9601U1FROH6BE";
  char thingspeak_A_key[20] = "xxxxxxxxxxxxxxxx";
  const char * thingspeak_A_key_c = "A9L9601U1FROH6BE";

  // Field1= Atm PM 1.0 (μg/m3)
  // Field2= Atm PM 2.5 (μg/m3)
  // Field3= Atm PM 10 (μg/m3)
  // Field4= Temperature (deg F)
  // Field5= Humidity (%)
  // Field6= Latitude (deg)
  // Field7= Longitude (deg)
  // Field8= AQI (US Standard)

  // BackpAQ PM Data 2:
  //unsigned long thingspeak_B_channel = 759545;
  const char * thingspeak_B_key_c = "6C1HZQR08LSTF5AG";
  unsigned long thingspeak_B_channel_i;
  //char thingspeak_B_channel[8] = "759545";
  char thingspeak_B_channel[10] = "xxxxxxx";
  //char thingspeak_B_key[20] = "6C1HZQR08LSTF5AG";
  char thingspeak_B_key[20] = "xxxxxxxxxxxxxxxx";
 
  // Field1= Particulate count 0.1 µm per 0.1 L
  // Field2= Particulate count 0.3 µm per 0.1 L
  // Field3= Particulate count 0.5 µm per 0.1 L
  // Field4= Particulate count 1.0 µm per 0.1 L
  // Field5= Particulate count 2.5 µm per 0.1 L
  // Field6= Particulate count 10 µm per 0.1 L
  // Field7= TVOC
  // Field8= eCO2

  // BackpAQ PM Data 3: Tracks!
  //unsigned long thingspeak_B_channel = 759545;
  const char * thingspeak_C_key_c = "8TT8NC0QHF6SK2B1";
  unsigned long thingspeak_C_channel_i = 1151057;
  //char thingspeak_B_channel[8] = "1151057";
  char thingspeak_C_channel[14] = "1151057";
  //char thingspeak_B_key[20] = "8TT8NC0QHF6SK2B1";
  char thingspeak_C_key[20] = "8TT8NC0QHF6SK2B1";
 
  // Field1= Particulate count 0.1 µm per 0.1 L
  // Field2= Particulate count 0.3 µm per 0.1 L
  // Field3= Particulate count 0.5 µm per 0.1 L
  // Field4= Particulate count 1.0 µm per 0.1 L
  // Field5= Particulate count 2.5 µm per 0.1 L
  
///////////////  Following are AQI Calculation Parameters //////
////////////////////////////////////////////////////////////////
// Define AQI Color Scheme (according to US EPA standard)
//////////////////////////////////////////////////////////////
#define AQI_GREEN     "#009966" // Safe
#define AQI_YELLOW    "#ffde33" // Moderate
#define AQI_ORANGE    "#ff9933" // Unhealthy for some
#define AQI_RED       "#cc0033" // Unhealthy
#define AQI_PURPLE    "#660099" // Very Unhealthy
#define AQI_MAROON    "#7e0023" // Hazardous
////////////////////////////////////////////////////////////////
//var aqi_colors = [
//    '009966',
//    'ffde33',
//    'ff9933',
//    'cc0033',
//    '660099',
//    '7e0023',
///  
//];
//var aqi_breakpoints = [
//    [0, 50],
//    [51, 100],
//    [101, 150],
//    [151, 200],
//    [201, 300],
//    [301, 400],
//    [401, 500],
//];
//var pm25_breakpoints = [
//    [0.0, 12.0],
 //   [12.1, 35.4],
///   [35.5, 55.4],
//    [55.5, 150.4],
//    [150.5, 250.4],
 //   [250.5, 350.4],
 //   [350.5, 500.0],
///];
