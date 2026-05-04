#ifndef BSP_SDRAM_H
#define BSP_SDRAM_H

#include "stm32f7xx_hal.h"

/* SDRAM device address — FMC Bank 1 (SDRAM Bank 1) */
#define SDRAM_BASE_ADDR         0xC0000000UL
#define SDRAM_SIZE              0x01000000UL   /* 16MB */

/* IS42S32400F geometry */
#define SDRAM_COLUMN_BITS       FMC_SDRAM_COLUMN_BITS_NUM_8
#define SDRAM_ROW_BITS          FMC_SDRAM_ROW_BITS_NUM_12
#define SDRAM_DATA_WIDTH        FMC_SDRAM_MEM_BUS_WIDTH_32
#define SDRAM_INTERN_BANKS      FMC_SDRAM_INTERN_BANKS_NUM_4
#define SDRAM_CAS_LATENCY       FMC_SDRAM_CAS_LATENCY_3

/* Mode register encoding for IS42S32400F
 * Burst length=1, Burst type=Sequential, CAS=3, Write burst=single */
#define SDRAM_MODEREG_BURST_LENGTH_1        0x0000U
#define SDRAM_MODEREG_BURST_TYPE_SEQ        0x0000U
#define SDRAM_MODEREG_CAS_LATENCY_3         0x0030U
#define SDRAM_MODEREG_WRITEBURST_SINGLE     0x0200U
#define SDRAM_MODE_REG  (SDRAM_MODEREG_BURST_LENGTH_1  | \
                         SDRAM_MODEREG_BURST_TYPE_SEQ  | \
                         SDRAM_MODEREG_CAS_LATENCY_3   | \
                         SDRAM_MODEREG_WRITEBURST_SINGLE)

/* Refresh count for 64ms / 4096 rows at 48MHz SDRAM clock,
 * minus 20 cycles safety margin: (48e6 * 64e-3) / 4096 - 20 = 730 */
#define SDRAM_REFRESH_COUNT     730U

HAL_StatusTypeDef BSP_SDRAM_Init(void);
void              BSP_SDRAM_MspInit(void);

#endif /* BSP_SDRAM_H */
