# reNarodMon для ESP32 / ESP-IDF

Библиотека для отправки данных на https://narodmon.ru/ с заданным минимальным интервалом и очередью отправки с помощью GET-запроса. Данная библиотека может быть использована только для ESP32, так как релизовано в виде задачи FreeRTOS и на специфических для ESP32 функциях. Теоретически, быблиотека может быть использована не только для ESP-IDF framework, но и в Arduino IDE, однако я не проверял эту возможность, Вы можете сделать это самостоятельно.
Значения полей контроллера (данные) передаются в очередь в виде строки (char*, _не "String"_!), которая будет автоматически удалена из кучи (heap) после отправки. Иными словами - для отправки Вы должны разместить в куче строку с данными и отправить ее в очередь библиотеки с помощью функции _nmSend_. Всё остальное - контроль минимального интервала, генерацию GET-запроса, освобождение ресурсов после отправки - берёт на себя библиотека.

---
## Использование:

Все функции возвращают true в случае успеха и false в случае какой-либо проблемы. В случае проблем библиотека сама напечатает отладочные сообщения в терминал, поэтому дополнительный возврат кодов ошибок не предусмотрен за ненадобностью. Да, многие не согласятся с таким подходом, но в таком случае Вы можете переписать по своему.

### Управление задачей
:arrow_forward: ***Создание и запуск задачи и очереди отправки***<br/>

Прежде чем устройство сможет начать отправку данных на narodmon.ru, необходимо создать и запустить задачу, которая и будет заниматься непосредственной пересылкой данных. Сделать это можно с помощью функции ***nmTaskCreate(bool createSuspended)***:
```
bool nmTaskCreate(bool createSuspended);
```
Если параметр ***createSuspended = true***, то созданная задача будет сразу же приостановлена до вызова ***nmTaskResume()***. Это может быть необходимо, если на момент создания задачи подключение к сети WiFi и (или) Интернет, ещё не установлено. Как только доступ к сети интернет будет получено, выполнить вызов ***nmTaskResume()*** для запуска задачи. Если Вы создаете задачу уже после установки соединения, то необходимо сразу запустить задачу в работу, установив ***createSuspended = false***.

:repeat_one: ***Приостановка и восстановление (продолжение) задачи***<br/>

В процессе работы устройства может возникнуть ситуация, когда временно прекращен доступ к API narodmon.ru. Это может быть по причине отключения от WiFi-сети, проблемах с провайдером и т.д. Чтобы не терять ресурсы на бесплодные попытки отправки данных, задачу можно приостановить. Сделать это можно с помощью следующих функций:

```
bool nmTaskSuspend();
bool nmTaskResume();
```
Впрочем, делать это совсем не обязательно - если устройство "знает", что доступа к сети интернет нет, то отправка выполняться всё равно не будет. Последние отправляемые данные будут сохранены в "очереди" отправки.

:x: ***Удаление задачи***<br/>

Если отправка данных на narodmon.ru более не требуется, задачу можно удалить. Например перед перезапуском устройства.

```
bool nmTaskDelete();
```

### Управление устройствами
Обычно на одном "физическом" контроллере необходимо инициализировать только одно устройство (в данном случае под "устройством" понимается устройство на сайте narodmon.ru). И добавить отправку в него данных сразу с нескольких сенсоров. Однако никто не запрещает отправлять с одного микроконтроллера данные в несколько устройств одновременно. 

:pager: ***Создание устройства***<br/>

С помощью функции ***nmDeviceInit(...)*** создается "учетная запись" устройства:
```
bool nmDeviceInit(const char* nmId, const char* nmOwner, const uint32_t nmInterval);
```
Параметры:
- **const char\* nmId** - идентификатор устройства (обычно MAC-адрес). Обязательное поле.
- **const char\* nmOwner** - владелец устройства. Облегчает "привязку" устройтсва к Вашей учетной записи при первом запуске. В дальнейшем можно передать NULL (nullptr). Не возбраняется указывать владельца при каждой отправке данных, однако это немного увеличивает длину GET-запроса, особененно если передаваемый трафик с устройства критичен.
- **uint32_t nmInterval** - минимальный интервал отправки данных в миллисекундах. narodmon.ru в общем случае требует минимальный интервал 5 минут, то есть 300 секунд. Можно задать больше, например 600 секунд.

### Отправка данных

Добавление данных в очередь отправки задачи осуществляется с помощью функции **nmSend(...)**. 
```
bool nmSend(const char* nmId, char * nmData);
```
Параметры:
- **const char\* nmId** - идентификатор устройства (обычно MAC-адрес). Тот же самый, что и при вызове **nmDeviceInit()**. Обязательное поле.
- **char\* nmData** - строка с данными. Перед отправкой Вы должны разместить в куче (heap) строку с необходимыми данными, в формате, определенным narodmon.ru для GET-запросов (_mac1=ЗНАЧ1&mac2=ЗНАЧ2&mac3=ЗНАЧ3_, где _macX_ - это идентификатор сенсора устройства), например: _Тout=15.78&Hout=78.54&Tin=23.40_. После успешной отправки (или нескольких неудачных попыток отправки) эта строка будет автоматически удалена из оперативной памяти, Вам не требуется заботится об её удалении. Если с момента последней отправки данных в выбранное устройство прошло слишком мало времени, то данные будут поставлены в очередь отправки до истечения интервала отправки. Если в очереди на данный контроллер уже есть данные, то они будут перезаписаны новыми данными.

### Регистрация обработчиков событий в главном цикле событий контроллера

С помощью  **nmEventHandlerRegister()** можно зарегистрировать обработчики системных событий - подключение к сети WiFi, наличие / отстутствие доступа к сети интернет и т.д. В этом случае "ручной" вызов **nmTaskSuspend()** и **nmTaskResume()** не требуется.

```
bool nmEventHandlerRegister();
```
---
## Параметры:

Параметры библиотеки задаются с помощью следущих макросов препроцессора (#define). Вы можете разместить их в любом удобном файле или нескольких файлах. Самое главное - компилятору должно быть "известно" об этих файлах при сборке библиотек.

```
// EN: TLS certificate for API
// RU: TLS-сертификат для API
#define CONFIG_NARODMON_TLS_PEM_START "_binary_isrg_root_x1_pem_start"
#define CONFIG_NARODMON_TLS_PEM_END "_binary_isrg_root_x1_pem_end"
// EN: Minimum server access interval in milliseconds
// RU: Минимальный интервал обращения к серверу в миллисекундах
#define CONFIG_NARODMON_MIN_INTERVAL 300000
// EN: Server access interval in milliseconds for API failures
// RU: Интервал обращения к серверу в миллисекундах при отказах API
#define CONFIG_NARODMON_ERROR_INTERVAL 180000
// EN: Use static memory allocation for the task and queue. CONFIG_FREERTOS_SUPPORT_STATIC_ALLOCATION must be enabled!
// RU: Использовать статическое выделение памяти под задачу и очередь. Должен быть включен параметр CONFIG_FREERTOS_SUPPORT_STATIC_ALLOCATION!
#define CONFIG_NARODMON_STATIC_ALLOCATION 1
// EN: Stack size for the task of sending data to narodmon.ru
// RU: Размер стека для задачи отправки данных на narodmon.ru
#define CONFIG_NARODMON_STACK_SIZE 3072
// EN: Queue size for the task of sending data to narodmon.ru
// RU: Размер очереди для задачи отправки данных на narodmon.ru
#define CONFIG_NARODMON_QUEUE_SIZE 8
// EN: Timeout for writing to the queue for sending data on narodmon.ru
// RU: Время ожидания записи в очередь отправки данных на narodmon.ru
#define CONFIG_NARODMON_QUEUE_WAIT 100
// EN: The priority of the task of sending data to narodmon.ru
// RU: Приоритет задачи отправки данных на narodmon.ru
#define CONFIG_NARODMON_PRIORITY CONFIG_DEFAULT_TASK_PRIORITY
// EN: Processor core for the task of sending data to narodmon.ru
// RU: Ядро процессора для задачи отправки данных на narodmon.ru
#define CONFIG_NARODMON_CORE 1
// EN: The number of attempts to send data to narodmon.ru
// RU: Количество попыток отправки данных на narodmon.ru
#define CONFIG_NARODMON_MAX_ATTEMPTS 3
// EN: The number of errors in accessing the server in a row, after which a notification will be sent to the event loop
// RU: Количество ошибок обращения к серверу подряд, после чего будет отправлено оповещение в цикл событий
#define CONFIG_NARODMON_ERROR_LIMIT 10
```

Параметры контроллера можно задать примерно так:
```
// EN: Enable sending data to narodmon.ru
// RU: Включить отправку данных на narodmon.ru
#define CONFIG_NARODMON_ENABLE 1
#if CONFIG_NARODMON_ENABLE
// EN: Frequency of sending data in seconds
// RU: Периодичность отправки данных в секундах
#define CONFIG_NARODMON_SEND_INTERVAL 300
#define CONFIG_NARODMON_DEVICE01_ID "a0:b1:c2:d3:e4:f5"
#define CONFIG_NARODMON_DEVICE01_OWNER "your_login_on_narodmon"
#endif // CONFIG_NARODMON_ENABLE
```
---
## Пример использования
1. Где-то в коде **void app_main(void)** (главная функция для ESP-IDF, которая запускается при старте FreeRTOS. Для Arduino это будет **void setup()**):
```
#if CONFIG_NARODMON_ENABLE
#include "reNarodMon.h"
#endif // CONFIG_NARODMON_ENABLE

...
...
...

  // Запуск службы отправки данных на narodmon.ru
  #if CONFIG_NARODMON_ENABLE
    nmTaskCreate(false);
    vTaskDelay(1);
  #endif // CONFIG_NARODMON_ENABLE
```

2. В теле задачи, которая занимается считыванием, анализом и публикацией данных с сенсоров:
```
void sensorsTaskExec(void *pvParameters)
{
  ...
  // Инициализация устройств NarodMon
  #if CONFIG_NARODMON_ENABLE
    nmDeviceInit(CONFIG_NARODMON_DEVICE01_ID, CONFIG_NARODMON_DEVICE01_OWNER, CONFIG_NARODMON_MIN_INTERVAL);
  #endif // CONFIG_NARODMON_ENABLE
  ...
  #if CONFIG_NARODMON_ENABLE
    esp_timer_t nmSendTimer;
    timerSet(&nmSendTimer, iNarodMonInterval*1000);
  #endif // CONFIG_NARODMON_ENABLE
  ...

  while (1) {
    // Чтение данных с сенсоров
    sensorOutdoor.readData();

    // narodmon.ru
    #if CONFIG_NARODMON_ENABLE
      if (statesInetIsAvailabled() && timerTimeout(&nmSendTimer) && (sensorOutdoor.getStatus() == SENSOR_STATUS_OK)) {
        timerSet(&nmSendTimer, iNarodMonInterval*1000);
        nmSend(CONFIG_NARODMON_DEVICE01_ID, malloc_stringf("T1=%.2f&H1=%.2f", 
          sensorOutdoor.getValue2(false).filteredValue, sensorOutdoor.getValue1(false).filteredValue));
      };
    #endif // CONFIG_NARODMON_ENABLE

    vTaskDelay(pdMS_TO_TICKS(_sensorsReadInterval * 1000));
  };
}
```
**Вопрос**: Зачем нужен таймер nmSendTimer, если библиотека сама умеет отслеживать необходимые интервалы?<br/>
**Ответ**: Вы правы, он не является обязательным. Но с помощью дополнительного внешнего таймера я могу регулировать интервал "извне", не изменяя программы (с помощью MQTT например). А библиотека контролирует только минимальный интервал, например при внеочередных изменениях (включение реле, например).<br/>

**Вопрос**: Это что за _malloc_stringf()_?<br/>
**Ответ**: Функция для динамического размещения строк в куче с форматированием. Ищите в https://github.com/kotyara12/rStrings. Там Вы найдете ещё несколько полезных функций.<br/>

**Вопрос**: Это что за _sensorOutdoor_?<br/>
**Ответ**: Экземпляр класса-сенсора, см. https://github.com/kotyara12/reSensors. Просто для наглядности. Вы можете использовать свой код.<br/>

_Если у Вас имеются дополнительные вопросы: пишите в telegram @kotyara1971_.
---

## Зависимости:

- freertos/FreeRTOS.h (ESP-IDF)
- freertos/task.h (ESP-IDF)
- esp_http_client.h (ESP-IDF)
- https://github.com/kotyara12/rTypes
- https://github.com/kotyara12/rLog
- https://github.com/kotyara12/rStrings
- https://github.com/kotyara12/reLed
- https://github.com/kotyara12/reStates
- https://github.com/kotyara12/reWiFi

## Примечания

Данные замечания относятся к моим библиотекам, размещенным на ресурсе https://github.com/kotyara12?tab=repositories.

- библиотеки, название которых начинается с префикса **re**, предназначены только для ESP32 и ESP-IDF (FreeRTOS)
- библиотеки, название которых начинается с префикса **ra**, предназначены только для ARDUINO
- библиотеки, название которых начинается с префикса **r**, могут быть использованы и для ARDUINO, и для ESP-IDF

Так как я в настроящее время разрабатываю программы в основном для ESP-IDF, основная часть моих библиотек предназначена только для этого фреймворка. Но Вы можете портировать их для другой системы, взяв за основу.
