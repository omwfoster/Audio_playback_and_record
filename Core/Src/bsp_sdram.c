#include "bsp_sdram.h"

static SDRAM_HandleTypeDef hsdram;
static FMC_SDRAM_TimingTypeDef sdram_timing;

/* ---------------------------------------------------------------------------
 * MspInit — FMC GPIO and clock
 * All pins match STM32F769I-DISCO schematic
 * --------------------------------------------------------------------------*/
void BSP_SDRAM_MspInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* FMC clock */
    __HAL_RCC_FMC_CLK_ENABLE();

    /* GPIO clocks already enabled in MX_GPIO_Init, but belt-and-braces */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FMC;

    /* GPIOD: D2, D3, D1, D15, D0, D14, D13, SDCLK is on G, but SDNCAS on G */
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 |
               GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* GPIOE: NBL0, NBL1, D4-D12 */
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 |
               GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
               GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &gpio);

    /* GPIOF: A0-A5, A6-A9 (SDNRAS on F11) */
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
               GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_11 | GPIO_PIN_12 |
               GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOF, &gpio);

    /* GPIOG: A10-A12, BA0-BA1, SDCLK, SDNCAS */
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 |
               GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOG, &gpio);

    /* GPIOH: D16-D23, SDNME, SDNE0, SDCKE0, D17 */
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_6 |
               GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
               GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
               GPIO_PIN_15;
    HAL_GPIO_Init(GPIOH, &gpio);

    /* GPIOI: D24-D31, NBL2, NBL3 */
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
               GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
               GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOI, &gpio);
}

/* ---------------------------------------------------------------------------
 * BSP_SDRAM_Init
 * --------------------------------------------------------------------------*/
HAL_StatusTypeDef BSP_SDRAM_Init(void)
{
    FMC_SDRAM_CommandTypeDef cmd = {0};
    HAL_StatusTypeDef status;

    BSP_SDRAM_MspInit();

    /* ---- FMC SDRAM handle ---- */
    hsdram.Instance  = FMC_SDRAM_DEVICE;
    hsdram.Init.SDBank             = FMC_SDRAM_BANK1;
    hsdram.Init.ColumnBitsNumber   = SDRAM_COLUMN_BITS;
    hsdram.Init.RowBitsNumber      = SDRAM_ROW_BITS;
    hsdram.Init.MemoryDataWidth    = SDRAM_DATA_WIDTH;
    hsdram.Init.InternalBankNumber = SDRAM_INTERN_BANKS;
    hsdram.Init.CASLatency         = SDRAM_CAS_LATENCY;
    hsdram.Init.WriteProtection    = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
    /* SDRAM clock = HCLK/2 = 48MHz, 2-cycle read pipe */
    hsdram.Init.SDClockPeriod      = FMC_SDRAM_CLOCK_PERIOD_2;
    hsdram.Init.ReadBurst          = FMC_SDRAM_RBURST_ENABLE;
    hsdram.Init.ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_0;

    /* ---- Timing at 48MHz (period ≈ 20.8ns) ---- */
    /* All values in clock cycles, register is (n-1) */
    sdram_timing.LoadToActiveDelay    = 2;  /* tMRD  = 2 clk           */
    sdram_timing.ExitSelfRefreshDelay = 7;  /* tXSR  = 70ns  → 4 clk  */
    sdram_timing.SelfRefreshTime      = 4;  /* tRAS  = 42ns  → 3 clk  */
    sdram_timing.RowCycleDelay        = 7;  /* tRC   = 63ns  → 4 clk  */
    sdram_timing.WriteRecoveryTime    = 2;  /* tWR   = 2 clk           */
    sdram_timing.RPDelay              = 2;  /* tRP   = 18ns  → 1 clk  */
    sdram_timing.RCDDelay             = 2;  /* tRCD  = 18ns  → 1 clk  */

    status = HAL_SDRAM_Init(&hsdram, &sdram_timing);
    if (status != HAL_OK) return status;

    /* ---- Initialisation sequence (JEDEC) ---- */

    /* 1. Clock enable */
    cmd.CommandMode            = FMC_SDRAM_CMD_CLK_ENABLE;
    cmd.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
    cmd.AutoRefreshNumber      = 1;
    cmd.ModeRegisterDefinition = 0;
    status = HAL_SDRAM_SendCommand(&hsdram, &cmd, 0xFFFF);
    if (status != HAL_OK) return status;

    /* 2. Wait ≥100µs — HAL_Delay(1) gives 1ms, sufficient */
    HAL_Delay(1);

    /* 3. Precharge all banks */
    cmd.CommandMode = FMC_SDRAM_CMD_PALL;
    status = HAL_SDRAM_SendCommand(&hsdram, &cmd, 0xFFFF);
    if (status != HAL_OK) return status;

    /* 4. Two auto-refresh cycles */
    cmd.CommandMode       = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    cmd.AutoRefreshNumber = 8;
    status = HAL_SDRAM_SendCommand(&hsdram, &cmd, 0xFFFF);
    if (status != HAL_OK) return status;

    /* 5. Load mode register */
    cmd.CommandMode            = FMC_SDRAM_CMD_LOAD_MODE;
    cmd.AutoRefreshNumber      = 1;
    cmd.ModeRegisterDefinition = SDRAM_MODE_REG;
    status = HAL_SDRAM_SendCommand(&hsdram, &cmd, 0xFFFF);
    if (status != HAL_OK) return status;

    /* 6. Set refresh rate */
    status = HAL_SDRAM_ProgramRefreshRate(&hsdram, SDRAM_REFRESH_COUNT);

    return status;
}
