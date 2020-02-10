//LIBRARY
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <DS3232RTC.h>        
#include <LowPower.h>
#include <SoftwareSerial.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADS1015.h>
#include <Wire.h>

SoftwareSerial serial1(6, 7); // RX, TX

//konfigurasi alat
char ID[5] = "0007";
float offset = 0.0;
String source = "GSM";

//konstanta
#define burst 5
#define interval 3
#define baud 9600

//alamat
#define ads_addr    0x48
#define rtc_addr    0x68
String url = "";

//definisi pin
#define wakeupPin 2
#define pwk        5
#define pres            A1 //channel
#define pres1            A2 //channel
#define tegangan        A0 //channel
#define Rled      3
#define connectPres         8

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

// create variables to restore the SPI & ADC register values after shutdown
byte keep_ADCSRA;
byte keep_SPCR;

//ADS1115
Adafruit_ADS1115 ads(ads_addr);

boolean gsm = 0; // indikator GSM sedang tidur atau aktif
boolean GSMerror = 0; //indikator GSM error atau tidak


const time_t ALARM_INTERVAL(interval * 60);  // alarm interval in seconds
time_t t, alarmTime;

tmElements_t tm;

//global variable
int waktuMenit;
byte pembagiWaktu = 60 / interval;

boolean ada = 0;
int i, kode, tahun;
char karakter;
byte indeksA, indeksB, indeksC;
byte bulan, hari, jam, menit, detik;
unsigned long reads = 0; //pressure
unsigned long reads1 = 0;
unsigned long awal, akhir, start;
float tekanan, volt;
String kalimat, network, APN, USER, PWD, operators, result;


void setup() {
  Serial.begin(baud);
  Serial.println(F("\r\nInisialisasi serial GSM"));
  Serial.flush();
  serial1.begin(baud);  // SIM900A
  pinMode(13, OUTPUT);
  pinMode(wakeupPin, INPUT_PULLUP); // Set interrupt pin

  //WELCOME SCREEN
  Serial.println(F("\r\nGSM IGAUGE PRO MINI 5V 2019"));
  Serial.print(F("Unit = "));
  Serial.println(ID);

  //CEK ADS1115
  Serial.println(F("Cek ADS1115"));
  Serial.print(F("ADS1115 = "));
  Wire.begin();
  Wire.beginTransmission(ads_addr);
  indeksA = Wire.endTransmission();
  if (indeksA == 0) {
    Serial.println("ADS1115 ada");
    ada = 1;
    Serial.println(ada);
  }

  Serial.println(ada);
  Wire.end();

  //CEK RTC
  Serial.println(F("Cek RTC"));
  Wire.begin();
  Wire.beginTransmission(rtc_addr);
  indeksA = Wire.endTransmission();
  if (indeksA == 0) {
    Serial.println(F("RTC ada"));
  }
  if (indeksA != 0) {
    Serial.println(F("RTC Error"));
    digitalWrite(13, HIGH);
    while (1);
  }
  Wire.end();
  setSyncProvider(RTC.get);

  // ATUR PIN YANG TIDAK DIGUNAKAN SEBAGAI OUTPUT LOW
  Serial.println(F("Atur pin digital sebagai output LOW"));
  Serial.flush();
  for (i = 3; i <= 13; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
    if (i == 5) i = 9;
  }

  //CEK APAKAH SENSOR TEKANAN TERHUBUNG
  Serial.print(F("Sensor tekanan "));
  pinMode(connectPres, INPUT);
  indeksA = digitalRead(connectPres);
  if (indeksA == 1) {
    Serial.println(F("tidak terhubung!!!!"));
    Serial.flush();
    power_all_disable ();
    while (1) {
      digitalWrite(13, HIGH);
      LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
      digitalWrite(13, LOW);
      LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
    }
  }

  //nyala kan LED kalau sensor terhubung
  Serial.println(F("terhubung"));
  Serial.flush();
  indikator();

  //waiting 15 second for GSM warm up
  for (indeksA = 1; indeksA <= 15; indeksA++) {
    Serial.print(indeksA);
    Serial.println(F(" detik"));
    Serial.flush();
    indikator();
  }

  Serial.println(F("Cek AT Command"));
  ceksim(); // Cek GSM modul apakah terhubung
  Serial.println(F("Registrasi SIM"));
  regsim(); // registrasi GSM modul ke network

  Serial.println(F("Atur Waktu Interupsi.\r\n I2C set di 400K")) ;
  Wire.setClock(400000) ;

  if (timeStatus() != timeSet) {
    Serial.println(F("Tidak bisa sinkronisasi dengan RTC. Atur menggunakan GSM"));
    waktu(); // ambil data waktu dari RTC network
  }
  else    Serial.println(F("Waktu RTC OK"));

  Serial.flush();
  serial1.end();

  // initialize the alarms to known values, clear the alarm flags, clear the alarm interrupt flags
  Serial.println(F("Clear Alarm RTC"));
  Serial.flush();
  clearRTC();
  tidurSementara();
}

void loop() {
  tidur(); // Enter sleep

  if ( RTC.alarm(ALARM_1) )  {
    detachInterrupt (digitalPinToInterrupt (wakeupPin)); // Disable interrupts on D2
  
  serial1.begin(9600);
    Serial.print("Waktu bangun = ");

    t = RTC.get();
    Serial.print(hour(t));
    Serial.print(":");
    Serial.print(minute(t));
    Serial.print(":");
    Serial.println(second(t));

    hari = day(t);
    bulan = month(t);
    tahun = year(t);
    jam = hour(t);
    menit = minute(t);
    detik = second(t);

    //nyalakan GSM jika mati
    if (gsm == 1) {
      Serial.println(F("Nyalakan GSM"));
      GSMreset();
    }

    ceksim(); // Cek GSM modul apakah terhubung
    regsim(); // registrasi GSM modul ke network

    bersihdata();

    //AMBIL DATA SENSOR TEKANAN
    Serial.println(F("ambil data sensor tekanan"));
    Serial.flush();

    //burst data
    for (i = 0; i < burst; i++) {
      digitalWrite(13, HIGH);
      if (ada == 0)    reads1 = analogRead(pres); //pressure
      if (ada > 0) reads1 = ads.readADC_SingleEnded(0);
      if (reads1 < 0) {
        reads = reads + 0;
      }
      else {
        reads = reads + reads1;
      }
      delay(300);
      digitalWrite(13, LOW);
      delay(700);
    }

    //tegangan
    if (ada == 0)    reads1 = analogRead(tegangan);
    if (ada > 0) reads1 = ads.readADC_SingleEnded(2);

    //KONVERSI TEKANAN
    volt = (float)reads1 / 1024.00 * 5 ; // nilai voltase dari nilai DN
    tekanan = ((float)reads / (float)burst) / 1024.00 * 5; // nilai voltase dari nilai DN
    Serial.print(F("TEGANGAN SENSOR = "));
    Serial.println(tekanan);
    tekanan = (3.00 * tekanan - 1.5) + float(offset);
    if (tekanan < 0) tekanan = 0;

    //debug value
    Serial.print(F("TEKANAN = "));
    Serial.println(tekanan, 2);
    Serial.print(F("VOLTASE = "));
    Serial.println(volt, 2);
    Serial.flush();

    if (GSMerror == 0) {
      Serial.println(F("Kirim data ke server"));
      sendServer();
    }

    //TURN OFF GSM MODULE
    GSMsleep();

    // calculate the alarm time
    serial1.end();
    delay(1000);
    time_t alarmTime = t + ALARM_INTERVAL;
    Serial.print("Tidur hingga = ");
    Serial.print(hour(alarmTime));
    Serial.print(":");
    Serial.print(minute(alarmTime));
    Serial.print(":");
    Serial.println(second(alarmTime));
  Serial.flush();

    // set the alarm
  clearRTC();
  delay(100);
    RTC.setAlarm(ALM1_MATCH_HOURS, 0, minute(alarmTime), hour(alarmTime), 0);
  RTC.alarm(ALARM_1);                   //ensure RTC interrupt flag is cleared
    RTC.alarmInterrupt(ALARM_1, true);
    RTC.alarmInterrupt(ALARM_2, false);
    RTC.alarm(ALARM_2);                   //ensure RTC interrupt flag is cleared
  delay(100);
  }
}

void wake () {
  sleep_disable ();         // first thing after waking from sleep:
}

void tidur () {
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);   
  noInterrupts ();          // make sure we don't get interrupted before we sleep
  sleep_enable ();          // enables the sleep bit in the mcucr register
  //sleep_bod_disable();
  attachInterrupt (digitalPinToInterrupt (wakeupPin), wake, FALLING);  // wake up on low level on wakeupPin
  interrupts ();           // interrupts allowed now, next instruction WILL be executed
  sleep_cpu ();            // here the device is put to sleep
}  // end of sleepNow

void clearRTC() {
  // initialize the alarms to known values, clear the alarm flags, clear the alarm interrupt flags
  RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
  RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);
  RTC.alarm(ALARM_2);
  RTC.alarmInterrupt(ALARM_1, false);
  RTC.alarmInterrupt(ALARM_2, false);
  RTC.squareWave(SQWAVE_NONE);
}

void tidurSementara() {
  t = RTC.get();
  printDateTime(t) ;

  menit = minute(t);
  Serial.print(F("menit sekarang = "));
  Serial.println(menit);

  Serial.print("Tidur hingga ");
  for (byte i = 0; i < pembagiWaktu; i++) {
    if (menit >= 60 - interval) {
      Serial.print(60 - menit);
      menit = 60 - menit;
      break;
    }
    if (menit >= i * interval && menit < (i + 1) * interval) {
      Serial.print((i + 1) * interval - menit);
      menit = (i + 1) * interval - menit;
      break;
    }
  }
  Serial.print(" menit >> ");

  alarmTime = t + menit * 60;  // calculate the alarm time
  Serial.print(hour(alarmTime));
  Serial.print(":");
  Serial.print(minute(alarmTime));
  Serial.print(":");
  Serial.println("0");
  Serial.flush();

  // set the alarm
  RTC.setAlarm(ALM1_MATCH_HOURS, 0, minute(alarmTime), hour(alarmTime), 0);
  RTC.alarm(ALARM_1); //ensure RTC interrupt flag is cleared
  RTC.alarmInterrupt(ALARM_1, true); // Enable alarm 1 interrupt A1IE
  RTC.alarmInterrupt(ALARM_2, false);
    RTC.alarm(ALARM_2);                   //ensure RTC interrupt flag is cleared
}

void printDateTime(time_t t) {
  Serial.print(day(t));
  Serial.print("/");
  Serial.print(month(t));
  Serial.print("/");
  Serial.print(year(t));
  Serial.print(" ");
  Serial.print(hour(t));
  Serial.print(":");
  Serial.print(minute(t));
  Serial.print(":");
  Serial.println(second(t));
  Serial.flush();
}

void indikator() {
  pinMode(13, OUTPUT);
  for (byte ind = 0; ind < 2; ind++) {
    digitalWrite(13, HIGH);
    delay(250);
    digitalWrite(13, LOW);
    delay(250);
  }
}

//////////////////////////   GSM ////////////////////////////////////////////////
void at() {
  serial1.println(F("AT"));
  serial1.flush();
  bacaserial(300);
}

void ceksim() { ///CONNECT AT
  indeksA = 0; indeksB = 0;
  for (indeksA = 0; indeksA < 20; indeksA++) {
    Serial.print(indeksA);
    Serial.print(" ");
    indeksB = ConnectAT(F("AT"), 200);
    Serial.print(indeksB);
    if (indeksB == 20) {
      Serial.println(F(" GSM MODULE OK!!"));
      Serial.flush();
      GSMerror = 0;
      break;
    }
    if (indeksB < 20) {
      Serial.print(indeksA);
      Serial.println(F(" GSM MODULE ERROR"));
      Serial.flush();
      GSMerror = 1;
      indikator();
    }
    delay(100);
  }
}

void regsim() {
  indeksC = 0;
  result = "";
  //Functionality
  Serial.println(F("Set Functionality"));
  
  serial1.println(F("AT+CFUN=1"));
  serial1.flush();
  bacaserial(300);
  delay(200);

  //Pilih Band
  Serial.println(F("Pilih Band"));
  
  serial1.println(F("AT+CBAND=\"ALL_BAND\""));
  serial1.flush();
  bacaserial(300);
  delay(200);

  //opsi jika tidak terhubung ke network
  indeksC = 0;
  while (indeksC <= 10) {
    Serial.print(indeksC + 1);
    Serial.print(' ');
    sim();
    indeksC++;
    if (operators.length() > 0)  {
      GSMerror = 0;
      
      Serial.println(F("Cek kualitas sinyal"));
      serial1.println(F("AT+CSQ"));
      serial1.flush();
      bacaserial(300);
      break;
    }
    if (indeksC == 10) {
      Serial.println(F("TIDAK ADA OPERATOR"));
      Serial.flush();
      GSMerror = 1;
    }
  }
}

void sim() {
  result = "";
  operators = "";
  //REGISTASI NETWORK
  
  Serial.println(F("Registrasi Network"));
  serial1.println(F("AT+CREG=1"));
  bacaserial(300);
  delay(2000);

  //CARI OPERATOR KARTU
  
  Serial.println(F("Cari operator"));
  serial1.println(F("AT+COPS?"));
  Serial.flush();
  serial1.flush();
  delay(200);
  while (serial1.available() > 0) {
    if (serial1.find("+COPS:")) {
      while (serial1.available() > 0) {
        karakter = serial1.read();
        result += karakter;
      }
    }
  }
  Serial.flush();
  serial1.flush();

  indeksA = result.indexOf('"');
  indeksB = result.indexOf('"', indeksA + 1);
  kalimat = result.substring(indeksA + 1, indeksB);
  if (kalimat == "51089") kalimat = "THREE";

  //Nama operator
  operators = kalimat;
  Serial.println(operators);
  Serial.flush();
  if (operators.length() == 0) GSMerror = 1;
  if (operators.length() > 0) GSMerror = 0;
  delay(200);
}

void waktu() {
  //WAKTU LOKAL BERDASARKAN NETWORK GSM
  Serial.println(F("Minta waktu lokal ke network GSM"));
  
  serial1.println(F("AT+CLTS=1"));
  bacaserial(300);
  delay(200);
  
  serial1.println(F("AT+CCLK?"));
  kalimat = "";
  start = millis();
  while ((start + 200) > millis()) {
    while (serial1.available() > 0) {
      karakter = serial1.read();
      kalimat = kalimat + karakter;
    }
  }
  serial1.flush();
  Serial.println(kalimat);
  Serial.flush();
  delay(200);
  tmElements_t tm;
  //parse string
  indeksB = 0;

  //tahun
  indeksA = kalimat.indexOf("\"", indeksB + 1);
  indeksB = kalimat.indexOf("/", indeksA + 1);
  tahun = 2000 + kalimat.substring(indeksA + 1, indeksB).toInt();

  //bulan
  indeksA = kalimat.indexOf("/", indeksB + 1);
  bulan = kalimat.substring(indeksB + 1, indeksA).toInt();

  //hari
  indeksB = kalimat.indexOf(",", indeksA + 1);
  hari = kalimat.substring(indeksA + 1, indeksB).toInt();

  //jam
  indeksA = kalimat.indexOf(":", indeksB + 1);
  jam = kalimat.substring(indeksB + 1, indeksA).toInt();

  //menit
  indeksB = kalimat.indexOf(":", indeksA + 1);
  menit = kalimat.substring(indeksA + 1, indeksB).toInt();

  //detik
  indeksA = kalimat.indexOf("+", indeksB + 1);
  detik = kalimat.substring(indeksB + 1, indeksA).toInt();

  //  Serial.println(tahun);
  //  Serial.println(bulan);
  //  Serial.println(hari);
  //  Serial.println(jam);
  //  Serial.println(menit);
  //  Serial.println(detik);

  if (tahun >= 2019) {
    tm.Year = CalendarYrToTm(tahun);
    tm.Month = bulan;
    tm.Day = hari;
    tm.Hour = jam;
    tm.Minute = menit;
    tm.Second = detik;
    t = makeTime(tm);
    RTC.set(t);
    setTime(t);
  }

  printDateTime(t);
  Serial.flush();

}

void apn(String nama) {
  if (nama == "TELKOMSEL" || nama == "IND TELKOMSEL" || nama == "TEL") {
    APN = "Telkomsel";
    USER = "wap";
    PWD = "wap123";
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
  if (nama == "IND XL") {
    APN = "axis";
    USER = "axis";
    PWD = "123456";
  }
  if (nama == "THREE" || nama == "3") {
    APN = "3data";
    USER = "3data";
    PWD = "3data";
  }
}

byte ConnectAT(String cmd, int d) {
  i = 0;

  while (1) {
    Serial.println(F("AT"));
    serial1.println(cmd);
    start = millis();
    while ((start + 500) > millis()) {
      while (serial1.available()) {
        if (serial1.find("OK"))
          i = 20;
      }
    }
    delay(d);
    if (i > 5) {
      break;
    }
    i++;
  }
  return i;
}

void bacaserial(int wait) {
  start = millis();
  while ((start + wait) > millis()) {
    while (serial1.available() > 0) {
      karakter = serial1.read();
      Serial.print(karakter);
    }
  }
  serial1.flush();
  Serial.flush();
}

void sendServer() {
  result = "";
  // ATTACH GPRS SERVICE
  
  Serial.println(F("ATTACH GPRS SERVICE"));
  serial1.println(F("AT+CGATT=1"));
  bacaserial(300);
  delay(200);
  result = "1";
 if (result.toInt() == 1) {
    //kirim data ke server
    
    serial1.println(F("AT+CIPSHUT"));
    bacaserial(300);
    Serial.flush();
    serial1.flush();
    delay(200);

    
    serial1.println(F("AT+SAPBR=0,1"));
    bacaserial(300);
    Serial.flush();
    serial1.flush();
    delay(200);

    //ATUR APN SESUAI DENGAN PROVIDER
    apn(operators);

    //CONNECTION TYPE
    
    serial1.println(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""));
    bacaserial(300);
    Serial.flush();
    serial1.flush();
    delay(200);

    //APN ID
    
    result = "AT+SAPBR=3,1,\"APN\",\"" + APN + "\"";
    serial1.println(result);
    bacaserial(300);
    Serial.flush();
    serial1.flush();
    delay(200);

    //APN USER
    
    result = "AT+SAPBR=3,1,\"USER\",\"" + USER + "\"";
    serial1.println(result);
    bacaserial(300);
    Serial.flush();
    serial1.flush();
    delay(200);

    //APN PASSWORD
    
    result = "AT+SAPBR=3,1,\"PWD\",\"" + PWD + "\"";
    serial1.println(result);
    serial1.flush();
    bacaserial(300);
    Serial.flush();
    serial1.flush();
    delay(500);

    //OPEN BEARER
    
    i = 0;

    Serial.println(F("AT+SAPBR=1,1"));
    serial1.println(F("AT+SAPBR=1,1"));
    start = millis();
    while ((start + 15000) > millis()) { //serial1.available() > 0
      if (serial1.find("OK") == true) {
        break;
      }
    }

    Serial.flush();
    serial1.flush();
    delay(500);

    //TERMINATE HTTP SERVICE
    
    Serial.println(F("AT+HTTPTERM"));
    serial1.println(F("AT+HTTPTERM"));
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    delay(200);

    
    Serial.println(F("AT+HTTPINIT"));
    serial1.println(F("AT+HTTPINIT"));
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    delay(200);

    //SET HTTP PARAMETERS VALUE
    
    Serial.println(F("AT+HTTPPARA=\"CID\",1"));
    serial1.println(F("AT+HTTPPARA=\"CID\",1"));
    serial1.flush();
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    delay(200);

    // send data measurement to server
    //SET HTTP URL
    
    result = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
    Serial.println(result);
    serial1.println(result);
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    delay(200);
  kalimat = "{\"Data\":\"" + String(ID) + ",";

    kalimat += String(tahun);
    if (bulan < 10) {
      kalimat += "0" + String(bulan);
    }
    if (bulan >= 10) {
      kalimat += String(bulan);
    }
    if (hari < 10) {
      kalimat += "0" + String(hari);
    }
    if (hari >= 10) {
      kalimat += String(hari);
    }
    if (jam < 10) {
      kalimat += "0" + String(jam);
    }
    if (jam >= 10) {
      kalimat += String(jam);
    }
    if (menit < 10) {
      kalimat += "0" + String(menit);
    }
    if (menit >= 10) {
      kalimat += String(menit);
    }
    if (detik < 10) {
      kalimat += "0" + String(detik);
    }
    if (detik >= 10) {
      kalimat += String(detik);
    }

    kalimat += "," + strTwoDigit(tekanan) + ",-9999,-9999,";
    kalimat += strTwoDigit(volt) + "," + String(burst) + "," ;
    kalimat += String(interval) + "\"}";


    //SET HTTP DATA FOR SENDING TO SERVER
    ;
    result = "AT+HTTPDATA=" + String(kalimat.length() + 1) + ",15000";
    Serial.println(result);
    serial1.println(result);
    Serial.flush();
    serial1.flush();
    while (serial1.available() > 0) {
      while (serial1.find("DOWNLOAD") == false) {
      }
    }
    bacaserial(500);

    //SEND DATA
    Serial.println(F("KIRIM DATANYA"));
    Serial.println(kalimat);
    serial1.println(kalimat);
    Serial.flush();
    serial1.flush();
    bacaserial(500);
    Serial.flush();
    serial1.flush();

    //HTTP METHOD ACTION
    start = millis();
    ;
    Serial.println(F("AT+HTTPACTION=1"));
    serial1.println(F("AT+HTTPACTION=1"));
    Serial.flush();
    serial1.flush();
    while (serial1.available() > 0) {
      while (serial1.find("OK") == false) {
        Serial.write(serial1.read());
        // if (serial1.find("ERROR")) {
        // goto serve;
        // }
      }
    }
    Serial.flush();
    serial1.flush();
    indeksA = '0';
    indeksB = '0';
    result = "";
    //CHECK KODE HTTPACTION
    while ((start + 30000) > millis()) {
      while (serial1.available() > 0) {
        karakter = serial1.read();
        // Serial.print(karakter);
        result += karakter;
        indeksA = result.indexOf(":");
        indeksB = result.length();
        if (indeksB - indeksA > 8) {
          Serial.println(F("keluar yuk"));
          break;
        }
      }
      if (indeksB - indeksA > 8) {
        Serial.println(F("hayuk"));
        break;
      }
    }
  
  Serial.print(F("Hasil nya ="));
  Serial.print(result);
    Serial.flush();
    serial1.flush();
  delay(100);


    indeksA = '0';
    indeksB = '0';
    ;
    serial1.println(F("AT+HTTPTERM"));
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    delay(300);
    indikator();

    // Serial.println(F("AT+SAPBR=0,1"));
    
    serial1.println(F("AT+SAPBR=0,1"));
    bacaserial(200);
    indeksA = '0';
    indeksA = result.indexOf(',');
    indeksB = result.indexOf(',', indeksA + 1);
    kode = result.substring(indeksA + 1, indeksB).toInt();

    Serial.print(F("kode="));
    Serial.println(kode);
    Serial.flush();
  }
  else {
    Serial.println(F("NETWORK ERROR"));
    Serial.flush();
  }
}

void GSMreset() { //WAKE UP GSM
  at();
  Serial.println(F(" "));
  Serial.println(F("AT+CSCLK=0"));
  serial1.println(F("AT+CSCLK=0"));
  Serial.flush();
  serial1.flush();
  delay(200);
  serial1.println(F("AT+CSCLK=0"));
  serial1.flush();
  bacaserial(200);
  Serial.flush();
  serial1.flush();
  delay(1000);
}

void GSMsleep() {
  gsm = 1;
  at();
  Serial.println(F("GSM SLEEP"));
  serial1.println(F("AT+CSCLK=2"));
  bacaserial(500);

}

String strTwoDigit(float nilai) {
  String result = String(nilai, 2);
  String angka, digit;
  indeksB = 0;
  indeksA = result.indexOf(".", indeksB + 1);
  digit = result.substring(indeksA + 1, result.length());
  angka = result.substring(0, indeksA);
  if (result.length() == 4) { //4.12
    angka = "00" + angka + digit;
  }
  if (result.length() == 5) { //51.23
    angka = "0" + angka + digit;
  }
  if (result.length() > 5) { //123.45
    angka = angka + digit;
  }
  return angka;
}

void bersihdata() {
  reads = 0; reads1 = 0; tekanan = 0; volt = 0;
}
