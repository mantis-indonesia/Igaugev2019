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


  problem:
  18-2-2019 cannot be uploaded through icsp

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

String ID = "BOGOR06";
String source = "MEGA2560";
float offset = 0.0;
#define DHTPIN          9
#define DHTTYPE         DHT11
#define oled           0x3c
#define ads_addr      0x48
#define rtc_addr      0x68
#define panjang       128
#define lebar         64
#define pres          0
#define pres1         1
#define tegangan      3
#define connectPres     4
#define connectPres1    6
#define SCKpin        52
#define MISOpin       50
#define MOSIpin       51
#define SSpin         53
//#define ce            48
//#define csn           49
//server
String url = "/api/product/pdam_dt_c.php";


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
int i, kode;
char sdcard[60];
char g;
byte a, b, c, interval, burst;
char str[13];
unsigned long reads = 0; //pressure
unsigned long waktu, start;
float tekanan, temp, humid, volt;
String y, network, APN, USER, PWD, sms, kuota, noHP, operators, result;

void setup() {
  //initialisation
  Serial.begin(9600);  // Serial USB
  Serial1.begin(9600);  // SIM900A
  pinMode(connectPres, INPUT);
  pinMode(connectPres1, INPUT);

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
  display.getTextBounds(F("WIFI OSH"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 17);
  display.println(F("WIFI OSH"));
  display.getTextBounds(F("WATER PRESSURE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 29);
  display.println(F("WATER PRESSURE"));
  display.getTextBounds(F("SENSOR DEVICE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 40);
  display.println(F("SENSOR DEVICE"));
  display.getTextBounds(F("2019"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((panjang - w) / 2, 52);
  display.println(F("2019"));
  display.display();
  i_Dis();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

  //display ID unit
  hapusmenu(52, 64);
  display.setTextColor(WHITE, BLACK);
  display.getTextBounds((char*)ID.c_str(), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(ID);
  display.display();
  i_Dis();
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);

  //RTC INIT
  i_En(oled);
  i_En(rtc_addr);
  hapusmenu(17, 64);
  display.getTextBounds(F("INIT CLOCK"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT CLOCK"));
  display.display();
  LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
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
    display.getTextBounds(F("LOST POWER!!"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.println(F("LOST POWER!!"));
    display.getTextBounds(F("CONTACT CS."), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.println(F("CONTACT CS."));
    display.display();
    off();
    while (1);
  }

  for (i = 1; i < 4; i++) {
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
    LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
  }

  //SD INIT
  hapusmenu(17, 64);
  display.getTextBounds(F("init SD Card..."), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT SD Card..."));
  display.display();
  pinMode(SSpin, OUTPUT);
  digitalWrite(SSpin, HIGH);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  spiEn();
  delay(100);
  if (!SD.begin(SSpin)) { //SD ERROR
    s_on();
    Serial.println(F("SD init error!!!"));
    Serial.flush();
    display.getTextBounds(F("SD Card Error!!!"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.println(F("SD Card Error!!!"));
    display.display();
    off();
    while (1);
  }

  s_on();
  Serial.println(F("SD CARD INIT OK!"));
  Serial.flush();
  display.getTextBounds(F("SD Card OK!"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.println(F("SD Card OK!"));
  display.display();
  off();
  delay(3000);

  //GET CONFIG TXT
  hapusmenu(17, 64);
  display.getTextBounds(F("CONFIGURATION FILE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("CONFIGURATION FILE"));
  display.display();
  pinMode(SSpin, OUTPUT);
  digitalWrite(SSpin, HIGH);
  spiEn();
  LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
  configs();
  delay(1500);
  off();

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
  off();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
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
  off();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  //---------------------------------------------------------------
  //DISPLAY NO HP
  hapusmenu(40, 64);
  display.getTextBounds(F("NO HP"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.print(F("NO HP"));
  display.getTextBounds(F("081234567898"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 52);
  display.print(noHP);
  display.display();
  off();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
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
  display.getTextBounds(F("CEK PRESSURE SENSOR"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("CEK PRESSURE SENSOR"));
  display.display();
  LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
  //cek via ads1115
  reads = digitalRead(connectPres);
  Serial.println(reads);

  if (reads == 0) {
    Serial.println("pressure sensor 1 not connected");
    Serial.flush();
    display.getTextBounds(F("Pres sensor not connected"), 0, 0, &posx, &posy, &w, &h);
    display.setCursor((128 - w) / 2, 40);
    display.println(F("Pres sensor not connected"));
    display.display();
    off();
    while (1);
  }

  Serial.println("pressure sensor 1 connected");
  display.getTextBounds(F("Pres sensor connected"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.println(F("Pres sensor connected"));
  display.display();
  off();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

  //DHT INIT
  s_on();
  Serial.println(F("INIT DHT11"));
  Serial.flush();
  hapusmenu(17, 64);
  delay(100);
  dht.begin();
  display.getTextBounds(F("INIT DHT11"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT DHT11"));
  display.display();
  LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
  display.getTextBounds(F("DHT READY!"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.println(F("DHT READY!"));
  display.display();
  off();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

  //INIT SIM800L & SEND SMS
  i_En(oled);
  s_on();
  Serial.println(F("INITIALIZATION SIM900A..."));
  Serial.flush();
  s_off();
  hapusmenu(17, 64);
  display.getTextBounds(F("INIT GSM"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("INIT GSM"));
  display.display();
  sim();

  //CEK KUOTA
   /*
  hapusmenu(17, 64);
  display.getTextBounds(F("CHECK BALANCE"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.println(F("CHECK BALANCE"));
  display.display();
  i_En(oled);
  display.getTextBounds(F("*888#3#2"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 30);
  display.print(F("*888#3#2"));
  display.display();
  //cekkuota();
  i_En(oled);
  display.getTextBounds(F("Finish!!!"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 40);
  display.print(F("Finish!!!"));
  display.display();
  off();
*/

  display.clearDisplay();
  displaydate();
  nows = rtc.now();
  setTime(nows.hour(), nows.minute(), nows.second(), nows.month(), nows.day(), nows.year());
  Alarm.timerRepeat(60, displaydate);
  Alarm.timerRepeat(60 * interval, ambil);
 // Alarm.alarmRepeat(5, 0, 0, cekkuota); // 5:00am every day

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
  reads = '0'; tekanan = '0'; temp = '0'; humid = '0'; volt = '0';
  y = '0'; filename = ""; y = "";
}

void ambil() {
  on();
  Alarm.delay(0);
  sekarang = rtc.now();
  waktu = sekarang.unixtime();
  bersihdata();

  // burst sampling
  for (i = 0; i < burst; i++) {
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
  display.fillRect(40, 17, 32, 64, BLACK); //clear display
  display.display();
  display.setCursor(40, 17);  display.print(tekanan, 1);
  display.setCursor(40, 30);  display.print(temp, 1);
  display.setCursor(40, 40);  display.print(humid, 1);
  display.setCursor(40, 50);  display.print(volt, 1);
  display.display();

  //SIMPAN DATA
  simpandata();

  //KIRIM data
  s_on();
  Serial.println(F("SEND DATA TO SERVER"));
  Serial.flush();
  s_off();
  server(1);
  //kode network
  spiEn();
  file = SD.open(filename, FILE_WRITE);
  file.print(kode);
  file.print(",");
  file.println(network);
  delay(100);
  file.close();

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
    off();
    if (start < ((interval * 60) - 15)) {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
      LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
    }
    else {
      break;
    }
  }
  i_En(rtc_addr);
  nows = rtc.now();
  setTime(nows.hour(), nows.minute(), nows.second(), nows.month(), nows.day(), nows.year());
  Serial.println("ready to get data");

}

void simpandata() {
  spiEn();
  SdFile::dateTimeCallback(dateTime);
  filename = "";
  filename = String(nows.year());
  if (nows.month() < 10) {
    filename += "0" + String(nows.month());
  }
  if (nows.month() >= 10) {
    filename += String(nows.month());
  }
  if (nows.day() < 10) {
    filename += "0" + String(nows.day());
  }
  if (nows.day() >= 10) {
    filename += String(nows.day());
  }
  filename += ".txt";

  file = SD.open(filename);
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
    file.close();
  }

  y = "";
  y = String(nows.year()) + '/';
  if (nows.month() < 10) {
    y += "0" + String(nows.month());
  }
  if (nows.month() >= 10) {
    y += String(nows.month());
  }
  y += "/";
  if (nows.day() < 10) {
    y += "0" + String(nows.day());
  }
  if (nows.day() >= 10) {
    y += String(nows.day());
  }
  y += " ";
  if (nows.hour() < 10) {
    y += "0" + String(nows.hour());
  }
  if (nows.hour() >= 10) {
    y += String(nows.hour());
  }
  y += ":";
  if (nows.minute() < 10) {
    y += "0" + String(nows.minute());
  }
  if (nows.minute() >= 10) {
    y += String(nows.minute());
  }
  y += ":";
  if (nows.second() < 10) {
    y += "0" + String(nows.second());
  }
  if (nows.second() >= 10) {
    y += String(nows.second());
  }
  y += "," + String(ID) + "," + String(tekanan, 2) + "," + String(temp, 2);
  y += "," +  String(humid, 2) + "," + String(volt, 2) + "," + String(burst) + "," + String(interval) + "," ;

  Serial.println(y);
  file.print(y);
  delay(100);
  file.close();
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

  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  interval = filename.substring(a + 1, b).toInt();

  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  burst = filename.substring(a + 1, b).toInt();

  a = filename.indexOf("=", b + 1);
  b = filename.indexOf("\r", a + 1);
  noHP = filename.substring(a + 1, b);
  filename = '0';

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
  if (nama == "EXCELCOM") {
    APN = "internet";
    USER = "";
    PWD = "";
  }
  if (nama == "THREE") {
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
  Serial.println(F("AT+COPS?"));
  Serial1.println(F("AT+COPS?"));
  delay(100);
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
  off();

  a = filename.indexOf('"');
  b = filename.indexOf('"', a + 1);
  y = filename.substring(a + 1, b);
  if (y == "51089") y = "THREE";

  operators = y;
  //option if not register at network
  if (operators == "")  {
    c++;
    if (c == 9) {
      i_En(oled);
      display.getTextBounds(F("NO OPERATOR FOUND"), 0, 0, &posx, &posy, &w, &h);
      display.setCursor((128 - w) / 2, 40);
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
  Serial1.println(F("AT+CSQ"));
  delay(100);
  while (Serial1.available() > 0) {
    if (Serial1.find("+CSQ: ")) {
      while (Serial1.available() > 0) {
        g = Serial1.read();
        filename += g;
      }
    }
  }
  Serial1.flush();

  a = (filename.substring(0, filename.indexOf(','))).toInt();
  Serial.print(a);
  Serial.print(" ");
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
}

void sim() { //udah fix
  power_timer0_enable();
  s_on();
  s1_on();
  i_En(oled);

  Serial.println(F("AT+CSCLK=0"));
  Serial1.println(F("AT+CSCLK=0"));
  bacaserial(100);
  Serial.flush();
  Serial1.flush();

  //CONNECT AT
  for (a = 0; a < 6; a++) {
    b = ConnectAT(F("AT"), 100);
    if (b == 8) {
      display.getTextBounds(F("GSM MODULE OK!!"), 0, 0, &posx, &posy, &w, &h);
      display.setCursor((128 - w) / 2, 30);
      display.println(F("GSM MODULE OK!!"));
      display.display();
      Serial.println(F("GSM MODULE OK!!"));
      Serial.flush();
      Serial1.flush();
      break;
    }
    if (b < 8) {
      display.getTextBounds(F("GSM MODULE ERROR!!"), 0, 0, &posx, &posy, &w, &h);
      display.setCursor((128 - w) / 2, 30);
      display.println(F("GSM MODULE ERROR!!"));
      display.display();
      Serial.println(F("SIM800L ERROR"));
      Serial.flush();
      Serial1.flush();

      if (a == 5) {
        display.getTextBounds(F("CONTACT CS!!!"), 0, 0, &posx, &posy, &w, &h);
        display.setCursor((128 - w) / 2, 40);
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

  //CEK SIM
  hapusmenu(30, 64);
  display.getTextBounds(F("OPERATOR"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 30);
  display.println(F("OPERATOR"));
  display.display();
  off();
  ceksim();
  i_En(oled);
  display.getTextBounds((char*)operators.c_str(), 0, 0, &posx, &posy, &w, &h); //(char*)WiFi.SSID().c_str()
  display.setCursor((128 - w) / 2, 40);
  display.println(operators);
  display.display();
  off();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

  //CHECK SIGNAL QUALITY
  hapusmenu(30, 64);
  display.getTextBounds(F("SIGNAL"), 0, 0, &posx, &posy, &w, &h);
  display.setCursor((128 - w) / 2, 30);
  display.println(F("SIGNAL"));
  display.display();
  off();
  sinyal();
  i_En(oled);
  display.getTextBounds((char*)network.c_str(), 0, 0, &posx, &posy, &w, &h); //(char*)WiFi.SSID().c_str()
  display.setCursor((128 - w) / 2, 40);
  display.println(network);
  display.display();
  off();
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

  //SIM800L sleep mode
  s1_on();
  s_on();
  Serial.println(F("AT+CSCLK=2"));
  Serial.flush();
  Serial1.println(F("AT+CSCLK=2"));
  Serial1.flush();
  off();
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

void server(byte t) {
  //CHECK GPRS ATTACHED OR NOT
serve:
  filename = "";
  s_on();
  s1_on();
  power_timer0_enable();
  Serial.print(F("AT+CGATT? "));
  Serial1.println(F("AT+CGATT?"));
  Serial.flush();
  Serial1.flush();
  delay(100);
  while (Serial1.available() > 0) {
    if (Serial1.find("+CGATT: ")) {
      while (Serial1.available() > 0) {
        g = Serial1.read();
        filename += g;
      }
    }
  }

  Serial.println(filename);
  Serial.flush();
  Serial1.flush();
  if (filename.toInt() == 1) {
    //kirim data ke server
    s_on();
    s1_on();
    Serial1.println(F("AT+CIPSHUT"));
    bacaserial(100);
    Serial.flush();
    Serial1.flush();

    //ATUR APN SESUAI DENGAN PROVIDER
    s_on();
    s1_on();
    apn(operators);

    //CONNECTION TYPE
    Serial1.println(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""));
    bacaserial(100);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);

    //APN ID
    s_on();
    s1_on();
    result = "AT+SAPBR=3,1,\"APN\",\"" + APN + "\"";
    Serial1.println(result);
    bacaserial(100);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);

    //APN USER
    s_on();
    s1_on();
    result = "AT+SAPBR=3,1,\"USER\",\"" + USER + "\"";
    Serial1.println(result);
    bacaserial(100);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);

    //APN PASSWORD
    s_on();
    s1_on();
    result = "AT+SAPBR=3,1,\"PWD\",\"" + PWD + "\"";
    Serial1.println(result);
    bacaserial(100);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);

    //OPEN BEARER
    s_on();
    s1_on();
    Serial1.println(F("AT+SAPBR=1,1"));
    bacaserial(1000);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);

    //QUERY BEARER
    s_on();
    s1_on();
    Serial.println(F("AT+SAPBR=2,1"));
    Serial1.println(F("AT+SAPBR=2,1"));
    power_timer0_enable();
    start = millis();
    while (start + 2000 > millis()) {
      while (Serial1.available() > 0) {
        Serial.print(Serial1.read());
        if (Serial1.find("OK")) {
          i = 5;
          break;
        }
      }
      if (i == 5) {
        break;
      }
    }
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);

    //TERMINATE HTTP SERVICE
    s_on();
    s1_on();
    Serial1.println(F("AT+HTTPTERM"));
    bacaserial(100);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
    s_on();
    s1_on();
    Serial1.println(F("AT+HTTPINIT"));
    bacaserial(100);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);

    //SET HTTP PARAMETERS VALUE
    s_on();
    s1_on();
    Serial1.println(F("AT+HTTPPARA=\"CID\",1"));
    bacaserial(100);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
    if (t == 1) { // send data measurement to server
      //SET HTTP URL
      s_on();
      s1_on();
      Serial.println(F("AT+HTTPPARA=\"URL\",\"http://www.mantisid.id/api/product/pdam_dt_c.php\""));
      Serial1.println(F("AT+HTTPPARA=\"URL\",\"http://www.mantisid.id/api/product/pdam_dt_c.php\""));
      bacaserial(1000);
      Serial.flush();
      Serial1.flush();
      //http://www.mantisid.id/api/product/pdam_dt_c.php?="Data":"'2017-11-05 10:00:00', '111.111111', '-6.2222222', '400.33', '34.00', '5.05', '1.66', 'pdam_001', '5', '3'"
      //"Data":"'2018-02-13 20:30:00','111.111111','-6.2222222','400.33','34.00','5.05','1.6645','BOGOR05','5','3'"
      //Formatnya Date, longitude, latitude, pressure, temperature, volt, ampere, source, id, burst interval, data interval
      y = "{\"Data\":\"'";
      y += String(sekarang.year()) + "-";
      if (sekarang.month() < 10) {
        y += "0" + String(sekarang.month()) + "-";
      }
      if (sekarang.month() >= 10) {
        y += String(sekarang.month()) + "-";
      }
      if (sekarang.day() < 10) {
        y += "0" + String(sekarang.day()) + " ";
      }
      if (sekarang.day() >= 10) {
        y += String(sekarang.day()) + " ";
      }
      if (sekarang.hour() < 10) {
        y += "0" + String(sekarang.hour()) + ":";
      }
      if (sekarang.hour() >= 10) {
        y += String(sekarang.hour()) + ":";
      }
      if (sekarang.minute() < 10) {
        y += "0" + String(sekarang.minute()) + ":";
      }
      if (sekarang.minute() >= 10) {
        y += String(sekarang.minute()) + ":";
      }
      if (sekarang.second() < 10) {
        y += "0" + String(sekarang.second());
      }
      if (sekarang.second() >= 10) {
        y += String(sekarang.second());
      }
      y += "','";
      y += "'99.9875,'";
      y += "'999.9123,'";
      y += String(tekanan, 2) + "','";
      y += String(temp, 2) + "','";
      y += String(volt, 2) + "','";
      y += String(humid, 4) + "','";
      y += String(source) + "','";
      y += String(ID) + "','";
      y += String(burst) + "','";
      y += String(interval) + "'\"}";
    }
    if (t == 2) { // send data kuota to server
      //GET TIME
      i_En(rtc_addr);
      nows = rtc.now();
      i_Dis();

      //SET HTTP URL
      s1_on();
      s_on();
      //Serial.println(F("AT+HTTPPARA=\"URL\",\"http://www.mantisid.id/api/product/pdam_sim_c.php\""));
      Serial1.println(F("AT+HTTPPARA=\"URL\",\"http://www.mantisid.id/api/product/pdam_sim_c.php\""));
      bacaserial(1000);
      Serial.flush();
      Serial1.flush();
      //http://www.mantisid.id/api/product/pdam_sim_c.php
      //Formatnya YYY-MM-DD HH:MM:SS, ID, PULSA, KUOTA
      y = "{\"Data\":\"'";
      y += String(nows.year()) + "-";
      if (nows.month() < 10) {
        y += "0" + String(nows.month()) + "-";
      }
      if (nows.month() >= 10) {
        y += String(nows.month()) + "-";
      }
      if (nows.day() < 10) {
        y += "0" + String(nows.day()) + " ";
      }
      if (nows.day() >= 10) {
        y += String(nows.day()) + " ";
      }
      if (nows.hour() < 10) {
        y += "0" + String(nows.hour()) + ":";
      }
      if (nows.hour() >= 10) {
        y += String(nows.hour()) + ":";
      }
      if (nows.minute() < 10) {
        y += "0" + String(nows.minute()) + ":";
      }
      if (nows.minute() >= 10) {
        y += String(nows.minute()) + ":";
      }
      if (nows.second() < 10) {
        y += "0" + String(nows.second());
      }
      if (nows.second() >= 10) {
        y += String(nows.second());
      }
      y += "','";
      y += String(ID) + "','";
      y += String(sms) + "','";
      y += String(kuota) + "'\"}";

      //simpan data sisa pulsa dan kuota ke dalam sd card
      result = "pulsa.ab";
      result.toCharArray(str, 13);
      // set date time callback function
      spiEn();
      delay(10);
      SdFile::dateTimeCallback(dateTime);
      delay(10);
      SD.begin(SSpin);
      file = SD.open(str, FILE_WRITE);
      file.println(y);
      Serial.println(y);
      Serial.flush();
      file.flush();
      file.close();
      spiDis();
    }
    //SET HTTP DATA FOR SENDING TO SERVER
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

    //SEND DATA
    s_on();
    s1_on();
    Serial.println(y);
    Serial1.println(y);
    Serial.flush();
    Serial1.flush();
    bacaserial(1000);

    //HTTP METHOD ACTION
    filename = "";
    power_timer0_enable();
    s_on();
    s1_on();
    start = millis();
    Serial1.println(F("AT+HTTPACTION=1"));
    while (Serial1.available() > 0) {
      while (Serial1.find("OK") == false) {
        if (Serial1.find("ERROR")) {
          goto serve;
        }
      }
    }
    Serial.flush();
    Serial1.flush();
    a = '0';
    b = '0';
    //CHECK KODE HTTPACTION
    while ((start + 30000) > millis()) {
      while (Serial1.available() > 0) {
        g = Serial1.read();
        filename += g;
        Serial.print(g);
        a = filename.indexOf(":");
        b = filename.length();
        if (b - a > 12)break;
      }
      if (b - a > 12) break;
    }
    Serial.println();
    Serial.flush();
    Serial1.flush();
    a = '0';
    b = '0';
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
    s_on();
    s1_on();
    Serial1.println(F("AT+HTTPTERM"));
    bacaserial(200);
    Serial.flush();
    Serial1.flush();
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
    s_on();
    s1_on();
    Serial.println(F("AT+SAPBR=0,1"));
    Serial1.println(F("AT+SAPBR=0,1"));
    Serial.flush();
    Serial1.flush();
    while (start + 1000 > millis()) {
      while (Serial1.available() > 0) {
        if (Serial1.find("OK")) {
          a = 1;
          break;
        }
      }
      if (a = 1) break;
    }
    a = '0';
    a = filename.indexOf(',');
    b = filename.indexOf(',', a + 1);
    kode = filename.substring(a + 1, b).toInt();
    statuscode(kode);
  }
  else {
    network = "Error";
    kode = 999;
    s_on();
    Serial.println(F("NETWORK ERROR"));
    Serial.flush();
    s_off();
  }
}

void cekkuota() {
  c = 0;
  s_on();
  s1_on();
  Serial1.println("AT");
  Serial.flush();
  Serial1.flush();
  bacaserial(100);
  off();
  LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
  s_on();
  s1_on();
  Serial1.println("AT+CSCLK=0");
  Serial.flush();
  Serial1.flush();
  off();
  LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
  s_on();
  s1_on();
  Serial1.println("AT+CSCLK=0");
  bacaserial(100);
  Serial.flush();
  Serial1.flush();
  off();
  LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
top:
  if (c == 5)  {
    sms = "sisa pulsa tidak diketahui";
    kuota = "sisa kuota tidak diketahui";
    goto down;
  }
  sms = "";
  kuota = "";

  power_timer0_enable();
  s_on();
  s1_on();
  Serial1.println("AT+CUSD=2");
  bacaserial(100);
  Serial.flush();
  Serial1.flush();
  off();
  LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);

  s_on();
  s1_on();
  Serial1.println("AT+CUSD=1");
  bacaserial(100);
  Serial.flush();
  Serial1.flush();
  off();
  LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
  sms = "";
  power_timer0_enable();
  s_on();
  s1_on();
  start = millis();
  Serial1.println("AT+CUSD=1,\"*888#\"");
  Serial1.flush();

  while ((start + 10000) > millis()) {
    while (Serial1.available() > 0) {
      g = Serial1.read();
      Serial.write(g);
      sms += g;
    }
  }
  Serial.flush();
  Serial1.flush();
  a = sms.indexOf(':');
  b = sms.indexOf('"', a + 1);
  i = sms.substring(a + 1, b - 1).toInt();
  Serial.print("+CUSD: ");
  Serial.println(i);
  if (i == 0) {
    c++;
    goto top;
  }
  a = sms.indexOf(':');
  b = sms.indexOf('.', a + 1);
  b = sms.indexOf('.', b + 1);
  c = sms.indexOf('/', a);
  kuota = sms.substring(c - 11, c + 8);
  Serial.println(kuota);
  sms = sms.substring(a + 5, b);
  sms = sms + '.' + kuota;
  Serial.println(sms);
  Serial.flush();

  LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
  start = millis();
  Serial1.println("AT+CUSD=1,\"3\"");
  while ((start + 5000) > millis()) {
    while (Serial1.available() > 0) {
      g = Serial1.read();
      Serial.write(g);
      kuota += g;
    }
  }
  Serial.flush();
  Serial1.flush();
  a = kuota.indexOf(':');
  b = kuota.indexOf('"', a + 1);
  i = kuota.substring(a + 1, b - 1).toInt();
  if (i == 0) {
    goto top;
  }
  //LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);

  kuota = "";
  start = millis();
  LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
  Serial1.println("AT+CUSD=1,\"2\"");
  while ((start + 5000) > millis()) {
    while (Serial1.available() > 0) {
      g = Serial1.read();
      Serial.write(g);
      kuota += g;
    }
  }
  Serial.flush();
  Serial1.flush();
  a = kuota.indexOf(':');
  b = kuota.indexOf('"', a + 1);
  i = kuota.substring(a + 1, b - 1).toInt();
  if (i == 0) {
    goto top;
  }
  a = kuota.indexOf(':');
  b = kuota.indexOf('.', a + 1);
  a = kuota.indexOf(':', a + 1);
  kuota = kuota.substring(a + 2, b);

down:
  a = 0; b = 0; i = 0;
  Serial.flush();
  Serial1.flush();
  Serial1.println("AT+CUSD=2");
  bacaserial(200);
  Serial.flush();
  Serial1.flush();
  Serial.println(sms);
  Serial.print(F("KUOTA : "));
  Serial.println(kuota);
  Serial.flush();
  Serial1.flush();
  server(2); //send kuota to server
  s_on();
  Serial.println(F("cek kuota selesai"));
  Serial.flush();
  s_off();
}








