#include <PubSubClient.h>
#include <WiFi.h>
#include <Mylib.h>
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESP32Time.h>
#include <NTPClient.h>
#include <string.h>


const char* ssid = "Nam Siêu Cấp Vô Địch";
const char* password = "darrenshan1209";

#define pin_hm 34  // pin doc du lieu tu cam bien do am dat
#define pin_valve GPIO_NUM_16 // pin kich van tuoi
#define MQTT_SERVER "broker-channel.cloud.shiftr.io"
#define MQTT_PORT 1883
#define MQTT_USER "broker-channel"
#define MQTT_PASSWORD "rvCvDiwfvqcVPjoV"
#define MQTT_TOPIC "IOT"

ESP32Time rtc(0);

// Dat thoi gian tuoi cay 
/* Thoi gian tuoi lan 1 */
uint8_t const time1_hr = 9;
uint8_t const time1_mn = 0;
/* Thoi gian tuoi lan 2 */
uint8_t const time2_hr = 15;
uint8_t const time2_mn = 0;

// Do am can tuoi (nguong duoi')
uint8_t hm1 = 50;

// Do am ngung tuoi (nguong tren)
uint8_t hm2 = 75;

//long long last_check;

/* on_off 
      = 0: van tat, san sang kiem tra thoi gian va do am.
      = 1: da kiem tra, xac nhan phu hop dieu kien, chuan bi tuoi.
      = 2: dang tuoi.
      */ 
uint8_t on_off = 0;
int current_ledState = LOW;
int last_ledState = LOW;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Hàm lấy thời gian của internet và của ESP32
void UpdateTimeRTC(){
  WiFiUDP ntpUDP;
  NTPClient timeClient (ntpUDP, "pool.ntp.org"); 
  timeClient.begin();
  timeClient.setTimeOffset(25200);
  while(!timeClient.update()){
    timeClient.forceUpdate();
  }
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime(&epochTime);
  rtc.setTime(timeClient.getSeconds(),
              timeClient.getMinutes(),
              timeClient.getHours(),
              timeClient.getDay(),
              ptm-> tm_mon+1, ptm->tm_year +1900);  
}

// Ham kiem tra thoi gian va do am de kich van 
void CheckOnActive(){
  UpdateTimeRTC();
  if(on_off == 0){
    // Khi kiem tra vao dau thoi gian hen gio-- va --- troi khong co mua (do am thap hon hm2)
    Serial.println(rtc.getSecond());
    if(((2 - rtc.getSecond()) >= 0) && ReadHumidity(pin_hm) < hm2){
      Serial.print("on1");
      // neu gio va phut bang hoac do am be hon hm1
      if( (rtc.getHour() == time1_hr && rtc.getMinute() == time1_mn) || 
      (rtc.getHour() == time2_hr && rtc.getMinute() == time2_mn) || ReadHumidity(pin_hm) < hm1) 
        // san sang kich van 
        on_off = 1;
        Serial.println(on_off);
    } 
  }
  else if (on_off == 1) {
    // bat dau kich van
    digitalWrite(pin_valve,HIGH);
    // co` thong bao van dang hoat dong
    on_off = 2;
  }
  else if (on_off == 2) {
    // khi van dang hoat dong , neu do am > hm2
    if(ReadHumidity(pin_hm) > hm2) {
      // tat van
      digitalWrite(pin_valve,LOW);
      // thong bao van dang tat, san sang kiem tra cho lan tiep theo
      on_off = 0;
    }
  }
}
void setup_wifi(){
  Serial.print("Kết nối với ...");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");

  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("Wifi đã được kết nối");
  Serial.println("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());

}
void connect_to_broker(){
  while(!client.connected()){
    Serial.print("Đang kết nối với MQTT Sever");
    String clientID = "Do Am La:";
    clientID += String(random(0xffff), HEX);
    if (client.connect(clientID.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("Đã kết nối");
      client.subscribe(MQTT_TOPIC);
    } else {
      Serial.print("Lỗi, rc=");
      Serial.print(client.state());
      Serial.println("Thử lại sau 2 giây");
      delay(2000);
    }
  }
}
void callback(char* topic, byte *payload, unsigned int length) {
  Serial.println("-------new message from broker-----");
  Serial.print("topic: ");
  Serial.println(topic);
  Serial.println(analogRead(pin_hm));
  Serial.print("message: ");
  Serial.write(payload, length);
  Serial.println();
  if (*payload == '1') current_ledState = HIGH;
  if (*payload == '0') current_ledState = LOW;
}

void setup() {
  Serial.begin(115200);
  /* Phuong thuc SetTime - Thu tu tham so: Ngay, thang, nam, gio, phut, giay, thu'*/
  rtc.setTime(10,6,22,8,59,50,2);
  pinMode(pin_hm,INPUT);
  pinMode(pin_valve,OUTPUT);
  // chac chan rang ban dau chan kich van la LOW
  digitalWrite(pin_valve,LOW);
  Serial.setTimeout(500);
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT );
  client.setCallback(callback);
  connect_to_broker();
}


void send_data() {
  client.publish(MQTT_TOPIC,String(ReadHumidity(pin_hm)).c_str(),true); 
  delay(1000);
}

void loop() {
  UpdateTimeRTC();
  CheckOnActive();
  delay(200);
  client.loop();
  if (!client.connected()) {
    connect_to_broker();
  }
  if (last_ledState != current_ledState) {
    last_ledState = current_ledState;
    digitalWrite(22, current_ledState);
    Serial.println(current_ledState);
  }
  send_data();
}