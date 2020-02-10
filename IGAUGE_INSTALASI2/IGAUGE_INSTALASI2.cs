/* I-GAUGE Pro Mini 2019 untuk instalasi unit 2
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
#include <Wire.h>
SoftwareSerial serial1(6, 7); // RX, TX

//VARIABLES
char ID[5] = "0024";
#define burst 5
byte interval = 12;
float offset = 0;
String source = "GSM";
#define baud 9600

String url = "";
String server = "";

#define pres            A0
#define tegangan        A1
#define Rled      2
#define pwk        5
#define Gled      3
#define connectPres     8
#define Bled      4

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
//variable
byte trial, indeksA, indeksB;
byte bulan, hari, jam, menit, detik;
int tahun, i;
char karakter;
unsigned long awal, akhir, start;
String kalimat , APN, USER, PWD, network, operators, result, kode;


void setup() {
  //initialisation
  Serial.begin(baud);  // Serial USB

  //WELCOME SCREEN
  Serial.println(F("\r\nGSM IGAUGE PRO MINI 3V3 2019"));
  Serial.print("Unit = ");
  Serial.println(ID);
  //pinMode
  Serial.println(F("Atur pinMode"));
  for (i = 2; i < 14; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
    if (i == 3) {
      i = 10;
    }
  }
  pinMode(connectPres, INPUT);
  digitalWrite(connectPres, HIGH);

  // disable unused pin function
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
  LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);

  s_on();
  Serial.println(F("Inisialisasi serial GSM"));
  Serial.flush();
  s_off();
  serial1.begin(baud);  // SIM900A
  s_on();
  on();
  delay(20000);
}

void loop() {
  // turn on GSM & Pressure sensor
  on();
  awal = millis();
  s_on();
  Serial.println(F("\r\nMulai Ambil Data"));
  Serial.println(F("Nyalakan pin Power Key GSM"));

  //nyalakan GSM jika mati
  if (gsm == 1) {
    GSMreset();
  }
  serial1.flush();
  Serial.flush();
  delay(3000);
  bacaserial(300);

  ceksim(); // Cek GSM modul apakah terhubung
  regsim(); // registrasi GSM modul ke network
  waktu(); //cek waktu dan tanggal dari network
  bersihdata();

  //AMBIL DATA SENSOR TEKANAN
  power_adc_enable();
  serial1.flush();
  delay(100);
  Serial.println(F("ambil data ADC"));
  Serial.flush();

  //burst data 5 seconds
  for (i = 0; i < burst; i++) {
    reads1 = analogRead(pres); //pressure
    if (reads1 < 0) {
      reads = reads + 0;
    }
    else {
      reads = reads + reads1;
    }
    delay(300);
    delay(700);
  }

  //KONVERSI
  reads1 = analogRead(tegangan);
  volt = (float)reads1 / 1024.00 * 3.3 * 14700 / 10000; // nilai voltase dari nilai DN * 147.00 / 100.00
  tekanan = ((float)reads / (float)burst) / 1024.00 * 3.3 * 14700 / 10000; // nilai voltase dari nilai DN | nilai 1.5152 itu konversi sistem 3.3v menuju 5v
  Serial.print("nilai voltase tekanan asli = ");
  Serial.println(tekanan);
  tekanan = (3.00 * tekanan - 1.50) + float(offset);
  Serial.print("nilai tekanan ditambahkan offset = ");
  Serial.println(tekanan);
  if (tekanan < 0) tekanan = 0;

  //debug value
  Serial.print(F("TEKANAN = "));
  Serial.println(tekanan, 2);
  Serial.print(F("VOLTASE = "));
  Serial.println(volt, 2);
  Serial.flush();

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

  s_on();
  Serial.println("ready to get data");
  Serial.flush();
  s_off();
}

void bersihdata() {
  reads = 0; reads1 = 0; tekanan = 0; volt = 0;
}

void ceksim() { //udah fix
  //CONNECT AT
  for (indeksA = 0; indeksA < 10; indeksA++) {
    indeksB = ConnectAT(F("AT"), 200);
    if (indeksB == 8) {
      Serial.println(F("GSM MODULE OK!!"));
      Serial.flush();
      GSMerror = 0;
      break;
    }
    if (indeksB < 8) {
      Serial.println(F("GSM MODULE ERROR"));
      Serial.flush();
      GSMerror = 1;
    }
  }
}

void regsim() {
  trial = 0;
  result = "";
  //Functionality
  serial1.println(F("AT+CFUN=1"));
  bacaserial(200);

  //Pilih Band
  serial1.println(F("AT+CBAND=\"ALL_BAND\""));
  bacaserial(200);

  //opsi jika tidak terhubung ke network
  while (trial <= 10) {
    sim();
    if (operators.length() > 0)  {
      GSMerror = 0;
    break;
    }
    if (trial == 10) {
      Serial.println(F("TIDAK ADA OPERATOR"));
      Serial.flush();
      GSMerror = 1;
    }
  }
}

void sim() {
  result = "";
  //REGISTASI NETWORK
  serial1.println(F("AT+CREG=1"));
  bacaserial(300);
  
  //WAKTU LOKAL BERDASARKAN NETWORK GSM
  serial1.println(F("AT+CLTS=1"));
  bacaserial(300);

  //CARI OPERATOR KARTU
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
  Serial.println(operatos);
  Serial.flush();
  GSMerror = 0;
}

void waktu() {
  serial1.println(F("AT+CCLK?"));
  kalimat = "";
  awal = millis();
  while ((awal + 200) > millis()) {
    while (serial1.available() > 0) {
      karakter = serial1.read();
      kalimat = kalimat + karakter;
    }
  }
  serial1.flush();
  Serial.println(kalimat);
  Serial.flush();

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
}

void sendServer() {
  result = "";
  // ATTACH GPRS SERVICE
  serial1.println(F("AT+CGATT=1"));
  Serial.flush();
  serial1.flush();
  bacaserial(200);

  // CHECK GPRS SERVICE APAKAH SUDAH TERKONEKSI
  serial1.println(F("AT+CGATT?"));
  delay(200);
  while (serial1.available() > 0) {
    if (serial1.find("+CGATT: ")) {
      while (serial1.available() > 0) {
        karakter = serial1.read();
        result += karakter;
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

    // send data measurement to server
    //SET HTTP URL
    result = "AT+HTTPPARA=\"URL\",\"" + server + url + "\"";
    serial1.println(result);
    bacaserial(200);
    Serial.flush();
    serial1.flush();
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
    kalimat += strTwoDigit(volt) + ",05,";
    kalimat += String(interval) + "\"}";


    //SET HTTP DATA FOR SENDING TO SERVER
    result = "AT+HTTPDATA=" + String(kalimat.length() + 1) + ",15000";
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
    Serial.println(kalimat);
    serial1.println(kalimat);
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
      while (serial1.find("OK") == false);
     
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
        result += karakter;
        //Serial.print(karakter);
        indeksA = result.indexOf(":");
        indeksB = result.length();
        if (indeksB - indeksA > 8) {
          Serial.println(F("MOVING OUT"));
          break;
        }
      }
      if (indeksB - indeksA > 8) {
        Serial.println(F("OK"));
        break;
      }
    }
    Serial.flush();
    serial1.flush();

    indeksA = '0';
    indeksB = '0';
    serial1.println(F("AT+HTTPTERM"));
    bacaserial(200);
    Serial.flush();
    serial1.flush();

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

byte ConnectAT(String cmd, int d) { //try to send AT 5 times
  trial = 0;
  s_on();
  power_timer0_enable();
  while (1) {
    serial1.println(cmd);
    while (serial1.available()) {
      if (serial1.find("OK"))
        trial = 8;
    }
    delay(d);
    if (trial > 5) {
      break;
    }
    trial++;
  }
  return trial;
}

void bacaserial(int wait) {
  awal = millis();
  while ((awal + wait) > millis()) {
    while (serial1.available() > 0) {
      karakter = serial1.read();
      Serial.print(karakter);
    }
  }
  serial1.flush();
  Serial.flush();
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

void GSMreset() {
  digitalWrite(pwk, HIGH);
  delay(200);
  digitalWrite(pwk, LOW);
  delay(2000);
  digitalWrite(pwk, HIGH);
  serial1.println(F("AT+CSCLK=0"));
  bacaserial(200);
}

void GSMturnoff(){
  serial1.println(F("AT+CPOWD=0"));
  serial1.flush();
  delay(200);
}


