# reNarodMon для ESP32 / ESP-IDF

Библиотека для отправки данных на https://narodmon.ru/ с заданным минимальным интервалом и очередью отправки с помощью GET-запроса. Данная библиотека может быть использована только для ESP32, так как релизовано в виде задачи FreeRTOS и на специфических для ESP32 функциях. Теоретически, быблиотека может быть использована не только для ESP-IDF framework, но и в Arduino IDE, однако я не проверял эту возможность, Вы можете сделать это самостоятельно.
Значения полей контроллера (данные) передаются в очередь в виде строки (char*, _не "String"_!), которая будет автоматически удалена из кучи (heap) после отправки. Иными словами - для отправки Вы должны разместить в куче строку с данными и отправить ее в очередь библиотеки с помощью функции _nmSend_. Всё остальное - контроль минимального интервала, генерацию GET-запроса, освобождение ресурсов после отправки - берёт на себя библиотека.

---

## Использование:

Все функции возвращают true в случае успеха и false в случае какой-либо проблемы. В случае проблем библиотека сама напечатает отладочные сообщения в терминал, поэтому дополнительный возврат кодов ошибок не предусмотрен за ненадобностью. Да, многие не согласятся с таким подходом, но в таком случае Вы можете переписать по своему.

***1. Создание и запуск задачи и очереди отправки***<br/>

Прежде чем устройство сможет начать отправку данных на narodmon.ru, необходимо создать и запустить задачу, которая и будет заниматься непосредственной пересылкой данных. Сделать это можно с помощью функции ***nmTaskCreate(bool createSuspended)***:
```
bool nmTaskCreate(bool createSuspended);
```
Если параметр ***createSuspended = true***, то созданная задача будет сразу же приостановлена до вызова ***nmTaskResume()***. Это может быть необходимо, если на момент создания задачи подключение к сети WiFi и (или) Интернет, ещё не установлено. Как только доступ к сети интернет будет получено, выполнить вызов ***nmTaskResume()*** для запуска задачи. Если Вы создаете задачу уже после установки соединения, то необходимо сразу запустить задачу в работу, установив ***createSuspended = false***.

***2. Приостановка и восстановление (продолжение) задачи***<br/>

В процессе работы устройства может возникнуть ситуация, когда временно прекращен доступ к API narodmon.ru. Это может быть по причине отключения от WiFi-сети, проблемах с провайдером и т.д. Чтобы не терять ресурсы на бесплодные попытки отправки данных, задачу можно приостановить. Сделать это можно с помощью следующих функций:

```
bool nmTaskSuspend();
bool nmTaskResume();
```
Впрочем, делать это совсем не обязательно - если устройство "знает", что доступа к сети интернет нет, то отправка выполняться всё равно не будет. Последние отправляемые данные будут сохранены в "очереди" отправки.

***3. Удаление задачи***<br/>

Если отправка данных на narodmon.ru более не требуется, задачу можно удалить. Например перед перезапуском устройства.

```
bool nmTaskDelete();
```

***4. Создание устройтства***<br/>

С помощью функции ***nmDeviceInit(...)*** создается "учетная запись" устройства (в данном случае под "устройством" понимается устройство на сайте narodmon.ru). 
```
bool nmDeviceInit(const char* nmId, const char* nmOwner, const uint32_t omInterval);
```
Параметры:
- **const char* nmId** - идентификатор устройства (обычно MAC-адрес). Обязательное поле.
- **const char* nmOwner** - владелец устройства (обычно MAC-адрес). Облегчает "привязку" устройтсва к Вашей учетной записи при первом запуске. В дальнейшем можно передать NULL (nullptr).
- **uint32_t omInterval** - минимальный интервал отправки данных в секундах. narodmon.ru в общем случае требует минимальный интервал 5 минут, то есть 300 секунд. Можно задать больше, например 600 секунд.

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
