/**
 * @file    app_io.c
 * @author  Victor Yan
 * @date    2024-10-17
 * @brief   Implementation of general IO (e.g., the state LED). This file
 *          is reused in mqtt_op.
 * @details
 */
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "app_io.h"
#if (USING_AT_RPOCESS_SKF_GW == 1)
#include "util_dbg.h"
#include "util_froto.h"
#undef INFO_OPEN
#define INFO_OPEN (0)
#undef DEBUG_OPEN
#define DEBUG_OPEN (0)
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
#include "global.h"
#include "string.h"
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */

DBG_LOCAL_LOG_DEBUG

/***************************************************/
/*****************Input Key*************************/
/***************************************************/
/**
 * @brief Just for test (deprecated)
 */
void
app_io_key_test(void)
{
  int kp_fd = 0;
  struct input_event kp_evinfo = { 0 };

  kp_fd = open(FPATH_USER_KEY, O_RDONLY);
  if(kp_fd < 0)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to open file %s, for %s",
                FPATH_USER_KEY,
                strerror(errno));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT,
            "Failed to open file %s, for %s",
            FPATH_USER_KEY,
            strerror(errno));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    return;
  }

  while(1)
  {
    if(read(kp_fd, &kp_evinfo,
            sizeof(struct input_event)) < sizeof(struct input_event))
    {
#if (USING_AT_RPOCESS_SKF_GW == 1)
      DBG_LOG_ERR("Failed to read, for %s", strerror(errno));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
      LOG_ERR(OUTPOINT, "Failed to read, for %s", strerror(errno));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
      continue;
    }
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_INFO(
      "K_EVENT info time= %ld.%ld, type=0x%X, code=0x%X, value=0x%X",
      kp_evinfo.time.tv_sec,
      kp_evinfo.time.tv_usec,
      kp_evinfo.type,
      kp_evinfo.code,
      kp_evinfo.value);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_INFO(OUTPOINT,
             "K_EVENT info time= %ld.%ld, type=0x%X, code=0x%X, value=0x%X",
             kp_evinfo.time.tv_sec,
             kp_evinfo.time.tv_usec,
             kp_evinfo.type,
             kp_evinfo.code,
             kp_evinfo.value);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    // sleep(1);
  }

  close(kp_fd);
}
/**
 * @brief To detect the input botton in a block manner. The function would
 *        be returned only when the key release has been detected.
 *        Some references:
 * https://www.kernel.org/doc/html/v5.0/input/event-codes.html
 * https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/input-event-codes.h
 */
enum key_state
app_io_key_get_state(void)
{
  enum key_state tpret = K_STATE_UNDEFINED;

  int kp_fd = 0;
  struct input_event kp_evinfo = { 0 };

  kp_fd = open(FPATH_USER_KEY, O_RDONLY);
  if(kp_fd < 0)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to open file %s, for %s",
                FPATH_USER_KEY,
                strerror(errno));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to open file %s, for %s",
            FPATH_USER_KEY,
            strerror(errno));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = K_STATE_UNDEFINED;
    return tpret;
  }

  while(1)
  {
    if(read(kp_fd, &kp_evinfo,
            sizeof(struct input_event)) < sizeof(struct input_event))
    {
#if (USING_AT_RPOCESS_SKF_GW == 1)
      DBG_LOG_ERR("Failed to read, for %s", strerror(errno));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
      LOG_ERR(OUTPOINT, "Failed to read, for %s", strerror(errno));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
      continue;
    }
    if((EV_KEY == kp_evinfo.type) && (0 == kp_evinfo.value))
    {
#if (USING_AT_RPOCESS_SKF_GW == 1)
      DBG_LOG_INFO("The key has been released at %d", time(NULL));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
      LOG_INFO(OUTPOINT, "The key has been released at %d", time(NULL));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
      tpret = K_STATE_RELEASED;
      break;
    }
  }

  close(kp_fd);

  return tpret;
}
/***************************************************/
/********************Ouput LED**********************/
/***************************************************/
#ifndef STR_GPIO_EXPORT_PATTERN
#define STR_GPIO_EXPORT_PATTERN " echo %d > /sys/class/gpio/export"
#endif
#ifndef STR_PATTERN_PIN_PATH
#define STR_PATTERN_PIN_PATH "/sys/class/gpio/gpio%d/direction"
#endif
#ifndef STR_GPIO_DIR_PATTERN
#define STR_GPIO_DIR_PATTERN "echo out > /sys/class/gpio/gpio%d/direction"
#endif
#ifndef STR_GPIO_VALUE_PATTERN
#define STR_GPIO_VALUE_PATTERN "echo %d > /sys/class/gpio/gpio%d/value"
#endif
#ifndef STR_GPIO_VALUE_FPATH
#define STR_GPIO_VALUE_FPATH "/sys/class/gpio/gpio%d/value"
#endif
#ifndef STR_GPIO_EXPORT_FPATH
#define STR_GPIO_EXPORT_FPATH "/sys/class/gpio/export"
#endif
#ifndef LED_PERIOD_S
#define LED_PERIOD_S (5U)
#endif

struct gpio_led_handle
{
  enum pin_number pin_no;
  int value_fd;
  enum led_state last_state;
  bool has_last_state;
};

struct led_controller g_led_ctr = { 0 };
static app_state_t is_gpio_exported(const enum pin_number kp_pin_no);
static app_state_t write_gpio_text_file(const char *pt_fpath,
                                        const char *pt_value);
static app_state_t init_gpio_output(const enum pin_number kp_pin_no);
static struct gpio_led_handle *find_gpio_led_handle(
  const enum pin_number kp_pin_no);
static app_state_t ensure_gpio_led_fd(struct gpio_led_handle *pt_handle);
static void close_gpio_led_fds(void);

#if (USING_AT_RPOCESS_SKF_GW == 1)
static struct gpio_led_handle g_gpio_led_handles[] = {
  { PIN_NO_VOUT0_DATA14, -1, LED_OFF, false },
  { PIN_NO_VOUT0_DATA15, -1, LED_OFF, false },
  { PIN_NO_GPMC0_WAIT0, -1, LED_OFF, false },
  { PIN_NO_GPMC0_CSN0, -1, LED_OFF, false },
};
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
static struct gpio_led_handle g_gpio_led_handles[] = {
  { PIN_NO_MCASP0_AFSX, -1, LED_OFF, false },
};
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */

static struct gpio_led_handle *
find_gpio_led_handle(const enum pin_number kp_pin_no)
{
  uint32_t idx = 0;

  for(idx = 0;
      idx < (sizeof(g_gpio_led_handles) / sizeof(g_gpio_led_handles[0]));
      idx++)
  {
    if(g_gpio_led_handles[idx].pin_no == kp_pin_no)
    {
      return &g_gpio_led_handles[idx];
    }
  }

  return NULL;
}

static app_state_t
write_gpio_text_file(const char *pt_fpath, const char *pt_value)
{
  app_state_t tpret = ST_OK;
  int kp_fd = -1;
  size_t value_len = 0;

  if((NULL == pt_fpath) || (NULL == pt_value))
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Unexpected NULL");
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Unexpected NULL");
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    return ST_ERR;
  }

  kp_fd = open(pt_fpath, O_WRONLY);
  if(kp_fd < 0)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to open %s, for %s", pt_fpath, strerror(errno));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to open %s, for %s", pt_fpath,
            strerror(errno));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    return ST_ERR;
  }

  value_len = strlen(pt_value);
  if(write(kp_fd, pt_value, value_len) != (ssize_t)value_len)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to write %s into %s, for %s", pt_value, pt_fpath,
                strerror(errno));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to write %s into %s, for %s", pt_value,
            pt_fpath, strerror(errno));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
  }

  close(kp_fd);
  return tpret;
}

static app_state_t
init_gpio_output(const enum pin_number kp_pin_no)
{
    uint8_t kp_dir_fpath[MAX_FPATH] = {0};
 
    snprintf((char *)kp_dir_fpath,
             MAX_FPATH,
             STR_PATTERN_PIN_PATH,
             kp_pin_no);
 
    /*
     * GPIO initialization is handled by
     * skf-gw-gpio-init.service(root)
     */
    if(access((char *)kp_dir_fpath, F_OK) != 0)
    {
        DBG_LOG_ERR(
            "GPIO%d is not initialized by root service",
            kp_pin_no);
 
        return ST_ERR;
    }
 
    return ST_OK;
}

static app_state_t
ensure_gpio_led_fd(struct gpio_led_handle *pt_handle)
{
  app_state_t tpret = ST_OK;
  uint8_t kp_fpath[MAX_FPATH] = { 0 };

  if(NULL == pt_handle)
  {
    return ST_ERR;
  }

  if(pt_handle->value_fd >= 0)
  {
    return tpret;
  }

  snprintf(kp_fpath, MAX_FPATH, STR_GPIO_VALUE_FPATH, pt_handle->pin_no);
  pt_handle->value_fd = open((const char *)kp_fpath, O_WRONLY);
  if(pt_handle->value_fd < 0)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to open %s, for %s", kp_fpath, strerror(errno));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to open %s, for %s", kp_fpath,
            strerror(errno));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
  }

  return tpret;
}

static void
close_gpio_led_fds(void)
{
  uint32_t idx = 0;

  for(idx = 0;
      idx < (sizeof(g_gpio_led_handles) / sizeof(g_gpio_led_handles[0]));
      idx++)
  {
    if(g_gpio_led_handles[idx].value_fd >= 0)
    {
      close(g_gpio_led_handles[idx].value_fd);
      g_gpio_led_handles[idx].value_fd = -1;
    }
    g_gpio_led_handles[idx].has_last_state = false;
  }
}

struct led_controller *
app_io_get_led_ctr(void)
{
  return &g_led_ctr;
}
/**
 * @brief To initialize the led controller @p pt_led_ctr (i.e., initialize
 *        the mutex, condition signal, and the queue)
 */
app_state_t
app_io_init_led_controller(struct led_controller *pt_led_ctr)
{
  app_state_t tpret = ST_OK;

  if(NULL == pt_led_ctr)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Unexpected NULL");
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Unexpected NULL");
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    return tpret;
  }

  if(0 != pthread_mutex_init(&pt_led_ctr->mtx, NULL))
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to initialize the mutex");
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to initialize the mutex");
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    return tpret;
  }

  if(0 != pthread_cond_init(&pt_led_ctr->cond, NULL))
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to initialize the condition signal");
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to initialize the condition signal");
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    return tpret;
  }

  pt_led_ctr->pt_event_queue = l_queue_new();
  if(NULL == pt_led_ctr->pt_event_queue)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to create the queue");
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to create the queue");
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    return tpret;
  }

  return tpret;
}
/**
 * @brief To de-initialize the led controller @p pt_led_ctr (i.e., destroy
 *        the mutex, condition signal, and the queue)
 */
void
app_io_deinit_led_controller(struct led_controller *pt_led_ctr)
{
  if(NULL == pt_led_ctr)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Unexpected NULL");
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Unexpected NULL");
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    return;
  }

  pthread_mutex_destroy(&pt_led_ctr->mtx);
  pthread_cond_destroy(&pt_led_ctr->cond);
  if(pt_led_ctr->pt_event_queue)
  {
    l_queue_destroy(pt_led_ctr->pt_event_queue, NULL);
  }

  close_gpio_led_fds();
}
/**
 * To initialize GPIOs for LED control
 */
app_state_t
app_io_init_gpio_for_led(void)
{
  app_state_t tpret = ST_OK;
  enum pin_number kp_pin_no = PIN_NO_UNDEFINED;

#if (USING_AT_RPOCESS_SKF_GW == 1)
  // PIN_NO_VOUT0_DATA14 = (24 + 59), // LED1 (Sensor status) Green
  kp_pin_no = PIN_NO_VOUT0_DATA14;
  if(ST_OK != init_gpio_output(kp_pin_no))
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to initialize gpio%d", kp_pin_no);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to initialize gpio%d", kp_pin_no);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    goto EXIT;
  }
  // And turn on the LED
  app_io_set_led_state(PIN_NO_VOUT0_DATA14, true);

  // PIN_NO_VOUT0_DATA15 = (24 + 60), // LED1 (Sensor status) Red
  kp_pin_no = PIN_NO_VOUT0_DATA15;
  if(ST_OK != init_gpio_output(kp_pin_no))
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to initialize gpio%d", kp_pin_no);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to initialize gpio%d", kp_pin_no);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    goto EXIT;
  }
  // And turn on the LED
  app_io_set_led_state(PIN_NO_VOUT0_DATA15, true);

  // PIN_NO_GPMC0_CSN0 = (24 + 41), // LED2 (Machine status) Red
  kp_pin_no = PIN_NO_GPMC0_CSN0;
  if(ST_OK != init_gpio_output(kp_pin_no))
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to initialize gpio%d", kp_pin_no);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to initialize gpio%d", kp_pin_no);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    goto EXIT;
  }
  // And turn on the LED
  app_io_set_led_state(PIN_NO_GPMC0_CSN0, true);

  // PIN_NO_GPMC0_WAIT0 = (24 + 37),  // LED2 (Machine status) Green
  kp_pin_no = PIN_NO_GPMC0_WAIT0;
  if(ST_OK != init_gpio_output(kp_pin_no))
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to initialize gpio%d", kp_pin_no);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to initialize gpio%d", kp_pin_no);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    goto EXIT;
  }
  // And turn on the LED
  app_io_set_led_state(PIN_NO_GPMC0_WAIT0, true);

#else /* USING_AT_RPOCESS_SKF_GW == 1 */

  // PIN_NO_MCASP0_AFSX = (116 + 12), // LED3 (GW status) Red
  kp_pin_no = PIN_NO_MCASP0_AFSX;
  if(ST_OK != init_gpio_output(kp_pin_no))
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to initialize gpio%d", kp_pin_no);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to initialize gpio%d", kp_pin_no);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    goto EXIT;
  }
  // And turn on the LED
  app_io_set_led_state(PIN_NO_MCASP0_AFSX, true);

#endif /* USING_AT_RPOCESS_SKF_GW == 1 */

EXIT:
  return tpret;
}
/**
 * @brief To post LED event (i.e., the state)
 */
app_state_t
app_io_post_led_event(const enum led_event kp_event)
{
  app_state_t tpret = ST_OK;
  struct led_controller *pt_led_ctr = app_io_get_led_ctr();

  if(NULL == pt_led_ctr)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Unexpected NULL");
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Unexpected NULL");
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    return tpret;
  }

  pthread_mutex_lock(&pt_led_ctr->mtx);
  l_queue_push_tail(pt_led_ctr->pt_event_queue, (void *)kp_event);
  pthread_cond_signal(&pt_led_ctr->cond);
  pthread_mutex_unlock(&pt_led_ctr->mtx);

  return tpret;
}
/**
 * @brief Set the GPIO
 */
app_state_t
app_io_set_led_state(
  enum pin_number kp_pin_no,
  enum led_state new_state)
{
  app_state_t tpret = ST_OK;
  uint8_t kp_stval = 0;
  char kp_chval = '0';
  struct gpio_led_handle *pt_handle = NULL;

  if(new_state)
  {
    kp_stval = 0;
    kp_chval = '0';
  }
  else
  {
    kp_stval = 1;
    kp_chval = '1';
  }

  pt_handle = find_gpio_led_handle(kp_pin_no);
  if(NULL == pt_handle)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Unexpected pin number %d", kp_pin_no);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Unexpected pin number %d", kp_pin_no);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    return ST_ERR;
  }

  if(pt_handle->has_last_state && (pt_handle->last_state == new_state))
  {
    return tpret;
  }

#if (USING_AT_RPOCESS_SKF_GW == 1)
  DBG_LOG_INFO("To write %d into gpio%d", kp_stval, kp_pin_no);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
  LOG_INFO(OUTPOINT, "To write %d into gpio%d", kp_stval, kp_pin_no);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */

  if(ST_OK != ensure_gpio_led_fd(pt_handle))
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to prepare gpio%d", kp_pin_no);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to prepare gpio%d", kp_pin_no);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    return ST_ERR;
  }

  if(lseek(pt_handle->value_fd, 0, SEEK_SET) < 0)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to seek gpio%d, for %s", kp_pin_no,
                strerror(errno));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to seek gpio%d, for %s", kp_pin_no,
            strerror(errno));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    return ST_ERR;
  }

  if(write(pt_handle->value_fd, &kp_chval, 1) != 1)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to write gpio%d, for %s", kp_pin_no,
                strerror(errno));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to write gpio%d, for %s", kp_pin_no,
            strerror(errno));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
    return tpret;
  }

  pt_handle->last_state = new_state;
  pt_handle->has_last_state = true;

  return tpret;
}
/**
 * @brief A thread to deal with the state (i.e., to turn on, turn off, or
 * blink LED according to the state)
 */
void *
thandler_for_led_control(void *pt_para)
{
  struct led_controller *pt_led_ctr = app_io_get_led_ctr();
  enum led_event kp_event = LED_EV_UNDEFINED;
  uint32_t kp_cnt = 0;

#if (USING_AT_RPOCESS_SKF_GW == 1)
  struct led_state_management kp_led_mgt_ble_st =
  { .pin_no = PIN_NO_VOUT0_DATA14,
    .evt = LED_EV_UNDEFINED,
    .led_bhv = LED_BHV_UNDEFINED,
    .cnt = 0 };
  struct led_state_management kp_led_mgt_gw_err =
  { .pin_no = PIN_NO_VOUT0_DATA15,
    .evt = LED_EV_UNDEFINED,
    .led_bhv = LED_BHV_ON,
    .cnt = 0 };
  struct led_state_management kp_led_mgt_sensor_warn =
  { .pin_no = PIN_NO_GPMC0_WAIT0,
    .evt = LED_EV_UNDEFINED,
    .led_bhv = LED_BHV_BLINK,
    .cnt = 0 };
  struct led_state_management kp_led_mgt_sensor_alarm =
  { .pin_no = PIN_NO_GPMC0_CSN0,
    .evt = LED_EV_UNDEFINED,
    .led_bhv = LED_BHV_BLINK,
    .cnt = 0 };
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
  struct led_state_management kp_led_mgt_mqtt_error =
  { .pin_no = PIN_NO_MCASP0_AFSX,
    .evt = LED_EV_UNDEFINED,
    .led_bhv = LED_BHV_BLINK,
    .cnt = 0 };
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
  sleep(1);

  // To set all LED off at the beginning
#if (USING_AT_RPOCESS_SKF_GW == 1)
  app_io_set_led_state(PIN_NO_GPMC0_CSN0, LED_OFF);
  app_io_set_led_state(PIN_NO_GPMC0_WAIT0, LED_OFF);
  app_io_set_led_state(PIN_NO_VOUT0_DATA14, LED_OFF);
  app_io_set_led_state(PIN_NO_VOUT0_DATA15, LED_OFF);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
  app_io_set_led_state(PIN_NO_MCASP0_AFSX, LED_OFF);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */

  while(1)
  {
    pthread_mutex_lock(&pt_led_ctr->mtx);
    while(1)
    {
      if(0 == l_queue_length(pt_led_ctr->pt_event_queue))
      {
        pthread_cond_wait(&pt_led_ctr->cond, &pt_led_ctr->mtx);
      }
      else
      {
        kp_event = (enum led_event)l_queue_pop_head(
          pt_led_ctr->pt_event_queue);
        break;
      }
    }
    pthread_mutex_unlock(&pt_led_ctr->mtx);

#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_INFO("LED event received is %d", kp_event);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_INFO(OUTPOINT, "LED event received is %d", kp_event);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */

    switch(kp_event)
    {
      case LED_EV_TIM_TICK:
      {
#if (USING_AT_RPOCESS_SKF_GW == 1)
        // BLE Advertising and connection
        if((LED_EV_BLE_ADV == kp_led_mgt_ble_st.evt) ||
           (LED_EV_BLE_CON == kp_led_mgt_ble_st.evt))
        {
          if(kp_led_mgt_ble_st.led_bhv == LED_BHV_BLINK)
          {
            if(0 == kp_led_mgt_ble_st.cnt % LED_PERIOD_S)
            {
              app_io_set_led_state(kp_led_mgt_ble_st.pin_no, LED_ON);
            }
            else
            {
              app_io_set_led_state(kp_led_mgt_ble_st.pin_no, LED_OFF);
            }
          }
          else
          {
            app_io_set_led_state(kp_led_mgt_ble_st.pin_no, LED_ON);
          }
          kp_led_mgt_ble_st.cnt++;
        }
        else
        {
          app_io_set_led_state(kp_led_mgt_ble_st.pin_no, LED_OFF);
        }

        // Gateway error
        if(LED_EV_GW_ERR == kp_led_mgt_gw_err.evt)
        {
          app_io_set_led_state(kp_led_mgt_gw_err.pin_no, LED_ON);
          kp_led_mgt_gw_err.cnt++;
        }
        else
        {
          app_io_set_led_state(kp_led_mgt_gw_err.pin_no, LED_OFF);
        }

        // Sensor alarm
        if(LED_EV_SENSOR_ALARM == kp_led_mgt_sensor_alarm.evt)
        {
          if(0 == kp_led_mgt_sensor_alarm.cnt % LED_PERIOD_S)
          {
            app_io_set_led_state(kp_led_mgt_sensor_alarm.pin_no, LED_ON);
          }
          else
          {
            app_io_set_led_state(kp_led_mgt_sensor_alarm.pin_no, LED_OFF);
          }
          kp_led_mgt_sensor_alarm.cnt++;
        }
        else
        {
          app_io_set_led_state(kp_led_mgt_sensor_alarm.pin_no, LED_OFF);
        }

        // Sensor warning
        if(LED_EV_SENSOR_WARN == kp_led_mgt_sensor_warn.evt)
        {
		  if(0 == kp_led_mgt_sensor_warn.cnt % LED_PERIOD_S)
          {
            app_io_set_led_state(kp_led_mgt_sensor_warn.pin_no, LED_ON);
          }
          else
          {
            app_io_set_led_state(kp_led_mgt_sensor_warn.pin_no, LED_OFF);
          }
          kp_led_mgt_sensor_warn.cnt++;
        }
        else
        {
          app_io_set_led_state(kp_led_mgt_sensor_warn.pin_no, LED_OFF);
        }
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
        // MQTT Error
        if(LED_EV_MQTT_ERR == kp_led_mgt_mqtt_error.evt)
        {
          if(0 == kp_led_mgt_mqtt_error.cnt % LED_PERIOD_S)
          {
            // on
			app_io_set_led_state( kp_led_mgt_mqtt_error.pin_no, LED_ON);
          }
          else
          {
            // off
            app_io_set_led_state( kp_led_mgt_mqtt_error.pin_no, LED_OFF);
          }
          kp_led_mgt_mqtt_error.cnt++;
        }
        else
        {
            app_io_set_led_state( kp_led_mgt_mqtt_error.pin_no, LED_OFF);
        }
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
        break;
      }
#if (USING_AT_RPOCESS_SKF_GW == 1)
      case LED_EV_BLE_ADV:
      {
        kp_led_mgt_ble_st.evt = LED_EV_BLE_ADV;
        kp_led_mgt_ble_st.led_bhv = LED_BHV_BLINK;
        kp_led_mgt_ble_st.cnt = 0;
        break;
      }
      case LED_EV_BLE_ADV_RL:
      {
        kp_led_mgt_ble_st.evt = LED_EV_UNDEFINED;
        kp_led_mgt_ble_st.led_bhv = LED_BHV_UNDEFINED;
        kp_led_mgt_ble_st.cnt = 0;
        break;
      }
      case LED_EV_BLE_CON:
      {
        kp_led_mgt_ble_st.evt = LED_EV_BLE_CON;
        kp_led_mgt_ble_st.led_bhv = LED_BHV_ON;
        kp_led_mgt_ble_st.cnt = 0;
        break;
      }
      case LED_EV_BLE_CON_RL:
      {
        kp_led_mgt_ble_st.evt = LED_EV_UNDEFINED;
        kp_led_mgt_ble_st.led_bhv = LED_BHV_UNDEFINED;
        kp_led_mgt_ble_st.cnt = 0;
        break;
      }
      case LED_EV_GW_ERR:
      {
        kp_led_mgt_gw_err.evt = LED_EV_GW_ERR;
        kp_led_mgt_gw_err.led_bhv = LED_BHV_ON;
        kp_led_mgt_gw_err.cnt = 0;
        break;
      }
      case LED_EV_GW_ERR_RL:
      {
        kp_led_mgt_gw_err.evt = LED_EV_UNDEFINED;
        kp_led_mgt_gw_err.led_bhv = LED_BHV_UNDEFINED;
        kp_led_mgt_gw_err.cnt = 0;
        break;
      }
      case LED_EV_SENSOR_ALARM:
      {
        kp_led_mgt_sensor_alarm.evt = LED_EV_SENSOR_ALARM;
        kp_led_mgt_sensor_alarm.led_bhv = LED_BHV_BLINK;
        kp_led_mgt_sensor_alarm.cnt = 0;
        break;
      }
      case LED_EV_SENSOR_ALARM_RL:
      {
        kp_led_mgt_sensor_alarm.evt = LED_EV_UNDEFINED;
        kp_led_mgt_sensor_alarm.led_bhv = LED_BHV_UNDEFINED;
        kp_led_mgt_sensor_alarm.cnt = 0;
        break;
      }
      case LED_EV_SENSOR_WARN:
      {
        kp_led_mgt_sensor_warn.evt = LED_EV_SENSOR_WARN;
        kp_led_mgt_sensor_warn.led_bhv = LED_BHV_BLINK;
        kp_led_mgt_sensor_warn.cnt = 0;
        break;
      }
      case LED_EV_SENSOR_WARN_RL:
      {
        kp_led_mgt_sensor_warn.evt = LED_EV_UNDEFINED;
        kp_led_mgt_sensor_warn.led_bhv = LED_BHV_UNDEFINED;
        kp_led_mgt_sensor_warn.cnt = 0;
        break;
      }
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
      case LED_EV_MQTT_ERR:
      {
        kp_led_mgt_mqtt_error.evt = LED_EV_MQTT_ERR;
        kp_led_mgt_mqtt_error.led_bhv = LED_BHV_BLINK;
        kp_led_mgt_mqtt_error.cnt = 0;
        break;
      }
      case LED_EV_MQTT_ERR_RL:
      {
        kp_led_mgt_mqtt_error.evt = LED_EV_UNDEFINED;
        kp_led_mgt_mqtt_error.led_bhv = LED_BHV_UNDEFINED;
        kp_led_mgt_mqtt_error.cnt = 0;
        break;
      }
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
      default:
      {
#if (USING_AT_RPOCESS_SKF_GW == 1)
        DBG_LOG_ERR("Unknown LED event");
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
        LOG_ERR(OUTPOINT, "Unknown LED event");
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
        break;
      }
    }
  }
}
static app_state_t
is_gpio_exported(const enum pin_number kp_pin_no)
{
  app_state_t tpret = ST_OK;
  uint8_t command[MAX_FPATH] = { 0 };

  snprintf(command, MAX_FPATH, STR_PATTERN_PIN_PATH, kp_pin_no);
#if (USING_AT_RPOCESS_SKF_GW == 1)
  DBG_LOG_INFO("To check the existence of %s", command);
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
  LOG_INFO(OUTPOINT, "To check the existence of %s", command);
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
  if(access(command, F_OK) != 0)
  {
#if (USING_AT_RPOCESS_SKF_GW == 1)
    DBG_LOG_ERR("Failed to access, for %s", strerror(errno));
#else /* USING_AT_RPOCESS_SKF_GW == 1 */
    LOG_ERR(OUTPOINT, "Failed to access, for %s", strerror(errno));
#endif /* USING_AT_RPOCESS_SKF_GW == 1 */
    tpret = ST_ERR;
  }
  return tpret;
}
