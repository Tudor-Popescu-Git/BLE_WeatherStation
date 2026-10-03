#ifndef BME680_H
#define BME680_H

#include "stm32wbxx_hal.h"

typedef struct
{
  int32_t t100;     /* temperature, 0.01 C    */
  int32_t rh1000;   /* humidity,    0.001 %RH */
  int32_t p_pa;     /* pressure,    Pa        */
} Env_t;

HAL_StatusTypeDef BME680_Init(void);
HAL_StatusTypeDef BME680_Trigger(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef BME680_Read(Env_t *env);

#endif /* BME680_H */