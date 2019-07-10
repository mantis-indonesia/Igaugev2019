/* I-GAUGE Pro Mini 2019
   BOARD : ARDUINO PRO MINI

   LED:
   RED - Sensor tekanan tidak terhubung
   CYAN - Sensor tekanan terhubung
   BIRU - GSM prosedur
   HIJAU - SLEEP
   MAGENTA -
   PUTIH -
*/

//LIBRARY
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <LowPower.h>
#include <SoftwareSerial.h>

SoftwareSerial serial1(6, 7); // TX, RX

//VARIABLES
char ID[5] = "0009";
#define burst 5
byte interval = 15;
float offset = 0.0;
String source = "GSM";
#define baud 9600

String url = "/project/osh_2019/api/product/osh_data_c.php";
String server = "http://www.mantisid.id";

#define pres            A0
#define tegangan        A1
#define Rled      2
#define pwk        5
#define Gled      3
#define connectPres     8
#define Bled      4
#define dPres         9

// from  http://heliosoph.mit-links.info/arduino-powered-by-capacitor-reducing-consumption/
//defines for DIDR0 setting
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

//GLOBAL VARIABLE
boolean gsm = 0;
boolean GSMerror = 0;
int i, kode, tahun;
char g;
byte a, b, c;
byte bulan, hari, jam, menit, detik;
unsigned long reads = 0; //pressure
unsigned long reads1 = 0;
unsigned long awal, akhir, start;
float tekanan, volt;
String y, network, APN, USER, PWD, operators, result;

void setup() {
  //initialisation
  Serial.begin(baud);  // Serial USB

  //WELCOME SCREEN
  s_on();
  Serial.println(F("\r\nGSM IGAUGE PRO MINI 3V3 2019"));
  Serial.flush();
  s_off();

  //pinMode
  s_on();
  Serial.println(F("Atur pinMode"));
  Serial.flush();
  s_off();
  for (i = 2; i < 14; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
    if (i == 3) {
      i = 10;
    }
  }
  pinMode(connectPres, INPUT);

  //state
  s_on();
  Serial.println(F("Nyalakan Pin"));
  Serial.flush();
  s_off();
  digitalWrite(connectPres, HIGH);
  digitalWrite(Rled, HIGH);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  digitalWrite(Gled, HIGH);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  digitalWrite(Bled, HIGH);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);


  ledOff();

  // disable unused pin function
  s_on();
  Serial.println(F("Matikan semua fungsi"));
  Serial.flush();
  off();

  //CHECK IF PRESSURE SENSOR CONNECTED
  a = digitalRead(connectPres);
  s_on();
  Serial.print(F("Sensor tekanan = "));
  Serial.println(a);
  Serial.flush();
  s_off();
  if (a == 1) {
    s_on();
    Serial.println(F("Sensor tekanan tidak terhubung!!!!"));
    Serial.flush();
    s_off();
    digitalWrite(Rled, HIGH);
    while (1) {
    }
  }

  //nyala kan LED kalau sensor terhubung
  s_on();
  Serial.println(F("Sensor tekanan terhubung"));
  Serial.flush();
  s_off();
  digitalWrite(Gled, HIGH);
  digitalWrite(Bled, HIGH);
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
  ledOff();
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);


  s_on();
  Serial.println(F("Inisialisasi serial GSM"));
  Serial.flush();
  s_off();
  serial1.begin(baud);  // SIM900A
  s_on();
  on();
}

void loop() {
  // turn on GSM & Pressure sensor
  on();
  awal = millis();
  indikator(Bled);
  indikator(Gled);
  s_on();
  Serial.println(F("\r\nMulai Ambil Data"));
  // Serial.println(F("Nyalakan pin Sensor"));
  Serial.println(F("Nyalakan pin Power Key GSM"));
  Serial.flush();
  // delay(100);

  //nyalakan GSM jika mati
  if (gsm == 1) {
    GSMreset();
  }
  serial1.flush();
  Serial.flush();
  delay(3000);
  bacaserial(300);

  ceksim();
  regsim();
  waktu();

  bersihdata();

  //AMBIL DATA SENSOR TEKANAN
  digitalWrite(dPres, HIGH);
  power_adc_enable();
  serial1.flush();
  delay(100);
  Serial.println(F("ambil data ADC"));
  Serial.flush();

  //burst data 5 seconds
  for (i = 0; i < burst; i++) {
    digitalWrite(Gled, HIGH);
    reads1 = analogRead(pres); //pressure
    if (reads1 < 0) {
      reads = reads + 0;
    }
    else {
      reads = reads + reads1;
    }
    delay(300);
    digitalWrite(Gled, LOW);
    delay(700);
  }

  //KONVERSI
  reads1 = analogRead(tegangan);
  volt = (float)reads1 / 1024.00 * 5 * 14700 / 10000; // nilai voltase dari nilai DN * 147.00 / 100.00
  tekanan = ((float)reads / (float)burst) / 1024.00 * 5; // nilai voltase dari nilai DN
  tekanan = (300.00 * tekanan - 150.00) * 0.01 - 0.5 + float(offset);
  if (tekanan < 0) tekanan = 0;

  //debug value
  Serial.print(F("TEKANAN = "));
  Serial.println(tekanan, 2);
  Serial.print(F("VOLTASE = "));
  Serial.println(volt, 2);
  Serial.flush();
  digitalWrite(dPres, LOW);


  if (GSMerror == 0) {
    sendServer();
  }

  //TURN OFF GSM MODULE
  serial1.println(F("AT+CPOWD=1"));
  bacaserial(4000);
  serial1.flush();
  Serial.flush();
  digitalWrite(pwk, LOW);
  gsm = 1;
  akhir = (millis() - awal) / 1000;
  awal=interval*60-akhir;
  Serial.print(F("\r\nwaktu yang dibutuhkan = "));
  Serial.print(akhir);
  Serial.println(F(" detik"));
  Serial.flush();
  Serial.print(F("interval = "));
  Serial.print(interval * 60);
  Serial.print(F(" detik  |  "));
  Serial.println(awal);
  Serial.flush();
  off();

  while (akhir < (awal - 20)) {

    if (GSMerror == 0) {
      digitalWrite(Gled, HIGH);
    }
    if (GSMerror == 1) {
      digitalWrite(Gled, HIGH);
      digitalWrite(Rled, HIGH);
    }
    s_on();
    Serial.println(akhir);
    Serial.flush();
    s_off();


    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
    ledOff();
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
    akhir = akhir + 20;

  }

  s_on();
  // awal = interval * 60 - akhir;
  Serial.print("sisa = ");
  Serial.println(akhir);
  Serial.flush();
  s_off();
  off();
  while (akhir < (awal - 4)) {
    LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
    akhir = akhir + 4;
  }


  s_on();
  Serial.print("sisa = ");
  Serial.println(akhir);
  Serial.flush();
  s_off();


  while (akhir < (awal - 2)) {
    LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
    akhir = akhir + 2;
  }

  for (i = 1; i < 13; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }

  // s_on();
  // Serial.println("ready to get data");
  // Serial.flush();
  // s_off();
}

void ledOff() {
  digitalWrite(Rled, LOW);  digitalWrite(Gled, LOW);  digitalWrite(Bled, LOW);
}

void bersihdata() {
  reads = 0; reads1 = 0; tekanan = 0; volt = 0;
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

void off() {
  power_adc_disable(); // ADC converter
  power_spi_disable(); // SPI
  power_usart0_disable(); // Serial (USART)
  power_timer0_disable();// Timer 0   >> millis() will stop working
  power_timer1_disable();// Timer 1   >> analogWrite() will stop working.
  power_timer2_disable();// Timer 2   >> tone() will stop working.
  power_twi_disable(); // TWI (I2C)
}

void on() {
  power_timer0_enable();// Timer 0   >> millis() will stop working
}

void indikator(byte nomor) {
  digitalWrite(nomor, HIGH);
  delay(500);
  digitalWrite(nomor, LOW);
  delay(500);
}

byte ConnectAT(String cmd, int d) {
  i = 0;
  s_on();
  power_timer0_enable();
  while (1) {
    serial1.println(cmd);
    while (serial1.available()) {
      if (serial1.find("OK"))
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

void regsim() {
  c = 0;
  result = "";
  // Serial.println(F("AT+CFUN=1"));
  serial1.println(F("AT+CFUN=1"));
  bacaserial(200);
  Serial.flush();
  serial1.flush();
  // Serial.println(F("AT+CBAND=\"ALL_BAND\""));
  serial1.println(F("AT+CBAND=\"ALL_BAND\""));
  bacaserial(200);
  Serial.flush();
  serial1.flush();

cops:
  result = "";
  // Serial.println(F("AT+CREG=1"));
  serial1.println(F("AT+CREG=1"));
  bacaserial(200);
  Serial.flush();
  serial1.flush();
  // Serial.println(F("AT+CLTS=1"));
  serial1.println(F("AT+CLTS=1"));
  bacaserial(200);
  Serial.flush();
  serial1.flush();
  delay(200);

  indikator(Bled);
  // Serial.println(F("AT+COPS?"));
  serial1.println(F("AT+COPS?"));
  Serial.flush();
  serial1.flush();
  delay(200);
  while (serial1.available() > 0) {
    if (serial1.find("+COPS:")) {
      while (serial1.available() > 0) {
        g = serial1.read();
        result += g;
      }
    }
  }
  Serial.flush();
  serial1.flush();

  a = result.indexOf('"');
  b = result.indexOf('"', a + 1);
  y = result.substring(a + 1, b);
  if (y == "51089") y = "THREE";

  operators = y;
  Serial.println(y);
  Serial.flush();

  //option if not register at network
  if (operators == "")  {
    c++;
    if (c < 10) {
      goto cops;
    }
    if (c == 10) {
      Serial.println(F("NO OPERATOR FOUND"));
      Serial.flush();
      GSMerror = 1;
      indikator(Rled);
    }
  }
}

void ceksim() { //udah fix
  //CONNECT AT
  for (a = 0; a < 6; a++) {
    indikator(Bled);
    b = ConnectAT(F("AT"), 200);
    if (b == 8) {
      Serial.println(F("GSM MODULE OK!!"));
      Serial.flush();
      GSMerror = 0;
      indikator(Gled);
      break;
    }
    if (b < 8) {
      Serial.println(F("GSM MODULE ERROR"));
      Serial.flush();
      GSMerror = 1;
      indikator(Rled);
    }
  }
}

void bacaserial(int wait) {
  start = millis();
  while ((start + wait) > millis()) {
    while (serial1.available() > 0) {
      g = serial1.read();
      Serial.print(g);
    }
  }
  serial1.flush();
  Serial.flush();
}

void sendServer() {
  result = "";
  // Serial.println(F("AT+CGATT= 1 "));
  serial1.println(F("AT+CGATT=1"));
  Serial.flush();
  serial1.flush();
  bacaserial(200);

  // Serial.print(F("AT+CGATT? "));
  serial1.println(F("AT+CGATT?"));
  delay(200);
  while (serial1.available() > 0) {
    if (serial1.find("+CGATT: ")) {
      while (serial1.available() > 0) {
        g = serial1.read();
        result += g;
      }
    }
  }

  if (result.toInt() == 1) {
    //kirim data ke server
    serial1.println(F("AT+CIPSHUT"));
    bacaserial(500);
    Serial.flush();
    serial1.flush();

    serial1.println(F("AT+SAPBR=0,1"));
    bacaserial(200);
    Serial.flush();
    serial1.flush();

    //ATUR APN SESUAI DENGAN PROVIDER
    apn(operators);

    //CONNECTION TYPE
    serial1.println(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""));
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    delay(100);

    //APN ID
    result = "AT+SAPBR=3,1,\"APN\",\"" + APN + "\"";
    serial1.println(result);
    Serial.flush();
    serial1.flush();
    delay(100);

    //APN USER
    result = "AT+SAPBR=3,1,\"USER\",\"" + USER + "\"";
    serial1.println(result);
    Serial.flush();
    serial1.flush();
    delay(100);

    //APN PASSWORD
    result = "AT+SAPBR=3,1,\"PWD\",\"" + PWD + "\"";
    serial1.println(result);
    serial1.flush();
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    delay(100);

    //OPEN BEARER
    serial1.println(F("AT+SAPBR=1,1"));
    bacaserial(5000);
    Serial.flush();
    serial1.flush();
    delay(100);

    //QUERY BEARER
    // serial1.println(F("AT+SAPBR=2,1"));
    // bacaserial(3000);
    // Serial.flush();
    // serial1.flush();
    // delay(100);

    //TERMINATE HTTP SERVICE
    serial1.println(F("AT+HTTPTERM"));
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    delay(100);
    serial1.println(F("AT+HTTPINIT"));
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    delay(100);

    //SET HTTP PARAMETERS VALUE
    serial1.println(F("AT+HTTPPARA=\"CID\",1"));
    serial1.flush();
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    for (i = 1; i < 3; i++) {
      indikator(Bled);
      indikator(Gled);
    }

    // send data measurement to server
    //SET HTTP URL
    result = "AT+HTTPPARA=\"URL\",\"" + server + url + "\"";
    // Serial.println(result);
    serial1.println(result);
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    //http://www.mantisid.id/api/product/osh_data_c.php?={"Data":"003,20190315213555,2456,02356,02201,01233,05,05"}
    // {"Data":"id unit,yyyymmddhhmmss,tekanan(5 digit),kelembaban(5 digit),suhu(5 digit),volt (5 digit), burst (2 digit), interval (2 digit)"}
    y = "{\"Data\":\"" + String(ID) + ",";

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

    y += "," + strTwoDigit(tekanan) + ",-9999,-9999,";
    y += strTwoDigit(volt) + ",05,";
    y += String(interval) + "\"}";


    //SET HTTP DATA FOR SENDING TO SERVER
    result = "AT+HTTPDATA=" + String(y.length() + 1) + ",15000";
    // Serial.println(result);
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
    Serial.println(y);
    serial1.println(y);
    Serial.flush();
    serial1.flush();
    bacaserial(1000);
    Serial.flush();
    serial1.flush();

    //HTTP METHOD ACTION
    start = millis();
    Serial.println(F("AT+HTTPACTION=1"));
    serial1.println(F("AT+HTTPACTION=1"));
    Serial.flush();
    serial1.flush();
    while (serial1.available() > 0) {
      while (serial1.find("OK") == false) {
        // if (serial1.find("ERROR")) {
        // goto serve;
        // }
      }
    }
    Serial.flush();
    serial1.flush();
    a = '0';
    b = '0';
    result = "";
    //CHECK KODE HTTPACTION
    while ((start + 30000) > millis()) {
      while (serial1.available() > 0) {
        g = serial1.read();
        result += g;
        //Serial.print(g);
        a = result.indexOf(":");
        b = result.length();
        if (b - a > 8) {
          Serial.println(F("keluar yuk"));
          break;
        }
      }
      if (b - a > 8) {
        Serial.println(F("hayuk"));
        break;
      }
    }
    Serial.flush();
    serial1.flush();


    a = '0';
    b = '0';
    serial1.println(F("AT+HTTPTERM"));
    bacaserial(200);
    Serial.flush();
    serial1.flush();
    indikator(Bled);

    // Serial.println(F("AT+SAPBR=0,1"));
    serial1.println(F("AT+SAPBR=0,1"));
    bacaserial(200);
    a = '0';
    a = result.indexOf(',');
    b = result.indexOf(',', a + 1);
    kode = result.substring(a + 1, b).toInt();

    Serial.print(F("kode="));
    Serial.println(kode);
    Serial.flush();
  }
  else {
    Serial.println(F("NETWORK ERROR"));
    Serial.flush();
    indikator(Rled);
    indikator(Rled);
  }

}

String strTwoDigit(float nilai) {
  String result = String(nilai, 2);
  String angka, digit;
  b = 0;
  a = result.indexOf(".", b + 1);
  digit = result.substring(a + 1, result.length());
  angka = result.substring(0, a);
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

void GSMreset() {
  digitalWrite(pwk, HIGH);
  delay(200);
  digitalWrite(pwk, LOW);
  delay(2000);
  digitalWrite(pwk, HIGH);
}

void waktu() {
  serial1.println(F("AT+CCLK?"));
  y = "";
  start = millis();
  while ((start + 200) > millis()) {
    while (serial1.available() > 0) {
      g = serial1.read();
      y = y + g;
    }
  }
  serial1.flush();
  Serial.println(y);
  Serial.flush();

  //parse string
  b = 0;

  //tahun
  a = y.indexOf("\"", b + 1);
  b = y.indexOf("/", a + 1);
  tahun = 2000 + y.substring(a + 1, b).toInt();

  //bulan
  a = y.indexOf("/", b + 1);
  bulan = y.substring(b + 1, a).toInt();

  //hari
  b = y.indexOf(",", a + 1);
  hari = y.substring(a + 1, b).toInt();

  //jam
  a = y.indexOf(":", b + 1);
  jam = y.substring(b + 1, a).toInt();

  //menit
  b = y.indexOf(":", a + 1);
  menit = y.substring(a + 1, b).toInt();

  //detik
  a = y.indexOf("+", b + 1);
  detik = y.substring(b + 1, a).toInt();
}
