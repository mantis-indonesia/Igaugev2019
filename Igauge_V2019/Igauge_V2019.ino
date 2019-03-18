/* I-GAUGE V 2019
   BOARD : ARDUINO MEGA 2560


   HISTORY:
   18-2-2019
   FIRST BUILD BY HOLLANDA
   CONFIGURE RTC DS1307  - LIBRARY RTCLIB
   MICRO SD CARD - SDFAT
   ADS1115 - ADS1015
   DISPLAY USING OLED SSD1306 128X64 - ADAFRUIT SSD1306

   24-2-2019
   editing functions
   adding lowpower

   28-2-2019
   GSM connection OK

  problem:
  18-2-2019 cannot be uploaded through icsp - solved
  4-3-2019 oled not displayed the font size properly
*/

//LIBRARY
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "SdFat.h"
#include <Wire.h>
#include "RTClib.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_ADS1015.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <DHT.h>
#include <LowPower.h>

String ID;// = "BOGOR06";
String source = "GSM";
float offset = 0.0;
//server
String url = "/api/product/pdam_dt_c.php";
String server = "http://www.mantisid.id";

#define DHTPIN      9
#define DHTTYPE         DHT11
#define oled      0x3c
#define ads_addr    0x48
#define rtc_addr    0x68
#define panjang     128
#define lebar           64
#define pres            0
#define pres1           1
#define tegangan        3
#define connectPres     4
#define connectPres1    6
#define SCKpin          52
#define MISOpin         50
#define MOSIpin         51
#define SSpin           53


// from  http://heliosoph.mit-links.info/arduino-powered-by-capacitor-reducing-consumption/
//defines for DIDR0 setting
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

// create variables to restore the SPI & ADC register values after shutdown
byte keep_ADCSRA;
byte keep_SPCR;

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
DateTime nows;
DateTime sekarang;

//SD CARD
SdFat SD;
File file;
String filename;

//ADS1115
Adafruit_ADS1115 ads(ads_addr);

//GLOBAL VARIABLE
int i, kode, tahun;
char sdcard[200];
char g;
byte a, b, c, interval, burst;
byte bulan, hari, jam, menit, detik;
char str[13];
unsigned long reads = 0; //pressure
unsigned long waktu, start;
float tekanan, temp, humid, volt;
float longitude = 999.9123;
float latitude = 99.1234;
String y, network, APN, USER, PWD, sms, kuota, noHP, operators, result;

void setup() {
  //initialisation
  Serial.begin(9600);  // Serial USB
  Serial1.begin(9600);  // SIM900A
  pinMode(connectPres, INPUT);
  pinMode(connectPres1, INPUT);
  digitalWrite(connectPres, HIGH);
  digitalWrite(connectPres1, HIGH);

  // disable unused pin function
  ADCSRA = 0; //ADC OFF
  keep_SPCR = SPCR;
  off();

  //WELCOME SCREEN
  s_on();
  Serial.println(F("\r\nWIFI IGAUGE V 2019"));
  Serial.flush();
  s_off();

  i_En(oled);
  display.begin(SSD1306_SWITCHCAPVCC, oled);
  display.clearDisplay();
  display.display();
  delay(10);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  display.getTextBounds(F("USAID - PDAM"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 0);
  display.println(F("USAID - PDAM"));
  display.getTextBounds(F("OPEN SOURCE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 15);
  display.println(F("OPEN SOURCE"));
  display.getTextBounds(F("HARDWARE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 25);
  display.println(F("HARDWARE"));
  display.getTextBounds(F("WATER PRESSURE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 35);
  display.println(F("WATER PRESSURE"));
  display.getTextBounds(F("SENSOR DEVICE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 45);
  display.println(F("SENSOR DEVICE"));
  display.getTextBounds(F("2019"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 55);
  display.println(F("2019"));
  display.display();
  i_Dis();
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

  //RTC INIT
  i_En(oled);
  i_En(rtc_addr);
  hapusmenu(15, 64);
  display.getTextBounds(F("INIT CLOCK"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT CLOCK"));
  display.display();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  s_on();
  if (! rtc.begin()) { // RTC NOT BEGIN
    Serial.println("Couldn't find RTC");
    display.getTextBounds(F("ERROR!! CONTACT CS."), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.println(F("ERROR!! CONTACT CS."));
    display.display();
    off();
    while (1);
  }

  if (rtc.lostPower()) { // RTC LOST POWER
    Serial.println("RTC LOST POWER!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    display.getTextBounds(F("LOST POWER!"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 35);
    display.println(F("LOST POWER!"));
    display.getTextBounds(F("CONTACT CS"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 45);
    display.println(F("CONTACT CS"));
    display.display();
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
    hapusmenu(35, 64);
    display.getTextBounds(F("SET TIME"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.println(F("SET TIME"));
    display.display();
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  }

  nows = rtc.now();
  interval = nows.second();
  while (nows.second() < interval + 3) {
    nows = rtc.now();
    filename = String(nows.month()) + '/' + String(nows.day()) + '/' + String(nows.year()) + ' ' + String(nows.hour()) + ':' + String(nows.minute()) + ':' + String(nows.second());
    Serial.println(filename);
    display.getTextBounds(F("yyyy/mm/dd hh:mm:ss"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.print(nows.year());
    display.print("/");
    lcd2digits(nows.month());
    display.print("/");
    lcd2digits(nows.day());
    display.print(" ");
    lcd2digits(nows.hour());
    display.print(":");
    lcd2digits(nows.minute());
    display.print(":");
    lcd2digits(nows.second());
    display.display();
    LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
  }

  //SD INIT
  LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
  hapusmenu(17, 64);
  display.getTextBounds(F("init SD Card..."), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT SD Card..."));
  display.display();
  pinMode(SSpin, OUTPUT);
  digitalWrite(SSpin, HIGH);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  spiEn();
  s_on();
  delay(100);
  if (!SD.begin(SSpin)) { //SD ERROR
    Serial.println(F("SD init error!!!"));
    Serial.flush();
    display.getTextBounds(F("SD Card Error!!!"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.println(F("SD Card Error!!!"));
    display.display();
    off();
    while (1);
  }

  Serial.println(F("SD CARD INIT OK!"));
  Serial.flush();
  display.getTextBounds(F("SD Card OK!"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.println(F("SD Card OK!"));
  display.display();
  off();
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);

  //GET CONFIG TXT
  hapusmenu(17, 64);
  display.getTextBounds(F("CONFIGURATION FILE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("CONFIGURATION FILE"));
  display.display();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  pinMode(SSpin, OUTPUT);
  digitalWrite(SSpin, HIGH);
  spiEn();
  LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
  configs();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  off();

  //DISPLAY BURST INTERVAL
  hapusmenu(40, 64);
  display.getTextBounds(F("BURST SAMPLING"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.print(F("BURST SAMPLING"));
  display.display();
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
  display.getTextBounds(F("10 SECONDS"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 52);
  display.print(burst);
  if (burst < 2) {
    display.print(F(" SECOND"));
  }
  else display.print(F(" SECONDS"));
  display.display();
  off();
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);

  //---------------------------------------------------------------
  //DISPLAY DATA INTERVAL
  hapusmenu(40, 64);
  display.getTextBounds(F("INTERVAL DATA"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.print(F("INTERVAL DATA"));
  display.display();
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
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
  off();
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);

  //---------------------------------------------------------------
  //DISPLAY NO HP
  hapusmenu(40, 64);
  display.getTextBounds(F("NO HP"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.print(F("NO HP"));
  display.display();
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
  display.getTextBounds(F("081234567898"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 52);
  display.print(noHP);
  display.display();
  off();
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);

  //---------------------------------------------------------------

  //check water pressure sensor connected or not
  s_on();
  i_En(ads_addr);
  Serial.println(F("INIT ADS1115"));
  Serial.flush();
  Serial.println(F("CEK PRESSURE SENSOR"));
  Serial.flush();
  ads.begin();
  hapusmenu(17, 64);
  display.getTextBounds(F("CHECK PRESSURE SENSOR"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("CHECK PRESSURE SENSOR"));
  display.display();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  //cek via ads1115
  reads = digitalRead(connectPres);
  Serial.println(reads);

  if (reads == 1) {
    Serial.println("pressure sensor 1 not connected");
    Serial.flush();
    display.getTextBounds(F("Pressure Sensor"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 35);
    display.println(F("Pressure Sensor"));
    display.getTextBounds(F("not connected!!"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 50);
    display.println(F("Not Connected!!"));
    display.display();
    off();
    while (1);
  }

  Serial.println("pressure sensor 1 connected");
  display.getTextBounds(F("Pressure sensor"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 35);
  display.println(F("Pressure Sensor"));
  display.getTextBounds(F("Connected"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 50);
  display.println(F("Connected"));
  display.display();
  off();
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);


  //DHT INIT
  s_on();
  Serial.println(F("INIT DHT11"));
  Serial.flush();
  hapusmenu(17, 64);
  delay(100);
  dht.begin();
  display.getTextBounds(F("INIT DHT SENSOR"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT DHT SENSOR"));
  display.display();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  display.getTextBounds(F("DHT READY!!"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.println(F("DHT READY!!"));
  display.display();
  off();
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);


  //INIT SIM900A & SEND SMS
  i_En(oled);
  s_on();
  Serial.println(F("INITIALIZATION SIM900A..."));
  Serial.flush();
  s_off();
  hapusmenu(17, 64);
  display.getTextBounds(F("INIT GSM MODULE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT GSM MODULE"));
  display.display();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  sim();

  //SEND SMS
  hapusmenu(17, 64);
  display.getTextBounds(F("SEND SMS TO ADMIN"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("SEND SMS TO ADMIN"));
  display.display();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  display.getTextBounds(F("081234567898"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 30);
  display.print(noHP);
  display.display();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

  sendSMS();

  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
  i_En(oled);
  display.getTextBounds(F("Finish!!!"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 50);
  display.print(F("Finish!!!"));
  display.display();
  off();
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);

  display.clearDisplay();
  displaydate();
  nows = rtc.now();
  setTime(nows.hour(), nows.minute(), nows.second(), nows.month(), nows.day(), nows.year());
  Alarm.timerRepeat(60 * interval, ambil);

  //display menu parameter
  hapusmenu(17, 64);
  display.setCursor(0, 17);  display.print(F("Pres"));
  display.setCursor(0, 30);  display.print(F("Temp"));
  display.setCursor(0, 40);  display.print(F("Humid"));
  display.setCursor(0, 50);  display.print(F("Volt"));

  display.setCursor(32, 17);  display.print(F("="));
  display.setCursor(32, 30);  display.print(F("="));
  display.setCursor(32, 40);  display.print(F("="));
  display.setCursor(32, 50);  display.print(F("="));

  display.setCursor(73, 17);  display.print(F("bar"));
  display.setCursor(73, 25);  display.print("o");
  display.setCursor(78, 30);  display.print("C");
  display.setCursor(75, 40);  display.print(F("RH"));
  display.setCursor(75, 50);  display.print(F("V"));

  //display.setCursor(100,52);  display.print(F("ERR"));
  display.display();
  off();
  ambil();
}

void loop() {
  Alarm.delay(0)  ;
}

void bersihdata() {
  Alarm.delay(0);
  reads = '0'; tekanan = '0'; temp = '0'; humid = '0'; volt = '0';
  filename = ""; y = "";
}

void ambil() {
  on();
  Alarm.delay(0);
  displaydate();
  hapusmenu(100, 52);
  //WAKE UP GSM
  Serial.println(F(" "));
  Serial.println(F("AT+CSCLK=0"));
  Serial1.println(F("AT+CSCLK=0"));
  Serial1.flush();
  delay(200);
  Serial1.println(F("AT+CSCLK=0"));
  Serial1.flush();
  bacaserial(200);

  sekarang = rtc.now();
  tahun = sekarang.year();
  bulan = sekarang.month();
  hari = sekarang.day();
  jam = sekarang.hour();
  menit = sekarang.minute();
  detik = sekarang.second();
  waktu = sekarang.unixtime();
  bersihdata();

  // burst sampling
  for (i = 0; i < burst; i++) {
    displaydate();
    reads += ads.readADC_SingleEnded(0); //pressure
    LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
  }

  //KONVERSI
  volt = ((float)reads / (float)burst) * 0.1875 / 1000.0000; // nilai voltase dari nilai DN
  tekanan = (300.00 * volt - 150.00) * 0.01 + float(offset);
  reads = ads.readADC_SingleEnded(tegangan);
  volt = (float)reads * 0.1875 / 1000.0000 * 3.0000 / 2.00000; // nilai voltase dari nilai DN
  humid = dht.readHumidity();      //humid
  temp = dht.readTemperature();    //temperature
  if (isnan(humid)) {
    humid = -99.9;
  }
  if (isnan(temp)) {
    temp = -99.9;
  }

  //display data
  displaydate();
  display.fillRect(40, 17, 32, 64, BLACK); //clear display
  display.fillRect(100, 52, 32, 64, BLACK); //clear display
  display.display();
  display.setCursor(40, 17);  display.print(tekanan, 1);
  display.setCursor(40, 30);  display.print(temp, 1);
  display.setCursor(40, 40);  display.print(humid, 1);
  display.setCursor(40, 50);  display.print(volt, 1);
  display.display();

  //debug serial
  displaydate();
  Serial.print(F("PRESSURE = "));
  Serial.println(tekanan, 1);
  Serial.print(F("TEMPERATURE = "));
  Serial.println(temp, 1);
  Serial.print(F("HUMIDITY = "));
  Serial.println(humid, 1);
  Serial.print(F("VOLTAGE = "));
  Serial.println(volt, 1);
  Serial.flush();

  //SIMPAN DATA
  displaydate();
  Serial.println(F("Simpan data"));
  Serial.flush();
  simpandata();

  //KIRIM data
  displaydate();
  s_on();
  Serial.println(F("SEND DATA TO SERVER"));
  Serial.flush();
  s_off();
  sendServer();

  //kode network
  displaydate();
  spiEn();
  s_on();
  Serial.println(F("simpan kode network"));
  Serial.flush();
  s_off();
  file = SD.open(filename, FILE_WRITE);
  file.print(kode);
  file.print(",");
  file.println(network);
  delay(100);
  file.close();

  //display kode network
  displaydate();
  i_En(oled);
  display.setCursor(100, 52);  display.print(kode);
  display.display();
  s_on();
  s1_on();
  Serial.println(F("AT+CSCLK=2"));
  Serial1.println(F("AT+CSCLK=2"));
  Serial.flush();
  Serial1.flush();
  off();

  while (1) {
    displaydate();
    setTime(nows.hour(), nows.minute(), nows.second(), nows.month(), nows.day(), nows.year());
    int start = nows.unixtime() - waktu;
    s_on();
    Serial.println(start);
    Serial.flush();
    s_off();
    off();
    if (start < ((interval * 60) - 15)) {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
      LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

    }
    if (start < ((interval * 60) - 5)) {
      LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
    }
    else {
      break;
    }
  }
  i_En(rtc_addr);
  displaydate();
  setTime(nows.hour(), nows.minute(), nows.second(), nows.month(), nows.day(), nows.year());
  s_on();
  Serial.println("ready to get data");
  Serial.flush();
  s_off();
}

void simpandata() {
  Alarm.delay(0);
  spiEn();
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

  Serial.println(filename);
  Serial.flush();

  displaydate();
  file = SD.open(filename, FILE_READ);
  a = file.available();
  file.close();

  // set date time callback function
  SdFile::dateTimeCallback(dateTime);
  file = SD.open(filename, FILE_WRITE);
  if (a == 0) {
    Serial.println(filename);
    file.print(F("Date (YYYY-MM-DD HH:MM:SS),"));
    file.print(F(" ID, Pressure (BAR), Temperature (°C), Humidity (RH), "));
    file.println(F("Voltage (V), Burst interval (SECOND), Data Interval (MINUTE), Network Code, Response")); //
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
  y += ", " + String(ID) + ", " + String(tekanan, 2) + ", " + String(temp, 2);
  y += ", " +  String(humid, 2) + ", " + String(volt, 2) + ", " + String(burst) + ", " + String(interval) + ", " ;

  Serial.println(y);
  file.print(y);
  delay(100);
  file.close();
  displaydate();
}

void configs() {
  i = 0;
  file = SD.open(F("config.txt"));
  if (file) {
    while (file.available()) {
      sdcard[i++] = file.read();
    }
  }
  else  { //error reading
    s_on();
    Serial.println(F("ERROR READING!!!"));
    display.getTextBounds(F("ERROR READING!!!"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.println(F("ERROR READING!!!"));
    display.display();
    off();
    while (1);
  }
  file.close();

  filename = String(sdcard);
  for ( a = 0; a < sizeof(sdcard); a++) { //clear variable
    sdcard[a] = (char)0;
  }
  
  s_on();
  Serial.println(filename);
  Serial.flush();
  display.getTextBounds(F("SUCCESS!!!"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.println(F("SUCCESS!!!"));
  display.display();
  Serial.println(F("GET CONFIG SUCCESS!!!"));
  Serial.flush();
  b = 0;

  //ID
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  ID = filename.substring(a + 2, b);

  //interval data
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  interval = filename.substring(a + 1, b).toInt();

  //burst interval
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  burst = filename.substring(a + 1, b).toInt();

  //no HP
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  noHP = filename.substring(a + 2, b);

  //LATITUDE
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  y = filename.substring(a + 2, b);
  latitude = y.toFloat();

  //LONGITUDE
  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  y = filename.substring(a + 2, b);
  longitude = y.toFloat();

  filename = '0';

  Serial.print(F("Station Name = "));
  Serial.println(ID);
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
  Serial.print(F("NO HP = "));
  Serial.println(noHP);
  Serial.print(F("Latitude = "));
  Serial.println(latitude);
  Serial.print(F("Longitude = "));
  Serial.println(longitude);

  Serial.flush();
}

void hapusmenu(byte h, byte h1) {
  i_En(oled);
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

void displaydate() {
  i_En(rtc_addr);
  i_En(oled);
  nows = rtc.now();
  display.getTextBounds(F("yyyy/mm/dd hh:mm "), 0, 0, &posx, &posy, &w, &h);
  //display.fillRect(0, 2, 128, h, BLACK); //clear display
  display.setTextColor(WHITE, BLACK);
  display.setCursor((128 - w), 0);
  display.print(nows.year()); display.print("/");
  lcd2digits(nows.month()); display.print("/");
  lcd2digits(nows.day()); display.print(" ");
  lcd2digits(nows.hour()); display.print(":");
  lcd2digits(nows.minute());
  display.display();
}

void dateTime(uint16_t* date, uint16_t* time) {
  nows = rtc.now();
  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(nows.year(), nows.month(), nows.day());

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(nows.hour(), nows.minute(), nows.second());
}

void statuscode(int w) {
  if (w < 100) {
    network = "Unknown";
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
}

void apn(String nama) {
  if (nama == "TELKOMSEL") {
    APN = "Telkomsel";
    USER = "";
    PWD = "";
  }
  if (nama == "INDOSAT") {
    APN = "indosatgprs";
    USER = "indosat";
    PWD = "indosatgprs";
  }
  if (nama == "EXCELCOM" || nama == "XL") {
    APN = "internet";
    USER = "";
    PWD = "";
  }
  if (nama == "THREE" || nama == "3") {
    APN = "3data";
    USER = "3data";
    PWD = "3data";
  }
}

void s_off() {
  power_usart0_disable();
}

void s_on() {
  power_usart0_enable();
}

void s1_off() {
  power_usart1_disable();
}

void s1_on() {
  power_usart1_enable();
}

void i_En(byte alamat) {
  power_twi_enable();
  power_timer0_enable();
  Wire.begin(alamat);
}

void i_Dis() {
  power_twi_disable();
  power_timer0_disable();
}

void spiEn() {
  power_timer0_enable();
  pinMode(SSpin, OUTPUT);
  digitalWrite(SSpin, HIGH);
  power_spi_enable();
  SPCR = keep_SPCR;
  delay(100);
}

void spiDis() {
  delay(100);
  SPCR = 0;
  power_spi_disable();
  pinMode(SSpin, OUTPUT);
  digitalWrite(SSpin, LOW);
  LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
}

void off() {
  power_adc_disable(); // ADC converter
  power_spi_disable(); // SPI
  power_usart0_disable(); // Serial (USART)
  power_usart1_disable();
  power_usart2_disable();
  power_timer0_disable();// Timer 0   >> millis() will stop working
  power_timer1_disable();// Timer 1   >> analogWrite() will stop working.
  power_timer2_disable();// Timer 2   >> tone() will stop working.
  power_timer3_disable();// Timer 3
  power_timer4_disable();// Timer 4
  power_timer5_disable();// Timer 5
  power_twi_disable(); // TWI (I2C)
}

void on() {
  power_spi_enable(); // SPI
  power_usart0_enable(); // Serial (USART)
  power_usart1_enable();
  power_usart2_enable();
  power_timer0_enable();// Timer 0   >> millis() will stop working
  power_timer1_enable();// Timer 1   >> analogWrite() will stop working.
  power_twi_enable(); // TWI (I2C)
}

byte ConnectAT(String cmd, int d) {
  i = 0;
  s_on();
  s1_on();
  power_timer0_enable();
  while (1) {
    Serial1.println(cmd);
    while (Serial1.available()) {
      if (Serial1.find("OK"))
        i = 8;
    }
    delay(d);
    if (i > 5) {
      break;
    }
    i++;
  }
  return i;
}

void ceksim() {
  c = 0;
cops:
  filename = "";
  s1_on();
  s_on();
  power_timer0_enable();
  Serial.println(F("AT+CREG=1"));
  Serial1.println(F("AT+CREG=1"));
  bacaserial(200);
  LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);

  Serial.println(F("AT+COPS?"));
  Serial1.println(F("AT+COPS?"));
  Serial.flush();
  Serial1.flush();
  delay(200);
  while (Serial1.available() > 0) {
    if (Serial1.find("+COPS:")) {
      while (Serial1.available() > 0) {
        g = Serial1.read();
        filename += g;
      }
    }
  }
  Serial.flush();
  Serial1.flush();

  a = filename.indexOf('"');
  b = filename.indexOf('"', a + 1);
  y = filename.substring(a + 1, b);
  if (y == "51089") y = "THREE";

  operators = y;
  Serial.println(y);
  Serial.flush();
  delay(100);

  //option if not register at network
  if (operators == "")  {
    c++;
    if (c == 15) {
      Serial.println(F("NO OPERATOR FOUND"));
      i_En(oled);
      display.getTextBounds(F("NO OPERATOR FOUND"), 0, 0, &posx, &posy, &w, &h);
      display.setCursor((128 - w) / 2, 50);
      display.println(F("NO OPERATOR FOUND"));
      display.display();
      off();
      while (1);
    }
    goto cops;
  }
}

void sinyal() {
signal:
  filename = "";
  s_on();
  s1_on();
  power_timer0_enable();
  Serial.println(F("AT+CSQ"));
  Serial1.println(F("AT+CSQ"));
  Serial.flush();
  Serial1.flush();
  delay(200);
  while (Serial1.available() > 0) {
    if (Serial1.find("+CSQ: ")) {
      while (Serial1.available() > 0) {
        g = Serial1.read();
        Serial.print(g);
        filename += g;
      }
    }
  }
  Serial.println(" ");
  Serial.println(filename);
  Serial.flush();
  Serial1.flush();

  a = (filename.substring(0, filename.indexOf(','))).toInt();
  Serial.print(a);
  Serial.print(" ");
  Serial.flush();
  if (a < 10) {
    network = "POOR";
  }
  if (a > 9 && a < 15) {
    network = "FAIR";
  }
  if (a > 14 && a < 20) {
    network = "GOOD";
  }
  if (a > 19 && a <= 31) {
    network = "EXCELLENT";
  }
  if (a == 99) {
    network = "UNKNOWN";
    goto signal;
  }
  Serial.println(network);
  delay(100);
}

void sim() { //udah fix
  power_timer0_enable();
  s_on();
  s1_on();
  i_En(oled);

  Serial.println(F("AT+CSCLK=0"));
  Serial1.println(F("AT+CSCLK=0"));
  bacaserial(200);
  Serial.flush();
  Serial1.flush();

  //CONNECT AT
  for (a = 0; a < 6; a++) {
    b = ConnectAT(F("AT"), 100);
    if (b == 8) {
      hapusmenu(40, 64);
      display.getTextBounds(F("GSM MODULE OK!!"), 0, 0, &posx, &posy, &w, &h);
      display.setCursor((128 - w) / 2, 40);
      display.println(F("GSM MODULE OK!!"));
      display.display();
      Serial.println(F("GSM MODULE OK!!"));
      Serial.flush();
      Serial1.flush();
      break;
    }
    if (b < 8) {
      display.getTextBounds(F("GSM MODULE ERROR!!"), 0, 0, &posx, &posy, &w, &h);
      display.setCursor((128 - w) / 2, 40);
      display.println(F("GSM MODULE ERROR!!"));
      display.display();
      Serial.println(F("SIM800L ERROR"));
      Serial.flush();
      Serial1.flush();

      if (a == 5) {
        display.getTextBounds(F("CONTACT CS!!!"), 0, 0, &posx, &posy, &w, &h);
        display.setCursor((128 - w) / 2, 50);
        display.println(F("CONTACT CS!!!"));
        display.display();
        Serial.println(F("CONTACT CS!!!"));
        Serial.flush();
        Serial1.flush();
        off();
        while (1);
      }
    }
  }
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

  Serial.println(F("AT+CPIN?"));
  Serial1.println(F("AT+CPIN?"));
  bacaserial(200);
  Serial.println(F("AT+CFUN=1"));
  Serial1.println(F("AT+CFUN=1"));
  bacaserial(200);
  Serial.println(F("AT+CBAND=\"ALL_BAND\""));
  Serial1.println(F("AT+CBAND=\"ALL_BAND\""));
  bacaserial(200);

  //CEK SIM
  hapusmenu(30, 64);
  display.getTextBounds(F("OPERATOR"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 35);
  display.println(F("OPERATOR"));
  display.display();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  off();
  ceksim();
  i_En(oled);
  display.getTextBounds((char*)operators.c_str(), 0, 0, &posx, &posy, &w, &h); //(char*)WiFi.SSID().c_str()
  display.setCursor((128 - w) / 2, 50);
  display.println(operators);
  display.display();
  off();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

  //CHECK SIGNAL QUALITY
  hapusmenu(30, 64);
  display.getTextBounds(F("SIGNAL"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 35);
  display.println(F("SIGNAL"));
  display.display();
  off();

  sinyal();

  i_En(oled);
  display.getTextBounds((char*)network.c_str(), 0, 0, &posx, &posy, &w, &h); //(char*)WiFi.SSID().c_str()
  display.setCursor((128 - w) / 2, 50);
  display.println(network);
  display.display();
  off();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
}

void bacaserial(int wait) {
  power_timer0_enable();
  s_on();
  s1_on();
  start = millis();
  while ((start + wait) > millis()) {
    while (Serial1.available() > 0) {
      g = Serial1.read();
      Serial.print(g);
    }
  }
}

void sendServer() {
  Alarm.delay(0);
serve:

  result = "";
  s_on();
  s1_on();
  power_timer0_enable();
  Serial.println(F("AT+CGATT= 1 "));
  Serial1.println(F("AT+CGATT=1"));
  Serial.flush();
  Serial1.flush();
  bacaserial(200);

  displaydate();
  Serial.print(F("AT+CGATT? "));
  Serial1.println(F("AT+CGATT?"));
  delay(200);
  while (Serial1.available() > 0) {
    if (Serial1.find("+CGATT: ")) {
      while (Serial1.available() > 0) {
        g = Serial1.read();
        result += g;
      }
    }
  }
  Serial.flush();
  Serial1.flush();
  Serial.println(result);
  Serial.flush();
  Serial1.flush();

  if (result.toInt() == 1) {
    //kirim data ke server
    displaydate();
    s_on();
    s1_on();
    Serial1.println(F("AT+CIPSHUT"));
    bacaserial(500);
    Serial.flush();
    Serial1.flush();
    displaydate();
    Serial1.println(F("AT+SAPBR=0,1"));
    bacaserial(200);
    Serial.flush();
    Serial1.flush();

    //ATUR APN SESUAI DENGAN PROVIDER
    s_on();
    s1_on();
    apn(operators);

    //CONNECTION TYPE
    displaydate();
    Serial1.println(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""));
    bacaserial(200);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);

    //APN ID
    displaydate();
    s_on();
    s1_on();
    result = "AT+SAPBR=3,1,\"APN\",\"" + APN + "\"";
    Serial1.println(result);
    Serial1.flush();
    bacaserial(200);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);

    //APN USER
    displaydate();
    s_on();
    s1_on();
    result = "AT+SAPBR=3,1,\"USER\",\"" + USER + "\"";
    Serial1.println(result);
    bacaserial(200);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);

    //APN PASSWORD
    displaydate();
    s_on();
    s1_on();
    result = "AT+SAPBR=3,1,\"PWD\",\"" + PWD + "\"";
    Serial1.println(result);
    bacaserial(200);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);

    //OPEN BEARER
    displaydate();
    s_on();
    s1_on();
    Serial1.println(F("AT+SAPBR=1,1"));
    bacaserial(5000);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);

    //QUERY BEARER
    displaydate();
    s_on();
    s1_on();
    Serial1.println(F("AT+SAPBR=2,1"));
    bacaserial(3000);

    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);

    //TERMINATE HTTP SERVICE
    displaydate();
    Serial1.println(F("AT+HTTPTERM"));
    bacaserial(200);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
    Serial1.println(F("AT+HTTPINIT"));
    bacaserial(200);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);

    //SET HTTP PARAMETERS VALUE
    displaydate();
    Serial1.println(F("AT+HTTPPARA=\"CID\",1"));
    Serial1.flush();
    bacaserial(200);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);

    // send data measurement to server
    //SET HTTP URL
    s_on();
    s1_on();
    result = "AT+HTTPPARA=\"URL\",\"" + server + url + "\"";
    Serial.println(result);
    Serial1.println(result);
    bacaserial(1500);
    Serial.flush();
    Serial1.flush();
    //http://www.mantisid.id/api/product/pdam_dt_c.php?="Data":"'2017-11-05 10:00:00', '111.111111', '-6.2222222', '400.33', '34.00', '5.05', '1.66', 'pdam_001', '5', '3'"
    //Formatnya Date, longitude, latitude, pressure, temperature, volt, ampere, source, id, burst interval, data interval
    y = "{\"Data\":\"'";
    y += String(tahun) + "-";
    if (bulan < 10) {
      y += "0" + String(bulan) + "-";
    }
    if (bulan >= 10) {
      y += String(bulan) + "-";
    }
    if (hari < 10) {
      y += "0" + String(hari) + " ";
    }
    if (hari >= 10) {
      y += String(hari) + " ";
    }
    if (jam < 10) {
      y += "0" + String(jam) + ":";
    }
    if (jam >= 10) {
      y += String(jam) + ":";
    }
    if (menit < 10) {
      y += "0" + String(menit) + ":";
    }
    if (menit >= 10) {
      y += String(menit) + ":";
    }
    if (detik < 10) {
      y += "0" + String(detik);
    }
    if (detik >= 10) {
      y += String(detik);
    }

    y += "','" + String(longitude, 4);
    y += "','" + String(latitude, 4) + "','";
    y += String(tekanan, 2) + "','";
    y += String(temp, 2) + "','";
    y += String(volt, 2) + "','";
    y += String(humid, 2) + "','";
    y += String(source) + "','";
    y += String(ID) + "','";
    y += String(burst) + "','";
    y += String(interval) + "'\"}";


    //SET HTTP DATA FOR SENDING TO SERVER
    displaydate();
    s_on();
    s1_on();
    power_timer0_enable();
    result = "AT+HTTPDATA=" + String(y.length() + 1) + ",15000";
    Serial.println(result);
    Serial1.println(result);
    Serial.flush();
    Serial1.flush();
    while (Serial1.available() > 0) {
      while (Serial1.find("DOWNLOAD") == false) {
      }
    }
    bacaserial(500);

    //SEND DATA
    displaydate();
    Serial.println(F("KIRIM DATANYA"));
    Serial.println(y);
    Serial1.println(y);
    Serial.flush();
    Serial1.flush();
    bacaserial(1000);
    Serial.flush();
    Serial1.flush();

    //HTTP METHOD ACTION
    displaydate();
    filename = "";
    power_timer0_enable();
    s_on();
    s1_on();
    start = millis();
    Serial.println(F("AT+HTTPACTION=1"));
    Serial1.println(F("AT+HTTPACTION=1"));
    Serial.flush();
    Serial1.flush();
    while (Serial1.available() > 0) {
      while (Serial1.find("OK") == false) {
        // if (Serial1.find("ERROR")) {
        // goto serve;
        // }
      }
    }
    Serial.flush();
    Serial1.flush();
    a = '0';
    b = '0';
    result = "";
    //CHECK KODE HTTPACTION
    while ((start + 30000) > millis()) {
      while (Serial1.available() > 0) {
        g = Serial1.read();
        result += g;
        Serial.print(g);
        a = result.indexOf(":");
        b = result.length();
        if (b - a > 8) {
          //Serial.println(F("keluar yuk"));
          break;
        }
      }
      if (b - a > 8) {
        //Serial.println(F("hayuk"));
        break;
      }
    }
    Serial.flush();
    Serial1.flush();

    displaydate();
    a = '0';
    b = '0';
    //LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
    s_on();
    s1_on();
    Serial1.println(F("AT+HTTPTERM"));
    bacaserial(200);
    Serial.flush();
    Serial1.flush();
    //LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
    displaydate();
    s_on();
    s1_on();
    Serial.println(F("AT+SAPBR=0,1"));
    Serial1.println(F("AT+SAPBR=0,1"));
    bacaserial(200);
    a = '0';
    a = result.indexOf(',');
    b = result.indexOf(',', a + 1);
    kode = result.substring(a + 1, b).toInt();

    displaydate();
    statuscode(kode);
    Serial.print(F("kode="));
    Serial.print(kode);
    Serial.print(F(" network="));
    Serial.println(network);
    Serial.flush();
  }
  else {
    network = "Error";
    kode = 999;
    s_on();
    Serial.println(F("NETWORK ERROR"));
    Serial.flush();
    s_off();
  }
  displaydate();
}

void sendSMS() {
  s_on();
  s1_on();
  power_timer0_enable();

  Serial.println(F("AT+CMGF = 1"));
  Serial1.println(F("AT+CMGF=1"));
  bacaserial(200);
  LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
  Serial.println(F("AT+CSCS=\"GSM\""));
  Serial.println(F("AT+CSCS=\"GSM\""));
  bacaserial(200);
  LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);

  i_En(oled);
  display.getTextBounds(F("Sending SMS"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.print(F("Sending SMS"));
  display.display();
  y = "AT+CMGS=\"" + noHP + "\"";
  Serial.println(y);
  Serial1.println(y);

  while (Serial1.find(">") == false) {
  }

  LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);

  // start = millis();
  y = "I-GAUGE LOGGER ID " + ID + " ready send data to server";
  Serial.println(y);
  Serial1.println(y);
  //delay(1000);
  Serial1.println((char)26);

  //WAITING OK
  a = 0;
  while (Serial1.available() > 0) {
    while (Serial1.find("OK") == false) {
      if (Serial1.find("OK") == true) {
        a = 1;
        Serial.println(F("OK receive"));
        break;
      }
    }
    if (a == 1) {
      Serial.println(F("OK getting out"));
      break;
    }
  }

  Serial.println(F("sms has been sent"));
  Serial.flush();
  Serial1.flush();
  display.getTextBounds(F("                  "), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.print(F("                  "));
  display.display();
  display.getTextBounds(F("SMS sent"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.print(F("SMS sent"));
  display.display();

  //SIM sleep mode
  s1_on();
  s_on();
  Serial.println(F("AT+CSCLK=2"));
  Serial.flush();
  Serial1.println(F("AT+CSCLK=2"));
  Serial1.flush();
  off();

}



/* 
  {"Data":"'2019-02-26 23:11:20','99.9875','999.9123','10.00','26.00','0.78','80.0000','MEGA2560','BOGOR06','0','0'"}

  AT+CIPSHUT
  AT+SAPBR=0,1
  AT+SAPBR=3,1,"CONTYPE","GPRS"
  AT+SAPBR=3,1,"APN","3data"
  AT+SAPBR=3,1,"USER","3data"
  AT+SAPBR=3,1,"PWD","3data"
  AT+SAPBR=1,1
  AT+SAPBR=2,1
  AT+HTTPTERM
  AT+HTTPINIT
  AT+HTTPPARA="CID",1
  AT+HTTPPARA="URL","http://www.mantisid.id/api/product/pdam_dt_c.php"
  AT+HTTPDATA=115,15000
  {"Data":"'2019-02-27 22:59:33','109.9875','-7.9123','0.16','26.00','0.76','69.0000','MEGA2560','BOGOR06','2','5'"}
  AT+HTTPACTION=1
  AT+HTTPTERM
  AT+SAPBR=0,1
*/




