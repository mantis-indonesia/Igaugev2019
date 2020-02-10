/* WEMOS ESP I-GAUGE
   BOARD : WEMOS D1 R2 MINI
   mantisid.id/project/osh_2019/api/product/osh_data_c.php
   http://mantisid.id/project/osh_2019/api/product/osh_data_r.php
   
   HISTORY:
   4-2-2019
   FIRST BUILD BY HOLLANDA
   CONFIGURE RTC DS3231  - LIBRARY RTCLIB
   MICRO SD CARD - SDFAT
   ADS1115 - ADS1015
   DISPLAY USING OLED SSD1306 128X64 - ADAFRUIT SSD1306

*/

//LIBRARY
#include "SdFat.h"
#include <Wire.h>
#include "RTClib.h"
#include <Adafruit_ADS1015.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <math.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager


IPAddress    apIP(10, 10, 10, 1);
String sn="0002";
String hotspot;

#define DHTPIN            D3
#define DHTTYPE           DHT22     // DHT 11 
#define oled 0x3c
#define ads_addr 0x48
#define cs D8
#define panjang 128
#define lebar 64
#define pres 1
#define button 2
#define tegangan 0

//OLED
#define OLED_RESET 0
Adafruit_SSD1306 display(OLED_RESET);
int16_t posx;
int16_t posy;
uint16_t w, h;

//DHT
DHT dht(DHTPIN, DHTTYPE);

//RTC DS3231
RTC_DS3231 rtc;
DateTime ESPtime;
DateTime sekarang;

//SD CARD
SdFat SD;
File file;
String filename;

//ADS1115
Adafruit_ADS1115 ads(ads_addr);

//server
String server = "http://www.mantisid.id";
String url = "/project/osh_2019/api/product/osh_data_c.php";
int httpCode;

//GLOBAL VARIABLE
int i, tahun;
char sdcard[200];
byte a, b, interval;
int burst;
byte bulan, hari, jam, menit, detik;
char str[13];
unsigned long reads = 0; //pressure
int16_t reads1 = 0; //pressure
unsigned long waktu;
float offset = 0.0;
float tekanan, temp, humid, volt;
float flon, flat;
String network, y;

//flag for saving data
bool shouldSaveConfig = false;

void setup() {
  //initialisation
  Serial.begin(9600);

  //WELCOME SCREEN
  Serial.println(F("\r\nWIFI IGAUGE V 2019"));
  Serial.flush();
  display.begin(SSD1306_SWITCHCAPVCC, oled);
  display.clearDisplay();
  // display.display();
  // delay(100);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  display.getTextBounds(F("USAID - PDAM"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 0);
  display.println(F("USAID - PDAM"));
  display.getTextBounds(F("WIFI OSH"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 20);
  display.println(F("WIFI OSH"));
  display.getTextBounds(F("WATER PRESSURE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 30);
  display.println(F("WATER PRESSURE"));
  display.getTextBounds(F("SENSOR DEVICE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 40);
  display.println(F("SENSOR DEVICE"));
  display.getTextBounds(F("2019"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 50);
  display.println(F("2019"));
  display.display();
  delay(4000);

  //RTC INIT
  hapusmenu(17, 64);
  display.setTextColor(WHITE, BLACK);
  display.getTextBounds(F("INIT CLOCK"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT CLOCK"));
  display.display();
  delay(2000);

  if (!rtc.begin() || rtc.lostPower()) {
    Serial.println("Couldn't find RTC");
    while (1) {
      display.getTextBounds(F("ERROR!! CONTACT CS."), 0, 0, &posx, &posy, &w, &h);
      display.setCursor((128 - w) / 2, 40);
      display.println(F("ERROR!! CONTACT CS."));
      display.display();
    }
  }

  for (i = 1; i < 4; i++) {
    ESPtime = rtc.now();
    filename = String(ESPtime.month()) + '/' + String(ESPtime.day()) + '/' + String(ESPtime.year()) + ' ' + String(ESPtime.hour()) + ':' + String(ESPtime.minute()) + ':' + String(ESPtime.second());
    Serial.println(filename);
    display.getTextBounds(F("yyyy/mm/dd hh:mm:ss"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.print(ESPtime.year());
    display.print("/");
    lcd2digits(ESPtime.month());
    display.print("/");
    lcd2digits(ESPtime.day());
    display.print(" ");
    lcd2digits(ESPtime.hour());
    display.print(":");
    lcd2digits(ESPtime.minute());
    display.print(":");
    lcd2digits(ESPtime.second());
    display.display();
    delay(1000);
  }

  //SD INIT
  hapusmenu(17, 64);
  display.getTextBounds(F("init SD Card..."), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT SD Card..."));
  display.display();
  delay(1000);
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);
  delay(200);
  if (!SD.begin(cs)) {
    while (1) {
      Serial.println(F("SD init error!!!"));
      display.getTextBounds(F("SD Card Error!!!"), 0, 0, &posx, &posy, &w, &h);
      display.setCursor((128 - w) / 2, 40);
      display.println(F("SD Card Error!!!"));
      display.display();
      delay(5000);
    }
  }

  Serial.println(F("SD CARD INIT OK!"));
  Serial.flush();
  display.getTextBounds(F("SD Card OK!"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.println(F("SD Card OK!"));
  display.display();
  delay(3000);

  //GET CONFIG TXT
  hapusmenu(17, 64);
  display.getTextBounds(F("CONFIGURATION FILE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("CONFIGURATION FILE"));
  display.display();
  delay(1000);
  configs();
  delay(2000);
  //Configuration file extraction procedur
  //---------------------------------------------------------------

  //DISPLAY BURST INTERVAL
  hapusmenu(40, 64);
  display.getTextBounds(F("BURST SAMPLING"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.print(F("BURST SAMPLING"));
  display.getTextBounds(F("10 SECONDS"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 52);
  display.print(burst);
  if (burst < 2) {
    display.print(F(" SECOND"));
  }
  else display.print(F(" SECONDS"));
  display.display();
  delay(2000);
  //---------------------------------------------------------------
  //DISPLAY DATA INTERVAL
  hapusmenu(40, 64);
  display.getTextBounds(F("INTERVAL DATA"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.print(F("INTERVAL DATA"));
  display.getTextBounds(F("10 MINUTES"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 52);
  display.print(interval);
  if (interval == 1) {
    display.print(F(" MINUTE"));
  }
  if (interval > 1) {
    display.print(F(" MINUTES"));
  }
  display.display();
  delay(2000);
  //---------------------------------------------------------------

  //ADS initialization
  Serial.println(F("INIT ADS1115"));
  Serial.flush();
  ads.begin();
  hapusmenu(17, 64);
  display.getTextBounds(F("INIT PRESSURE SENSOR"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT PRESSURE SENSOR"));
  display.display();
  delay(1000);
  display.getTextBounds(F("PRESSURE SENSOR READY"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.println(F("PRESSURE SENSOR READY"));
  display.display();
  delay(2000);
  /*
    //check water pressure sensor connected or not
    Serial.println(F("CEK PRESSURE SENSOR"));
    Serial.flush();
    hapusmenu(17, 64);
    display.getTextBounds(F("CEK PRESSURE SENSOR"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 20);
    display.println(F("CEK PRESSURE SENSOR"));
    display.display();
    delay(1000);
    //cek via ads1115
    reads = digitalRead(connectPres);

    Serial.println(reads);
    if (reads == 1) {
      Serial.println("pressure sensor 1 not connected");

      while (1) {
        display.getTextBounds(F("PRESSURE SENSOR"), 0, 0, &posx, &posy, &w, &h);
        display.setCursor((128 - w) / 2, 30);
        display.println(F("PRESSURE SENSOR"));
        display.getTextBounds(F("NOT CONNECTED"), 0, 0, &posx, &posy, &w, &h);
        display.setCursor((128 - w) / 2, 40);
        display.println(F("NOT CONNECTED"));
        display.display();
        delay(1000);
      }
    }

    Serial.println("pressure sensor 1 connected");
    display.getTextBounds(F("PRESSURE SENSOR"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 30);
    display.println(F("PRESSURE SENSOR"));
    display.getTextBounds(F("CONNECTED"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.println(F("CONNECTED"));
    display.display();
    delay(2000);
  */
  //DHT INIT
  Serial.println(F("INIT DHT11"));
  Serial.flush();
  hapusmenu(17, 64);
  dht.begin();
  display.getTextBounds(F("INIT DHT11"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.print(F("INIT DHT"));
  display.println(DHTTYPE);
  display.display();
  delay(1000);
  display.getTextBounds(F("DHT READY!"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.println(F("DHT READY!"));
  display.display();
  delay(2000);

  Serial.print(F("OPEN WIFI "));
  Serial.println(hotspot);
  Serial.print(F("IP="));
  Serial.println(apIP);

  hapusmenu(17, 64);
  display.getTextBounds(F("OPEN WIFI"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.print(F("OPEN WIFI"));
  display.getTextBounds((char*)hotspot.c_str(), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 35);
  display.print(hotspot);
  display.display();
  delay(500);
  display.getTextBounds(F("ip=10.10.10.1"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 50);
  display.print(F("IP="));
  display.print(apIP);
  display.display();
  delay(1000);

  waktu = millis();
  while (millis() - waktu < 3000) {
    reads = ads.readADC_SingleEnded(button);
    delay(1000);
  }
  Serial.print("reset button ");
  Serial.println(reads);

  //WIFI MANAGER
  //connect to network
  WiFiManager wifiManager;
  wifiManager.setAPStaticIPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); //IP, Gateway, DNS
  IPAddress myIP = WiFi.softAPIP();
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setBreakAfterConfig(true);

  if ( reads < 10 ) {
    wifiManager.resetSettings();
    wifiManager.setAPStaticIPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); //IP, Gateway, DNS
    IPAddress myIP = WiFi.softAPIP();
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setBreakAfterConfig(true);
    wifiManager.autoConnect((char*)hotspot.c_str());
    wifiManager.setSaveConfigCallback(saveConfigCallback);
  }

  if (!wifiManager.autoConnect((char*)hotspot.c_str())) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //system_deep_sleep(3e6);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
    //system_deep_sleep(5e6);
  }

  ////AFTER FINISH SETTING GOES TO HERE
  hapusmenu(17, 64);
  Serial.print(F("AP Local IP: "));
  Serial.println(WiFi.localIP());
  display.getTextBounds(F("CONNECTED TO AP"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 17);
  display.print(F("CONNECTED TO AP"));
  display.getTextBounds((char*)WiFi.SSID().c_str(), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 35);
  display.print(WiFi.SSID().c_str());
  display.display();
  delay(1000);
  filename = WiFi.localIP().toString();
  filename.toCharArray(sdcard, filename.length() + 1);
  display.getTextBounds(sdcard, 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 50);
  display.print(filename);
  display.display();
  WiFi.mode(WIFI_STA);
  delay(5000);


  display.clearDisplay();
  displaydate();

  //display menu parameter
  hapusmenu(17, 64);
  display.setCursor(0, 20);  display.print(F("Temp"));
  display.setCursor(0, 30);  display.print(F("Humid"));
  display.setCursor(0, 40);  display.print(F("Light"));

  display.setCursor(32, 20);  display.print(F("="));
  display.setCursor(32, 30);  display.print(F("="));
  display.setCursor(32, 40);  display.print(F("="));

  display.setCursor(80, 15);  display.print("o");
  display.setCursor(85, 20);  display.print("C");
  display.setCursor(80, 40);  display.print(F("RH"));
  display.setCursor(80, 50);  display.print(F("%"));

  display.display();

  //display.setCursor(100,52);  display.print(F("ERR"));
}

void bersihdata() {
  reads1=0; reads =0; tekanan = 0.00; temp = 0.00; humid = 0.00; volt = 0.00; filename = ""; y = "";
}

void configs() {
  i = 0;
  file = SD.open(F("config.txt"));
  if (file) {
    while (file.available()) {
      sdcard[i++] = file.read();
    }
  }
  else  {

    while (1) {
      Serial.println(F("ERROR READING!!!"));
      display.getTextBounds(F("ERROR READING!!!"), 0, 0, &posx, &posy, &w, &h);
      display.setCursor((128 - w) / 2, 40);
      display.println(F("ERROR READING!!!"));
      display.display();
      delay(10000);
    }
  }
  file.close();

  filename = String(sdcard);
  for ( a = 0; a < sizeof(sdcard); a++) {
    sdcard[a] = (char)0;
  }
  Serial.println(filename);
  display.getTextBounds(F("SUCCESS!!!"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.println(F("SUCCESS!!!"));
  display.display();
  Serial.println(F("GET CONFIG SUCCESS!!!"));

  b = 0;

  //NAMA STASIUN
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  hotspot = filename.substring(a + 2, b);

  //INTERVAL
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  interval = filename.substring(a + 1, b).toInt();

  //BURST
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  burst = filename.substring(a + 1, b).toInt();

  //OFFSET
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  y = filename.substring(a + 2, b);
  offset = y.toFloat();

  //LATITUDE
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  y = filename.substring(a + 2, b);
  flat = y.toFloat();

  //LONGITUDE
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  y = filename.substring(a + 2, b);
  flon = y.toFloat();

  filename = '0';

  Serial.print(F("NAMA STASIUN = "));
  Serial.println(hotspot);
  Serial.print(F("Interval data = "));
  Serial.print(interval);
  if (interval < 2)
    Serial.println(F(" minute"));
  else Serial.println(F(" minutes"));
  Serial.print(F("Burst Interval = "));
  Serial.print(burst);
  if (interval < 2)
    Serial.println(F(" second"));
  else Serial.println(F(" seconds"));
  Serial.print(F("OFFSET = "));
  Serial.println(offset, 4);
  Serial.print(F("LATITUDE = "));
  Serial.println(flat, 4);
  Serial.print(F("LONGITUDE = "));
  Serial.println(flon, 4);
  Serial.flush();
}

void hapusmenu(byte h, byte h1) {
  display.fillRect(0, h, 128, h1, BLACK); //clear display
  display.display();
  display.setTextColor(WHITE, BLACK);
}

void lcd2digits(int number) {
  if (number >= 0 && number < 10) {
    display.write('0');
  }
  display.print(number);
}

void dateTime(uint16_t* date, uint16_t* time) {
  ESPtime = rtc.now();
  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(ESPtime.year(), ESPtime.month(), ESPtime.day());

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(ESPtime.hour(), ESPtime.minute(), ESPtime.second());
}

void loop() {
  waktu = millis();
  bersihdata();
  Serial.println("ready to get data");
  sekarang = rtc.now();
  tahun = ESPtime.year();
  bulan = ESPtime.month();
  hari = ESPtime.day();
  jam = ESPtime.hour();
  menit = ESPtime.minute();
  detik = ESPtime.second();

  displaydate();

  // burst sampling
  reads1=0; reads=0;
  for (i = 0; i < burst; i++) {
    reads1=ads.readADC_SingleEnded(1); //pressure
    if(reads1<0){
      reads=reads+0;
    }
    else{
    reads = reads+reads1;
    }
    delay(1000);
  }
  Serial.println(reads);
  //KONVERSI
  volt = ((float)reads / (float)burst) * 0.1875 / 1000.0000; // nilai voltase dari nilai DN
  volt = volt * 147.00 / 100.00;
  Serial.println(volt);
  tekanan = (300.00 * volt - 150.00) * 0.01 + offset;
  if (tekanan < 0) tekanan = 0;
  reads = ads.readADC_SingleEnded(tegangan);
  volt = (float)reads * 0.1875 / 1000.0000 * 147.00 / 100.00; // nilai voltase dari nilai DN
  humid = dht.readHumidity();      //humid
  temp = dht.readTemperature();    //temperature
  if (isnan(humid)) {
    humid = -99.9;
  }
  if (isnan(temp)) {
    temp = -99.9;
  }

  Serial.print(F("PRESSURE = "));
  Serial.println(tekanan, 1);
  Serial.print(F("TEMPERATURE = "));
  Serial.println(temp, 1);
  Serial.print(F("HUMIDITY = "));
  Serial.println(humid, 1);
  Serial.print(F("VOLTAGE = "));
  Serial.println(volt, 1);
  delay(1000);

  //display data
  display.fillRect(40, 17, 32, 64, BLACK); //clear display
  display.fillRect(100, 52, 32, 64, BLACK); //clear display
  display.display();
  display.setCursor(40, 17);  display.print(tekanan, 1);
  display.setCursor(40, 30);  display.print(temp, 1);
  display.setCursor(40, 40);  display.print(humid, 1);
  display.setCursor(40, 50);  display.print(volt, 1);
  display.display();
  delay(2000);

  //SIMPAN DATA
  displaydate();
  Serial.println("Simpan data");
  simpandata();
  delay(1000);

  //KIRIM data
  Serial.println("Kirim data");
  displaydate();
  httppost();
  delay(1000);
  // a = 0;
  while (millis() - waktu < interval * 60 * 1000) {
    displaydate();
    // a++;
    // Serial.println(a);
    delay(1000);
  }
}

void displaydate() {
  ESPtime = rtc.now();
  delay(100);
  display.getTextBounds(F("yyyy/mm/dd hh:mm "), 0, 0, &posx, &posy, &w, &h);
  display.setTextColor(WHITE, BLACK);
  display.setCursor((128 - w), 0);
  display.print(ESPtime.year()); display.print("/");
  lcd2digits(ESPtime.month()); display.print("/");
  lcd2digits(ESPtime.day()); display.print(" ");
  lcd2digits(ESPtime.hour()); display.print(":");
  lcd2digits(ESPtime.minute());
  display.display();
  Serial.print(ESPtime.year(), DEC);
  Serial.print('/');
  Serial.print(ESPtime.month(), DEC);
  Serial.print('/');
  Serial.print(ESPtime.day(), DEC);
  Serial.print(" ");
  Serial.print(ESPtime.hour(), DEC);
  Serial.print(':');
  Serial.print(ESPtime.minute(), DEC);
  Serial.print(':');
  Serial.print(ESPtime.second(), DEC);
  Serial.println();
}

void simpandata() {
  SdFile::dateTimeCallback(dateTime);
  filename = "";
  filename = String(tahun);
  if (bulan < 10) {
    filename += "0" + String(bulan);
  }
  if (bulan >= 10) {
    filename += String(bulan);
  }
  if (hari < 10) {
    filename += "0" + String(hari);
  }
  if (hari >= 10) {
    filename += String(hari);
  }
  filename += ".txt";

  file = SD.open(filename, FILE_READ);
  a = file.available();
  file.close();
  // set date time callback function
  SdFile::dateTimeCallback(dateTime);
  file = SD.open(filename, FILE_WRITE);

  if (a == 0) {
    Serial.println(filename);
    file.print(F("Date (YYYY-MM-DD HH:MM:SS),"));
    file.print(F("ID,Pressure (BAR),Temperature (°C), Humidity (RH),"));
    file.println(F("Voltage (V),Burst interval (SECOND),Data Interval (MINUTE),Network Code,Response")); //
    // SdFile::dateTimeCallback(dateTime);
  }

  y = "";
  y = String(tahun) + '/';
  if (bulan < 10) {
    y += "0" + String(bulan);
  }
  if (bulan >= 10) {
    y += String(bulan);
  }
  y += "/";
  if (hari < 10) {
    y += "0" + String(hari);
  }
  if (hari >= 10) {
    y += String(hari);
  }
  y += " ";
  if (jam < 10) {
    y += "0" + String(jam);
  }
  if (jam >= 10) {
    y += String(jam);
  }
  y += ":";
  if (menit < 10) {
    y += "0" + String(menit);
  }
  if (menit >= 10) {
    y += String(menit);
  }
  y += ":";
  if (detik < 10) {
    y += "0" + String(detik);
  }
  if (detik >= 10) {
    y += String(detik);
  }
  y += "," + String(sn) + "," + String(tekanan, 2) + "," + String(temp, 2);
  y += "," +  String(humid, 2) + "," + String(volt, 2) + "," + String(burst) + "," + String(interval);

  Serial.println(y);
  file.print(y);
  delay(100);
  file.close();
}

void httppost () {
    // http://www.mantisid.id/api/product/osh_data_c.php?={"Data":"003,20190315213555,2456,02356,02201,01233,05,05"}
    // {"Data":"id unit,yyyymmddhhmmss,tekanan(5 digit),kelembaban(5 digit),suhu(5 digit),volt (5 digit), burst (2 digit), interval (2 digit)"}
  y = "{\"Data\":\"" + sn +",";
    y += String(tahun);
    if (bulan < 10) {
      y += "0" + String(bulan);
    }
    if (bulan >= 10) {
      y += String(bulan);
    }
    if (hari < 10) {
      y += "0" + String(hari);
    }
    if (hari >= 10) {
      y += String(hari);
    }
    if (jam < 10) {
      y += "0" + String(jam);
    }
    if (jam >= 10) {
      y += String(jam);
    }
    if (menit < 10) {
      y += "0" + String(menit);
    }
    if (menit >= 10) {
      y += String(menit);
    }
    if (detik < 10) {
      y += "0" + String(detik);
    }
    if (detik >= 10) {
      y += String(detik);
    }

    y += "," + strTwoDigit(tekanan) + ",";
    y += strTwoDigit(humid) + ",";
    y += strTwoDigit(temp) + ",";
    y += strTwoDigit(volt) + ",";
    y += String(burst) + ",";
    y += String(interval) + "\"}";

  Serial.println(y);

  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    HTTPClient http;    //Declare object of class HTTPClient
    http.begin(server + url);      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header
    httpCode = http.POST(y);   //Send the request
    String payload = http.getString();                  //Get the response payload

    statuscode(httpCode);
    Serial.print(httpCode);   //Print HTTP return code
    Serial.print(" ");
    Serial.println(network);
    Serial.println(payload);    //Print request response payload
    http.end();  //Close connection

    //print code to display
    display.setCursor(100, 52);
    display.println(httpCode);
    display.display();
  } else {
    Serial.println("Error in WiFi connection");
    httpCode = 0;
    statuscode(httpCode);
  }
  //save to sd card
  file = SD.open(filename, FILE_WRITE);
  file.print(',');
  file.print(httpCode);
  file.print(',');
  file.println(network);
  delay(100);
  file.close();
  delay(2000);
}

String statuscode(int w) {
  if (w < 100) {
    network = "Error";
  }
  if (w == 100) {
    network = "Continue";
  }
  if (w == 101) {
    network = "Switching Protocols";
  }
  if (w == 200) {
    network = "OK";
  }
  if (w == 201) {
    network = "Created";
  }
  if (w == 202) {
    network = "Accepted";
  }
  if (w == 203) {
    network = "Non-Authoritative Information";
  }
  if (w == 204) {
    network = "No Content";
  }
  if (w == 205) {
    network = "Reset Content";
  }
  if (w == 206) {
    network = "Partial Content";
  }
  if (w == 300) {
    network = "Multiple Choices";
  }
  if (w == 301) {
    network = "Moved Permanently";
  }
  if (w == 302) {
    network = "Found";
  }
  if (w == 303) {
    network = "See Other";
  }
  if (w == 304) {
    network = "Not Modified";
  }
  if (w == 305) {
    network = "Use Proxy";
  }
  if (w == 307) {
    network = "Temporary Redirect";
  }
  if (w == 400) {
    network = "Bad Request";
  }
  if (w == 401) {
    network = "Unauthorized";
  }
  if (w == 402) {
    network = "Payment Required";
  }
  if (w == 403) {
    network = "Forbidden";
  }
  if (w == 404) {
    network = "Not Found";
  }
  if (w == 405) {
    network = "Method Not Allowed";
  }
  if (w == 406) {
    network = "Not Acceptable";
  }
  if (w == 407) {
    network = "Proxy Authentication Required";
  }
  if (w == 408) {
    network = "Request Time-out";
  }
  if (w == 409) {
    network = "Conflict";
  }
  if (w == 410) {
    network = "Gone";
  }
  if (w == 411) {
    network = "Length Required";
  }
  if (w == 412) {
    network = "Precondition Failed";
  }
  if (w == 413) {
    network = "Request Entity Too Large";
  }
  if (w == 414) {
    network = "Request-URI Too Large";
  }
  if (w == 415) {
    network = "Unsupported Media Type";
  }
  if (w == 416) {
    network = "Requested range not satisfiable";
  }
  if (w == 417) {
    network = "Expectation Failed ";
  }
  if (w == 500) {
    network = "Internal Server Error";
  }
  if (w == 501) {
    network = "Not Implemented";
  }
  if (w == 502) {
    network = "Bad Gateway";
  }
  if (w == 503) {
    network = "Service Unavailable";
  }
  if (w == 504) {
    network = "Gateway Time-out";
  }
  if (w == 505) {
    network = "HTTP Version not supported";
  }
  if (w == 600) {
    network = "Not HTTP PDU";
  }
  if (w == 601) {
    network = "Network Error";
  }
  if (w == 602) {
    network = "No memory";
  }
  if (w == 603) {
    network = "DNS Error";
  }
  if (w == 604) {
    network = "Stack Busy";
  }
  return network;
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

String strTwoDigit(float nilai){
  String result=String(nilai,2);
  String angka, digit;
  b=0;
  a = result.indexOf(".", b + 1);
  digit = result.substring(a + 1, result.length());
  angka = result.substring(0,a);
    if(result.length()==4) { //4.12
    angka="00"+angka+digit;
  }
  if(result.length()==5){ //51.23
    angka="0"+angka+digit;
  }
  if(result.length()>5){//123.45
    angka=angka+digit;
  }
  return angka;
}





