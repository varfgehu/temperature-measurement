# Wireless temperature and humidity measurement and collection

Transfer sensor data from the DHT22 temperature and humidity sensor to InfluxDB running on a Raspberry Pi
The device is optimalised for power consumption for battery use.
Sending the controller to Deep Sleep state after a measurement.
Temperature, humidity and time data are stored in RTC memory.
Controller wakes up in every minute.
Data transmission after 10 measurements via WiFi.
WiFi is only turned on for the time of the data transmission.
Display is only used in start up, for checking purposses: testing measurement, WiFi connection, time setting.
Display is for future development.

Used devices:
    - ESP32-WROOM-32D microcontroller
    - DHT22 temperature and humidity sensor
    - SSD1306 128 x 64 OLED display (I2C)


Port configuration:
    - DHT22 1-Wire comm --> ESP32 Port 4
    - SSD1326 OLED display SCL pin --> ESP32 Port 21
    - SSD1326 OLED display SDA pin --> ESP32 Port 22
(Make sure to connect 5V and GND for the sensor and the display)

Make sure to set the following Macro definitions for your own needs:
    - WIFI_SSID
    - WIFI_PASSWORD
    - INFLUXDB_REST_SERVICE_URL