/**
  ******************************************************************************
  * @file    sd_diskio.c
  * @brief   SD card Disk I/O driver for FatFs.
  *
  *          Transfers use SDMMC2 DMA (DMA2 Stream0 RX / Stream5 TX) and
  *          wait for the completion callback. The DFSDM mics were moved to
  *          Stream4/Stream1 so they no longer share these streams.
  *          D-Cache coherency is maintained around each transfer, and
  *          buffers that aren't word-aligned go through an aligned scratch
  *          sector (the DMA needs word alignment).
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "../../Drivers/disco_bsp/stm32f769i_discovery_sd.h"

/* BSP SD handle (defined in stm32f769i_discovery_sd.c) — used to report the
 * precise SDMMC error bits when a transfer fails. Key values:
 *   0x002 DATA_CRC_FAIL   0x008 DATA_TIMEOUT   0x010 TX_UNDERRUN
 *   0x020 RX_OVERRUN      0x4000000 WRITE_PROT_VIOLATION */
extern SD_HandleTypeDef uSdHandle;

/* FatFs R0.15 removed the _USE_WRITE / _USE_IOCTL switches and makes disk_write
 * and disk_ioctl UNCONDITIONAL members of Diskio_drvTypeDef (see ff_gen_drv.h).
 * The guards below were left over from R0.12; without these defines they expand
 * to 0, so SD_Driver would omit SD_write/SD_ioctl, leaving disk_write = NULL and
 * crashing the first f_write with a null-pointer hard fault. Force them on so
 * the driver table populates all five slots. */
#ifndef _USE_WRITE
#define _USE_WRITE 1
#endif
#ifndef _USE_IOCTL
#define _USE_IOCTL 1
#endif

/* Private defines -----------------------------------------------------------*/
#define SD_TIMEOUT_MS    5000U
#define SD_BLOCK_SIZE    512U

/* Private variables ---------------------------------------------------------*/
static volatile DSTATUS Stat = STA_NOINIT;

/* Set from the SDMMC2 IRQ (HAL_SD_IRQHandler -> callbacks), polled below */
static volatile uint8_t WriteDone;
static volatile uint8_t ReadDone;
static volatile uint8_t XferError;

/* Bounce sector for buffers the DMA can't use directly (not 4-byte aligned).
 * 32-byte aligned so cache maintenance on it is exact. */
static uint8_t Scratch[SD_BLOCK_SIZE] __attribute__((aligned(32)));

/* Private function prototypes -----------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS SD_initialize(BYTE lun);
DSTATUS SD_status(BYTE lun);
DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count);
#endif
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff);
#endif

const Diskio_drvTypeDef SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read,
#if _USE_WRITE == 1
  SD_write,
#endif
#if _USE_IOCTL == 1
  SD_ioctl,
#endif
};

/* Private functions ---------------------------------------------------------*/

static DSTATUS SD_CheckStatus(BYTE lun)
{
  Stat = STA_NOINIT;
  if (BSP_SD_GetCardState() == MSD_OK)
  {
    Stat &= ~STA_NOINIT;
  }
  return Stat;
}

DSTATUS SD_initialize(BYTE lun)
{
  if (BSP_SD_Init() == MSD_OK)
  {
    Stat = SD_CheckStatus(lun);
  }
  return Stat;
}

DSTATUS SD_status(BYTE lun)
{
  return SD_CheckStatus(lun);
}

/* Round a buffer out to whole 32-byte cache lines for SCB_*_by_Addr */
static void dcache_range(const void *buf, uint32_t len, uint32_t **start, int32_t *size)
{
  uint32_t a = (uint32_t)buf & ~31u;
  uint32_t e = ((uint32_t)buf + len + 31u) & ~31u;
  *start = (uint32_t *)a;
  *size  = (int32_t)(e - a);
}

/* Wait for a DMA transfer's completion flag, then for the card to leave the
 * programming/receiving state. Returns 0 on success. */
static int wait_xfer(volatile uint8_t *done, uint32_t t0)
{
  while (!*done && !XferError)
  {
    if (HAL_GetTick() - t0 > SD_TIMEOUT_MS)
    {
      HAL_SD_Abort(&uSdHandle);
      return -1;
    }
  }
  if (XferError)
  {
    return -1;
  }
  while (BSP_SD_GetCardState() != MSD_OK)
  {
    if (HAL_GetTick() - t0 > SD_TIMEOUT_MS)
    {
      return -1;
    }
  }
  return 0;
}

static int read_dma(uint8_t *dst, uint32_t sector, uint32_t count)
{
  uint32_t *cs; int32_t cn;
  dcache_range(dst, count * SD_BLOCK_SIZE, &cs, &cn);

  /* Write back anything dirty around dst so the invalidate after the
   * transfer can't throw away CPU data sharing its edge cache lines */
  SCB_CleanInvalidateDCache_by_Addr(cs, cn);

  ReadDone = 0; XferError = 0;
  uint32_t t0 = HAL_GetTick();
  if (BSP_SD_ReadBlocks_DMA((uint32_t *)dst, sector, count) != MSD_OK)
  {
    return -1;
  }
  if (wait_xfer(&ReadDone, t0) != 0)
  {
    return -1;
  }

  /* Drop any lines speculatively refilled during the DMA */
  SCB_InvalidateDCache_by_Addr(cs, cn);
  return 0;
}

static int write_dma(const uint8_t *src, uint32_t sector, uint32_t count)
{
  uint32_t *cs; int32_t cn;
  dcache_range(src, count * SD_BLOCK_SIZE, &cs, &cn);
  SCB_CleanDCache_by_Addr(cs, cn);

  WriteDone = 0; XferError = 0;
  uint32_t t0 = HAL_GetTick();
  if (BSP_SD_WriteBlocks_DMA((uint32_t *)src, sector, count) != MSD_OK)
  {
    return -1;
  }
  return wait_xfer(&WriteDone, t0);
}

DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  if (((uint32_t)buff & 3u) == 0)
  {
    return (read_dma(buff, (uint32_t)sector, count) == 0) ? RES_OK : RES_ERROR;
  }

  /* Unaligned: one sector at a time through Scratch */
  for (UINT i = 0; i < count; i++)
  {
    if (read_dma(Scratch, (uint32_t)sector + i, 1) != 0)
    {
      return RES_ERROR;
    }
    memcpy(buff + i * SD_BLOCK_SIZE, Scratch, SD_BLOCK_SIZE);
  }
  return RES_OK;
}

#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  int err = 0;

  if (((uint32_t)buff & 3u) == 0)
  {
    err = write_dma(buff, (uint32_t)sector, count);
  }
  else
  {
    /* Unaligned: one sector at a time through Scratch */
    for (UINT i = 0; i < count && err == 0; i++)
    {
      memcpy(Scratch, buff + i * SD_BLOCK_SIZE, SD_BLOCK_SIZE);
      err = write_dma(Scratch, (uint32_t)sector + i, 1);
    }
  }

  if (err != 0)
  {
    printf("SD_write: HAL err=0x%08lX sector=%lu n=%u\r\n",
           uSdHandle.ErrorCode, (uint32_t)sector, count);
    return RES_ERROR;
  }
  return RES_OK;
}
#endif /* _USE_WRITE == 1 */

#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  BSP_SD_CardInfo CardInfo;

  if (Stat & STA_NOINIT)
    return RES_NOTRDY;

  switch (cmd)
  {
    case CTRL_SYNC:
      res = RES_OK;
      break;

    case GET_SECTOR_COUNT:
      BSP_SD_GetCardInfo(&CardInfo);
      *(DWORD *)buff = CardInfo.LogBlockNbr;
      res = RES_OK;
      break;

    case GET_SECTOR_SIZE:
      BSP_SD_GetCardInfo(&CardInfo);
      *(WORD *)buff = CardInfo.LogBlockSize;
      res = RES_OK;
      break;

    case GET_BLOCK_SIZE:
      BSP_SD_GetCardInfo(&CardInfo);
      *(DWORD *)buff = CardInfo.LogBlockSize / SD_BLOCK_SIZE;
      res = RES_OK;
      break;

    default:
      res = RES_PARERR;
      break;
  }
  return res;
}
#endif /* _USE_IOCTL == 1 */

/* SDMMC2 DMA completion callbacks (weak in stm32f769i_discovery_sd.c / HAL),
 * called from SDMMC2_IRQHandler. */
void BSP_SD_WriteCpltCallback(void)
{
  WriteDone = 1;
}

void BSP_SD_ReadCpltCallback(void)
{
  ReadDone = 1;
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
  (void)hsd;
  XferError = 1;
}
