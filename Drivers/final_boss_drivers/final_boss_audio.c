

/* Includes ------------------------------------------------------------------*/
#include "final_boss_audio.h"
#include "main.h"
#include <string.h>





typedef struct
{
  uint16_t      *pRecBuf;       /* Pointer to record user buffer */
  uint32_t      RecSize;        /* Size to record in mono, double size to record in stereo */
}AUDIOIN_TypeDef;


#define SCRATCH_BUFF_SIZE  512

__attribute__((section(".dma_buffers")))
int32_t Scratch[SCRATCH_BUFF_SIZE];

__attribute__((section(".dma_buffers")))
static AUDIO_IN_BufferTypeDef  BufferCtl;

__attribute__((section(".dma_buffers")))
uint16_t raw_dma_samples[PCM_BUFFER_SIZE];

static __IO uint32_t uwVolume = 100;




#define DFSDM_OVER_SAMPLING(__FREQUENCY__) \
        (__FREQUENCY__ == AUDIO_FREQUENCY_8K)  ? 256 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_11K) ? 256 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_16K) ? 128 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_22K) ? 128 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_32K) ? 64 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_44K) ? 64  \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_48K) ? 40 : 20  \

#define DFSDM_CLOCK_DIVIDER(__FREQUENCY__) \
        (__FREQUENCY__ == AUDIO_FREQUENCY_8K)  ? 24 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_11K) ? 4 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_16K) ? 24 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_22K) ? 4 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_32K) ? 24 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_44K) ? 4  \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_48K) ? 25 : 25  \

#define DFSDM_FILTER_ORDER(__FREQUENCY__) \
        (__FREQUENCY__ == AUDIO_FREQUENCY_8K)  ? DFSDM_FILTER_SINC3_ORDER \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_11K) ? DFSDM_FILTER_SINC3_ORDER \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_16K) ? DFSDM_FILTER_SINC3_ORDER \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_22K) ? DFSDM_FILTER_SINC3_ORDER \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_32K) ? DFSDM_FILTER_SINC4_ORDER \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_44K) ? DFSDM_FILTER_SINC3_ORDER  \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_48K) ? DFSDM_FILTER_SINC3_ORDER : DFSDM_FILTER_SINC5_ORDER  \

#define DFSDM_RIGHT_BIT_SHIFT(__FREQUENCY__) \
        (__FREQUENCY__ == AUDIO_FREQUENCY_8K)  ? 8 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_11K) ? 8 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_16K) ? 3 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_22K) ? 4 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_32K) ? 7 \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_44K) ? 0  \
      : (__FREQUENCY__ == AUDIO_FREQUENCY_48K) ? 0 : 4  \

/* Saturate the record PCM sample */
#define SaturaLH(N, L, H) (((N)<(L))?(L):(((N)>(H))?(H):(N)))
/**
  * @}
  */ 
  
/** @defgroup STM32F769I_EVAL_AUDIO_Private_Variables STM32F769I_EVAL_AUDIO Private Variables
  * @{
  */
/*### PLAY ###*/


/*### RECORD ###*/
AUDIOIN_TypeDef                 hAudioIn;



DFSDM_Channel_HandleTypeDef     haudio_in_dfsdm_leftchannel;
DFSDM_Channel_HandleTypeDef     haudio_in_dfsdm_rightchannel;
DFSDM_Filter_HandleTypeDef      haudio_in_dfsdm_leftfilter;
DFSDM_Filter_HandleTypeDef      haudio_in_dfsdm_rightfilter;
DMA_HandleTypeDef               hdma_dfsdm_left;
DMA_HandleTypeDef               hdma_dfsdm_right;

/* Buffers for right and left samples */

int32_t                         *pScratchBuff[CHANNEL_NBR];
int32_t                         ScratchSize;



/* Buffers status flags */

volatile uint32_t                        DmaRecHalfBuffCplt  = 0;

volatile uint32_t                        DmaRecBuffCplt      = 0;


/* Application Buffer Trigger */
__IO uint32_t                   AppBuffTrigger          = 0;
__IO uint32_t                   AppBuffHalf             = 0;



/**
  * @}
  */ 

/** @defgroup STM32F769I_EVAL_AUDIO_Private_Function_Prototypes STM32F769I_EVAL_AUDIO Private Function Prototypes
  * @{
  */

static void    DFSDMx_ChannelMspInit(void);
static void    DFSDMx_FilterMspInit(void);
static void    DFSDMx_ChannelMspDeInit(void);
static void    DFSDMx_FilterMspDeInit(void);
static uint8_t DFSDMx_Init(uint32_t AudioFreq);
static uint8_t DFSDMx_DeInit(void);


/**
  * @}
  */

/** @defgroup STM32F769I_EVAL_AUDIO_out_Private_Functions STM32F769I_EVAL_AUDIO_Out Private Functions
  * @{
  */ 


void BSP_AUDIO_IN_Start_Sample()
{

	uint32_t byteswritten = 0;
	hAudioIn.pRecBuf = (uint16_t*)raw_dma_samples;
    BSP_AUDIO_IN_Init(AUDIO_FREQUENCY_16K, BIT_RESOLUTION, CHANNEL_NBR);
    BSP_AUDIO_IN_AllocScratch (Scratch, SCRATCH_BUFF_SIZE);
    BSP_AUDIO_IN_Start((uint16_t*)&BufferCtl.pcm_buff[0], PCM_BUFFER_SIZE);
    BufferCtl.fptr = byteswritten;
    BufferCtl.pcm_ptr = 0;
    BufferCtl.offset = 0;
    BufferCtl.wr_state = BUFFER_EMPTY;
}

/**
  * @brief  Initialize wave recording. 
  * @param  AudioFreq: Audio frequency to be configured for the DFSDM peripheral. 
  * @param  BitRes: Audio frequency to be configured for the DFSDM peripheral.
  * @param  ChnlNbr: Audio frequency to be configured for the DFSDM peripheral.
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t BSP_AUDIO_IN_Init(uint32_t AudioFreq, uint32_t BitRes, uint32_t ChnlNbr)
{ 
  
  /* PLL clock is set depending by the AudioFreq (44.1khz vs 48khz groups) */ 
  BSP_AUDIO_IN_ClockConfig(&haudio_in_dfsdm_leftfilter, AudioFreq, NULL);
  
  /* Init the SAI MSP: this __weak function can be redefined by the application*/
  BSP_AUDIO_IN_MspInit();
  
  /* Initializes DFSDM peripheral */
  DFSDMx_Init(AudioFreq);
  
  /* Return AUDIO_OK when all operations are correctly done */
  return AUDIO_OK;
}

/**
  * @brief  Allocate channel buffer scratch 
  * @param  pScratch : pointer to scratch tables.
  * @param  size of scratch buffer
  */
uint8_t BSP_AUDIO_IN_AllocScratch (int32_t *pScratch, uint32_t size)
{ 
  uint32_t idx;
  
  ScratchSize = size / CHANNEL_NBR;
  
  /* copy scratch pointers */
  for (idx = 0; idx < CHANNEL_NBR ; idx++)
  {
    pScratchBuff[idx] = (int32_t *)(pScratch + idx * ScratchSize);
  }
  /* Return AUDIO_OK */
  return AUDIO_OK;
}

/**
  * @brief  Start audio recording.
  * @param  pbuf: Main buffer pointer for the recorded data storing  
  * @param  size: Current size of the recorded buffer
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t BSP_AUDIO_IN_Start(uint16_t* pbuf, uint32_t size)
{
  hAudioIn.pRecBuf = pbuf;
  hAudioIn.RecSize = size;
  /* Reset Application Buffer Trigger */
  AppBuffTrigger = 0;
  AppBuffHalf = 0;
  
  /* Call the Media layer start function for right channel */
  if(HAL_OK != HAL_DFSDM_FilterRegularStart_DMA(&haudio_in_dfsdm_rightfilter, pScratchBuff[0], ScratchSize))
  {
    return AUDIO_ERROR;
  }
  
  /* Call the Media layer start function for left channel */
  if(HAL_OK != HAL_DFSDM_FilterRegularStart_DMA(&haudio_in_dfsdm_leftfilter, pScratchBuff[1], ScratchSize))
  {
    return AUDIO_ERROR;
  }
  
  /* Return AUDIO_OK when all operations are correctly done */
  return AUDIO_OK;
}

/**
  * @brief  Stop audio recording.
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t BSP_AUDIO_IN_Stop(void)
{
  AppBuffTrigger = 0;
  AppBuffHalf    = 0;
  
  /* Call the Media layer stop function for right channel */
  if(HAL_OK != HAL_DFSDM_FilterRegularStop_DMA(&haudio_in_dfsdm_rightfilter))
  {
    return AUDIO_ERROR;
  }
  
  /* Call the Media layer stop function for left channel */
  if(HAL_OK != HAL_DFSDM_FilterRegularStop_DMA(&haudio_in_dfsdm_leftfilter))
  {
    return AUDIO_ERROR;
  }
  
  /* Return AUDIO_OK when all operations are correctly done */
  return AUDIO_OK;
}

/**
  * @brief  Pause the audio file stream.
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t BSP_AUDIO_IN_Pause(void)
{
  /* Call the Media layer stop function */
  if(HAL_OK != HAL_DFSDM_FilterRegularStop_DMA(&haudio_in_dfsdm_rightfilter))
  {
    return AUDIO_ERROR;
  }
  
  /* Call the Media layer stop function */
  if(HAL_OK != HAL_DFSDM_FilterRegularStop_DMA(&haudio_in_dfsdm_leftfilter))
  {
    return AUDIO_ERROR;
  }
  
  /* Return AUDIO_OK when all operations are correctly done */
  return AUDIO_OK;
}

/**
  * @brief  Resume the audio file stream.
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
uint8_t BSP_AUDIO_IN_Resume(void)
{
  /* Call the Media layer start function for right channel */
  if(HAL_OK != HAL_DFSDM_FilterRegularStart_DMA(&haudio_in_dfsdm_rightfilter, pScratchBuff[0], ScratchSize))
  {
    return AUDIO_ERROR;
  }
  
  /* Call the Media layer start function for left channel */
  if(HAL_OK != HAL_DFSDM_FilterRegularStart_DMA(&haudio_in_dfsdm_leftfilter, pScratchBuff[1], ScratchSize))
  {
    return AUDIO_ERROR;
  }
  
  /* Return AUDIO_OK when all operations are correctly done */
  return AUDIO_OK;
}

/**
  * @brief  Deinit the audio IN peripherals.
  * @retval None
  */
void BSP_AUDIO_IN_DeInit(void)
{
  BSP_AUDIO_IN_MspDeInit();
  DFSDMx_DeInit();
}


/**
  * @brief  Regular conversion complete callback. 
  * @note   In interrupt mode, user has to read conversion value in this function
            using HAL_DFSDM_FilterGetRegularValue.
  * @param  hdfsdm_filter : DFSDM filter handle.
  * @retval None
  */
void HAL_DFSDM_FilterRegConvCpltCallback(DFSDM_Filter_HandleTypeDef *hdfsdm_filter)
{
  uint32_t index = 0;



    for(index = (ScratchSize/2) ; index < ScratchSize; index++)
    {
      hAudioIn.pRecBuf[AppBuffTrigger]     = (uint16_t)(SaturaLH((pScratchBuff[0][index] >> 8), -32760, 32760));
      hAudioIn.pRecBuf[AppBuffTrigger + 1] = (uint16_t)(SaturaLH((pScratchBuff[1][index] >> 8), -32760, 32760));
      AppBuffTrigger +=2;
    }


  /* Call Half Transfer Complete callback */
  if((AppBuffTrigger == hAudioIn.RecSize/2) && (AppBuffHalf == 0))
  {
  AppBuffHalf = 1;  
  BSP_AUDIO_IN_HalfTransfer_CallBack();
  }
  /* Call Transfer Complete callback */
  if(AppBuffTrigger == hAudioIn.RecSize)
  {
    /* Reset Application Buffer Trigger */
    AppBuffTrigger = 0;
    AppBuffHalf = 0; 
    /* Call the record update function to get the next buffer to fill and its size (size is ignored) */
    BSP_AUDIO_IN_TransferComplete_CallBack();
  }  
}

/**
  * @brief  Half regular conversion complete callback. 
  * @param  hdfsdm_filter : DFSDM filter handle.
  * @retval None
  */
void HAL_DFSDM_FilterRegConvHalfCpltCallback(DFSDM_Filter_HandleTypeDef *hdfsdm_filter)
{
  uint32_t index = 0;



      for(index = 0; index < ScratchSize/2; index++)
      {
        hAudioIn.pRecBuf[AppBuffTrigger]     = (int16_t)(SaturaLH((pScratchBuff[0][index] >> 8), -32760, 32760));
        hAudioIn.pRecBuf[AppBuffTrigger + 1] = (int16_t)(SaturaLH((pScratchBuff[1][index] >> 8), -32760, 32760));
        AppBuffTrigger +=2;
      }


  /* Call Half Transfer Complete callback */
  if((AppBuffTrigger == hAudioIn.RecSize/2) && (AppBuffHalf == 0))
  { 
  AppBuffHalf = 1;  
  BSP_AUDIO_IN_HalfTransfer_CallBack();
  }
  /* Call Transfer Complete callback */
  if(AppBuffTrigger == hAudioIn.RecSize)
  {
    /* Reset Application Buffer Trigger */
    AppBuffTrigger = 0;
    AppBuffHalf = 0;
    /* Call the record update function to get the next buffer to fill and its size (size is ignored) */
    BSP_AUDIO_IN_TransferComplete_CallBack();
  }  
}

/**
  * @brief  User callback when record buffer is filled.
  * @retval None
  */
__weak void BSP_AUDIO_IN_TransferComplete_CallBack(void)
{
  /* This function should be implemented by the user application.
     It is called into this driver when the current buffer is filled
     to prepare the next buffer pointer and its size. */
}

/**
  * @brief  Manages the DMA Half Transfer complete event.
  * @retval None
  */
__weak void BSP_AUDIO_IN_HalfTransfer_CallBack(void)
{ 
  /* This function should be implemented by the user application.
     It is called into this driver when the current buffer is filled
     to prepare the next buffer pointer and its size. */
}

/**
  * @brief  Audio IN Error callback function.
  * @retval None
  */
__weak void BSP_AUDIO_IN_Error_Callback(void)
{   
  /* This function is called when an Interrupt due to transfer error on or peripheral
     error occurs. */
}

/**
  * @brief  Initialize BSP_AUDIO_IN MSP.
  * @retval None
  */
__weak void BSP_AUDIO_IN_MspInit(void)
{ 
  /* MSP channels initialization */
  DFSDMx_ChannelMspInit();  
  /* MSP filters initialization */
  DFSDMx_FilterMspInit();
}

/**
  * @brief  DeInitialize BSP_AUDIO_IN MSP.
  * @retval None
  */
__weak void BSP_AUDIO_IN_MspDeInit(void)
{ 
  /* MSP channels initialization */
  DFSDMx_ChannelMspDeInit();  
  /* MSP filters initialization */
  DFSDMx_FilterMspDeInit();
}

/**
  * @brief  Clock Config.
  * @param  hdfsdm_filter: might be required to set audio peripheral predivider if any.
  * @param  AudioFreq: Audio frequency used to play the audio stream.
  * @param  Params   
  * @note   This API is called by BSP_AUDIO_IN_Init()
  *         Being __weak it can be overwritten by the application     
  * @retval None
  */
__weak void BSP_AUDIO_IN_ClockConfig(DFSDM_Filter_HandleTypeDef *hdfsdm_filter, uint32_t AudioFreq, void *Params)
{ 
  RCC_PeriphCLKInitTypeDef rcc_ex_clk_init_struct;

  HAL_RCCEx_GetPeriphCLKConfig(&rcc_ex_clk_init_struct);
  
  /* Set the PLL configuration according to the audio frequency */
  if((AudioFreq == AUDIO_FREQUENCY_11K) || (AudioFreq == AUDIO_FREQUENCY_22K) || (AudioFreq == AUDIO_FREQUENCY_44K))
  {
    /* Configure PLLSAI prescalers */
    /* PLLI2S_VCO: VCO_429M 
    SAI_CLK(first level) = PLLI2S_VCO/PLLI2SQ = 429/2 = 214.5 Mhz
    SAI_CLK_x = SAI_CLK(first level)/PLLI2SDIVQ = 214.5/19 = 11.289 Mhz */ 
    rcc_ex_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_SAI2;
    rcc_ex_clk_init_struct.Sai2ClockSelection = RCC_SAI2CLKSOURCE_PLLI2S;
    rcc_ex_clk_init_struct.PLLI2S.PLLI2SN = 429;
    rcc_ex_clk_init_struct.PLLI2S.PLLI2SQ = 2;
    rcc_ex_clk_init_struct.PLLI2SDivQ = 19;
    
    HAL_RCCEx_PeriphCLKConfig(&rcc_ex_clk_init_struct);
    
  }
  else /* AUDIO_FREQUENCY_8K, AUDIO_FREQUENCY_16K, AUDIO_FREQUENCY_32K, AUDIO_FREQUENCY_48K, AUDIO_FREQUENCY_96K */
  {
    /* SAI clock config 
    PLLI2S_VCO: VCO_344M 
    SAI_CLK(first level) = PLLI2S_VCO/PLLI2SQ = 344/7 = 49.142 Mhz 
    SAI_CLK_x = SAI_CLK(first level)/PLLI2SDIVQ = 49.142/1 = 49.142 Mhz */  
    rcc_ex_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_SAI2;
    rcc_ex_clk_init_struct.Sai2ClockSelection = RCC_SAI2CLKSOURCE_PLLI2S;
    rcc_ex_clk_init_struct.PLLI2S.PLLI2SN = 344; 
    rcc_ex_clk_init_struct.PLLI2S.PLLI2SQ = 7; 
    rcc_ex_clk_init_struct.PLLI2SDivQ = 1;   
    HAL_RCCEx_PeriphCLKConfig(&rcc_ex_clk_init_struct);
  }
  
  rcc_ex_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_DFSDM1_AUDIO;
  rcc_ex_clk_init_struct.Dfsdm1AudioClockSelection = RCC_DFSDM1AUDIOCLKSOURCE_SAI2;
  HAL_RCCEx_PeriphCLKConfig(&rcc_ex_clk_init_struct); 
}

/*******************************************************************************
                            Static Functions
*******************************************************************************/
/**
  * @brief  Initialize the Digital Filter for Sigma-Delta Modulators interface (DFSDM).
  * @param  AudioFreq: Audio frequency to be used to set correctly the DFSDM peripheral.
  * @note   Channel output Clock Divider and Filter Oversampling are calculated as follow: 
  *         - Clock_Divider = CLK(input DFSDM)/CLK(micro) with
  *           1MHZ < CLK(micro) < 3.2MHZ (TYP 2.4MHZ for MP34DT01TR)
  *         - Oversampling = CLK(input DFSDM)/(Clock_Divider * AudioFreq)  
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
static uint8_t DFSDMx_Init(uint32_t AudioFreq)
{
  /*####CHANNEL 1####*/
  __HAL_DFSDM_CHANNEL_RESET_HANDLE_STATE(&haudio_in_dfsdm_leftchannel);  
  haudio_in_dfsdm_leftchannel.Instance                      = DFSDM1_Channel1;  
  haudio_in_dfsdm_leftchannel.Init.OutputClock.Activation   = ENABLE;
  haudio_in_dfsdm_leftchannel.Init.OutputClock.Selection    = DFSDM_CHANNEL_OUTPUT_CLOCK_AUDIO;
  /* Set the DFSDM clock OUT audio frequency configuration */
  haudio_in_dfsdm_leftchannel.Init.OutputClock.Divider      = DFSDM_CLOCK_DIVIDER(AudioFreq);
  haudio_in_dfsdm_leftchannel.Init.Input.Multiplexer        = DFSDM_CHANNEL_EXTERNAL_INPUTS;
  haudio_in_dfsdm_leftchannel.Init.Input.DataPacking        = DFSDM_CHANNEL_STANDARD_MODE;
  haudio_in_dfsdm_leftchannel.Init.Input.Pins               = DFSDM_CHANNEL_SAME_CHANNEL_PINS;
  /* Request to sample stable data for LEFT micro on Rising edge */
  haudio_in_dfsdm_leftchannel.Init.SerialInterface.Type     = DFSDM_CHANNEL_SPI_RISING;
  haudio_in_dfsdm_leftchannel.Init.SerialInterface.SpiClock = DFSDM_CHANNEL_SPI_CLOCK_INTERNAL;
  haudio_in_dfsdm_leftchannel.Init.Awd.FilterOrder          = DFSDM_CHANNEL_FASTSINC_ORDER;
  haudio_in_dfsdm_leftchannel.Init.Awd.Oversampling         = 10;
  haudio_in_dfsdm_leftchannel.Init.Offset                   = 0;
  haudio_in_dfsdm_leftchannel.Init.RightBitShift            = DFSDM_RIGHT_BIT_SHIFT(AudioFreq);
  if(HAL_OK != HAL_DFSDM_ChannelInit(&haudio_in_dfsdm_leftchannel))
  {
    return AUDIO_ERROR;
  }

  /*####FILTER 0####*/
  __HAL_DFSDM_FILTER_RESET_HANDLE_STATE(&haudio_in_dfsdm_leftfilter);
  haudio_in_dfsdm_leftfilter.Instance                          = AUDIO_DFSDMx_LEFT_FILTER;  
  haudio_in_dfsdm_leftfilter.Init.RegularParam.Trigger         = DFSDM_FILTER_SW_TRIGGER;
  haudio_in_dfsdm_leftfilter.Init.RegularParam.FastMode        = ENABLE;
  haudio_in_dfsdm_leftfilter.Init.RegularParam.DmaMode         = ENABLE;
  haudio_in_dfsdm_leftfilter.Init.InjectedParam.Trigger        = DFSDM_FILTER_SW_TRIGGER;
  haudio_in_dfsdm_leftfilter.Init.InjectedParam.ScanMode       = ENABLE;
  haudio_in_dfsdm_leftfilter.Init.InjectedParam.DmaMode        = DISABLE;
  haudio_in_dfsdm_leftfilter.Init.InjectedParam.ExtTrigger     = DFSDM_FILTER_EXT_TRIG_TIM1_TRGO;
  haudio_in_dfsdm_leftfilter.Init.InjectedParam.ExtTriggerEdge = DFSDM_FILTER_EXT_TRIG_RISING_EDGE;
  haudio_in_dfsdm_leftfilter.Init.FilterParam.SincOrder        = DFSDM_FILTER_ORDER(AudioFreq);
  /* Set the DFSDM Filters Oversampling to have correct sample rate */
  haudio_in_dfsdm_leftfilter.Init.FilterParam.Oversampling     = DFSDM_OVER_SAMPLING(AudioFreq);
  haudio_in_dfsdm_leftfilter.Init.FilterParam.IntOversampling  = 1;
  if(HAL_OK != HAL_DFSDM_FilterInit(&haudio_in_dfsdm_leftfilter))
  {
    return AUDIO_ERROR;
  }

  /* Configure injected channel */
  if(HAL_OK != HAL_DFSDM_FilterConfigRegChannel(&haudio_in_dfsdm_leftfilter, DFSDM_CHANNEL_1, DFSDM_CONTINUOUS_CONV_ON))
  {
    return AUDIO_ERROR;
  }
  
  /*####CHANNEL 0####*/
  __HAL_DFSDM_CHANNEL_RESET_HANDLE_STATE(&haudio_in_dfsdm_rightchannel);  
  haudio_in_dfsdm_rightchannel.Instance                      = DFSDM1_Channel0;  
  haudio_in_dfsdm_rightchannel.Init.OutputClock.Activation   = ENABLE;
  haudio_in_dfsdm_rightchannel.Init.OutputClock.Selection    = DFSDM_CHANNEL_OUTPUT_CLOCK_AUDIO;
  /* Set the DFSDM clock OUT audio frequency configuration */
  haudio_in_dfsdm_rightchannel.Init.OutputClock.Divider      = DFSDM_CLOCK_DIVIDER(AudioFreq);
  haudio_in_dfsdm_rightchannel.Init.Input.Multiplexer        = DFSDM_CHANNEL_EXTERNAL_INPUTS;
  haudio_in_dfsdm_rightchannel.Init.Input.DataPacking        = DFSDM_CHANNEL_STANDARD_MODE;
  haudio_in_dfsdm_rightchannel.Init.Input.Pins               = DFSDM_CHANNEL_FOLLOWING_CHANNEL_PINS;
  /* Request to sample stable data for RIGHT micro on Falling edge */
  haudio_in_dfsdm_rightchannel.Init.SerialInterface.Type     = DFSDM_CHANNEL_SPI_FALLING;
  haudio_in_dfsdm_rightchannel.Init.SerialInterface.SpiClock = DFSDM_CHANNEL_SPI_CLOCK_INTERNAL;
  haudio_in_dfsdm_rightchannel.Init.Awd.FilterOrder          = DFSDM_CHANNEL_FASTSINC_ORDER;
  haudio_in_dfsdm_rightchannel.Init.Awd.Oversampling         = 10;
  haudio_in_dfsdm_rightchannel.Init.Offset                   = 0;
  haudio_in_dfsdm_rightchannel.Init.RightBitShift            = DFSDM_RIGHT_BIT_SHIFT(AudioFreq);
  if(HAL_OK != HAL_DFSDM_ChannelInit(&haudio_in_dfsdm_rightchannel))
  {
    return AUDIO_ERROR;
  }
  
  /*####FILTER 1####*/
  __HAL_DFSDM_FILTER_RESET_HANDLE_STATE(&haudio_in_dfsdm_rightfilter);
  haudio_in_dfsdm_rightfilter.Instance                          = AUDIO_DFSDMx_RIGHT_FILTER;
  haudio_in_dfsdm_rightfilter.Init.RegularParam.Trigger         = DFSDM_FILTER_SYNC_TRIGGER;
  haudio_in_dfsdm_rightfilter.Init.RegularParam.FastMode        = ENABLE;
  haudio_in_dfsdm_rightfilter.Init.RegularParam.DmaMode         = ENABLE;
  haudio_in_dfsdm_rightfilter.Init.InjectedParam.Trigger        = DFSDM_FILTER_SW_TRIGGER;
  haudio_in_dfsdm_rightfilter.Init.InjectedParam.ScanMode       = DISABLE;
  haudio_in_dfsdm_rightfilter.Init.InjectedParam.DmaMode        = DISABLE;
  haudio_in_dfsdm_rightfilter.Init.InjectedParam.ExtTrigger     = DFSDM_FILTER_EXT_TRIG_TIM1_TRGO;
  haudio_in_dfsdm_rightfilter.Init.InjectedParam.ExtTriggerEdge = DFSDM_FILTER_EXT_TRIG_RISING_EDGE;
  haudio_in_dfsdm_rightfilter.Init.FilterParam.SincOrder        = DFSDM_FILTER_ORDER(AudioFreq);
  /* Set the DFSDM Filters Oversampling to have correct sample rate */
  haudio_in_dfsdm_rightfilter.Init.FilterParam.Oversampling     = DFSDM_OVER_SAMPLING(AudioFreq);
  haudio_in_dfsdm_rightfilter.Init.FilterParam.IntOversampling  = 1;
  if(HAL_OK != HAL_DFSDM_FilterInit(&haudio_in_dfsdm_rightfilter))
  {
    return AUDIO_ERROR;
  }
  /* Configure injected channel */
  if(HAL_OK != HAL_DFSDM_FilterConfigRegChannel(&haudio_in_dfsdm_rightfilter, DFSDM_CHANNEL_0, DFSDM_CONTINUOUS_CONV_ON))
  {
    return AUDIO_ERROR;
  }  
  
  return AUDIO_OK;
}

/**
  * @brief  De-initialize the Digital Filter for Sigma-Delta Modulators interface (DFSDM).
  * @retval AUDIO_OK if correct communication, else wrong communication
  */
static uint8_t DFSDMx_DeInit(void)
{
  /* De-initializes the DFSDM filters to allow access to DFSDM internal registers */
  if(HAL_OK != HAL_DFSDM_FilterDeInit(&haudio_in_dfsdm_leftfilter))
  {
    return AUDIO_ERROR;
  }

  if(HAL_OK != HAL_DFSDM_FilterDeInit(&haudio_in_dfsdm_rightfilter))
  {
    return AUDIO_ERROR;
  }

  /* De-initializes the DFSDM channels to allow access to DFSDM internal registers */
  if(HAL_OK != HAL_DFSDM_ChannelDeInit(&haudio_in_dfsdm_leftchannel))
  {
    return AUDIO_ERROR;
  }

  if(HAL_OK != HAL_DFSDM_ChannelDeInit(&haudio_in_dfsdm_rightchannel))
  {
    return AUDIO_ERROR;
  }

  return AUDIO_OK;
}

/**
  * @brief  Initialize the DFSDM channel MSP.
  * @retval None
  */
static void DFSDMx_ChannelMspInit(void)
{
  GPIO_InitTypeDef  GPIO_InitStruct;  

  /* Enable DFSDM clock */
  AUDIO_DFSDMx_CLK_ENABLE();
  
  /* Enable GPIO clock */
  AUDIO_DFSDMx_CKOUT_DMIC_DATIN_GPIO_CLK_ENABLE();
  
  /* DFSDM pins configuration: DFSDM_CKOUT, DMIC_DATIN pins ------------------*/
  GPIO_InitStruct.Pin = AUDIO_DFSDMx_CKOUT_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = AUDIO_DFSDMx_CKOUT_AF;
  HAL_GPIO_Init(AUDIO_DFSDMx_CKOUT_DMIC_DATIN_GPIO_PORT, &GPIO_InitStruct);
  
  GPIO_InitStruct.Pin = AUDIO_DFSDMx_DMIC_DATIN_PIN;
  GPIO_InitStruct.Alternate = AUDIO_DFSDMx_DMIC_DATIN_AF;
  HAL_GPIO_Init(AUDIO_DFSDMx_CKOUT_DMIC_DATIN_GPIO_PORT, &GPIO_InitStruct); 
}

/**
  * @brief  DeInitialize the DFSDM channel MSP.
  * @retval None
  */
static void DFSDMx_ChannelMspDeInit(void)
{
  GPIO_InitTypeDef  GPIO_InitStruct;  

  /* DFSDM pins configuration: DFSDM_CKOUT, DMIC_DATIN pins ------------------*/
  GPIO_InitStruct.Pin = AUDIO_DFSDMx_CKOUT_PIN | AUDIO_DFSDMx_DMIC_DATIN_PIN;
  HAL_GPIO_DeInit(AUDIO_DFSDMx_CKOUT_DMIC_DATIN_GPIO_PORT, GPIO_InitStruct.Pin);
}

/**
  * @brief  Initialize the DFSDM filter MSP.
  * @retval None
  */
static void DFSDMx_FilterMspInit(void)
{  
  /* Enable DFSDM clock */
  AUDIO_DFSDMx_CLK_ENABLE();
  
  /* Enable the DMA clock */
  AUDIO_DFSDMx_DMAx_CLK_ENABLE();
  
  hdma_dfsdm_left.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  hdma_dfsdm_left.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_dfsdm_left.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_dfsdm_left.Init.PeriphDataAlignment = AUDIO_DFSDMx_DMAx_PERIPH_DATA_SIZE;
  hdma_dfsdm_left.Init.MemDataAlignment    = AUDIO_DFSDMx_DMAx_MEM_DATA_SIZE;
  hdma_dfsdm_left.Init.Mode                = DMA_CIRCULAR;
  hdma_dfsdm_left.Init.Priority            = DMA_PRIORITY_HIGH;
  hdma_dfsdm_left.Instance               = DMA2_Stream0;
  hdma_dfsdm_left.Init.Channel           = AUDIO_DFSDMx_DMAx_LEFT_CHANNEL; 
  
  /* Associate the DMA handle */
  __HAL_LINKDMA(&haudio_in_dfsdm_leftfilter, hdmaReg, hdma_dfsdm_left);
  
  /* Reset DMA handle state */
  __HAL_DMA_RESET_HANDLE_STATE(&hdma_dfsdm_left);
  
  /* Configure the DMA Channel */
  HAL_DMA_Init(&hdma_dfsdm_left);      
  
  /* DMA IRQ Channel configuration */
  HAL_NVIC_SetPriority(AUDIO_DFSDMx_DMAx_LEFT_IRQ, AUDIO_IN_IRQ_PREPRIO, 0);
  HAL_NVIC_EnableIRQ(AUDIO_DFSDMx_DMAx_LEFT_IRQ);
  
  
  /* Configure the hdma_dfsdmReg handle parameters */ 
  hdma_dfsdm_right.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  hdma_dfsdm_right.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_dfsdm_right.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_dfsdm_right.Init.PeriphDataAlignment = AUDIO_DFSDMx_DMAx_PERIPH_DATA_SIZE;
  hdma_dfsdm_right.Init.MemDataAlignment    = AUDIO_DFSDMx_DMAx_MEM_DATA_SIZE;
  hdma_dfsdm_right.Init.Mode                = DMA_CIRCULAR;
  hdma_dfsdm_right.Init.Priority            = DMA_PRIORITY_HIGH;  
  hdma_dfsdm_right.Instance                 = DMA2_Stream5;
  hdma_dfsdm_right.Init.Channel             = AUDIO_DFSDMx_DMAx_RIGHT_CHANNEL;
  
  /* Associate the DMA handle */
  __HAL_LINKDMA(&haudio_in_dfsdm_rightfilter, hdmaReg, hdma_dfsdm_right);
  
  /* Reset DMA handle state */
  __HAL_DMA_RESET_HANDLE_STATE(&hdma_dfsdm_right);
  
  /* Configure the DMA Channel */
  HAL_DMA_Init(&hdma_dfsdm_right);      
  
  /* DMA IRQ Channel configuration */
  HAL_NVIC_SetPriority(AUDIO_DFSDMx_DMAx_RIGHT_IRQ, AUDIO_IN_IRQ_PREPRIO, 0);
  HAL_NVIC_EnableIRQ(AUDIO_DFSDMx_DMAx_RIGHT_IRQ);
}

/**
  * @brief  DeInitialize the DFSDM filter MSP.
  * @retval None
  */
static void DFSDMx_FilterMspDeInit(void)
{
  /* Configure the DMA Channel */
  HAL_DMA_DeInit(&hdma_dfsdm_left);
  HAL_DMA_DeInit(&hdma_dfsdm_right);  
}

/**
  * @}
  */ 
  
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */ 

