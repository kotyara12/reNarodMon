/* 
   EN: Module for sending data to https://narodmon.ru/ from ESP32
   RU: Модуль для отправки данных на https://narodmon.ru/ из ESP32
   --------------------------
   (с) 2022 Разживин Александр | Razzhivin Alexander
   kotyara12@yandex.ru | https://kotyara12.ru | tg: @kotyara1971
*/

#ifndef __RE_NARODMON_H__
#define __RE_NARODMON_H__

#include <stddef.h>
#include <sys/types.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h" 
#include "freertos/semphr.h" 
#include "project_config.h"
#include "def_consts.h"

#ifdef __cplusplus
extern "C" {
#endif

bool nmDevicesInit();

/**
 * EN: Task management: create, suspend, resume and delete
 * RU: Управление задачей: создание, приостановка, восстановление и удаление
 **/
bool nmTaskCreate(bool createSuspended);
bool nmTaskSuspend();
bool nmTaskResume();
bool nmTaskDelete();

/**
 * EN: Adding a new device to the list
 * RU: Добавление нового устройства в список
 * 
 * @param nmId - Device ID (MAC) / Идентификатор устройства (MAC)
 * @param nmOwner - Owner device / Владелец устройства (не обязательное поле)
 * @param nmInterval - Minimal interval / Минимальный интервал
 **/
bool nmDeviceInit(const char* nmId, const char* nmOwner, const uint32_t nmInterval);

/**
 * EN: Sending data to the specified controller. The fields string will be removed after submission.
 * If little time has passed since the last data sent to the controller, the data will be queued.
 * If there is already data in the queue for this controller, it will be overwritten with new data.
 * 
 * RU: Отправка данных в заданное устройство. Строка data будет удалена после отправки. 
 * Если с момента последней отправки данных на сайт прошло мало времени, то данные будут поставлены в очередь.
 * Если в очереди на данное устройство уже есть данные, то они будут перезаписаны новыми данными.
 * 
 * @param nmId - Device ID (MAC) / Идентификатор устройства (MAC)
 * @param nmData - Data in the format mac1=VALUE1&mac2=VALUE2&mac3=VALUE3... / Данные в формате mac1=ЗНАЧ1&mac2=ЗНАЧ2&mac3=ЗНАЧ3...
 **/
bool nmSend(const char* nmId, char * nmData);

/**
 * EN: Registering event handlers in the main event loop
 * 
 * RU: Регистрация обработчиков событий в главном цикле событий
 **/
bool nmEventHandlerRegister();

#ifdef __cplusplus
}
#endif

#endif // __RE_NARODMON_H__
