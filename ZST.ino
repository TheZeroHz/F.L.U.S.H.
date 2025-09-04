#define VERSION "1.0.4"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "HX711.h"
#include "ZScore.h"
#include "DW.h"
#include "WS2812ColorLib.h"
#include "TTP223Touch.h"
#include "Cable.h"
#include "Audio.h"
#include <FS.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include <otadrive_esp.h>
#include "ZGuard.h"
#include "HallEffectSecurity.h"
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#include <SimpleCLI.h>

// Firebase Task Queue Structure
typedef enum {
  FB_TASK_NOTIFY,
  FB_TASK_UPDATE_WEIGHT,
  FB_TASK_UPDATE_FIRST_LOAD,
  FB_TASK_UPDATE_LAST_UNLOAD,
  FB_TASK_UPDATE_PULL_COUNT,
  FB_TASK_CHECK_CMD,
  FB_TASK_SET_ONLINE
} firebase_task_type_t;

typedef struct {
  firebase_task_type_t type;
  int int_value1;      // For weight, code, count
  int int_value2;      // For additional int data
  char string_value1[32];  // For device_id (fixed size)
  char string_value2[64];  // For timestamp (fixed size)
} firebase_message_t;

// Queue for Firebase tasks
QueueHandle_t firebaseQueue;
const int FIREBASE_QUEUE_SIZE = 20;

// Initialize SimpleCLI
SimpleCLI cli;
AsyncWebServer rcli_server(80);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
WiFiManager wm;

String DEV_ID= "TH-001";
String TIME ="3:40 AM 5/16/2025";
#define DATABASE_URL "fir-f1319-default-rtdb.firebaseio.com"

#define HX711_POWER_PIN 9
#define HX711_SCK_PIN 3
#define HX711_DT_PIN 8
#define TOUCH_PIN 1
#define TPU4056_CHARGER_PIN 16
#define HALL_DATA_PIN 12
#define HALL_POWER_PIN 13

HallEffectSecurity slider(HALL_DATA_PIN, HALL_POWER_PIN);
Cable cable(TPU4056_CHARGER_PIN, 5);
#define RGB_INDICATOR_PIN 48

#define WINDOW_SIZE 6
#define OTA_API_KEY "57e519fe-2f67-43ca-a339-d28710afa35e"
const char* colors[] = { "Red", "Green", "Blue", "Yellow", "Magenta",
                         "Cyan", "White", "Orange", "Purple", "Pink" };
bool first_read = true, firebase_refresh = false;
double zs = 0;
float HX711_CALIBRATION_FACTOR = 436.2;
int weight_In_g;
int pull_count = 0;

enum ntfi{
  slider_miss,
  cable_miss,
  tissue_theft,
  low_tissue,
  partial_theft,
  weight_manip
};
bool ntf[6]={false,false,false,false,false,false};

WS2812ColorLib indicator(RGB_INDICATOR_PIN);
TTP223Touch touch(TOUCH_PIN, 50, 1000);
HX711 LOADCELL_HX711(HX711_POWER_PIN);

Zscore zscore(WINDOW_SIZE);
DW dw;
ZGuard zguard;
unsigned long t1, fbread;
String notifier_serial="0000";

#define SHIELD_BROKE_CODE 101
#define POOL_LONG_CODE 102
#define BIG_THEFT_CODE 103
#define WEIGHT_MANIPULATION_CODE 104
#define SLIDER_MISSING_CODE 105
#define POWER_CABLE_MISSING_CODE 106
#define LOW_TISSUE_CODE 107

#define FIRST_LOAD_PATH "BUBT/Devices/Load/TH-001"
#define CURRENT_AVAILABLE_PATH "BUBT/Devices/Available/TH-001"
#define LAST_UNLOAD_PATH "BUBT/Devices/Unload/TH-001"
#define ONLINE_PATH "BUBT/Devices/Online/TH-001"
#define PULL_COUNT_PATH "BUBT/Devices/PollCount/TH-001"
#define SAFETY_PATH "BUBT/Devices/Safety/TH-001"
#define CMD_PATH "BUBT/Devices/CMD/TH-001"
#define NOTIFIER_CODE_PATH "BUBT/Notification/Log/Code/"
#define NOTIFIER_IDENTITY_PATH "BUBT/Notification/Log/Sender/"
#define NOTIFIER_TIMER_PATH "BUBT/Notification/Log/Time/"

// Firebase Task Queue Functions
bool sendFirebaseMessage(firebase_task_type_t type, int value1 = 0, int value2 = 0, const char* str1 = "", const char* str2 = "") {
  firebase_message_t message;
  memset(&message, 0, sizeof(message)); // Initialize all members to 0/null
  
  message.type = type;
  message.int_value1 = value1;
  message.int_value2 = value2;
  
  strncpy(message.string_value1, str1, sizeof(message.string_value1) - 1);
  message.string_value1[sizeof(message.string_value1) - 1] = '\0';
  
  strncpy(message.string_value2, str2, sizeof(message.string_value2) - 1);
  message.string_value2[sizeof(message.string_value2) - 1] = '\0';
  
  return xQueueSend(firebaseQueue, &message, pdMS_TO_TICKS(100)) == pdTRUE;
}

void snInc() {
  int value = notifier_serial.toInt() + 1;
  if (value > 9999) value = 0;
  char buffer[5];
  sprintf(buffer, "%04d", value);
  notifier_serial = String(buffer);
}

void notify(int code) {
  if(!sendFirebaseMessage(FB_TASK_NOTIFY, code, 0, DEV_ID.c_str(), TIME.c_str())) {
    Serial.println("Failed to queue notification");
  }
}

void updateWeight(int weight) {
  if(!sendFirebaseMessage(FB_TASK_UPDATE_WEIGHT, weight)) {
    Serial.println("Failed to queue weight update");
  }
}

void updateFirstLoad(int weight) {
  if(!sendFirebaseMessage(FB_TASK_UPDATE_FIRST_LOAD, weight)) {
    Serial.println("Failed to queue first load update");
  }
}

void updatePullCount(int count) {
  if(!sendFirebaseMessage(FB_TASK_UPDATE_PULL_COUNT, count)) {
    Serial.println("Failed to queue pull count update");
  }
}

void checkFirebaseCmd() {
  if(!sendFirebaseMessage(FB_TASK_CHECK_CMD)) {
    Serial.println("Failed to queue command check");
  }
}

// Firebase Task Implementation
void FirebaseTask(void *parameter) {
  firebase_message_t message;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(250); // 250ms cycle
  
  while (true) {
    bool messageProcessed = false;
    
    // Process only ONE message per cycle to prevent blocking
    if(xQueueReceive(firebaseQueue, &message, pdMS_TO_TICKS(10)) == pdTRUE) {
      if(Firebase.ready()) {
        messageProcessed = true;
        
        switch(message.type) {
          case FB_TASK_NOTIFY:
            snInc();
            Serial.printf("Set Notification Code... %s\n", 
              Firebase.RTDB.setInt(&fbdo, NOTIFIER_CODE_PATH + notifier_serial, 
              message.int_value1) ? "ok" : fbdo.errorReason().c_str());
            
            // Yield after each Firebase operation
            vTaskDelay(pdMS_TO_TICKS(50));
            
            Serial.printf("Set Notification Sender... %s\n", 
              Firebase.RTDB.setString(&fbdo, NOTIFIER_IDENTITY_PATH + notifier_serial, 
              String(message.string_value1)) ? "ok" : fbdo.errorReason().c_str());
            
            vTaskDelay(pdMS_TO_TICKS(50));
            
            Serial.printf("Set Notification Time... %s\n", 
              Firebase.RTDB.setString(&fbdo, NOTIFIER_TIMER_PATH + notifier_serial, 
              String(message.string_value2)) ? "ok" : fbdo.errorReason().c_str());
            break;
            
          case FB_TASK_UPDATE_WEIGHT:
            Serial.printf("Updating current weight... %s\n", 
              Firebase.RTDB.setInt(&fbdo, CURRENT_AVAILABLE_PATH, 
              message.int_value1) ? "ok" : fbdo.errorReason().c_str());
            break;
            
          case FB_TASK_UPDATE_FIRST_LOAD:
            Serial.printf("Set initial load weight... %s\n", 
              Firebase.RTDB.setInt(&fbdo, FIRST_LOAD_PATH, 
              message.int_value1) ? "ok" : fbdo.errorReason().c_str());
            break;
            
          case FB_TASK_UPDATE_LAST_UNLOAD:
            Serial.printf("Set last unload weight... %s\n", 
              Firebase.RTDB.setInt(&fbdo, LAST_UNLOAD_PATH, 
              message.int_value1) ? "ok" : fbdo.errorReason().c_str());
            break;
            
          case FB_TASK_UPDATE_PULL_COUNT:
            Serial.printf("Updating pull count... %s\n", 
              Firebase.RTDB.setInt(&fbdo, PULL_COUNT_PATH, 
              message.int_value1) ? "ok" : fbdo.errorReason().c_str());
            break;
            
          case FB_TASK_CHECK_CMD: {
            Serial.printf("Getting CMD... %s\n", 
              Firebase.RTDB.getString(&fbdo, CMD_PATH) ? 
              fbdo.to<const char*>() : fbdo.errorReason().c_str());
            
            String cmd = fbdo.stringData();
            if (cmd == "unload" || cmd == "load") {
              Firebase.RTDB.setInt(&fbdo, LAST_UNLOAD_PATH, weight_In_g);
              Firebase.RTDB.setString(&fbdo, CMD_PATH, " ");
              vTaskDelay(pdMS_TO_TICKS(1000)); // Shorter delay before restart
              ESP.restart();
            } else if (cmd == "upgrade") {
              Firebase.RTDB.setString(&fbdo, CMD_PATH, " ");
              upgrade();
            }
            break;
          }
            
          case FB_TASK_SET_ONLINE:
            Firebase.RTDB.setBool(&fbdo, ONLINE_PATH, true);
            break;
        }
      } else {
        Serial.println("Firebase not ready, skipping task");
      }
    }
    
    // Regular periodic tasks (less frequent)
    static unsigned long lastPeriodicUpdate = 0;
    if(millis() - lastPeriodicUpdate > 30000) { // Every 30 seconds
      sendFirebaseMessage(FB_TASK_SET_ONLINE);
      lastPeriodicUpdate = millis();
    }
    
    // Use periodic wake up to prevent watchdog issues
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// Callback functions for CLI commands
void upgradeCallback(cmd* c) {
  WebSerial.println("Calling upgrade()...");
  upgrade();
}

void restartCallback(cmd* c){
  WebSerial.println("Restarting...");
  ESP.restart();
}

void ewfsCallback(cmd* c){
  WebSerial.println("Enabling FileMngr...");
  webshareEnable();
}

void dwfsCallback(cmd* c){
  WebSerial.println("Disabling FileMngr...");
  webshareDisable();
}

void ledCallback(cmd* c) {
  Command cmd(c);
  Argument percentArg = cmd.getArgument(0);
  int percent = percentArg.getValue().toInt();
  WebSerial.print("Calling setBrigt() with percent: ");
  WebSerial.println(percent);
  indicator.setBrightness(percent/100);
}

void volumeCallback(cmd* c) {
  Command cmd(c);
  Argument volumeArg = cmd.getArgument(0);
  int volume = volumeArg.getValue().toInt();
  WebSerial.print("Calling setVolume() with volume: ");
  WebSerial.println(volume);
  audioSetVolume(volume);
}

void playCallback(cmd* c) {
  Command cmd(c);
  Argument pathArg = cmd.getArgument(0);
  String path = pathArg.getValue();
  WebSerial.print("Calling playAudio() with path: ");
  WebSerial.println(path);
  audioConnecttoSPIFFS(path.c_str());
}

void recvMsg(uint8_t *data, size_t len){
  String input = "";
  for(int i=0; i < len; i++){
    input += char(data[i]);
  }
  input.trim();
  if (input.startsWith("th-001")) {
    input = input.substring(7);
    input.trim();
    cli.parse(input);
  } else {
    WebSerial.println("Invalid prefix! Expected: th-001 <command> <args...>");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  wm.setConfigPortalTimeout(60);
  
  // Create Firebase queue
  firebaseQueue = xQueueCreate(FIREBASE_QUEUE_SIZE, sizeof(firebase_message_t));
  if(firebaseQueue == NULL) {
    Serial.println("Failed to create Firebase queue");
    ESP.restart();
  }
  
  // Register CLI commands
  Command upgradeCmd = cli.addSingleArgCmd("upgrade", upgradeCallback);
  Command restartCmd = cli.addSingleArgCmd("restart", restartCallback);
  Command ewfsCmd=cli.addSingleArgCmd("ewfs", ewfsCallback);
  Command dwfsCmd=cli.addSingleArgCmd("dwfs", dwfsCallback);
  Command ledCmd = cli.addCmd("led", ledCallback);
  ledCmd.addArgument("percent");
  Command volumeCmd = cli.addCmd("volume", volumeCallback);
  volumeCmd.addArgument("volume");
  Command playCmd = cli.addCmd("play", playCallback);
  playCmd.addArgument("path");

  Serial.println();
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  
  slider.begin();
  audioInit();
  audioSetVolume(18);
  audioConnecttoSPIFFS("boot.mp3");
  while(audiostatus()){
    delay(1);
  }
  
  Serial.println("Hall Effect Security Sensor Initialized");
  Serial.println("Cable calibration...");
  
  if(!wm.autoConnect("Flush 001","bubt@2025")) {
    Serial.println("failed to connect and hit timeout");
  } 
  
  Serial.println("[WiFi] WiFi is connected!");
  Serial.print("[WiFi] IP address: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("Firebase Started");
  config.database_url = DATABASE_URL;
  config.signer.test_mode = true;
  config.token_status_callback = tokenStatusCallback;
  
  Firebase.reconnectNetwork(true);
  fbdo.keepAlive(15, 5, 3);
  fbdo.setBSSLBufferSize(8192, 8192);
  fbdo.setResponseSize(16384);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);
  
  webshareInit();
  WebSerial.begin(&rcli_server);
  WebSerial.onMessage(recvMsg);
  rcli_server.begin();

  // Create Firebase task
  xTaskCreatePinnedToCore(
      FirebaseTask,        
      "FirebaseTask",      
      10000,               
      NULL,                
      1,                   
      NULL, 
      0                    
  );
  
  touch.begin();
  indicator.begin();
  indicator.setBrightness(0.40);
  indicator.setColorByName("Red");
  
  Serial.println("Unload Everything From Holder!");
  audioConnecttoSPIFFS("unload.mp3");
  while(audiostatus()){
    delay(1);
  }
  delay(3000);
  
  while (!touch.isTouched()) {
    touch.update();
  }
  touch.reset();
  indicator.setColorByName("Blue");
  delay(1000);
  
  LOADCELL_HX711.begin(HX711_DT_PIN, HX711_SCK_PIN);
  delay(1000);
  LOADCELL_HX711.set_scale(HX711_CALIBRATION_FACTOR);
  LOADCELL_HX711.tare();
  
  Serial.println("Load The Roll Inside Holder!");
  audioConnecttoSPIFFS("load.mp3");
  while(audiostatus()){
    delay(1);
  }
  
  while (!touch.isTouched()) {
    touch.update();
  }
  touch.reset();
  indicator.setColorByName("Green");
  
  OTADRIVE.setInfo(OTA_API_KEY, VERSION);
  OTADRIVE.onUpdateFirmwareProgress(OnProgress);
  delay(500);
  fbread = millis();
}

bool wsafe = true;
int csum = 0;
int dw_before_pooling = 0, w_before_pooling = 0;
int POOL_STATE=BEFORE_POOL;
int D_W=0;
int load_w=0;
int DW_CNT=0,DW_SUM=0;

void loop() {
  if (cable.IsConnected()) {
    ntf[cable_miss]=false;
  } else {
    Serial.println("Cable is disconnected.");
    if(!ntf[cable_miss]){
      audioConnecttoSPIFFS("cable.mp3");
      notify(POWER_CABLE_MISSING_CODE);
      ntf[cable_miss]=true;
    }
    Serial.print("Slider Missing!");
  }

  HallEffectSecurity::SystemState state = slider.getSystemState();
  float flux = slider.getFlux();

  Serial.print("Flux: ");
  Serial.print(flux);
  Serial.print(" uT => State: ");
  if(flux>=60&&flux<=70){
    ntf[slider_miss]=false;
  }
  else{
    if(!ntf[slider_miss]){
      audioConnecttoSPIFFS("cable.mp3");
      notify(SLIDER_MISSING_CODE);
      ntf[slider_miss]=true;
    }
    Serial.print("Slider Missing!");
  }

  if (LOADCELL_HX711.is_ready()) {
    weight_In_g = LOADCELL_HX711.get_units(10);
    Serial.print("Weight: ");
    Serial.print(weight_In_g);
    
    if (first_read) {
      if(weight_In_g==0||weight_In_g>100){
        Serial.println("Are you joking???No weight Loaded. Retry calibrating....");
        audioConnecttoSPIFFS("tryload.mp3");
        while(audiostatus()){
          delay(1);
        }
        ESP.restart();
      }
      load_w=weight_In_g;
      updateFirstLoad(weight_In_g);
      for (int i = 0; i < WINDOW_SIZE; i++) zs = zscore.addAndCalculate(weight_In_g);
      first_read = false;
    } else {
      zs = zscore.addAndCalculate(weight_In_g);
    }
    
    dw.update(weight_In_g, zs);
    D_W = dw.getDW();
    if(D_W!=0){
      DW_CNT++;
      DW_SUM=DW_SUM+D_W;
    }
    if(DW_CNT==4){
      DW_CNT=0;
      if(DW_SUM<-11){
        Serial.println("Mini Theft Happens!");
        audioConnecttoSPIFFS("minitheft.mp3");
      }
    }
    if(D_W<-9){
      Serial.println("Big Theft Happens!");
      audioConnecttoSPIFFS("bigtheft.mp3");
      notify(BIG_THEFT_CODE);
    }

    Serial.print(" Z-score:");
    Serial.print(zs);
    Serial.print(" Weight Derivative:");
    Serial.print(D_W);
    Serial.print(" CheckSum:");
    Serial.print(csum);
    Serial.print(" Safety:");
    Serial.print(wsafe);
    
    POOL_STATE = dw.getPoolState();
    if (POOL_STATE == BEFORE_POOL) {
      dw_before_pooling = D_W;
      w_before_pooling = weight_In_g;
      if(weight_In_g<10&&!ntf[low_tissue]){
        notify(LOW_TISSUE_CODE);
        ntf[low_tissue]=true;
      }
      Serial.println(" IDLE");
      indicator.setColorByName("Green");
      
      if (millis() - fbread > 5000) {
        firebase_refresh=true;
        fbread=millis();
        // Queue weight update and command check
        updateWeight(weight_In_g);
        checkFirebaseCmd();
      }
      t1 = millis();
    } else if (POOL_STATE == POOLING) {
      if (millis() - t1 > 10000) {
        Serial.println("Polling for long time seems like someone trying to penetrate the systems security");
        audioConnecttoSPIFFS("slider1.mp3");
      }
      Serial.println(" Pooling");
      indicator.setColorByName("Purple");
    } else {
      pull_count++;
      Serial.println(" End Pooling");
      zguard.addDerivate(D_W);
      csum = zguard.checkSum(weight_In_g);
      wsafe = zguard.isSafe();
      updatePullCount(pull_count);
    }
  } else {
    indicator.setColorByName("Red");
    Serial.println("HX711 not found.");
  }
  
  delay(100);
}

void upgrade() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Request For Update!");
    indicator.setColorByName("White");
    auto inf = OTADRIVE.updateFirmwareInfo();
    if (inf.available) {
      indicator.setColorByName("Cyan");
      Serial.printf("UV%s", inf.version.c_str());
      Serial.println("");
      OTADRIVE.updateFirmware();
    } else {
      Serial.println("No update available!");
    }
  } else
    Serial.println("Sorry device is offline! Please connect to the internet...");
  indicator.setColorByName("Green");
}

void OnProgress(int progress, int total) {
  static int last_percent = 0;
  int percent = (100 * progress) / total;
  if (percent != last_percent) {
    if (percent > 100) indicator.setColorByName(colors[percent / 10]);
    Serial.println("Updating " + (String)percent + "%");
    last_percent = percent;
    indicator.setColorByName("Green");
  }
}

void listDir(fs::FS& fs, const char* dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}