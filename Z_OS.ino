#include "Audio.h"
#include <FS.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPFMfGK.h>

#define I2S_DOUT 6 //Some times labeled as DIN OK PCB
#define I2S_BCLK 5 // OK PCB
#define I2S_LRC 4 // OK PCB

#define app_core 1
#define utility_core 0

#define audio_stack 5000
#define cable_stack 5000
#define webshare_stack 10000

bool WebShareEnabled=false;


TaskHandle_t audio_handle = NULL;
TaskHandle_t cable_handle = NULL;
TaskHandle_t webshare_handle = NULL;

Audio audio;
const word filemanagerport = 8080;
ESPFMfGK filemgr(filemanagerport); 

//****************************************************************************************
//                             C A B L E _ C H E C K E R                                 *
//****************************************************************************************



//****************************************************************************************
//                                   A U D I O _ T A S K                                 *
//****************************************************************************************
struct audioMessage {
  uint8_t cmd;
  const char* txt;
  uint32_t value;
  uint32_t ret;
  const char* lang;
} audioTxMessage, audioRxMessage;

enum : uint8_t { SET_VOLUME,
                 GET_VOLUME,
                 CONNECTTOHOST,
                 CONNECTTOSD,
                 CONNECTTOSPEECH,
                 GET_AUDIOSTATUS };

QueueHandle_t audioSetQueue = NULL;
QueueHandle_t audioGetQueue = NULL;

void CreateQueues() {
  audioSetQueue = xQueueCreate(10, sizeof(struct audioMessage));
  audioGetQueue = xQueueCreate(10, sizeof(struct audioMessage));
}

void audioTask(void* parameter) {
  CreateQueues();
  if (!audioSetQueue || !audioGetQueue) {
    log_e("queues are not initialized");
    while (true) { ; }  // endless loop
  }

  struct audioMessage audioRxTaskMessage;
  struct audioMessage audioTxTaskMessage;

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

  while (true) {
    if (xQueueReceive(audioSetQueue, &audioRxTaskMessage, 1) == pdPASS) {
      if (audioRxTaskMessage.cmd == SET_VOLUME) {
        audioTxTaskMessage.cmd = SET_VOLUME;
        audio.setVolume(audioRxTaskMessage.value);
        audioTxTaskMessage.ret = 1;
        xQueueSend(audioGetQueue, &audioTxTaskMessage, portMAX_DELAY);
      } else if (audioRxTaskMessage.cmd == CONNECTTOHOST) {
        audioTxTaskMessage.cmd = CONNECTTOHOST;
        audioTxTaskMessage.ret = audio.connecttohost(audioRxTaskMessage.txt);
        xQueueSend(audioGetQueue, &audioTxTaskMessage, portMAX_DELAY);
      } else if (audioRxTaskMessage.cmd == CONNECTTOSD) {
        audioTxTaskMessage.cmd = CONNECTTOSD;
        audioTxTaskMessage.ret = audio.connecttoFS(SPIFFS,audioRxTaskMessage.txt);
        xQueueSend(audioGetQueue, &audioTxTaskMessage, portMAX_DELAY);
      } else if (audioRxTaskMessage.cmd == GET_VOLUME) {
        audioTxTaskMessage.cmd = GET_VOLUME;
        audioTxTaskMessage.ret = audio.getVolume();
        xQueueSend(audioGetQueue, &audioTxTaskMessage, portMAX_DELAY);
      } else if (audioRxTaskMessage.cmd == GET_AUDIOSTATUS) {
        audioTxTaskMessage.cmd = GET_AUDIOSTATUS;
        audioTxTaskMessage.ret = audio.isRunning();
        xQueueSend(audioGetQueue, &audioTxTaskMessage, portMAX_DELAY);
      } else if (audioRxTaskMessage.cmd == CONNECTTOSPEECH) {
        audioTxTaskMessage.cmd = CONNECTTOSPEECH;
        audioTxTaskMessage.ret = audio.connecttospeech(audioRxTaskMessage.txt, audioRxTaskMessage.lang);
        xQueueSend(audioGetQueue, &audioTxTaskMessage, portMAX_DELAY);
      } else {
        log_i("error");
      }
    }
    audio.loop();
    if (!audio.isRunning()) {
      vTaskDelay(200 / portTICK_PERIOD_MS);
    }
  }
}

void audioInit() {
  xTaskCreatePinnedToCore(
    audioTask,             /* Function to implement the task */
    "audioplay",           /* Name of the task */
    5000,                  /* Stack size in words */
    NULL,                  /* Task input parameter */
    2 | portPRIVILEGE_BIT, /* Priority of the task */
    NULL,                  /* Task handle. */
    1                      /* Core where the task should run */
  );
}


struct audioMessage transmitReceive(audioMessage msg) {
  xQueueSend(audioSetQueue, &msg, portMAX_DELAY);
  if (xQueueReceive(audioGetQueue, &audioRxMessage, portMAX_DELAY) == pdPASS) {
    if (msg.cmd != audioRxMessage.cmd) {
      log_e("wrong reply from message queue");
    }
  }
  return audioRxMessage;
}

void audioSetVolume(uint8_t vol) {
  audioTxMessage.cmd = SET_VOLUME;
  audioTxMessage.value = vol;
  audioMessage RX = transmitReceive(audioTxMessage);
}

uint8_t audioGetVolume() {
  audioTxMessage.cmd = GET_VOLUME;
  audioMessage RX = transmitReceive(audioTxMessage);
  return RX.ret;
}

bool audioConnecttohost(const char* host) {
  audioTxMessage.cmd = CONNECTTOHOST;
  audioTxMessage.txt = host;
  audioMessage RX = transmitReceive(audioTxMessage);
  return RX.ret;
}

bool audioConnecttoSPIFFS(const char* filename) {

  audioTxMessage.cmd = CONNECTTOSD;
  audioTxMessage.txt = filename;
  audioMessage RX = transmitReceive(audioTxMessage);
  return RX.ret;
}
void audio_info(const char* info) {
  Serial.print("info        ");
  Serial.println(info);
}
bool audioConnecttoSpeech(const char* speech, const char* language) {
  audioTxMessage.cmd = CONNECTTOSPEECH;
  audioTxMessage.txt = speech;
  audioTxMessage.lang = language;
  audioMessage RX = transmitReceive(audioTxMessage);
  return RX.ret;
}

bool audiostatus() {
  audioTxMessage.cmd = GET_AUDIOSTATUS;
  audioMessage RX = transmitReceive(audioTxMessage);
  return RX.ret;
}

//****************************************************************************************
//                                  END OF AUDIO PORTION                                 *
//****************************************************************************************



//****************************************************************************************
//                                   WebShare  _ T A S K                                 *
//****************************************************************************************


// Adds the filé systems
void addFileSystems(void) {
    if (!filemgr.AddFS(SPIFFS, "Flash/SPIFF", false))Serial.println(F("Adding FFAT failed."));
    else Serial.println(F("Web Filemanager's Storage is Ok!"));
}

uint32_t checkFileFlags(fs::FS &fs, String filename, uint32_t flags) {
  // Checks if target file name is valid for action. This will simply allow everything by returning the queried flag
  if (flags & ESPFMfGK::flagIsValidAction) {
    return flags & (~ESPFMfGK::flagIsValidAction);
  }

  // Checks if target file name is valid for action.
  if (flags & ESPFMfGK::flagIsValidTargetFilename) {
    return flags & (~ESPFMfGK::flagIsValidTargetFilename);
  }

  // Default actions
  uint32_t defaultflags = ESPFMfGK::flagCanDelete | ESPFMfGK::flagCanRename | ESPFMfGK::flagCanGZip |  // ^t
                          ESPFMfGK::flagCanDownload | ESPFMfGK::flagCanUpload | ESPFMfGK::flagCanEdit | // ^t
                          ESPFMfGK::flagAllowPreview;

  return defaultflags;
}

void setupFilemanager(void) {
  // See above.
  filemgr.checkFileFlags = checkFileFlags;

  filemgr.WebPageTitle = "FileManager";
  filemgr.BackgroundColor = "white";
  filemgr.textareaCharset = "accept-charset=\"utf-8\"";

  if ((WiFi.status() == WL_CONNECTED) && (filemgr.begin())) {
    Serial.print(F("Open Filemanager with http://"));
    Serial.print(WiFi.localIP());
    Serial.print(F(":"));
    Serial.print(filemanagerport);
    Serial.print(F("/"));
    Serial.println();
  } else {
    Serial.print(F("Filemanager: did not start"));
  }
}

//
struct webshareMessage {
  uint8_t cmd;
  String txt;
  bool status;
  uint32_t value;
  uint32_t ret;
} webshareTxMessage, webshareRxMessage;

enum : uint8_t {
  GETWEBSHARE_ADDRESS,
  WEBSHARE_STATE,
  WEBSHARE_DISABLE,
  WEBSHARE_ENABLE
};

QueueHandle_t webshareSetQueue = NULL;
QueueHandle_t webshareGetQueue = NULL;

void webshareCreateQueues() {
  webshareSetQueue = xQueueCreate(10, sizeof(struct webshareMessage));
  webshareGetQueue = xQueueCreate(10, sizeof(struct webshareMessage));
}

void webshareTask(void* parameter) {
  webshareCreateQueues();
  if (!webshareSetQueue || !webshareGetQueue) {
    log_e("queues are not initialized");
    while (true) { ; }  // endless loop
  }

  struct webshareMessage webshareRxTaskMessage;
  struct webshareMessage webshareTxTaskMessage;
  addFileSystems();
  setupFilemanager();
  while (true) {
    
    if (xQueueReceive(webshareSetQueue, &webshareRxTaskMessage, 1) == pdPASS) {
    if (webshareRxTaskMessage.cmd == WEBSHARE_ENABLE) {
        webshareTxTaskMessage.cmd = WEBSHARE_ENABLE;
        WebShareEnabled=true;
        xQueueSend(webshareGetQueue, &webshareTxTaskMessage, portMAX_DELAY);
      } else if (webshareRxTaskMessage.cmd == WEBSHARE_DISABLE) {
        webshareTxTaskMessage.cmd = WEBSHARE_DISABLE;
        WebShareEnabled=false;
        xQueueSend(webshareGetQueue, &webshareTxTaskMessage, portMAX_DELAY);
      } else {
        log_i("error");
      }
    }
    if(WebShareEnabled){filemgr.handleClient();}
    else sleep(5);
  }
}

void webshareInit() {
  xTaskCreatePinnedToCore(
    webshareTask,              /* Function to implement the task */
    "webshare",             /* Name of the task */
    webshare_stack,         /* Stack size in words */
    NULL,                  /* Task input parameter */
    2 | portPRIVILEGE_BIT, /* Priority of the task */
    &webshare_handle,       /* Task handle. */
    app_core               /* Core where the task should run */
  );
}

struct webshareMessage transmitReceive(webshareMessage msg) {
  xQueueSend(webshareSetQueue, &msg, portMAX_DELAY);
  if (xQueueReceive(webshareGetQueue, &webshareRxMessage, portMAX_DELAY) == pdPASS) {
    if (msg.cmd != webshareRxMessage.cmd) {
      log_e("wrong reply from message queue");
    }
  }
  return webshareRxMessage;
}




void webshareEnable() {
  webshareTxMessage.cmd = WEBSHARE_ENABLE;
  webshareMessage RX = transmitReceive(webshareTxMessage);
}

void webshareDisable() {
  webshareTxMessage.cmd = WEBSHARE_DISABLE;
  webshareMessage RX = transmitReceive(webshareTxMessage);
}

//****************************************************************************************
//                                  END OF WebShare PORTION                              *
//****************************************************************************************