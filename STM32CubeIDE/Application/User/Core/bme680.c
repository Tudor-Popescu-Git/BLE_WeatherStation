#include "bme680.h"

static uint16_t par_t1, par_p1, par_h1, par_h2;
static int16_t  par_t2, par_p2, par_p4, par_p5, par_p8, par_p9;
static int8_t   par_t3, par_p3, par_p6, par_p7, par_h3, par_h4, par_h5, par_h7;
static uint8_t  par_p10, par_h6;
static uint8_t  calib_ok = 0;
static I2C_HandleTypeDef *hi2c_inst = NULL;

static HAL_StatusTypeDef bme_rd(uint8_t reg, uint8_t *buf, uint16_t len)
{
  return HAL_I2C_Mem_Read(hi2c_inst, BME_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

static HAL_StatusTypeDef bme_wr(uint8_t reg, uint8_t val)
{
  return HAL_I2C_Mem_Write(hi2c_inst, BME_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

HAL_StatusTypeDef BME680_Init(I2C_HandleTypeDef *hi2c)
{
  uint8_t e[10], k[24];
  if(hi2c == NULL) {return HAL_ERROR;}
  hi2c_inst = hi2c;
  if (calib_ok) return HAL_OK;
  if (bme_rd(0xE1, e, 10) != HAL_OK) return HAL_ERROR;   /* 0xE1..0xEA */
  if (bme_rd(0x89, k, 24) != HAL_OK) return HAL_ERROR;   /* 0x89..0xA0 */

  par_t1  = (uint16_t)((e[9] << 8) | e[8]);
  par_t2  = (int16_t)((k[2] << 8) | k[1]);
  par_t3  = (int8_t)k[3];

  par_p1  = (uint16_t)((k[6] << 8) | k[5]);
  par_p2  = (int16_t)((k[8] << 8) | k[7]);
  par_p3  = (int8_t)k[9];
  par_p4  = (int16_t)((k[12] << 8) | k[11]);
  par_p5  = (int16_t)((k[14] << 8) | k[13]);
  par_p7  = (int8_t)k[15];                    /* 0x98 - P7 before P6 */
  par_p6  = (int8_t)k[16];                    /* 0x99 */
  par_p8  = (int16_t)((k[20] << 8) | k[19]);
  par_p9  = (int16_t)((k[22] << 8) | k[21]);
  par_p10 = k[23];

  par_h1  = (uint16_t)((e[2] << 4) | (e[1] & 0x0F));
  par_h2  = (uint16_t)((e[0] << 4) | (e[1] >> 4));
  par_h3  = (int8_t)e[3];
  par_h4  = (int8_t)e[4];
  par_h5  = (int8_t)e[5];
  par_h6  = e[6];
  par_h7  = (int8_t)e[7];

  calib_ok = 1;
  return HAL_OK;
}

HAL_StatusTypeDef BME680_Trigger(I2C_HandleTypeDef *hi2c)
{
  if (BME680_Init(hi2c)    != HAL_OK) return HAL_ERROR;
  if (bme_wr(0x70, 0x08)   		!= HAL_OK) return HAL_ERROR;   /* heater off        */
  if (bme_wr(0x71, 0x00)		!= HAL_OK) return HAL_ERROR;   /* no gas            */
  if (bme_wr(0x72, 0x01)		!= HAL_OK) return HAL_ERROR;   /* hum 1x, before 0x74 */
  return bme_wr(0x74, 0x55);                              /* T 2x, P 16x, forced */
}

HAL_StatusTypeDef BME680_Read(Env_t *env)
{
  uint8_t c[10];                                          /* 0x1D..0x26 */

  if (!calib_ok || bme_rd(0x1D, c, 10) != HAL_OK) return HAL_ERROR;
  if (!(c[0] & 0x80)) return HAL_BUSY;                    /* no new data yet */

  int32_t p_adc = ((int32_t)c[2] << 12) | ((int32_t)c[3] << 4) | (c[4] >> 4);
  int32_t t_adc = ((int32_t)c[5] << 12) | ((int32_t)c[6] << 4) | (c[7] >> 4);
  int32_t h_adc = ((int32_t)c[8] << 8)  | c[9];

  /* temperature */
  int32_t v1 = (t_adc >> 3) - ((int32_t)par_t1 << 1);
  int32_t v2 = (v1 * (int32_t)par_t2) >> 11;
  int32_t v3 = ((v1 >> 1) * (v1 >> 1)) >> 12;
  v3 = (v3 * ((int32_t)par_t3 << 4)) >> 14;
  int32_t t_fine = v2 + v3;
  int32_t t100   = ((t_fine * 5) + 128) >> 8;

  /* pressure */
  v1 = (t_fine >> 1) - 64000;
  v2 = ((((v1 >> 2) * (v1 >> 2)) >> 11) * (int32_t)par_p6) >> 2;
  v2 = v2 + ((v1 * (int32_t)par_p5) << 1);
  v2 = (v2 >> 2) + ((int32_t)par_p4 << 16);
  v1 = (((((v1 >> 2) * (v1 >> 2)) >> 13) * ((int32_t)par_p3 << 5)) >> 3)
       + (((int32_t)par_p2 * v1) >> 1);
  v1 = v1 >> 18;
  v1 = ((32768 + v1) * (int32_t)par_p1) >> 15;
  if (v1 == 0) return HAL_ERROR;

  int32_t p = 1048576 - p_adc;
  p = (int32_t)((uint32_t)(p - (v2 >> 12)) * 3125u);      /* unsigned, as in Bosch's code */
  p = (p >= 0x40000000) ? ((p / v1) << 1) : ((p << 1) / v1);
  v1 = ((int32_t)par_p9 * (((p >> 3) * (p >> 3)) >> 13)) >> 12;
  v2 = ((p >> 2) * (int32_t)par_p8) >> 13;
  v3 = ((p >> 8) * (p >> 8) * (p >> 8) * (int32_t)par_p10) >> 17;
  p  = p + ((v1 + v2 + v3 + ((int32_t)par_p7 << 7)) >> 4);

  /* humidity */
  int32_t ts = t100;
  int32_t h1 = h_adc - ((int32_t)par_h1 * 16) - (((ts * (int32_t)par_h3) / 100) >> 1);
  int32_t h2 = ((int32_t)par_h2 *
               (((ts * (int32_t)par_h4) / 100) +
                (((ts * ((ts * (int32_t)par_h5) / 100)) >> 6) / 100) +
                (1 << 14))) >> 10;
  int32_t h3 = h1 * h2;
  int32_t h4 = (((int32_t)par_h6 << 7) + ((ts * (int32_t)par_h7) / 100)) >> 4;
  int32_t h5 = ((h3 >> 14) * (h3 >> 14)) >> 10;
  int32_t h6 = (h4 * h5) >> 1;
  int32_t rh = (((h3 + h6) >> 10) * 1000) >> 12;
  if (rh > 100000) rh = 100000;
  if (rh < 0)      rh = 0;

  env->t100   = t100;
  env->rh1000 = rh;
  env->p_pa   = p;
  return HAL_OK;
}
