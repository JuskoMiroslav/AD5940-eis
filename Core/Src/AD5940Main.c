/*!
 *****************************************************************************
 @file:    AD5940Main.c
 @author:  Neo Xu
 @brief:   Electrochemical impedance spectroscopy based on example
 AD5940_Impedance This project is optomized for 3-lead electrochemical sensors
 that typically have an impedance <200ohm. For optimum performance RCAL should
 be close to impedance of the sensor.
 -----------------------------------------------------------------------------

 Copyright (c) 2017-2019 Analog Devices, Inc. All Rights Reserved.

 This software is proprietary to Analog Devices, Inc. and its licensors.
 By using this software you agree to the terms of the associated
 Analog Devices Software License Agreement.

 *****************************************************************************/
#include "AD5940.H"
#include "AD5940.h"
#include "Impedance.h"
#include <stddef.h>

/* Function declaration */
void send_packet(const char *type, const char *data);

#define APPBUFF_SIZE 512
uint32_t AppBuff[APPBUFF_SIZE];

#define TYPE_FLOAT 0
#define TYPE_INT32 1
#define TYPE_UINT32 2
#define TYPE_UINT8 3
#define TYPE_BOOL 4

#define CNFLIST_SIZE 35

#define OFFSET_SWEEP(member)                                                   \
  (offsetof(AppIMPCfg_Type, SweepCfg) + offsetof(SoftSweepCfg_Type, member))

typedef struct {
  const char *cmd_name;
  uint8_t value_type;
  size_t offset;
} configcommand_list_t;

void print_param(AppIMPCfg_Type *cfg, configcommand_list_t *item);

static AD5940Err AppADCPgaCal(void);
static AD5940Err AppHSDACCal(void);

configcommand_list_t config_cmd_list[CNFLIST_SIZE] = {
    /* Core measurement parameters */
    {"IsRunning", TYPE_BOOL, offsetof(AppIMPCfg_Type, bIsRunning)},
    {"ImpODR", TYPE_FLOAT, offsetof(AppIMPCfg_Type, ImpODR)},
    {"NumOfData", TYPE_INT32, offsetof(AppIMPCfg_Type, NumOfData)},
    {"RcalVal", TYPE_FLOAT, offsetof(AppIMPCfg_Type, RcalVal)},
    {"SinFreq", TYPE_FLOAT, offsetof(AppIMPCfg_Type, SinFreq)},
    {"DacVoltPP", TYPE_FLOAT, offsetof(AppIMPCfg_Type, DacVoltPP)},
    {"BiasVolt", TYPE_FLOAT, offsetof(AppIMPCfg_Type, BiasVolt)},
    /* Clock / timing */
    {"WuptClkFreq", TYPE_FLOAT, offsetof(AppIMPCfg_Type, WuptClkFreq)},
    {"SysClkFreq", TYPE_FLOAT, offsetof(AppIMPCfg_Type, SysClkFreq)},
    {"AdcClkFreq", TYPE_FLOAT, offsetof(AppIMPCfg_Type, AdcClkFreq)},

    /* DFT / signal processing */
    {"DftNum", TYPE_UINT32, offsetof(AppIMPCfg_Type, DftNum)},
    {"DftSrc", TYPE_UINT32, offsetof(AppIMPCfg_Type, DftSrc)},
    {"HanWinEn", TYPE_BOOL, offsetof(AppIMPCfg_Type, HanWinEn)},

    /* ADC configuration */
    {"AdcPgaGain", TYPE_UINT32, offsetof(AppIMPCfg_Type, AdcPgaGain)},
    {"ADCSinc3Osr", TYPE_UINT8, offsetof(AppIMPCfg_Type, ADCSinc3Osr)},
    {"ADCSinc2Osr", TYPE_UINT8, offsetof(AppIMPCfg_Type, ADCSinc2Osr)},
    {"ADCAvgNum", TYPE_UINT8, offsetof(AppIMPCfg_Type, ADCAvgNum)},
    {"ADC_Rate", TYPE_UINT8, offsetof(AppIMPCfg_Type, ADC_Rate)},

    /* Analog front-end (important for impedance) */
    {"HstiaRtiaSel", TYPE_UINT32, offsetof(AppIMPCfg_Type, HstiaRtiaSel)},
    {"ExcitBufGain", TYPE_UINT32, offsetof(AppIMPCfg_Type, ExcitBufGain)},
    {"HsDacGain", TYPE_UINT32, offsetof(AppIMPCfg_Type, HsDacGain)},
    {"HsDacRate", TYPE_UINT32, offsetof(AppIMPCfg_Type, HsDacUpdateRate)},

    /* Low power path */
    {"LptiaRtiaSel", TYPE_UINT32, offsetof(AppIMPCfg_Type, LptiaRtiaSel)},
    {"LpTiaRf", TYPE_UINT32, offsetof(AppIMPCfg_Type, LpTiaRf)},
    {"LpTiaRl", TYPE_UINT32, offsetof(AppIMPCfg_Type, LpTiaRl)},
    {"Vzero", TYPE_FLOAT, offsetof(AppIMPCfg_Type, Vzero)},
    {"Vbias", TYPE_FLOAT, offsetof(AppIMPCfg_Type, Vbias)},

    /* System / control */
    {"PwrMod", TYPE_UINT32, offsetof(AppIMPCfg_Type, PwrMod)},
    {"FifoThresh", TYPE_UINT32, offsetof(AppIMPCfg_Type, FifoThresh)},

    /* Software Controlled Sweep Function*/
    {"SweepEn", TYPE_BOOL, OFFSET_SWEEP(SweepEn)},
    {"SweepStart", TYPE_FLOAT, OFFSET_SWEEP(SweepStart)},
    {"SweepStop", TYPE_FLOAT, OFFSET_SWEEP(SweepStop)},
    {"SweepPoints", TYPE_UINT32, OFFSET_SWEEP(SweepPoints)},
    {"SweepLog", TYPE_BOOL, OFFSET_SWEEP(SweepLog)},
    {"SweepIndex", TYPE_UINT32, OFFSET_SWEEP(SweepIndex)},

    ///* Status / debug */
    //{ "SweepCurrFreq", TYPE_FLOAT, offsetof(AppIMPCfg_Type, SweepCurrFreq) },
    //{ "SweepNextFreq", TYPE_FLOAT, offsetof(AppIMPCfg_Type, SweepNextFreq) },
    //{ "FreqofData", TYPE_FLOAT, offsetof(AppIMPCfg_Type, FreqofData) },
    //{ "FifoCount", TYPE_UINT32, offsetof(AppIMPCfg_Type, FifoDataCount) },
    //{ "IMPInited", TYPE_BOOL, offsetof(AppIMPCfg_Type, IMPInited) },
};

int32_t ImpedanceShowResult(uint32_t *pData, uint32_t DataCount) {
  float freq;
  char data[200];

  fImpPol_Type *pImp = (fImpPol_Type *)pData;

  if (isRunning() == bTRUE) {
    AppIMPCtrl(IMPCTRL_GETFREQ, &freq);

    for (int i = 0; i < DataCount; i++) {

      snprintf(data, sizeof(data), "%.2f,%.6f,%.6f", freq, pImp[i].Magnitude,
               pImp[i].Phase * 180 / MATH_PI);

      send_packet("IMP", data);
    }
  }
  return 0;
}

static int32_t AD5940PlatformCfg(void) {
  CLKCfg_Type clk_cfg;
  FIFOCfg_Type fifo_cfg;
  AGPIOCfg_Type gpio_cfg;

  /* Use hardware reset */
  AD5940_HWReset();
  AD5940_Initialize();
  /* Platform configuration */
  /* Step1. Configure clock */
  clk_cfg.ADCClkDiv = ADCCLKDIV_1;
  clk_cfg.ADCCLkSrc = ADCCLKSRC_XTAL;
  clk_cfg.SysClkDiv = SYSCLKDIV_1;
  clk_cfg.SysClkSrc = SYSCLKSRC_XTAL;
  clk_cfg.HfOSC32MHzMode = bFALSE;
  clk_cfg.HFOSCEn = bFALSE;
  clk_cfg.HFXTALEn = bTRUE;
  clk_cfg.LFOSCEn = bTRUE;
  AD5940_CLKCfg(&clk_cfg);
  /* Step2. Configure FIFO and Sequencer*/
  fifo_cfg.FIFOEn = bFALSE;
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize =
      FIFOSIZE_4KB; /* 4kB for FIFO, The reset 2kB for sequencer */
  fifo_cfg.FIFOSrc = FIFOSRC_DFT;
  fifo_cfg.FIFOThresh = 6;
  AD5940_FIFOCfg(&fifo_cfg);
  fifo_cfg.FIFOEn = bTRUE;
  AD5940_FIFOCfg(&fifo_cfg);

  /* Step3. Interrupt controller */
  AD5940_INTCCfg(
      AFEINTC_1, AFEINTSRC_ALLINT,
      bTRUE); /* Enable all interrupt in INTC1, so we can check INTC flags */
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  AD5940_INTCCfg(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH, bTRUE);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);
  /* Step4: Reconfigure GPIO */
  gpio_cfg.FuncSet = GP0_INT | GP1_SLEEP | GP2_SYNC;
  gpio_cfg.InputEnSet = 0;
  gpio_cfg.OutputEnSet = AGPIO_Pin0 | AGPIO_Pin1 | AGPIO_Pin2;
  gpio_cfg.OutVal = 0;
  gpio_cfg.PullEnSet = 0;
  AD5940_AGPIOCfg(&gpio_cfg);
  AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK); /* Allow AFE to enter sleep mode. */
  return 0;
}

void AD5940ImpedanceStructInit(void) {
  AppIMPCfg_Type *pImpedanceCfg;

  AppIMPGetCfg(&pImpedanceCfg);
  /* Step1: configure initialization sequence Info */
  pImpedanceCfg->SeqStartAddr = 0;
  pImpedanceCfg->MaxSeqLen = 512; /* @todo add checker in function */

  pImpedanceCfg->RcalVal = 10.0;
  pImpedanceCfg->FifoThresh = 6;
  pImpedanceCfg->SinFreq = 10000.0;

  /* Configure Excitation Waveform
   *
   *	 Output waveform = DacVoltPP * ExcitBufGain * HsDacGain
   *
   *		= 300 * 0.25 * 0.2 = 15mV pk-pk
   *
   */
  pImpedanceCfg->DacVoltPP = 500; /* Maximum value is 600mV*/
  pImpedanceCfg->ExcitBufGain = EXCITBUFGAIN_0P25;
  pImpedanceCfg->HsDacGain = HSDACGAIN_0P2;
  pImpedanceCfg->ADCRefVolt =
      1.82; /* ADC reference voltage in V, used for gain calculation in PGA
               calibration. Should be set according to actual hardware design.
             */

  /* Set switch matrix to onboard(EVAL-AD5940ELECZ) gas sensor. */
  pImpedanceCfg->DswitchSel = SWD_CE0;
  pImpedanceCfg->PswitchSel = SWP_RE0;
  pImpedanceCfg->NswitchSel = SWN_SE0LOAD;
  pImpedanceCfg->TswitchSel = SWT_SE0LOAD;
  /* The dummy sensor is as low as 5kOhm. We need to make sure RTIA is small
   * enough that HSTIA won't be saturated. */
  pImpedanceCfg->HstiaRtiaSel = HSTIARTIA_200;
  pImpedanceCfg->BiasVolt = 0.0;
  /* Configure the sweep function. */
  pImpedanceCfg->SweepCfg.SweepEn = bFALSE;
  pImpedanceCfg->SweepCfg.SweepStart = 100.0f; /* Start from 1kHz */
  pImpedanceCfg->SweepCfg.SweepStop = 200e3f;  /* Stop at 100kHz */
  pImpedanceCfg->SweepCfg.SweepPoints = 101;   /* Points is 101 */
  pImpedanceCfg->SweepCfg.SweepLog = bTRUE;
  /* Configure Power Mode. Use HP mode if frequency is higher than 80kHz. */
  pImpedanceCfg->PwrMod = AFEPWR_HP;
  /* Configure filters if necessary */
  pImpedanceCfg->ADCSinc3Osr =
      ADCSINC3OSR_2; /* Sample rate is 800kSPS/OSR = 400kSPS */
  pImpedanceCfg->ADCSinc2Osr = ADCSINC2OSR_22;
  pImpedanceCfg->DftNum = DFTNUM_8192;
  pImpedanceCfg->DftSrc = DFTSRC_SINC3;
}

void AD5940_Main(void) {
  uint32_t temp;
  AD5940PlatformCfg();
  AD5940ImpedanceStructInit();

  AppADCPgaCal();          /* Kalibrácia referencie a zisku ADC */
  AppHSDACCal();		 /* Kalibrácia zisku HSDAC */

  AppIMPInit(AppBuff,
             APPBUFF_SIZE); /* Initialize IMP application. Provide a buffer,
                               which is used to store sequencer commands */
  AppIMPCtrl(IMPCTRL_START,
             0); /* Control IMP measurement to start. Second parameter has no
                    meaning with this command. */
  while (1) {
    if (AD5940_GetMCUIntFlag()) {
      AD5940_ClrMCUIntFlag();
      temp = APPBUFF_SIZE;
      AppIMPISR(AppBuff, &temp);
      ImpedanceShowResult(AppBuff, temp);
    }
  }
}

uint32_t command_get_cfg(char *param1_str, double para2) {
  AppIMPCfg_Type *cfg;
  AppIMPGetCfg(&cfg);

  // if no parameter → print all
  if (param1_str == NULL || *param1_str == '\0') {
    for (int i = 0; i < CNFLIST_SIZE; i++) {
      print_param(cfg, &config_cmd_list[i]);
    }
    return 0;
  }

  // search for specific parameter
  for (int i = 0; i < CNFLIST_SIZE; i++) {
    if (strcmp(param1_str, config_cmd_list[i].cmd_name) == 0) {
      print_param(cfg, &config_cmd_list[i]);
      return 0;
    }
  }

  printf("Parameter not found\r\n");
  return 1;
}
uint32_t command_set_cfg(char *param1_str, double para2) {
  AppIMPCfg_Type *pImpedanceCfg;

  AppIMPGetCfg(&pImpedanceCfg);
  for (int i = 0; i < CNFLIST_SIZE; i++) {
    if (strcmp(param1_str, config_cmd_list[i].cmd_name) == 0) {
      AppIMPCtrl(IMPCTRL_SHUTDOWN, 0);
      configcommand_list_t *item = &config_cmd_list[i];
      void *field_ptr = (uint8_t *)pImpedanceCfg + item->offset;
      switch (item->value_type) {
      case TYPE_FLOAT:
        *(float *)field_ptr = (float)para2;
        break;

      case TYPE_INT32:
        *(int32_t *)field_ptr = (int32_t)para2;
        break;

      case TYPE_UINT32:
        *(uint32_t *)field_ptr = (uint32_t)para2;
        break;

      case TYPE_UINT8:
        *(uint8_t *)field_ptr = (uint8_t)para2;
        break;

      case TYPE_BOOL:
        *(uint8_t *)field_ptr = (para2 != 0);
        break;

      default:
        printf("Unsupported type\r\n");
        return 1;
      }
      //			AD5940PlatformCfg();
      //			AD5940ImpedanceStructInit();
      pImpedanceCfg->bParaChanged = bTRUE;
      AppIMPInit(AppBuff,
                 APPBUFF_SIZE); /* Initialize IMP application. Provide a buffer,
                                   which is used to store sequencer commands */

      AppIMPGetCfg(&pImpedanceCfg);

      //			AppIMPCtrl(IMPCTRL_START, 0);
    }
  }
  return 0;
}
void print_param(AppIMPCfg_Type *cfg, configcommand_list_t *item) {
  char data[100];
  void *field_ptr = (uint8_t *)cfg + item->offset;

  switch (item->value_type) {
  case TYPE_FLOAT:
    snprintf(data, sizeof(data), "%s=%f", item->cmd_name, *(float *)field_ptr);
    break;

  case TYPE_INT32:
    snprintf(data, sizeof(data), "%s=%ld", item->cmd_name,
             *(int32_t *)field_ptr);
    break;

  case TYPE_UINT32:
    snprintf(data, sizeof(data), "%s=%lu", item->cmd_name,
             *(uint32_t *)field_ptr);
    break;

  case TYPE_UINT8:
    snprintf(data, sizeof(data), "%s=%u", item->cmd_name,
             *(uint8_t *)field_ptr);
    break;

  case TYPE_BOOL:
    snprintf(data, sizeof(data), "%s=%s", item->cmd_name,
             (*(uint8_t *)field_ptr) ? "true" : "false");
    break;
  }

  send_packet("CFG", data);
}

uint32_t EIS_start(uint32_t para1, uint32_t para2) {
  AppIMPCtrl(IMPCTRL_START, 0);
  return 0;
}
uint32_t EIS_stop(uint32_t para1, uint32_t para2) {
  AppIMPCtrl(IMPCTRL_STOPNOW, 0);
  return 0;
}
uint32_t IDN(uint32_t para1, uint32_t para2) {
  AppIMPCtrl(IMPCTRL_STOPNOW, 0);

  char data[100];

  snprintf(data, sizeof(data), "ADI,AD5940-EIS,STM32,v1.0");

  send_packet("IDN", data);
  return 0;
}
static AD5940Err AppADCPgaCal(void) {
  ADCPGACal_Type pga_cal;

  /* Calibrate ADC PGA(offset and gain) */
  AppIMPCfg_Type *AppAMPCfg;
  AppIMPGetCfg(&AppAMPCfg);
  pga_cal.AdcClkFreq = AppAMPCfg->AdcClkFreq;
  pga_cal.SysClkFreq = AppAMPCfg->SysClkFreq;
  pga_cal.ADCPga = ADCPGA_1;
  pga_cal.ADCSinc2Osr =
      ADCSINC2OSR_1333; /* 800kSPS/4/1333 = 150Hz,  T = 6.67ms*/
  pga_cal.ADCSinc3Osr = ADCSINC3OSR_4;
  pga_cal.TimeOut10us = 10 * 100; /* 10ms max */
  pga_cal.VRef1p82 = AppAMPCfg->ADCRefVolt;
  pga_cal.VRef1p11 = 1.0979f;
  pga_cal.PGACalType =
      PGACALTYPE_OFFSETGAIN; /* Calibrate Offset and Gain errors */
  AD5940_ADCPGACal(&pga_cal);
  /* Calibrate Offset and Gain for PGA = 1.5 */
  pga_cal.ADCPga = ADCPGA_1P5;
  AD5940_ADCPGACal(&pga_cal);
  /* Calibrate Offset and Gain for PGA = 2 */
  pga_cal.ADCPga = ADCPGA_2;
  AD5940_ADCPGACal(&pga_cal);
  /* Calibrate Offset and Gain for PGA = 4 */
  pga_cal.ADCPga = ADCPGA_4;
  AD5940_ADCPGACal(&pga_cal);
  /* Calibrate Offset and Gain for PGA = 9 */
  pga_cal.ADCPga = ADCPGA_9;
  AD5940_ADCPGACal(&pga_cal);
  return AD5940ERR_OK;
}
static AD5940Err AppHSDACCal() {
  HSDACCal_Type hsdac_cal;

  AD5940_AFEPwrBW(AFEPWR_LP, AFEBW_250KHZ);

  hsdac_cal.ExcitBufGain =
      EXCITBUFGAIN_2; /**< Select from  EXCITBUFGAIN_2, EXCITBUFGAIN_0P25 */
  hsdac_cal.HsDacGain =
      HSDACGAIN_0P2; /**< Select from  HSDACGAIN_1, HSDACGAIN_0P2 */
  AD5940_HSDACCal(&hsdac_cal);

  hsdac_cal.ExcitBufGain =
      EXCITBUFGAIN_0P25; /**< Select from  EXCITBUFGAIN_2, EXCITBUFGAIN_0P25 */
  hsdac_cal.HsDacGain =
      HSDACGAIN_1; /**< Select from  HSDACGAIN_1, HSDACGAIN_0P2 */
  AD5940_HSDACCal(&hsdac_cal);

  hsdac_cal.ExcitBufGain =
      EXCITBUFGAIN_0P25; /**< Select from  EXCITBUFGAIN_2, EXCITBUFGAIN_0P25 */
  hsdac_cal.HsDacGain =
      HSDACGAIN_0P2; /**< Select from  HSDACGAIN_1, HSDACGAIN_0P2 */
  AD5940_HSDACCal(&hsdac_cal);
  return AD5940ERR_OK;
}
