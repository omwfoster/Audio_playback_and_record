/**
  ******************************************************************************
  * @file    sd_diskio.c
  * @brief   SD card Disk I/O driver for FatFs.
  *
  *          Uses blocking BSP_SD_ReadBlocks / BSP_SD_WriteBlocks (no DMA) so
  *          it works safely alongside the DFSDM DMA audio capture.
  *          D-Cache coherency is maintained via SCB_CleanInvalidateDCache().
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"
#include "stm32f769i_discovery_sd.h"
#include "sd_diskio.h"

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

DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;

  /* Flush D-Cache lines that may alias the destination buffer before DMA/SDMMC fills it */
  SCB_CleanInvalidateDCache_by_Addr((uint32_t *)buff, count * SD_BLOCK_SIZE);

  if (BSP_SD_ReadBlocks((uint32_t *)buff, (uint32_t)sector, count, SD_TIMEOUT_MS) == MSD_OK)
  {
    uint32_t t = HAL_GetTick();
    while (BSP_SD_GetCardState() != MSD_OK)
    {
      if (HAL_GetTick() - t > SD_TIMEOUT_MS)
        return RES_ERROR;
    }
    res = RES_OK;
  }
  return res;
}

#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;

  /* Clean D-Cache so SDMMC sees up-to-date data */
  SCB_CleanDCache_by_Addr((uint32_t *)buff, count * SD_BLOCK_SIZE);

  if (BSP_SD_WriteBlocks((uint32_t *)buff, (uint32_t)sector, count, SD_TIMEOUT_MS) == MSD_OK)
  {
    uint32_t t = HAL_GetTick();
    while (BSP_SD_GetCardState() != MSD_OK)
    {
      if (HAL_GetTick() - t > SD_TIMEOUT_MS)
        return RES_ERROR;
    }
    res = RES_OK;
  }
  return res;
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
