#include "main.h"
#include <string.h>
#include "stm32f3xx_hal.h"
#include "defines.h"

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_adc2;

COMP_HandleTypeDef hcomp2;
COMP_HandleTypeDef hcomp4;
COMP_HandleTypeDef hcomp6;

DAC_HandleTypeDef hdac1;
DAC_HandleTypeDef hdac2;

HRTIM_HandleTypeDef hhrtim1;

I2C_HandleTypeDef hi2c1;
DMA_HandleTypeDef hdma_i2c1_rx;
DMA_HandleTypeDef hdma_i2c1_tx;

TSC_HandleTypeDef htsc;
TSC_IOConfigTypeDef IoConfig;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

void SystemClock_Config(void);
static void GPIO_Init(void);
static void DMA_Init(void);
static void ADC1_Init(void);
static void ADC2_Init(void);
static void COMP2_Init(void);
static void COMP4_Init(void);
static void COMP6_Init(void);
static void HRTIM1_Init(void);
static void TSC_Init(void);
static void I2C1_Init(void);
static void USART1_UART_Init(void);
static void DAC1_Init(void);
static void DAC2_Init(void);
void HAL_HRTIM_MspPostInit(HRTIM_HandleTypeDef *hhrtim);
void configure_RT(uint8_t _register, uint8_t _mask);
static void init_RT(void);
static void start_HRTIM1(void);
uint16_t read_RT_ADC(void);
void set_pwm(uint8_t timer, float duty);
void boost_reg();

#if defined(SCOPE_CHANNELS)
void set_scope_channel(uint8_t ch, int16_t val);
void console_scope();
uint8_t uart_buf[(7 * SCOPE_CHANNELS) + 2];
volatile int16_t ch_buf[2 * SCOPE_CHANNELS];
#endif

__IO int32_t uhTSCAcquisitionValue[3];
__IO int32_t uhTSCOffsetValue[3];
uint8_t IdxBank = 0;
uint32_t ready = 0;

float targetCW = 0.0f;
float targetWW = 90.0f;
float Magiekonstante = 0.0005f;
float avgConst = 0.99;

float Vin,Vout;
float Temp1,Temp2;
float IoutCW,IoutWW;
float IavgCW,IavgWW;
float dutyCW = MIN_DUTY;
float dutyWW = MIN_DUTY;
float errorCW,errorWW;

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  set_pwm(HRTIM_TIMERINDEX_TIMER_D, 0);
  set_pwm(HRTIM_TIMERINDEX_TIMER_C, 0);

  GPIO_Init();
  DMA_Init();
  ADC1_Init();
  ADC2_Init();
  COMP2_Init();
  COMP4_Init();
  COMP6_Init();
  HRTIM1_Init();
  TSC_Init();
  I2C1_Init();
  USART1_UART_Init();
  DAC1_Init();
  DAC2_Init();

  HAL_COMP_Start(&hcomp2);
  HAL_COMP_Start(&hcomp4);
  HAL_COMP_Start(&hcomp6);

  HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);
  HAL_DAC_Start(&hdac2, DAC_CHANNEL_1);

  HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, FAULT_CURRENT);
  HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R, FAULT_VOLTAGE);

  init_RT();
  start_HRTIM1();

  HAL_GPIO_WritePin(GPIOA, LED1_Pin, 0);
  HAL_GPIO_WritePin(GPIOA, LED2_Pin, 0);
  HAL_GPIO_WritePin(GPIOA, LED3_Pin, 0);
  
  set_pwm(HRTIM_TIMERINDEX_TIMER_D, MIN_DUTY);
  set_pwm(HRTIM_TIMERINDEX_TIMER_C, MIN_DUTY);

  while (1)
  {
    for (int i = 0; i < 2000; i++) {

      IoutCW = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_2)/4096.0f*3.0f*1000.0f;
      IoutWW = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_3)/4096.0f*3.0f*1000.0f;

      IavgCW = IavgCW * avgConst + IoutCW * (1.0f-avgConst);
      IavgWW = IavgWW * avgConst + IoutWW * (1.0f-avgConst);

      errorCW = targetCW - IavgCW;
      errorWW = targetWW - IavgWW;

      dutyCW += (Magiekonstante * errorCW);
      dutyCW = CLAMP(dutyCW, MIN_DUTY, MAX_DUTY);

      dutyWW += (Magiekonstante * errorWW);
      dutyWW = CLAMP(dutyWW, MIN_DUTY, MAX_DUTY);

      set_pwm(HRTIM_TIMERINDEX_TIMER_D, dutyCW);
      set_pwm(HRTIM_TIMERINDEX_TIMER_C, dutyWW);
    }

      set_scope_channel(0,dutyWW*10.0f);
      set_scope_channel(1, targetWW); //VIN - mV
      set_scope_channel(2, errorWW); //NTC
      set_scope_channel(3, HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_3)/4096.0*3.0*1000.0); //ISens1 - mA
      set_scope_channel(4, IavgWW); //Isens2 - mA
      set_scope_channel(5, HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1)/2048.0*2.12*3.0*1000); //VBAT - mV
      console_scope();

      HAL_GPIO_TogglePin(GPIOA, LED1_Pin);

      /*
      set_scope_channel(0,duty);
      set_scope_channel(1, HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1)/2048.0*2.12*3.0*1000); //VIN - mV
      set_scope_channel(2, HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_1)); //NTC
      set_scope_channel(3, HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_2)/4096.0*3.0*1000.0); //ISens1 - mA
      set_scope_channel(4, HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_3)/4096.0*3.0*1000.0); //Isens2 - mA
      set_scope_channel(5, HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_4)/2048.0*2.12*3.0*1000); //VBAT - mV
      */
  }
}

void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_HRTIM1 | RCC_PERIPHCLK_USART1
                                       | RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.Hrtim1ClockSelection = RCC_HRTIM1CLK_PLLCLK;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

static void ADC1_Init(void)
{
  ADC_MultiModeTypeDef multimode;
  ADC_InjectionConfTypeDef InjectionConfig;

  hadc1.Instance = ADC1;

  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.NbrOfDiscConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  HAL_ADC_Init(&hadc1);

  multimode.DMAAccessMode = ADC_DMAACCESSMODE_DISABLED;
  multimode.Mode = ADC_MODE_INDEPENDENT;
  multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_1CYCLE;
  HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode);

  /* Discontinuous injected mode: 1st injected conversion for Vout on Ch11 */
  InjectionConfig.InjectedChannel = ADC_CHANNEL_12;
  InjectionConfig.InjectedRank = ADC_INJECTED_RANK_1;
  InjectionConfig.InjectedSamplingTime = ADC_SAMPLETIME_601CYCLES_5;
  InjectionConfig.InjectedSingleDiff = ADC_SINGLE_ENDED;
  InjectionConfig.InjectedOffsetNumber = ADC_OFFSET_NONE;
  InjectionConfig.InjectedOffset = 0;
  InjectionConfig.InjectedNbrOfConversion = 1;
  InjectionConfig.InjectedDiscontinuousConvMode = DISABLE;
  InjectionConfig.AutoInjectedConv = DISABLE;
  InjectionConfig.QueueInjectedContext = DISABLE;
  InjectionConfig.ExternalTrigInjecConv = ADC_EXTERNALTRIGINJECCONV_HRTIM_TRG2;
  InjectionConfig.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_RISING;
  HAL_ADCEx_InjectedConfigChannel(&hadc1, &InjectionConfig);

  /* Configure the 2nd injected conversion for Vin on Ch12 */
  //InjectionConfig.InjectedChannel = ADC_CHANNEL_12;
  //InjectionConfig.InjectedRank = ADC_INJECTED_RANK_2;
  //InjectionConfig.InjectedSamplingTime = ADC_SAMPLETIME_601CYCLES_5;
  //HAL_ADCEx_InjectedConfigChannel(&hadc1, &InjectionConfig);

  //InjectionConfig.InjectedChannel = ADC_CHANNEL_13;
  //InjectionConfig.InjectedRank = ADC_INJECTED_RANK_3;
  //InjectionConfig.InjectedSamplingTime = ADC_SAMPLETIME_601CYCLES_5;
  //HAL_ADCEx_InjectedConfigChannel(&hadc1, &InjectionConfig);

  /* Run the ADC calibration in single-ended mode */
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);

  /* Start ADC2 Injected Conversions */
  HAL_ADCEx_InjectedStart(&hadc1);

  /**Configure Regular Channel
  */
  /*
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = 1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig);
  */
}

static void ADC2_Init(void)
{
  ADC_MultiModeTypeDef MultiModeConfig;
  ADC_InjectionConfTypeDef InjectionConfig;

  hadc2.Instance = ADC2;

  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.ScanConvMode = ENABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.NbrOfDiscConversion = 1;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  HAL_ADC_Init(&hadc2);

  MultiModeConfig.DMAAccessMode = ADC_DMAACCESSMODE_DISABLED;
  MultiModeConfig.Mode = ADC_MODE_INDEPENDENT;
  MultiModeConfig.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_1CYCLE;
  HAL_ADCEx_MultiModeConfigChannel(&hadc2, &MultiModeConfig);

  /* Discontinuous injected mode: 1st injected conversion for Iout on Ch13 */
  InjectionConfig.InjectedChannel = ADC_CHANNEL_12;
  InjectionConfig.InjectedRank = ADC_INJECTED_RANK_1;
  InjectionConfig.InjectedSamplingTime = ADC_SAMPLETIME_601CYCLES_5;
  InjectionConfig.InjectedSingleDiff = ADC_SINGLE_ENDED;
  InjectionConfig.InjectedOffsetNumber = ADC_OFFSET_NONE;
  InjectionConfig.InjectedOffset = 0;
  InjectionConfig.InjectedNbrOfConversion = 4;
  InjectionConfig.InjectedDiscontinuousConvMode = DISABLE;
  InjectionConfig.AutoInjectedConv = DISABLE;
  InjectionConfig.QueueInjectedContext = DISABLE;
  InjectionConfig.ExternalTrigInjecConv = ADC_EXTERNALTRIGINJECCONV_HRTIM_TRG2;
  InjectionConfig.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_RISING;
  HAL_ADCEx_InjectedConfigChannel(&hadc2, &InjectionConfig);

  /* Configure the 2nd injected conversion for NTC1 on Ch14 */
  InjectionConfig.InjectedChannel = ADC_CHANNEL_1;
  InjectionConfig.InjectedRank = ADC_INJECTED_RANK_2;
  InjectionConfig.InjectedSamplingTime = ADC_SAMPLETIME_601CYCLES_5;
  HAL_ADCEx_InjectedConfigChannel(&hadc2, &InjectionConfig);

  InjectionConfig.InjectedChannel = ADC_CHANNEL_2;
  InjectionConfig.InjectedRank = ADC_INJECTED_RANK_3;
  InjectionConfig.InjectedSamplingTime = ADC_SAMPLETIME_601CYCLES_5;
  HAL_ADCEx_InjectedConfigChannel(&hadc2, &InjectionConfig);

  InjectionConfig.InjectedChannel = ADC_CHANNEL_3;
  InjectionConfig.InjectedRank = ADC_INJECTED_RANK_4;
  InjectionConfig.InjectedSamplingTime = ADC_SAMPLETIME_601CYCLES_5;
  HAL_ADCEx_InjectedConfigChannel(&hadc2, &InjectionConfig);

  /* Run the ADC calibration in single-ended mode */
  HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);

  /* Start ADC2 Injected Conversions */
  HAL_ADCEx_InjectedStart(&hadc2);
  /**Configure Regular Channel
  */
  /*
  sConfig.Channel = ADC_CHANNEL_12;
  sConfig.Rank = 1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig);
  */
}

static void COMP2_Init(void)
{

  hcomp2.Instance = COMP2;

  hcomp2.Init.InvertingInput = COMP_INVERTINGINPUT_DAC1_CH2;
  hcomp2.Init.NonInvertingInput = COMP_NONINVERTINGINPUT_IO1;
  hcomp2.Init.Output = HRTIM_FAULT_1;
  hcomp2.Init.OutputPol = COMP_OUTPUTPOL_INVERTED;
  hcomp2.Init.BlankingSrce = COMP_BLANKINGSRCE_NONE;
  hcomp2.Init.TriggerMode = COMP_TRIGGERMODE_NONE;
  HAL_COMP_Init(&hcomp2);
}

static void COMP4_Init(void)
{

  hcomp4.Instance = COMP4;

  hcomp4.Init.InvertingInput = COMP_INVERTINGINPUT_DAC1_CH2;
  hcomp4.Init.NonInvertingInput = COMP_NONINVERTINGINPUT_IO1;
  hcomp4.Init.Output = HRTIM_FAULT_1;
  hcomp4.Init.OutputPol = COMP_OUTPUTPOL_INVERTED;
  hcomp4.Init.BlankingSrce = COMP_BLANKINGSRCE_NONE;
  hcomp4.Init.TriggerMode = COMP_TRIGGERMODE_NONE;
  HAL_COMP_Init(&hcomp4);
}

static void COMP6_Init(void)
{

  hcomp6.Instance = COMP6;

  hcomp6.Init.InvertingInput = COMP_INVERTINGINPUT_DAC2_CH1;
  hcomp6.Init.NonInvertingInput = COMP_NONINVERTINGINPUT_IO1;
  hcomp6.Init.Output = HRTIM_FAULT_1;
  hcomp6.Init.OutputPol = COMP_OUTPUTPOL_INVERTED;
  hcomp6.Init.BlankingSrce = COMP_BLANKINGSRCE_NONE;
  hcomp6.Init.TriggerMode = COMP_TRIGGERMODE_NONE;
  HAL_COMP_Init(&hcomp6);
}

static void DAC1_Init(void)
{

  DAC_ChannelConfTypeDef sConfig;

  hdac1.Instance = DAC1;
  HAL_DAC_Init(&hdac1);

  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputSwitch = DAC_OUTPUTSWITCH_DISABLE;
  HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_2);
}

static void DAC2_Init(void)
{

  DAC_ChannelConfTypeDef sConfig;

  hdac2.Instance = DAC2;
  HAL_DAC_Init(&hdac2);

  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputSwitch = DAC_OUTPUTSWITCH_DISABLE;
  HAL_DAC_ConfigChannel(&hdac2, &sConfig, DAC_CHANNEL_1);
}

static void HRTIM1_Init(void)
{

  HRTIM_FaultCfgTypeDef pFaultCfg;
  HRTIM_TimeBaseCfgTypeDef pTimeBaseCfg;
  HRTIM_TimerCfgTypeDef pTimerCfg;
  HRTIM_OutputCfgTypeDef pOutputCfg;
  HRTIM_ADCTriggerCfgTypeDef adc_trigger_config;
  HRTIM_CompareCfgTypeDef compare_config;

  hhrtim1.Instance = HRTIM1;

  hhrtim1.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
  hhrtim1.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
  HAL_HRTIM_Init(&hhrtim1);

  HAL_HRTIM_FaultPrescalerConfig(&hhrtim1, HRTIM_FAULTPRESCALER_DIV1);

  pFaultCfg.Source = HRTIM_FAULTSOURCE_INTERNAL;
  pFaultCfg.Polarity = HRTIM_FAULTPOLARITY_LOW;
  pFaultCfg.Filter = HRTIM_FAULTFILTER_NONE;
  pFaultCfg.Lock = HRTIM_FAULTLOCK_READWRITE;
  HAL_HRTIM_FaultConfig(&hhrtim1, HRTIM_FAULT_1, &pFaultCfg);

  HAL_HRTIM_FaultModeCtl(&hhrtim1, HRTIM_FAULT_1, HRTIM_FAULTMODECTL_ENABLED);

  pTimeBaseCfg.Period = HRTIM_PERIOD;
  pTimeBaseCfg.RepetitionCounter = 127;
  pTimeBaseCfg.PrescalerRatio = HRTIM_PRESCALERRATIO_MUL32;
  pTimeBaseCfg.Mode = HRTIM_MODE_CONTINUOUS;
  HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_MASTER, &pTimeBaseCfg);

  pTimerCfg.DMARequests = HRTIM_MASTER_DMA_NONE;
  pTimerCfg.DMASrcAddress = 0x0000;
  pTimerCfg.DMADstAddress = 0x0000;
  pTimerCfg.DMASize = 0x0;
  pTimerCfg.HalfModeEnable = HRTIM_HALFMODE_DISABLED;
  pTimerCfg.StartOnSync = HRTIM_SYNCSTART_DISABLED;
  pTimerCfg.ResetOnSync = HRTIM_SYNCRESET_DISABLED;
  pTimerCfg.DACSynchro = HRTIM_DACSYNC_NONE;
  pTimerCfg.PreloadEnable = HRTIM_PRELOAD_ENABLED;
  pTimerCfg.UpdateGating = HRTIM_UPDATEGATING_INDEPENDENT;
  pTimerCfg.BurstMode = HRTIM_TIMERBURSTMODE_MAINTAINCLOCK;
  pTimerCfg.RepetitionUpdate = HRTIM_UPDATEONREPETITION_ENABLED;
  pTimerCfg.ResetUpdate = HRTIM_TIMUPDATEONRESET_DISABLED;
  pTimerCfg.InterruptRequests = HRTIM_TIM_IT_REP;
  pTimerCfg.PushPull = HRTIM_TIMPUSHPULLMODE_DISABLED;
  pTimerCfg.FaultEnable = HRTIM_TIMFAULTENABLE_FAULT1;
  pTimerCfg.FaultLock = HRTIM_TIMFAULTLOCK_READWRITE;
  pTimerCfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_ENABLED;
  pTimerCfg.DelayedProtectionMode = HRTIM_TIMER_A_B_C_DELAYEDPROTECTION_DISABLED;
  pTimerCfg.DelayedProtectionMode |= HRTIM_TIMER_D_E_DELAYEDPROTECTION_DISABLED;
  pTimerCfg.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_NONE;
  pTimerCfg.ResetTrigger = HRTIM_TIMRESETTRIGGER_NONE;

  HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, &pTimerCfg);

  HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, &pTimerCfg);

  pOutputCfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;
  pOutputCfg.SetSource = HRTIM_OUTPUTSET_TIMPER;
  pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP1;
  pOutputCfg.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
  pOutputCfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
  pOutputCfg.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_NONE;
  pOutputCfg.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
  pOutputCfg.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;

  HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, HRTIM_OUTPUT_TC1, &pOutputCfg);

  HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_OUTPUT_TD1, &pOutputCfg);

  HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, HRTIM_OUTPUT_TC2, &pOutputCfg);

  HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, HRTIM_OUTPUT_TD2, &pOutputCfg);

  compare_config.AutoDelayedMode = HRTIM_AUTODELAYEDMODE_REGULAR;
  compare_config.AutoDelayedTimeout = 0;
  compare_config.CompareValue = HRTIM_PERIOD / 10 * 8.5; /* Samples in middle of ON time */
  HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, HRTIM_COMPAREUNIT_2, &compare_config);

  adc_trigger_config.Trigger = HRTIM_ADCTRIGGEREVENT24_TIMERC_CMP2;
  adc_trigger_config.UpdateSource = HRTIM_ADCTRIGGERUPDATE_TIMER_C;
  HAL_HRTIM_ADCTriggerConfig(&hhrtim1, HRTIM_ADCTRIGGER_2, &adc_trigger_config);

  HAL_HRTIM_ADCTriggerConfig(&hhrtim1, HRTIM_ADCTRIGGER_1, &adc_trigger_config);

  HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, &pTimeBaseCfg);

  HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D, &pTimeBaseCfg);

  HAL_HRTIM_MspPostInit(&hhrtim1);

}

static void I2C1_Init(void)
{

  hi2c1.Instance = I2C1;

  hi2c1.Init.Timing = 0x2000090E;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  HAL_I2C_Init(&hi2c1);

  HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);

  HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0);

}

static void TSC_Init(void)
{

  htsc.Instance = TSC;

  htsc.Init.CTPulseHighLength = TSC_CTPH_1CYCLE;
  htsc.Init.CTPulseLowLength = TSC_CTPL_1CYCLE;
  htsc.Init.SpreadSpectrum = DISABLE;
  htsc.Init.SpreadSpectrumDeviation = 1;
  htsc.Init.SpreadSpectrumPrescaler = TSC_SS_PRESC_DIV1;
  htsc.Init.PulseGeneratorPrescaler = TSC_PG_PRESC_DIV64;
  htsc.Init.MaxCountValue = TSC_MCV_8191;
  htsc.Init.IODefaultMode = TSC_IODEF_OUT_PP_LOW;
  htsc.Init.SynchroPinPolarity = TSC_SYNC_POLARITY_FALLING;
  htsc.Init.AcquisitionMode = TSC_ACQ_MODE_NORMAL;
  htsc.Init.MaxCountInterrupt = DISABLE;
  htsc.Init.ChannelIOs = 0;//TSC_GROUP1_IO2|TSC_GROUP1_IO3|TSC_GROUP1_IO4|TSC_GROUP5_IO2
  //  |TSC_GROUP5_IO3|TSC_GROUP5_IO4;
  htsc.Init.SamplingIOs = 0;//TSC_GROUP1_IO1|TSC_GROUP5_IO1;
  HAL_TSC_Init(&htsc);

  IoConfig.ChannelIOs  = TSC_GROUP1_IO1; /* Start with the first channel */
  IoConfig.SamplingIOs = TSC_GROUP1_IO4;
  IoConfig.ShieldIOs   = 0;
  HAL_TSC_IOConfig(&htsc, &IoConfig);

  IoConfig.ChannelIOs  = TSC_GROUP5_IO1; /* Start with the first channel */
  IoConfig.SamplingIOs = TSC_GROUP5_IO4;
  IoConfig.ShieldIOs   = 0;
  HAL_TSC_IOConfig(&htsc, &IoConfig);
}

static void USART1_UART_Init(void)
{

  huart1.Instance = USART1;

  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  HAL_UART_Init(&huart1);
}

static void DMA_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();

  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 5, 6);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 5, 7);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
}

static void GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, LED1_Pin | LED2_Pin | LED3_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = LED1_Pin | LED2_Pin | LED3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void start_HRTIM1(void) {
  __HAL_HRTIM_ENABLE(&hhrtim1, HRTIM_TIMERID_MASTER);
  __HAL_HRTIM_ENABLE(&hhrtim1, HRTIM_TIMERID_TIMER_C);
  __HAL_HRTIM_ENABLE(&hhrtim1, HRTIM_TIMERID_TIMER_D);

  HRTIM1->sCommonRegs.OENR = HRTIM_OENR_TD1OEN;
  HRTIM1->sCommonRegs.OENR = HRTIM_OENR_TD2OEN;
  HRTIM1->sCommonRegs.OENR = HRTIM_OENR_TC1OEN;
  HRTIM1->sCommonRegs.OENR = HRTIM_OENR_TC2OEN;
}

static void init_RT(void) {
  configure_RT(CHG_CTRL2, IINLIM_MASK);
  configure_RT(CHG_CTRL3, SET_ILIM_3A);
}

void boost_reg() {
  HAL_GPIO_TogglePin(GPIOA, LED2_Pin);
}

void configure_RT(uint8_t _register, uint8_t _mask) {
  uint8_t _tmp_data[2] = {_register, _mask};
  HAL_I2C_Master_Transmit(&hi2c1, RT_ADDRESS, _tmp_data, sizeof(_tmp_data), 500);
}

uint16_t read_RT_ADC(void) {
  uint8_t _ADC_H, _ADC_L;
  uint8_t _tmp_data_H = ADC_DATA_H;
  uint8_t _tmp_data_L = ADC_DATA_L;

  HAL_I2C_Master_Transmit(&hi2c1, RT_ADDRESS, &_tmp_data_H, sizeof(_tmp_data_H), 500);
  HAL_I2C_Master_Receive(&hi2c1, RT_ADDRESS, &_ADC_H, 1, 500);
  HAL_I2C_Master_Transmit(&hi2c1, RT_ADDRESS, &_tmp_data_L, sizeof(_tmp_data_L), 500);
  HAL_I2C_Master_Receive(&hi2c1, RT_ADDRESS, &_ADC_L, 1, 500);

  uint16_t _tmp_data = ((_ADC_H << 8) | (_ADC_L & 0xFF));
  return _tmp_data;
}

#if defined(SCOPE_CHANNELS)
void set_scope_channel(uint8_t ch, int16_t val) {
  ch_buf[ch] = val;
}

void console_scope(void) {
  memset(uart_buf, 0, sizeof(uart_buf));

#if (SCOPE_CHANNELS == 1)
  sprintf((char*)uart_buf, "%i\n\r", ch_buf[0]);
#elif (SCOPE_CHANNELS == 2)
  sprintf((char*)uart_buf, "%i\t%i\n\r", ch_buf[0], ch_buf[1]);
#elif (SCOPE_CHANNELS == 3)
  sprintf((char*)uart_buf, "%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2]);
#elif (SCOPE_CHANNELS == 4)
  sprintf((char*)uart_buf, "%i\t%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3]);
#elif (SCOPE_CHANNELS == 5)
  sprintf((char*)uart_buf, "%i\t%i\t%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3], ch_buf[4]);
#elif (SCOPE_CHANNELS == 6)
  sprintf((char*)uart_buf, "%i\t%i\t%i\t%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3], ch_buf[4], ch_buf[5]);
#elif (SCOPE_CHANNELS == 7)
  sprintf((char*)uart_buf, "%i\t%i\t%i\t%i\t%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3], ch_buf[4], ch_buf[5], ch_buf[6]);
#elif (SCOPE_CHANNELS == 8)
  sprintf((char*)uart_buf, "%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\n\r", ch_buf[0], ch_buf[1], ch_buf[2], ch_buf[3], ch_buf[4], ch_buf[5], ch_buf[6], ch_buf[7]);
#endif

  HAL_UART_Transmit_DMA(&huart1, (uint8_t *)uart_buf, strlen((char*)uart_buf));
  huart1.gState = HAL_UART_STATE_READY;
}
#endif

void primitive_TSC_task(void) {

  switch (IdxBank)
  {
  case 0:
    IoConfig.ChannelIOs = TSC_GROUP1_IO1; /* Second channel */
    IdxBank = 1;
    break;
  case 1:
    IoConfig.ChannelIOs = TSC_GROUP1_IO2; /* Third channel */
    IdxBank = 2;
    break;
  case 2:
    IoConfig.ChannelIOs = TSC_GROUP1_IO3; /* First channel */
    IdxBank = 0;/* TSC init function */

    break;
  default:
    break;
  }

  HAL_TSC_IOConfig(&htsc, &IoConfig);

  /*##-2- Discharge the touch-sensing IOs ##################################*/
  /* Must be done before each acquisition */
  HAL_TSC_IODischarge(&htsc, ENABLE);
  HAL_Delay(1); /* 1 ms is more than enough to discharge all capacitors */

  /*##-3- Start the acquisition process #####HAL_TSC_GroupGetValue###############################*/
  HAL_TSC_Start(&htsc);

  /*##-4- Wait for the end of acquisition ##################################*/
  /*  Before starting a new acquisition, you need to check the current state of
       the peripheral; if its busy you need to wait for the end of current
       acquisition before starting a new one. */
  while (HAL_TSC_GetState(&htsc) == HAL_TSC_STATE_BUSY)
  {
    /* For simplicity reasons, this example is just waiting till the end of the
       acquisition, but application may perform other tasks while acquisition
       operation is ongoing. */
  }

  /*##-5- Clear flags ######################################################*/
  __HAL_TSC_CLEAR_FLAG(&htsc, (TSC_FLAG_EOA | TSC_FLAG_MCE));

  /*##-6- Check if the acquisition is correct (no max count) ###############*/
  if (HAL_TSC_GroupGetStatus(&htsc, TSC_GROUP1_IDX) == TSC_GROUP_COMPLETED)
  {
    /*##-7- Store the acquisition value ####################################*/
    uhTSCAcquisitionValue[IdxBank] = HAL_TSC_GroupGetValue(&htsc, TSC_GROUP1_IDX);

    if (ready < 300) {
      uhTSCOffsetValue[IdxBank] =  uhTSCOffsetValue[IdxBank] * 0.9f + uhTSCAcquisitionValue[IdxBank] * 0.1f;
      ready++;
    } else {
      uhTSCAcquisitionValue[IdxBank] = uhTSCAcquisitionValue[IdxBank] - uhTSCOffsetValue[IdxBank]; // uhTSCOffsetValue[IdxBank] - uhTSCAcquisitionValue[IdxBank];
      if (IdxBank == 2) {
        HAL_GPIO_WritePin(GPIOA, LED3_Pin, 1);
        uhTSCAcquisitionValue[IdxBank] = uhTSCAcquisitionValue[IdxBank] * 2;
      }

      uhTSCAcquisitionValue[IdxBank] = CLAMP(uhTSCAcquisitionValue[IdxBank], -2000, 0);

      int16_t z = ((uhTSCAcquisitionValue[0] + uhTSCAcquisitionValue[1]) / 2) - uhTSCAcquisitionValue[2];
      int16_t x = ((uhTSCAcquisitionValue[0] + uhTSCAcquisitionValue[2]) / 2) - uhTSCAcquisitionValue[1];
      int16_t y = ((uhTSCAcquisitionValue[1] + uhTSCAcquisitionValue[2]) / 2) - uhTSCAcquisitionValue[0];


      uint16_t distance = 0;
      uint8_t section = 0;
      if (x < y && x < z && y < z) {
        section = 1;
        distance = 2 * TOUCH_SCALE - ((z * TOUCH_SCALE) / (y + z));
      } else if (x < y && x < z && y > z) {
        section = 2;
        distance = ((y * TOUCH_SCALE) / (y + z)) + TOUCH_SCALE;
      } else if (z < y && z < x && x < y) {
        section = 3;
        distance = 5 * TOUCH_SCALE - ((y * TOUCH_SCALE) / (y + x));
      } else if (z < y && z < x && x > y) {
        section = 4;
        distance = ((x * TOUCH_SCALE) / (y + x)) + 4 * TOUCH_SCALE;
      } else if (y < x && y < z && z < x) {
        section = 5;
        distance = 8 * TOUCH_SCALE - ((x * TOUCH_SCALE) / (x + z));
      } else if (y < x && y < z && z > x) {
        section = 6;
        distance = ((z * TOUCH_SCALE) / (x + z)) + 7 * TOUCH_SCALE;
      }
      if (MIN(MIN(uhTSCAcquisitionValue[0], uhTSCAcquisitionValue[1]), uhTSCAcquisitionValue[2]) > -100) {
        distance = 0;
        section = 0;
        HAL_GPIO_WritePin(GPIOA, LED1_Pin, 0);
      } else {
        HAL_GPIO_WritePin(GPIOA, LED1_Pin, 1);
      }
      set_scope_channel(1, (uint16_t)section);
      set_scope_channel(0, (uint16_t)distance / 1.43f);
      //setScopeChannel(2, read_RT_ADC());
      //configure_RT(CHG_ADC,0x11);
      //setScopeChannel(2, y);
      //setScopeChannel(3, MIN(MIN(uhTSCAcquisitionValue[0], uhTSCAcquisitionValue[1]), uhTSCAcquisitionValue[2]));
      console_scope();
    }
  }
}


void set_pwm(uint8_t timer, float duty) {

  if (duty < MIN_DUTY) duty = MIN_DUTY;
  if (duty > MAX_DUTY) duty = MAX_DUTY;

  HRTIM1->sTimerxRegs[timer].CMP1xR = HRTIM_PERIOD / 1000 * (duty * 10);
  HRTIM1->sTimerxRegs[timer].CMP2xR = HRTIM_PERIOD - (HRTIM_PERIOD / 1000 * (duty * 10));
  HRTIM1->sTimerxRegs[timer].SETx1R = HRTIM_SET1R_PER;
  HRTIM1->sTimerxRegs[timer].RSTx1R = HRTIM_RST1R_CMP1;
  HRTIM1->sTimerxRegs[timer].SETx2R = HRTIM_SET2R_CMP2;
  HRTIM1->sTimerxRegs[timer].RSTx2R = HRTIM_RST2R_PER;
}

void _Error_Handler(char * file, int line)
{
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t* file, uint32_t line)
{

}

#endif
