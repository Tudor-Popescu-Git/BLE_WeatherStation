#include "weather_app.h"


/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    p2p_server_app.c
  * @author  MCD Application Team
  * @brief   Peer to peer Server Application
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_common.h"
#include "dbg_trace.h"
#include "ble.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdlib.h>
#include <string.h>
#include "hw_if.h"
#include <assert.h>


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* Measurement period: 10 s for bench testing, set to 600 (10 min) for real use. */
#define ENV_PERIOD_S      10u        /* 100 ms */

/* History buffer */
#define HIST_CAPACITY     2016u        /* records; 14 days at 10 min */
#define HIST_REC_SIZE     16u          /* bytes per record = one notification */
#define HIST_SEQ_MARKER   0xFFFFFFFFu  /* sequence value that marks the end of a transfer */

/* 128-bit UUID 7a1e0001-5c2b-4f6e-9b3a-1d2c3e4f5a6b, little-endian; n = 01 service, 03 control, 04 data */
#define HIST_UUID128(n)   {0x6b,0x5a,0x4f,0x3e,0x2c,0x1d,0x3a,0x9b,0x6e,0x4f,0x2b,0x5c,(n),0x00,0x1e,0x7a}

#define HF_REQUEST 0x01u
#define HF_TXPOOL  0x02u
#define HF_DISC    0x04u

typedef struct
{
  uint32_t seq;      /* sequence number */
  uint32_t up_s;     /* board uptime in seconds */
  int16_t  t100;     /* 0.01 C */
  uint16_t rh100;    /* 0.01 % */
  uint32_t p_pa;     /* Pa */
} HistRec_t;

_Static_assert(1, "HistRec_t must be 16 bytes");
_Static_assert(sizeof(HistRec_t) == HIST_REC_SIZE, "HistRec_t must be 16 bytes");

/* USER CODE END PTD */

/* Private defines ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macros -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/**
 * START of Section BLE_APP_CONTEXT
 */

static uint16_t EssSvcHdle, EssTempCharHdle, EssHumCharHdle, EssPressCharHdle;

/* History */
static HistRec_t HistBuf[HIST_CAPACITY];
static uint32_t  HistNextSeq;                     /* sequence number the next record gets */
static uint16_t  HistSvcHdle, HistCtrlCharHdle, HistDataCharHdle;
static uint8_t   HistNotifyOn;                    /* phone enabled notifications on the data characteristic */
static uint8_t   HistStreaming;                   /* a transfer is in progress */
static uint32_t  HistSendNext, HistSendEnd;       /* next sequence to send / one past the last one to send */
static volatile uint32_t HistReqFrom;

/* Uptime in seconds, built from the 1 kHz HAL tick */
static uint32_t  UpLastTick, UpRemMs, UpSeconds;
/**
 * END of Section BLE_APP_CONTEXT
 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void ESS_Init(void);
static void ENV_Period_Cb(void);
static void ENV_Conv_Cb(void);
static void SensorTask(void *arg);
static void ENV_Read_Task(void);

static void     HIST_Init(void);
static void     HIST_Reset(void);
static void     HIST_Add(int16_t t100, uint16_t rh100, uint32_t p_pa);
static void     HIST_StartRequest(uint32_t from);
static void 	HistTask(void *arg);
static void HIST_SendSome(void);
static uint32_t HIST_Oldest(void);
static uint32_t UptimeS(void);

static void SensorTask(void *arg);
static void HistTask(void *arg);
/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
/* USER CODE BEGIN FD_LOCAL_FUNCTIONS*/

void SensorTask(void *arg)
{
  uint32_t next = osKernelGetTickCount();
  for (;;) {
    /* body of SensorTask; on failure skip to the delay */
    osDelay(100);                 /* replaces the 100 ms conversion timer */
    /* body of ENV_Read_Task: read, 3x ESS_Update, HIST_Add */
    next += ENV_PERIOD_S * 1000u;
    osDelayUntil(next);           /* fixed 10 s rhythm, no drift */
  }
}

void HistTask(void *arg)
{
  for (;;) {
    uint32_t f = osThreadFlagsWait(HF_REQUEST | HF_TXPOOL | HF_DISC,
                                   osFlagsWaitAny, osWaitForever);
    if (f & HF_DISC)    { HIST_Reset(); continue; }
    if (f & HF_REQUEST) { HIST_StartRequest(HistReqFrom); }
    HIST_SendSome();      /* old HIST_Send_Task body, unchanged logic */
  }
}


#if (CFG_LPM_SUPPORTED != 0)
#warning "UptimeS() uses SysTick, which stops in Stop mode: replace it with an RTC-based count before enabling low power"
#endif

/* Seconds since boot. Call at least once every 49 days. */
static uint32_t UptimeS(void)
{
  uint32_t now = HAL_GetTick();

  UpRemMs   += (uint32_t)(now - UpLastTick);
  UpLastTick = now;
  UpSeconds += UpRemMs / 1000u;
  UpRemMs   %= 1000u;
  return UpSeconds;
}

static void ESS_Init(void)
{
  Service_UUID_t svc_uuid;
  Char_UUID_t    chr_uuid;
  tBleStatus     ret;

  svc_uuid.Service_UUID_16 = 0x181A;                     /* Environmental Sensing */
  ret = aci_gatt_add_service(UUID_TYPE_16, &svc_uuid, PRIMARY_SERVICE,
                             1 + 3 * 3, &EssSvcHdle);
  APP_DBG_MSG("ESS service: 0x%02X\r\n", ret);

  chr_uuid.Char_UUID_16 = 0x2A6E;                        /* Temperature: sint16, 0.01 C */
  ret = aci_gatt_add_char(EssSvcHdle, UUID_TYPE_16, &chr_uuid, 2,
                          CHAR_PROP_READ | CHAR_PROP_NOTIFY, ATTR_PERMISSION_NONE,
                          GATT_DONT_NOTIFY_EVENTS, 10, 0, &EssTempCharHdle);
  APP_DBG_MSG("ESS temp char: 0x%02X\r\n", ret);

  chr_uuid.Char_UUID_16 = 0x2A6F;                        /* Humidity: uint16, 0.01 % */
  ret = aci_gatt_add_char(EssSvcHdle, UUID_TYPE_16, &chr_uuid, 2,
                          CHAR_PROP_READ | CHAR_PROP_NOTIFY, ATTR_PERMISSION_NONE,
                          GATT_DONT_NOTIFY_EVENTS, 10, 0, &EssHumCharHdle);
  APP_DBG_MSG("ESS hum char: 0x%02X\r\n", ret);

  chr_uuid.Char_UUID_16 = 0x2A6D;                        /* Pressure: uint32, 0.1 Pa */
  ret = aci_gatt_add_char(EssSvcHdle, UUID_TYPE_16, &chr_uuid, 4,
                          CHAR_PROP_READ | CHAR_PROP_NOTIFY, ATTR_PERMISSION_NONE,
                          GATT_DONT_NOTIFY_EVENTS, 10, 0, &EssPressCharHdle);

  APP_DBG_MSG("ESS press char: 0x%02X\r\n", ret);
}

/* Timer callbacks run in interrupt context: only schedule a task, nothing else */

/* Writes an unsigned little-endian value of len (1..4) bytes to an ESS characteristic */
static void ESS_Update(uint16_t char_hdl, uint8_t len, uint32_t value)
{
  uint8_t    b[4];
  uint8_t    i;
  tBleStatus ret;

  for (i = 0; i < len; i++)
  {
    b[i] = (uint8_t)(value >> (8u * i));
  }
  ret = aci_gatt_update_char_value(EssSvcHdle, char_hdl, 0, len, b);
  if (ret != BLE_STATUS_SUCCESS)
  {
    APP_DBG_MSG("ENV: update of char 0x%04X failed: 0x%02X\r\n", char_hdl, ret);
  }
}

static HAL_StatusTypeDef ENV_Trigger(void)
{
	HAL_StatusTypeDef ret_val = 0;
  I2C_HandleTypeDef *hi2c = NULL;

  if (Export_hi2c(&hi2c) != HAL_OK)
  {
    APP_DBG_MSG("ENV: trigger failed, i2c inst not configured\r\n");
    return ret_val;
  }

  if (BME680_Trigger(hi2c) == HAL_OK)
  {
  }
  else
  {
    APP_DBG_MSG("ENV: trigger failed, i2c err 0x%02lX\r\n", (unsigned long)HAL_I2C_GetError(hi2c));
  }
  return ret_val;
}

static HAL_StatusTypeDef ENV_Read(void)
{
  Env_t              env;
  HAL_StatusTypeDef  st;
  I2C_HandleTypeDef *hi2c = NULL;
  int16_t            t;
  uint16_t           rh;
  uint32_t           p;
  const char        *sign;
  long               ta;
  HAL_StatusTypeDef	ret_val = 0;

  if (Export_hi2c(&hi2c) != HAL_OK)
  {
    APP_DBG_MSG("ENV: read failed, i2c inst not configured\r\n");
    return ret_val;
  }

  st = BME680_Read(&env);
  if (st == HAL_BUSY)
  {
    APP_DBG_MSG("ENV: conversion not finished\r\n");
    return ret_val;
  }
  if (st != HAL_OK)
  {
    APP_DBG_MSG("ENV: read failed, i2c err 0x%02lX\r\n", (unsigned long)HAL_I2C_GetError(hi2c));
    return ret_val;
  }

  t    = (int16_t)env.t100;                /* 0.01 C */
  rh   = (uint16_t)(env.rh1000 / 10);      /* 0.01 % */
  p    = (uint32_t)env.p_pa * 10u;         /* 0.1 Pa */
  sign = (env.t100 < 0) ? "-" : "";
  ta   = labs((long)env.t100);

  ESS_Update(EssTempCharHdle,  2, (uint16_t)t);
  ESS_Update(EssHumCharHdle,   2, rh);
  ESS_Update(EssPressCharHdle, 4, p);

  APP_DBG_MSG("ENV T=%s%ld.%02ld C  RH=%ld.%02ld %%  P=%ld Pa\r\n",
              sign, ta / 100, ta % 100,
              (long)(env.rh1000 / 1000), (long)((env.rh1000 % 1000) / 10),
              (long)env.p_pa);

  HIST_Add(t, rh, (uint32_t)env.p_pa);
  return ret_val;
}

/*************************************************************
 * History buffer and history GATT service
 *************************************************************/
static void HIST_Put16(uint8_t *b, uint16_t v)
{
  b[0] = (uint8_t)v;
  b[1] = (uint8_t)(v >> 8);
}

static void HIST_Put32(uint8_t *b, uint32_t v)
{
  b[0] = (uint8_t)v;
  b[1] = (uint8_t)(v >> 8);
  b[2] = (uint8_t)(v >> 16);
  b[3] = (uint8_t)(v >> 24);
}

/* Record layout, little-endian: seq(4) up_s(4) t100(2) rh100(2) p_pa(4) */
static void HIST_Pack(const HistRec_t *r, uint8_t *b)
{
  HIST_Put32(&b[0],  r->seq);
  HIST_Put32(&b[4],  r->up_s);
  HIST_Put16(&b[8],  (uint16_t)r->t100);
  HIST_Put16(&b[10], r->rh100);
  HIST_Put32(&b[12], r->p_pa);
}

static uint32_t HIST_Oldest(void)
{
  return (HistNextSeq > HIST_CAPACITY) ? (HistNextSeq - HIST_CAPACITY) : 0u;
}

static void HIST_Add(int16_t t100, uint16_t rh100, uint32_t p_pa)
{
  HistRec_t *r = &HistBuf[HistNextSeq % HIST_CAPACITY];

  r->seq   = HistNextSeq;
  r->up_s  = UptimeS();
  r->t100  = t100;
  r->rh100 = rh100;
  r->p_pa  = p_pa;
  HistNextSeq++;
}

static void HIST_Init(void)
{
  static const uint8_t uuid_svc[16]  = HIST_UUID128(0x01);
  static const uint8_t uuid_ctrl[16] = HIST_UUID128(0x03);
  static const uint8_t uuid_data[16] = HIST_UUID128(0x04);
  Service_UUID_t svc_uuid;
  Char_UUID_t    chr_uuid;
  tBleStatus     ret;

  memcpy(svc_uuid.Service_UUID_128, uuid_svc, 16);
  ret = aci_gatt_add_service(UUID_TYPE_128, &svc_uuid, PRIMARY_SERVICE,
                             1 + 2 + 3, &HistSvcHdle);
  APP_DBG_MSG("HIST service: 0x%02X\r\n", ret);

  memcpy(chr_uuid.Char_UUID_128, uuid_ctrl, 16);          /* control: uint32 LE, first sequence wanted */
  ret = aci_gatt_add_char(HistSvcHdle, UUID_TYPE_128, &chr_uuid, 4,
                          CHAR_PROP_WRITE, ATTR_PERMISSION_NONE,
                          GATT_NOTIFY_ATTRIBUTE_WRITE, 10, 0, &HistCtrlCharHdle);
  APP_DBG_MSG("HIST control char: 0x%02X\r\n", ret);

  memcpy(chr_uuid.Char_UUID_128, uuid_data, 16);          /* data: one record per notification */
  ret = aci_gatt_add_char(HistSvcHdle, UUID_TYPE_128, &chr_uuid, HIST_REC_SIZE,
                          CHAR_PROP_NOTIFY, ATTR_PERMISSION_NONE,
                          GATT_NOTIFY_ATTRIBUTE_WRITE, 10, 0, &HistDataCharHdle);
  APP_DBG_MSG("HIST data char: 0x%02X\r\n", ret);

  UpLastTick = HAL_GetTick();
}

static void HIST_Reset(void)
{
  HistStreaming = 0u;
  HistNotifyOn  = 0u;
}

static void HIST_StartRequest(uint32_t from)
{
  uint32_t oldest = HIST_Oldest();

  if (!HistNotifyOn)
  {
    APP_DBG_MSG("HIST: request ignored, notifications not enabled\r\n");
    return;
  }

  if (from < oldest)       { from = oldest; }
  if (from > HistNextSeq)  { from = HistNextSeq; }

  HistSendNext  = from;
  HistSendEnd   = HistNextSeq;
  HistStreaming = 1u;

  APP_DBG_MSG("HIST: request, sending seq %lu to %lu (%lu records)\r\n",
              (unsigned long)HistSendNext, (unsigned long)HistSendEnd,
              (unsigned long)(HistSendEnd - HistSendNext));

}

/* Sends records until the BLE TX pool is full, then waits for ACI_GATT_TX_POOL_AVAILABLE */
static void HIST_SendSome(void)
{
  uint8_t    b[HIST_REC_SIZE];
  tBleStatus ret;

  if (!HistStreaming || !HistNotifyOn)
  {
    HistStreaming = 0u;
    return;
  }

  while (HistSendNext < HistSendEnd)
  {
    if (HistSendNext < HIST_Oldest())          /* overwritten while sending */
    {
      HistSendNext = HIST_Oldest();
      continue;
    }

    HIST_Pack(&HistBuf[HistSendNext % HIST_CAPACITY], b);
    ret = aci_gatt_update_char_value(HistSvcHdle, HistDataCharHdle, 0, HIST_REC_SIZE, b);
    if (ret == BLE_STATUS_INSUFFICIENT_RESOURCES)
    {
      return;                                  /* resumes from HIST_OnGattEvent */
    }
    if (ret != BLE_STATUS_SUCCESS)
    {
      APP_DBG_MSG("HIST: send failed at seq %lu: 0x%02X\r\n", (unsigned long)HistSendNext, ret);
      HistStreaming = 0u;
      return;
    }
    HistSendNext++;
  }

  /* End marker: seq = 0xFFFFFFFF, uptime now, pressure field = next sequence number */
  memset(b, 0, sizeof(b));
  HIST_Put32(&b[0],  HIST_SEQ_MARKER);
  HIST_Put32(&b[4],  UptimeS());
  HIST_Put32(&b[12], HistNextSeq);
  ret = aci_gatt_update_char_value(HistSvcHdle, HistDataCharHdle, 0, HIST_REC_SIZE, b);
  if (ret == BLE_STATUS_INSUFFICIENT_RESOURCES)
  {
    return;
  }

  HistStreaming = 0u;
  if (ret == BLE_STATUS_SUCCESS)
  {
    APP_DBG_MSG("HIST: done\r\n");
  }
  else
  {
    APP_DBG_MSG("HIST: end marker failed: 0x%02X\r\n", ret);
  }
}

/* Called from SVCCTL_App_Notification() (app_ble.c). Event context: only set state and schedule tasks. */
void WEATHER_OnGattEvent(uint16_t ecode, void *p_data)
{
  switch (ecode)
  {
    case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
    {
      aci_gatt_attribute_modified_event_rp0 *m = (aci_gatt_attribute_modified_event_rp0 *)p_data;

      if (m->Attr_Handle == (uint16_t)(HistDataCharHdle + 2u))          /* data char CCCD */
      {
        HistNotifyOn = (m->Attr_Data_Length >= 1u) && ((m->Attr_Data[0] & 0x01u) != 0u);
        if (!HistNotifyOn)
        {
          HistStreaming = 0u;
        }
        APP_DBG_MSG("HIST: notifications %s\r\n", HistNotifyOn ? "enabled" : "disabled");
      }
      else if (m->Attr_Handle == (uint16_t)(HistCtrlCharHdle + 1u))     /* control char value */
      {
        if (m->Attr_Data_Length == 4u)
        {
          uint32_t from = (uint32_t)m->Attr_Data[0]
                        | ((uint32_t)m->Attr_Data[1] << 8)
                        | ((uint32_t)m->Attr_Data[2] << 16)
                        | ((uint32_t)m->Attr_Data[3] << 24);
          HIST_StartRequest(from);
        }
        else
        {
          APP_DBG_MSG("HIST: control write ignored, length %u\r\n", (unsigned)m->Attr_Data_Length);
        }
      }
      break;
    }

    case ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE:
      if (HistStreaming)
      {
    	  osThreadFlagsSet(HistTaskId, HF_TXPOOL);
      }
      break;

    default:
      break;
  }
}

/* USER CODE END FD_LOCAL_FUNCTIONS*/
