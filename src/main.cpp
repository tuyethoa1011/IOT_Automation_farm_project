#include <PubSubClient.h>
#include <WiFi.h>
#include <Mylib.h>
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESP32Time.h>
#include <NTPClient.h>
#include <string.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 5 //pin đọc dữ liệu cảm biến nhiệt độ, độ ẩm
#define DHTTYPE  DHT11
#define pin_hm 34  // pin doc du lieu tu cam bien do am dat
#define pin_valve GPIO_NUM_16 // pin kich van tuoi
//#define pin_button GPIO_NUM_17 // pin chuyen che do (Manual/Auto)
#define MQTT_SERVER "broker1011.cloud.shiftr.io"
#define MQTT_PORT 1883
#define MQTT_USER "broker1011"
#define MQTT_PASSWORD "JPq0XvVj0dhW078L"
#define MQTT_TOPIC "sensor/soil_mst"
#define MQTT_DHT_TOPIC_H "sensor/humidity"
#define MQTT_DHT_TOPIC_T "sensor/temperature"
#define USER_INTERRACT_MODE_TOPIC "mode"
#define USER_INTERRACT_MANUAL_TOPIC "mode/water_manual"

ESP32Time rtc(0);

DHT dht(DHTPIN,DHTTYPE); //khai báo tạo object dht

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
  int cnt = 0;
  //Khởi tạo baud 115200
  Serial.begin(115200);
  //Mode wifi là station
  WiFi.mode(WIFI_STA);
  //Chờ kết nối
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if(cnt++ >= 10){
       WiFi.beginSmartConfig();
       while(1){
           delay(1000);
           //Kiểm tra kết nối thành công in thông báo
           if(WiFi.smartConfigDone()){
             Serial.println("SmartConfig Success");
             break;
           }
       }
    }
}
}
void connect_to_broker(){
  while(!client.connected()){
    Serial.print("Đang kết nối với MQTT Sever");
    String clientID = "Do Am La:";
    clientID += String(random(0xffff), HEX);
    if (client.connect(clientID.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("Đã kết nối");
      client.subscribe(MQTT_TOPIC); 
      client.subscribe(MQTT_DHT_TOPIC_H); 
      client.subscribe(MQTT_DHT_TOPIC_T); 
    } else {
      Serial.print("Lỗi, rc=");
      Serial.print(client.state());
      Serial.println("Thử lại sau 2 giây");
      delay(2000);
    }
  }
}

static bool manual_mode = false; 
static bool auto_mode = true; //auto mode luôn bật chỉ tắt khi bật manual

void callback(char* topic, byte *payload, unsigned int length) {
  //trước khi nhận message hoặc topic, clear chuỗi và byte trước khi ghi
  memset((char*)&topic,0,sizeof(topic));
  memset((char*)&payload,0,sizeof(payload));

  Serial.println("-------new message from broker-----");
  Serial.print("topic: ");
  Serial.println(topic);
  Serial.print("message: ");
  Serial.write(payload, length);

  if(auto_mode && !manual_mode)
  {
    if(strcmp((const char*)&topic,USER_INTERRACT_MODE_TOPIC) == 0) //check topic mode
    {
      if(strcmp((const char*)&payload,"manual") == 0) 
      {
        manual_mode = true;
        auto_mode = false;
      } else if (strcmp((const char*)&payload,"auto") == 0)
      {
        //do nothing
        auto_mode = true;
        manual_mode = false;
      }
    }
  } else if(manual_mode && !auto_mode)
  {
    //nếu topic nhận được chuyển chế độ thì chỉnh lại biến
    if(strcmp((const char*)&topic,USER_INTERRACT_MANUAL_TOPIC) == 0)
    {
      if(payload[0] == '1') //bật van tưới
      {
        digitalWrite(pin_valve,HIGH); //mở van
        on_off = 1; //thông báo trạng thái hiện tại van đang tưới
      } else if(payload[0] == '0') //tắt van tưới
      {
        digitalWrite(pin_valve,LOW); //tắt van
        on_off = 0; //thông báo trạng thái hiện tại van tắt
      }
    }
    if(strcmp((const char*)&topic,USER_INTERRACT_MODE_TOPIC) == 0) //check topic mode
    {
      if(strcmp((const char*)&payload,"manual") == 0) 
      {
        manual_mode = true;
        auto_mode = false;
      } else if (strcmp((const char*)&payload,"auto") == 0)
      {
        //do nothing
        auto_mode = true;
        manual_mode = false;
      }
    }
  }

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
  client.publish(MQTT_DHT_TOPIC_H,String(analogRead(DHTPIN)).c_str(),true); 
  client.publish(MQTT_DHT_TOPIC_T,String(analogRead(DHTPIN)).c_str(),true); 
  //delay(1000*10);
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