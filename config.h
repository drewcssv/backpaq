//// BackpAQ    V0.1                                /////
//// (c) 2019 Sustainable Silicon Valley
//// Written by A. L. Clark 

// Blynk Parameters
char auth[] = "r6AOkB99mJnVjGzD7V7nZMI6GTveDcbY";  // Blynk Auth token

//////////////////////////////////////////////////////
///////// Replace Wifi connection information by //////
///////// your own values                       //////
//////////////////////////////////////////////////////

char ssid[] = "artemis"; // WiFi SSID
char pass[] = "opaopaopa";  // WiFi Password
//char ssid[] = "Poway House"; // WiFi SSID
//char pass[] = "6197172939";  // WiFi Password
//char ssid[] = "34033471";
//char pass[] = "5051B623F35C7DA4BA7DA5698E";
//char ssid[] = "Drew's 5s iPhone";
//char pass[] = "esjt938qrh9f0";

// Particle Sensor Parameters
#define MIN_WARM_TIME 35000 //warm-up period requred for sensor to enable fan and prepare air chamber
const int MAX_WIFI_TRY = 4;
const int   SLEEP_TIME = 5 * 60 * 1000; // 5 minutes sleep time

// Thingspeak

  //////////////////////////////////////////////////////
  ///////// if you want to push data on thingspeak /////
  ///////// Replace API keys by your own values ////////
  //////////////////////////////////////////////////////
  const char* ThingSpeak_API_Server = "api.thingspeak.com";
  /*  Create or use a ThingSpeak account. Data are sent only every minute, so the free plan must be enough.
   *  Create two channels with the following fields collection
   *  The names of the field are not important, but the order of the fields should be defined as below
  // PM Channel:
  // Field1= CF1 PM 1.0 (μg/m3)
  // Field2= CF1 PM 2.5 (μg/m3)
  // Field3= CF1 PM 10 (μg/m3)
  // Field4= Atm PM 1.0 (μg/m3)
  // Field5= Atm PM 2.5 (μg/m3)
  // Field6= Atm PM 10 (μg/m3)
  // Field7= Humidity (%)
  // Field8= Temperature (°C or F)*/
  //const String ThingSpeak_PM_APIKey = "QFZ9HV72F6KDY3T7"; // WRITE key #1  channel 891066

  /*
  // Raw Dust Channel:
  // Field1= Particule count 0.3 µm per 0.1 L
  // Field2= Particule count 0.5 µm per 0.1 L
  // Field3= Particule count 1.0 µm per 0.1 L
  // Field4= Particule count 2.5 µm per 0.1 L
  // Field5= Particule count 5.0 µm per 0.1 L
  // Field6= Particule count 10 µm per 0.1 L
  // Field7= US AQI */
  //const String ThingSpeak_Raw_APIKey = "2DQRPYCORQRT9T2D"; // WRITE key #2  channel 896665
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
