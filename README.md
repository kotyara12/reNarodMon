# reOpenMon for ESP32

**EN**: Sending sensor data to http://open-monitoring.online/ with a specified interval and sending queue. For ESP32 only, since it was released as a FreeRTOS task and on ESP32-specific functions. Controller field values (data) are passed to the queue as a string (char*), which is automatically deleted after sending. That is, to send, you must place a line with data on the heap, and then send it to the library queue.

> Controller parameters must be set in "project_config.h" (see below for description)
---
**RU**: Отправка данных сенсоров на http://open-monitoring.online/ с заданным интервалом и очередью отправки. Только для ESP32, так как релизовано в виде задачи FreeRTOS и на специфических для ESP32 функциях. Значения полей контроллера (данные) передаются в очередь в виде строки (char*), которая автоматически удаляется после отправки. То есть для отправки Вы должны разместить в куче строку с данными, а затем отправить ее в очередь библиотеки.

> Параметры контроллера должны быть заданы в "project_config.h" (описание см. ниже)
---

## Using / Использование:

**EN**: ***Task management: create, suspend, resume and delete***<br/>
**RU**: ***Управление задачей: создание, приостановка, восстановление и удаление***<br/>
```
// @param createSuspended - Create but do not run a task / Создать, но не запускать задачу

bool omTaskCreate(bool createSuspended);
bool omTaskSuspend();
bool omTaskResume();
bool omTaskDelete();
```

**EN**: ***Controller initialization***<br/>
**RU**: ***Создание контроллера***<br/>
```
// @param om_id - Controller ID / Идентификатор контроллера
// @param om_key - Controller token / Токен контроллера

om_ctrl_t * omInitController(const uint32_t om_id, const char * om_key);
```

**EN**: ***Removing a controller and free resources***<br/>
**RU**: ***Удаление контроллера и освобождение ресурсов***<br/>
```
void omFreeController(om_ctrl_t * omController);
```

**EN**: ***Sending data to the specified controller***<br/>
_If little time has passed since the last data sent to the controller, the data will be queued. If there is already data in the queue for this controller, it will be overwritten with new data_<br/>
**RU**: ***Отправка данных в заданный контроллер***<br/>
_Если с момента последней отправки данных в контроллер прошло мало времени, то данные будут поставлены в очередь. Если в очереди на данный контроллер уже есть данные, то они будут перезаписаны новыми данными_<br/>
```
// @param omId - Controller ID / Идентификатор контроллера
// @param omFields - Data in the format p1=VALUE1&p2=VALUE2&p3=VALUE3... / Данные в формате p1=ЗНАЧ1&p2=ЗНАЧ2&p3=ЗНАЧ3...

bool omSend(const uint32_t omId, char * omFields);
```

**EN**: ***Registering event handlers in the main event loop***<br/>
**RU**: ***Регистрация обработчиков событий в главном цикле событий***<br/>
```
bool omEventHandlerRegister();
```

## Parameters / Параметры:

**EN**: _Enable sending data to open-monitoring.online_<br/>
**RU**: _Включить отправку данных на open-monitoring.online_<br/>
- #define CONFIG_OPENMON_ENABLE 1<br/>

**EN**: _Frequency of sending data in seconds_<br/>
**RU**: _Периодичность отправки данных в секундах_<br/>
- #define CONFIG_OPENMON_SEND_INTERVAL 180<br/>

**EN**: _Minimum server access interval in milliseconds_<br/>
**RU**: _Минимальный интервал обращения к серверу в миллисекундах_<br/>
- #define CONFIG_OPENMON_MIN_INTERVAL 60100<br/>

**EN**: _Stack size for the task of sending data to open-monitoring.online_<br/>
**RU**: _Размер стека для задачи отправки данных на open-monitoring.online_<br/>
- #define CONFIG_OPENMON_STACK_SIZE 2048<br/>

**EN**: _Queue size for the task of sending data to open-monitoring.online_<br/>
**RU**: _Размер очереди для задачи отправки данных на open-monitoring.online_<br/>
- #define CONFIG_OPENMON_QUEUE_SIZE 8<br/>

**EN**: _Timeout for writing to the queue for sending data on open-monitoring.online_<br/>
**RU**: _Время ожидания записи в очередь отправки данных на open-monitoring.online_<br/>
- #define CONFIG_OPENMON_QUEUE_WAIT 100<br/>

**EN**: _The priority of the task of sending data to open-monitoring.online_<br/>
**RU**: _Приоритет задачи отправки данных на open-monitoring.online_<br/>
- #define CONFIG_OPENMON_PRIORITY 5<br/>

**EN**: _Processor core for the task of sending data to open-monitoring.online_<br/>
**RU**: _Ядро процессора для задачи отправки данных на open-monitoring.online_<br/>
- #define CONFIG_OPENMON_CORE 1<br/>

**EN**: _The number of attempts to send data to open-monitoring.online_<br/>
**RU**: _Количество попыток отправки данных на open-monitoring.online_<br/>
- #define CONFIG_OPENMON_MAX_ATTEMPTS 3<br/>

**EN**: _Controller ids and tokens for open-monitoring.online_<br/>
**RU**: _Идентификаторы контроллеров и токены для open-monitoring.online_<br/>
#define CONFIG_OPENMON_CTR01_ID 0001<br/>
#define CONFIG_OPENMON_CTR01_TOKEN "aaaaaa"<br/>
#define CONFIG_OPENMON_CTR02_ID 0002<br/>
#define CONFIG_OPENMON_CTR02_TOKEN "bbbbbb"<br/>
#define CONFIG_OPENMON_CTR03_ID 0003<br/>
#define CONFIG_OPENMON_CTR03_TOKEN "cccccc"<br/>

## Dependencies / Зависимости:
- freertos/FreeRTOS.h (ESP-IDF)
- freertos/task.h (ESP-IDF)
- esp_http_client.h (ESP-IDF)
- https://github.com/kotyara12/rTypes
- https://github.com/kotyara12/rLog
- https://github.com/kotyara12/rStrings
- https://github.com/kotyara12/reLed
- https://github.com/kotyara12/reStates
- https://github.com/kotyara12/reWiFi

## Note
**EN**: These comments refer to my libraries hosted on the resource https://github.com/kotyara12?tab=repositories.

- libraries whose name starts with the **re** prefix are intended only for ESP32 and ESP-IDF (FreeRTOS)
- libraries whose name begins with the **ra** prefix are intended only for ARDUINO
- libraries whose name starts with the **r** prefix can be used for both ARDUINO and ESP-IDF

Since I am currently developing programs mainly for ESP-IDF, the bulk of my libraries are intended only for this framework. But you can port them to another system using.

**RU**: Данные замечания относятся к моим библиотекам, размещенным на ресурсе https://github.com/kotyara12?tab=repositories.

- библиотеки, название которых начинается с префикса **re**, предназначены только для ESP32 и ESP-IDF (FreeRTOS)
- библиотеки, название которых начинается с префикса **ra**, предназначены только для ARDUINO
- библиотеки, название которых начинается с префикса **r**, могут быть использованы и для ARDUINO, и для ESP-IDF

Так как я в настроящее время разрабатываю программы в основном для ESP-IDF, основная часть моих библиотек предназначена только для этого фреймворка. Но Вы можете портировать их для другой системы, взяв за основу.
