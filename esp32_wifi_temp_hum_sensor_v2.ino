/*
 * Transfer sensor data from the DHT22 temperature and humidity sensor to InfluxDB
 * and caches results, if the network is not awailable yet.
 * 
 * 
 * Written by Gergő Várfalvi based on examples taken from various ESP32 tutorials.
 */


// OLED display
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// DHT22 sensor
#include<DHT.h>

// ESP LOG for timestamp
#include<esp_log.h>
#include<time.h>

// ESP sleep
#include<esp_sleep.h>

// HTTP Client for data transfer
#include<HTTPClient.h>

// WiFi
#include<Wifi.h>

// string preparation and cache
#include <string>
#include <sstream>
#include <QList.h>


// WiFi Auth
#define WIFI_SSID "esp32_test"
#define WIFI_PASSWORD "esp32_test"

#define WIFI_RECONNECT_TRY_MAX 10

// InfluxDB URL
#define INFLUXDB_REST_SERVICE_URL "http://192.168.1.111:8086/write?db=home"

// Timeserver URL
#define TIMESERVER "pool.ntp.org"

// Sleeptime between measurements
#define SLEEP_TIME 60*1000  // in ms

// Transfer after this number of items have been collected
#define DATA_TRANSFER_BATCH_SIZE 10

// Sensor
#define DHT_TYPE DHT11

// OLED size
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display heigth, in pixels

#define SERIAL_DEBUG 1


#define SETUP_DELAY 3000U  // delay between periferia test, in ms

#define TIME_TO_SLEEP 10
#define uS_TO_S_FACTOR 1000000

// Structures
struct Measurement
{
  float temperature;
  float humidity;
  time_t time;
};

struct mArray
{
  unsigned int num_of_data;
  Measurement measurements[DATA_TRANSFER_BATCH_SIZE];
};


// Global Variables
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool rtc_is_set = false;
struct timeval saved_rtc_time;

RTC_DATA_ATTR struct mArray rct_storage = {
                  0,    // num_of_data
                  {{0}}
                  };   // Queue, store it in RTC to keep data between sleeps
HTTPClient http;                                      // HTTPClient user for transfering data to Influx

// Device instances
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT HT(4, DHT_TYPE);


void setup() {
  if(rtc_is_set == true)
  {
    gettimeofday(&saved_rtc_time, NULL);
  }
  // put your setup code here, to run once:
  #if SERIAL_DEBUG == 1
  Serial.begin(115200);
  Serial.println("DHT22 to DB startup");
  #endif
  
  // Increment boot number
  bootCount++;
  #if SERIAL_DEBUG == 1
  Serial.println(bootCount);
  #endif
  
  // Start display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    #if SERIAL_DEBUG == 1
    Serial.println(F("SSD1306 allocation failed"));
    #endif
    for(;;);
  }
  display.clearDisplay();
  display.display();

  test_display();

  
  // Disable Bluetooth for power saving
  btStop();

  //initialize DTH
  HT.begin();

  if(bootCount == 1)
  { 
    // testing sensor
    test_ht();
    
    // Connect and test WiFi
    connectToNetwork();
  
    // Obtaining time from time server...
    setTime();
  }


  // Handle wakeup event depending on the reason of the wakeup
  handle_wakeup();
  
  // Setting up deep sleep
  // Set timer to 10 seconds
  int64_t saved_time_us = (int64_t)saved_rtc_time.tv_sec * 1000000L + (int64_t)saved_rtc_time.tv_usec;
  struct timeval rtc_now;
  gettimeofday(&rtc_now, NULL);
  int64_t sleep_time_us = (TIME_TO_SLEEP * uS_TO_S_FACTOR) - ( ((int64_t)rtc_now.tv_sec * 1000000L + (int64_t)rtc_now.tv_usec) - saved_time_us ) ;
  esp_sleep_enable_timer_wakeup(sleep_time_us);
  
  Serial.print("Set ESP32 to sleep for every ");
  Serial.print(int(sleep_time_us));
  Serial.println(" microseconds.");

  // Go to sleep now
  esp_deep_sleep_start();
  
}

void loop() {
  // put your main code here, to run repeatedly:

}

void test_display()
{
  if(bootCount == 1)
  {
    display.display();
    delay(SETUP_DELAY);
  }
}

void test_ht()
{
  float test_temp = HT.readTemperature();
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.setTextSize(1);
  display.print(test_temp);
  display.display();

  #if SERIAL_DEBUG == 1
  Serial.println("Temperature: ");
  Serial.println(test_temp);
  #endif
  delay(SETUP_DELAY);
}

bool connectToNetwork()
{
  int count = 0;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  //delay(500);
  //WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED && count < WIFI_RECONNECT_TRY_MAX)
  {
    delay(500);
    Serial.println("Establishing connection to WiFi..");
    count++;
  }

  #if SERIAL_DEBUG == 1
  Serial.println("Connected to network");
  #endif
  
  display.setCursor(0, 10);
  display.print("Connected to network, PI: ");
  display.print(WiFi.localIP());
  display.display();
  delay(SETUP_DELAY);

  return WiFi.status() == WL_CONNECTED;
}

void setTime()
{
  #if SERIAL_DEFINE == 1
  Serial.println("Obtaining time from time server...");
  #endif
  
  configTime(0,0, TIMESERVER);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo))
  {
    Serial.println("ERROR: Failed to obtain time!");
    while(1);
  }
  else
  {
    rtc_is_set = true;
    #if SERIAL_DEFINE == 1
    Serial.println("Done...");
    #endif
  }
  
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  display.setCursor(0, 30);
  display.print("Time set: ");
  display.print(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  display.display();
}

// Function that prints the reason by which ESP32 has been awaken from sleep
void handle_wakeup()
{
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason)
  {
    case 1:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;

    case 2:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case 3:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      //Serial.println("Wakeup caused by timer");
      // Make measurement
      struct Measurement measurement;
      measurement.temperature = HT.readTemperature();
      measurement.humidity = HT.readHumidity();
      time(&measurement.time);

      // Add to queue
      //queue.push_back(measurement);
      add_to_marray(&measurement);
      show_all_storage();
      // If data in storage reaches 10 measurements attempt sending via wifi
      if( rct_storage.num_of_data == DATA_TRANSFER_BATCH_SIZE )
      {
        show_all_storage();
        transferData();
        Serial.println("After transfer");
        show_all_storage();
      }
      
      break;
    case 5:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.println("Wakeup was not caused by deep sleep");
      break;
  }
}

void add_to_marray(struct Measurement *m)
{
  if(rct_storage.num_of_data < DATA_TRANSFER_BATCH_SIZE)
  {
    rct_storage.measurements[rct_storage.num_of_data].temperature = m->temperature;
    rct_storage.measurements[rct_storage.num_of_data].humidity = m->humidity;
    rct_storage.measurements[rct_storage.num_of_data].time = m->time;
    rct_storage.num_of_data++;
  }
}

void transferData()
{
  // Connect to Wifi
  if(!connectToNetwork())
  {
    Serial.println("transferData unsuccessfull network connection");
    return;
  }

  // Transfer data
  Serial.println("Transfering data to InfluxDB");
  transferBatch();

  // disable Wifi
  WiFi.mode(WIFI_OFF);
}

boolean transferBatch() {
    http.begin(INFLUXDB_REST_SERVICE_URL);
    http.addHeader("Content-type", "text/plain");
    struct Measurement m;
    std::ostringstream oss;
    
    for(int i = 0; i < DATA_TRANSFER_BATCH_SIZE; i++ )
    {
      m = rct_storage.measurements[i];
      oss << "dht11,sensor=dht11,host=" << WiFi.macAddress().c_str() <<
          " temperature=" << m.temperature <<
          ",humidity=" << m.humidity<<
          " " << m.time << "000000000\n"; // we need to add 9 zeros to the time, since InfluxDB expects ns.
    }
    //Serial.println("oss:");
    //Serial.println(oss.str().c_str());
    int httpResponseCode = http.POST(oss.str().c_str());
    http.end(); 

    if (httpResponseCode != 204)
    {
      Serial.println("Error on sending POST");
      Serial.print("Response code: ");
      Serial.println(httpResponseCode);
      return false;
    } 


    //Serial.println("bootCount: before clearing");
    //Serial.println(bootCount);
    // clear the transferred items from the queue
    //memset(rct_storage, 0, sizeof(insigned int) + sizeof(struct Measurement)*10);
    for (int i=0; i < DATA_TRANSFER_BATCH_SIZE; i++) {
      //queue.clear(i);
      rct_storage.measurements[i].temperature = 0;
      rct_storage.measurements[i].humidity = 0;
      rct_storage.measurements[i].time = 0; 
    }
    rct_storage.num_of_data = 0;
    //Serial.println("bootCount: after clearing");
    //Serial.println(bootCount);
    return true;



    /*
    
    int endIndex = rct_storage.num_of_data;
    if (endIndex < 0) {
      return false;
    }
    int startIndex = endIndex - DATA_TRANSFER_BATCH_SIZE;
    if (startIndex < 0) {
      startIndex = 0;
    }
    
    http.begin(INFLUXDB_REST_SERVICE_URL);
    http.addHeader("Content-type", "text/plain");
    std::ostringstream oss;
    for (int i=endIndex; i>=startIndex; i--) {
      struct Measurement m = rct_storage.measurements[i];
      oss << "dht11,sensor=dht11,host=" << WiFi.macAddress().c_str() <<
          " temperature=" << m.temperature <<
          ",humidity=" << m.humidity<<
          " " << m.time << "000000000\n"; // we need to add 9 zeros to the time, since InfluxDB expects ns.
    }
    int httpResponseCode = http.POST(oss.str().c_str());
    http.end(); 

    if (httpResponseCode != 204) {
      Serial.println("Error on sending POST");
      return false;
    } 

    Serial.println("bootCount: before clearing");
    Serial.println(bootCount);
    // clear the transferred items from the queue
    for (int i=endIndex; i>=startIndex; i--) {
      //queue.clear(i);
      rct_storage.measurements[i].temperature = 0;
      rct_storage.measurements[i].humidity = 0;
      rct_storage.measurements[i].time = 0; 
    }
    rct_storage.num_of_data = 0;
    Serial.println("bootCount: after clearing");
    Serial.println(bootCount);
    return true;
    */
}

void show_all_storage()
{
  #if SERIAL_DEBUG == 1
  Serial.println("######################################");
  Serial.println("RCT storage content:");
  Serial.print("num_of_data: ");
  Serial.println(rct_storage.num_of_data);
  Serial.println("Measurement elemets:");
  
  for(int i = 0; i < DATA_TRANSFER_BATCH_SIZE; i++)
  {
    Serial.print("i = ");
    Serial.println(i);
    Serial.print("Temperature = ");
    Serial.println(rct_storage.measurements[i].temperature);
    Serial.print("Humidity = ");
    Serial.println(rct_storage.measurements[i].humidity);
        Serial.print("Time = ");
    Serial.println(rct_storage.measurements[i].time);
    Serial.println("");
  }
  #endif
}
