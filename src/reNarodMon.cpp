#include "project_config.h"

#if CONFIG_NARODMON_ENABLE

#include "reNarodMon.h"
#include <cstring>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "rLog.h"
#include "rTypes.h"
#include "rStrings.h"
#include "esp_http_client.h"
#include "reWiFi.h"
#include "reEvents.h"
#include "reLed.h"
#include "reStates.h"
#include "reEsp32.h"
#include "sys/queue.h"
#include "def_consts.h"

#define API_NARODMON_HOST "narodmon.ru"
#define API_NARODMON_PORT 443
#define API_NARODMON_SEND_PATH "/get"
#define API_NARODMON_SEND_DATA_SHORT "ID=%s&time=%d&%s"
#define API_NARODMON_SEND_DATA_OWNER "ID=%s&owner=%s&time=%d&%s"
#define API_NARODMON_TIMEOUT_MS 30000

extern const char api_narodmon_pem_start[] asm(CONFIG_NARODMON_TLS_PEM_START);
extern const char api_narodmon_pem_end[]   asm(CONFIG_NARODMON_TLS_PEM_END); 


typedef enum {
  NM_OK         = 0,
  NM_ERROR_API  = 1,
  NM_ERROR_HTTP = 2
} nmSendStatus_t;

typedef struct nmDevice_t {
  const char* id;
  const char* owner;
  uint32_t interval;
  TickType_t next_send;
  uint8_t attempt;
  char* data;
  time_t timestamp;
  SLIST_ENTRY(nmDevice_t) next;
} nmDevice_t;
typedef struct nmDevice_t *nmDeviceHandle_t;

SLIST_HEAD(nmHead_t, nmDevice_t);
typedef struct nmHead_t *nmHeadHandle_t;

typedef struct {
  const char* id;
  char* data;
} nmQueueItem_t;  

#define NARODMON_QUEUE_ITEM_SIZE sizeof(nmQueueItem_t*)

TaskHandle_t _nmTask;
QueueHandle_t _nmQueue = nullptr;
nmHeadHandle_t _nmDevices = nullptr;

static const char* logTAG = "NARM";
static const char* nmTaskName = "narod_mon";

#if CONFIG_NARODMON_STATIC_ALLOCATION
StaticQueue_t _nmQueueBuffer;
StaticTask_t _nmTaskBuffer;
StackType_t _nmTaskStack[CONFIG_NARODMON_STACK_SIZE];
uint8_t _nmQueueStorage [CONFIG_NARODMON_QUEUE_SIZE * NARODMON_QUEUE_ITEM_SIZE];
#endif // CONFIG_NARODMON_STATIC_ALLOCATION

// -----------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------- Device list -------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

bool nmDevicesInit()
{
  _nmDevices = (nmHead_t*)esp_calloc(1, sizeof(nmHead_t));
  if (_nmDevices) {
    SLIST_INIT(_nmDevices);
  };
  return (_nmDevices);
}

bool nmDeviceInit(const char* nmId, const char* nmOwner, const uint32_t nmInterval)
{
  if (!_nmDevices) {
    nmDevicesInit();
  };

  if (!_nmDevices) {
    rlog_e(logTAG, "The device list has not been initialized!");
    return false;
  };
    
  nmDeviceHandle_t device = (nmDevice_t*)esp_calloc(1, sizeof(nmDevice_t));
  if (!device) {
    rlog_e(logTAG, "Memory allocation error for data structure!");
    return false;
  };

  device->id = nmId;
  device->owner = nmOwner;
  if (nmInterval < CONFIG_NARODMON_MIN_INTERVAL) {
    device->interval = pdMS_TO_TICKS(CONFIG_NARODMON_MIN_INTERVAL);
  } else {
    device->interval = pdMS_TO_TICKS(nmInterval);
  };
  device->next_send = 0;
  device->attempt = 0;
  device->data = nullptr;
  device->timestamp = 0;
  SLIST_NEXT(device, next) = nullptr;
  SLIST_INSERT_HEAD(_nmDevices, device, next);
  return true;
}

nmDeviceHandle_t nmDeviceFind(const char* nmId)
{
  nmDeviceHandle_t item;
  SLIST_FOREACH(item, _nmDevices, next) {
    if (strcasecmp(item->id, nmId) == 0) {
      return item;
    }
  }
  return nullptr;
} 

void nmDevicesFree()
{
  nmDeviceHandle_t item, tmp;
  SLIST_FOREACH_SAFE(item, _nmDevices, next, tmp) {
    SLIST_REMOVE(_nmDevices, item, nmDevice_t, next);
    if (item->data) free(item->data);
    free(item);
  };
  free(_nmDevices);
}

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------ Adding data to the send queue ----------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

bool nmSend(const char* nmId, char * nmData)
{
  if (_nmQueue) {
    nmQueueItem_t* item = (nmQueueItem_t*)esp_calloc(1, sizeof(nmQueueItem_t));
    if (item) {
      item->id = nmId;
      item->data = nmData;
      if (xQueueSend(_nmQueue, &item, CONFIG_NARODMON_QUEUE_WAIT / portTICK_RATE_MS) == pdPASS) {
        return true;
      };
    };
    rloga_e("Error adding message to queue [ %s ]!", nmTaskName);
    eventLoopPostSystem(RE_SYS_NARODMON_ERROR, RE_SYS_SET, false);
  };
  return false;
}

// -----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------- Call API --------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

nmSendStatus_t nmSendEx(const nmDeviceHandle_t device)
{
  nmSendStatus_t _result = NM_OK;
  char* get_request = nullptr;

  // Create the text of the GET request
  if (device->data) {
    if (device->owner) {
      get_request = malloc_stringf(API_NARODMON_SEND_DATA_OWNER, device->id, device->owner, device->timestamp, device->data);
    } else {
      get_request = malloc_stringf(API_NARODMON_SEND_DATA_SHORT, device->id, device->timestamp, device->data);
    };
  };

  if (get_request) {
    // Configuring request parameters
    esp_http_client_config_t cfgHttp;
    memset(&cfgHttp, 0, sizeof(cfgHttp));
    cfgHttp.method = HTTP_METHOD_GET;
    cfgHttp.host = API_NARODMON_HOST;
    cfgHttp.port = API_NARODMON_PORT;
    cfgHttp.path = API_NARODMON_SEND_PATH;
    cfgHttp.timeout_ms = API_NARODMON_TIMEOUT_MS;
    cfgHttp.query = get_request;
    cfgHttp.use_global_ca_store = false;
    cfgHttp.transport_type = HTTP_TRANSPORT_OVER_SSL;
    cfgHttp.cert_pem = api_narodmon_pem_start;
    cfgHttp.skip_cert_common_name_check = false;
    cfgHttp.is_async = false;

    // Make a request to the API
    esp_http_client_handle_t client = esp_http_client_init(&cfgHttp);
    if (client != NULL) {
      esp_err_t err = esp_http_client_perform(client);
      if (err == ESP_OK) {
        int retCode = esp_http_client_get_status_code(client);
        if ((retCode >= HttpStatus_Ok) && (retCode <= HttpStatus_BadRequest)) {
          _result = NM_OK;
          rlog_i(logTAG, "Data sent [ %s ]: %s", device->id, get_request);
        } else {
          _result = NM_ERROR_API;
          rlog_e(logTAG, "Failed to send message, API error code: #%d!", retCode);
        };
        // Flashing system LED
        #if CONFIG_SYSLED_SEND_ACTIVITY
        ledSysActivity();
        #endif // CONFIG_SYSLED_SEND_ACTIVITY
      }
      else {
        _result = NM_ERROR_HTTP;
        rlog_e(logTAG, "Failed to complete request to narodmon.ru, error code: 0x%x!", err);
      };
      esp_http_client_cleanup(client);
    }
    else {
      _result = NM_ERROR_HTTP;
      rlog_e(logTAG, "Failed to complete request to narodmon.ru!");
    };
  };
  // Remove the request from memory
  if (get_request) free(get_request);
  return _result;
}

// -----------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------- Queue processing --------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

void nmTaskExec(void *pvParameters)
{
  nmQueueItem_t * item = nullptr;
  nmDeviceHandle_t device = nullptr;
  TickType_t wait_queue = portMAX_DELAY;
  static nmSendStatus_t send_status = NM_ERROR_HTTP;
  static uint32_t send_errors = 0;
  static time_t time_first_error = 0;
  
  while (true) {
    // Receiving new data
    if (xQueueReceive(_nmQueue, &item, wait_queue) == pdPASS) {
      device = nmDeviceFind(item->id);
      if (device) {
        // Replacing device data with new ones from the transporter
        device->attempt = 0;
        device->timestamp = time(nullptr);
        if (device->data) free(device->data);
        device->data = item->data;
        item->data = nullptr;
      } else {
        rlog_e(logTAG, "Device [ %s ] not found!", item->id);
      };
      // Free transporter
      if (item->data) free(item->data);
      free(item);
      item = nullptr;
    };

    // Check internet availability 
    if (statesInetIsAvailabled()) {
      device = nullptr;
      wait_queue = portMAX_DELAY;
      SLIST_FOREACH(device, _nmDevices, next) {
        // Sending data
        if (device->data) {
          if (device->next_send < xTaskGetTickCount()) {
            // Attempt to send data
            device->attempt++;
            send_status = nmSendEx(device);
            if (send_status == NM_OK) {
              // Calculate the time of the next dispatch in the given device
              device->next_send = xTaskGetTickCount() + device->interval;
              device->attempt = 0;
              if (device->data) {
                free(device->data);
                device->data = nullptr;
              };
              // If the error counter exceeds the threshold, then a notification has been sent - send a recovery notification
              if (send_errors >= CONFIG_NARODMON_ERROR_LIMIT) {
                eventLoopPostSystem(RE_SYS_NARODMON_ERROR, RE_SYS_CLEAR, false, time_first_error);
              };
              time_first_error = 0;
              send_errors = 0;
            } else {
              // Increase the number of errors in a row and fix the time of the first error
              send_errors++;
              if (time_first_error == 0) {
                time_first_error = time(nullptr);
              };
              // Calculate the time of the next dispatch in the given device
              if (device->attempt < CONFIG_NARODMON_MAX_ATTEMPTS) {
                device->next_send = xTaskGetTickCount() + pdMS_TO_TICKS(CONFIG_NARODMON_ERROR_INTERVAL);
              } else {
                if (device->data) {
                  free(device->data);
                  device->data = nullptr;
                };
                device->next_send = xTaskGetTickCount() + device->interval;
                device->attempt = 0;
                rlog_e(logTAG, "Failed to send data to device [ %s ]!", device->id);
              };
              // If the error counter has reached the threshold, send a notification
              if (send_errors == CONFIG_NARODMON_ERROR_LIMIT) {
                eventLoopPostSystem(RE_SYS_NARODMON_ERROR, RE_SYS_SET, false, time_first_error);
              };
            };
          };

          // Find the minimum delay before the next sending to the device
          if (device->data) {
            TickType_t send_delay = 0;
            if (device->next_send > xTaskGetTickCount()) {
              send_delay = device->next_send - xTaskGetTickCount();
            };
            if (send_delay > device->interval) {
              device->next_send = xTaskGetTickCount() + device->interval;
              send_delay = device->interval;
            };
            if (send_delay < wait_queue) {
              wait_queue = send_delay;
            };
          };
        };
      };
    } else {
      // If the Internet is not available, repeat the check every second
      wait_queue = pdMS_TO_TICKS(1000); 
    };
  };
  nmTaskDelete();
}

// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------- Task routines ------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

bool nmTaskSuspend()
{
  if ((_nmTask) && (eTaskGetState(_nmTask) != eSuspended)) {
    vTaskSuspend(_nmTask);
    if (eTaskGetState(_nmTask) == eSuspended) {
      rloga_d("Task [ %s ] has been suspended", nmTaskName);
      return true;
    } else {
      rloga_e("Failed to suspend task [ %s ]!", nmTaskName);
    };
  };
  return false;  
}

bool nmTaskResume()
{
  if ((_nmTask) && (eTaskGetState(_nmTask) == eSuspended)) {
    vTaskResume(_nmTask);
    if (eTaskGetState(_nmTask) != eSuspended) {
      rloga_i("Task [ %s ] has been successfully resumed", nmTaskName);
      return true;
    } else {
      rloga_e("Failed to resume task [ %s ]!", nmTaskName);
    };
  };
  return false;  
}

bool nmTaskCreate(bool createSuspended) 
{
  if (!_nmTask) {
    if (!_nmDevices) {
      if (!nmDevicesInit()) {
        rloga_e("Failed to create a list of devices!");
        eventLoopPostSystem(RE_SYS_ERROR, RE_SYS_SET, false);
        return false;
      };
    };

    if (!_nmQueue) {
      #if CONFIG_NARODMON_STATIC_ALLOCATION
      _nmQueue = xQueueCreateStatic(CONFIG_NARODMON_QUEUE_SIZE, NARODMON_QUEUE_ITEM_SIZE, &(_nmQueueStorage[0]), &_nmQueueBuffer);
      #else
      _nmQueue = xQueueCreate(CONFIG_NARODMON_QUEUE_SIZE, NARODMON_QUEUE_ITEM_SIZE);
      #endif // CONFIG_NARODMON_STATIC_ALLOCATION
      if (!_nmQueue) {
        nmDevicesFree();
        rloga_e("Failed to create a queue for sending data to NarodMon!");
        eventLoopPostSystem(RE_SYS_ERROR, RE_SYS_SET, false);
        return false;
      };
    };
    
    #if CONFIG_NARODMON_STATIC_ALLOCATION
    _nmTask = xTaskCreateStaticPinnedToCore(nmTaskExec, nmTaskName, CONFIG_NARODMON_STACK_SIZE, NULL, CONFIG_NARODMON_PRIORITY, _nmTaskStack, &_nmTaskBuffer, CONFIG_NARODMON_CORE); 
    #else
    xTaskCreatePinnedToCore(nmTaskExec, nmTaskName, CONFIG_NARODMON_STACK_SIZE, NULL, CONFIG_NARODMON_PRIORITY, &_nmTask, CONFIG_NARODMON_CORE); 
    #endif // CONFIG_NARODMON_STATIC_ALLOCATION
    if (_nmTask == NULL) {
      vQueueDelete(_nmQueue);
      nmDevicesFree();
      rloga_e("Failed to create task for sending data to NarodMon!");
      eventLoopPostSystem(RE_SYS_ERROR, RE_SYS_SET, false);
      return false;
    }
    else {
      if (createSuspended) {
        rloga_i("Task [ %s ] has been successfully created", nmTaskName);
        nmTaskSuspend();
        eventLoopPostSystem(RE_SYS_NARODMON_ERROR, RE_SYS_SET, false);
      } else {
        rloga_i("Task [ %s ] has been successfully started", nmTaskName);
        eventLoopPostSystem(RE_SYS_NARODMON_ERROR, RE_SYS_CLEAR, false);
      };
      return nmEventHandlerRegister();
    };
  };
  return false;
}

bool nmTaskDelete()
{
  nmDevicesFree();

  if (_nmQueue != NULL) {
    vQueueDelete(_nmQueue);
    _nmQueue = NULL;
  };

  if (_nmTask != NULL) {
    vTaskDelete(_nmTask);
    _nmTask = NULL;
    rloga_d("Task [ %s ] was deleted", nmTaskName);
  };
  
  return true;
}

// -----------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------- Events handlers ---------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static void nmWiFiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if (event_id == RE_WIFI_STA_PING_OK) {
    nmTaskResume();
  };
}

static void nmOtaEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if ((event_id == RE_SYS_OTA) && (event_data)) {
    re_system_event_data_t* data = (re_system_event_data_t*)event_data;
    if (data->type == RE_SYS_SET) {
      nmTaskSuspend();
    } else {
      nmTaskResume();
    };
  };
}

bool nmEventHandlerRegister()
{
  return eventHandlerRegister(RE_WIFI_EVENTS, RE_WIFI_STA_PING_OK, &nmWiFiEventHandler, nullptr)
      && eventHandlerRegister(RE_SYSTEM_EVENTS, RE_SYS_OTA, &nmOtaEventHandler, nullptr);
};

#endif // CONFIG_NARODMON_ENABLE
