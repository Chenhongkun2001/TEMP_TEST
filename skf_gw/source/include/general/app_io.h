/**
 * @file    app_io.h
 * @author  Victor Yan
 * @date    2024-10-17
 * @brief   Header file of app_io.c
 * @details
 */
#ifndef __APP_IO_H__
#define __APP_IO_H__

// set 1 when this file using in skf_gw
#define USING_AT_RPOCESS_SKF_GW (1)

#if (USING_AT_RPOCESS_SKF_GW == 1) 
#include "sys_def.h"
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <ell/queue.h>

// The max length of file path
#ifndef MAX_FPATH
#define MAX_FPATH (128U)
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */

typedef enum{
	ST_OK = 0,
	ST_ERR = 1,
	ST_BUSSY = 2,
}app_state_t;
#endif

/***************************************************/
/*****************Input Key*************************/
/***************************************************/
#ifndef FPATH_USER_KEY
#define FPATH_USER_KEY "/dev/input/by-path/platform-gpio-keys1-event"
#endif

enum key_state
{
  K_STATE_UNDEFINED = 0,
  K_STATE_PRESSED = 1,
  K_STATE_RELEASED = 2,
};

void app_io_key_test(void); // Just for test (deprecated)
enum key_state app_io_key_get_state(void);

/***************************************************/
/********************Ouput LED**********************/
/***************************************************/
struct led_controller
{
  pthread_mutex_t mtx;
  pthread_cond_t cond;
  struct l_queue *pt_event_queue;
};

enum pin_number
{
  PIN_NO_UNDEFINED = 0,
  PIN_NO_VOUT0_DATA14 = (24 + 59), // LED1 (Sensor status) Green
  PIN_NO_VOUT0_DATA15 = (24 + 60), // LED1 (Sensor status) Red
  PIN_NO_GPMC0_WAIT0 = (24 + 37),  // LED2 (Machine status) Green
  PIN_NO_GPMC0_CSN0 = (24 + 41), // LED2 (Machine status) Red
  PIN_NO_MCASP0_AFSX = (116 + 12), // LED3 (GW status) Red
};

enum led_behaviour
{
  LED_BHV_UNDEFINED = 0,
  LED_BHV_ON = 1,
  LED_BHV_BLINK = 2, // on-1S, off-4S
};

enum led_state
{
  LED_OFF = false,
  LED_ON = true
};

enum led_event
{
  LED_EV_UNDEFINED = 0,
  LED_EV_TIM_TICK = 1,
  // Red LED of LED3 (GW status) blinks when MQTT error happens
  LED_EV_MQTT_ERR = 0x10, // blink
  // Red LED of LED3 (GW status) turns off when MQTT error cancels
  LED_EV_MQTT_ERR_RL = 0x11, // RL stands for released
  // Green LED of LED1 (sensor status) blinks when the gateway is
  // advertising
  LED_EV_BLE_ADV = 0x12,  // blink
  // Green LED of LED1 (sensor status) does not blink when the gateway is
  // not advertising
  LED_EV_BLE_ADV_RL = 0x13,
  // Green LED of LED1 (sensor status) turns on when BLE connection has
  // been established
  LED_EV_BLE_CON = 0x14, // on
  // Green LED of LED1 (sensor status) truns off when the gateway is
  // not in a BLE connection
  LED_EV_BLE_CON_RL = 0x15,
  // Red LED of LED1 (sensor status) truns on when an error happens
  LED_EV_GW_ERR = 0x16, // on
  // Red LED of LED1 (sensor status) truns off when an error cancels
  LED_EV_GW_ERR_RL = 0x17,
  // Green LED of LED2 (machine status) blinks when a sensor warning
  // happens
  LED_EV_SENSOR_WARN = 0x18, // blink
  // Green LED of LED2 (machine status) turns off when no warning exists
  LED_EV_SENSOR_WARN_RL = 0x19,
  // Red LED of LED2 (machine status) blinks when a sensor warning happens
  LED_EV_SENSOR_ALARM = 0x1A, // blink
  // Red LED of LED2 (machine status) turns off when no warning exists
  LED_EV_SENSOR_ALARM_RL = 0x1B
};

struct led_state_management
{
  enum pin_number pin_no;
  enum led_event evt;
  enum led_behaviour led_bhv;
  uint32_t cnt;
};

struct led_controller *app_io_get_led_ctr(void);
app_state_t app_io_init_led_controller(struct led_controller *pt_led_ctr);
void app_io_deinit_led_controller(struct led_controller *pt_led_ctr);
app_state_t app_io_init_gpio_for_led(void);
app_state_t app_io_set_led_state(
  enum pin_number kp_pin_no,
  enum led_state new_state);
app_state_t app_io_post_led_event(const enum led_event kp_event);
void *thandler_for_led_control(void *pt_para);

#endif /* __APP_IO_H__ */
