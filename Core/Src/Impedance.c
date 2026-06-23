/*!
 *****************************************************************************
 @file:    Impedance.c
 @author:  Neo Xu
 @brief:   Electrochemical impedance spectroscopy based on example
AD5940_Impedance
 -----------------------------------------------------------------------------

Copyright (c) 2017-2019 Analog Devices, Inc. All Rights Reserved.

This software is proprietary to Analog Devices, Inc. and its licensors.
By using this software you agree to the terms of the associated
Analog Devices Software License Agreement.

*****************************************************************************/
#include "Impedance.h"
#include "AD5940.H"
#include "AD5940.h"
#include "ad5940.h"
#include "math.h"
#include "string.h"
#include <stdio.h>

/* Default LPDAC resolution(2.5V internal reference). */
#define DAC12BITVOLT_1LSB (2200.0f / 4095)        // mV
#define DAC6BITVOLT_1LSB (DAC12BITVOLT_1LSB * 64) // mV
 uint32_t rd;
/*
  Application configuration structure. Specified by user from template.
  The variables are usable in this whole application.
  It includes basic configuration for sequencer generator and application
  related parameters
*/
AppIMPCfg_Type AppIMPCfg = {
    .bParaChanged = bFALSE,
    .SeqStartAddr = 0,
    .MaxSeqLen = 0,

    .SeqStartAddrCal = 0,
    .MaxSeqLenCal = 0,

    .ImpODR = 20.0, /* 20.0 Hz*/
    .NumOfData = -1,
    .SysClkFreq = 16000000.0,
    .WuptClkFreq = 32000.0,
    .AdcClkFreq = 16000000.0,
    .RcalVal = 100000.0,

    .DswitchSel = SWD_CE0,
    .PswitchSel = SWP_CE0,
    .NswitchSel = SWN_SE0LOAD,
    .TswitchSel = SWT_SE0LOAD,

    .PwrMod = AFEPWR_HP,

    .LptiaRtiaSel = LPTIARTIA_4K, /* COnfigure RTIA */
    .LpTiaRf = LPTIARF_1M,        /* Configure LPF resistor */
    .LpTiaRl = LPTIARLOAD_100R,

    .HstiaRtiaSel = HSTIARTIA_1K,
    .ExcitBufGain = EXCITBUFGAIN_0P25,
    .HsDacGain = HSDACGAIN_0P2,
    .HsDacUpdateRate = 0x1B,
    .DacVoltPP = 300.0,
    .BiasVolt = -0.0f,
    .Vzero = 1100,
    .SinFreq = 100000.0, /* 1000Hz */

    .DftNum = DFTNUM_16384,
    .DftSrc = DFTSRC_SINC3,
    .HanWinEn = bTRUE,

    .AdcPgaGain = ADCPGA_1,
    .ADCSinc3Osr = ADCSINC3OSR_2,
    .ADCSinc2Osr = ADCSINC2OSR_22,

    .ADCAvgNum = ADCAVGNUM_16,

    .SweepCfg.SweepEn = bTRUE,
    .SweepCfg.SweepStart = 1000,
    .SweepCfg.SweepStop = 100000.0,
    .SweepCfg.SweepPoints = 101,
    .SweepCfg.SweepLog = bFALSE,
    .SweepCfg.SweepIndex = 0,

    .FifoThresh = 4,
    .IMPInited = bFALSE,
    .StopRequired = bFALSE,
};

/**
   This function is provided for upper controllers that want to change
   application parameters specially for user defined parameters.
*/
int32_t AppIMPGetCfg(void *pCfg) {
  if (pCfg) {
    *(AppIMPCfg_Type **)pCfg = &AppIMPCfg;
    return AD5940ERR_OK;
  }
  return AD5940ERR_PARA;
}

AD5940Err AppIMPSetCalibrationResistor(AppIMPRcalSelection RcalSelect) {
  AppIMPCfg.RcalSelection = RcalSelect;

  switch (RcalSelect) {
  case RCAL_220R:
    AppIMPCfg.RcalVal = 220.0f;
    AppIMPCfg.DswitchSelCal = SWD_RCAL0;
    AppIMPCfg.PswitchSelCal = SWP_RCAL0;
    AppIMPCfg.NswitchSelCal = SWN_RCAL1;
    AppIMPCfg.TswitchSelCal = SWT_RCAL1;
    break;

  case RCAL_2K:
    AppIMPCfg.RcalVal = 2000.0f;
    AppIMPCfg.DswitchSelCal = SWD_AIN3;
    AppIMPCfg.PswitchSelCal = SWP_AIN3;
    AppIMPCfg.NswitchSelCal = SWN_AIN0;
    AppIMPCfg.TswitchSelCal = SWT_AIN0;
    break;

  case RCAL_25P5K:
    AppIMPCfg.RcalVal = 25500.0f;
    AppIMPCfg.DswitchSelCal = SWD_AIN2;
    AppIMPCfg.PswitchSelCal = SWP_AIN2;
    AppIMPCfg.NswitchSelCal = SWN_AIN0;
    AppIMPCfg.TswitchSelCal = SWT_AIN0;
    break;

  case RCAL_100K:
    AppIMPCfg.RcalVal = 100000.0f;
    AppIMPCfg.DswitchSelCal = SWD_AIN1;
    AppIMPCfg.PswitchSelCal = SWP_AIN1;
    AppIMPCfg.NswitchSelCal = SWN_AIN0;
    AppIMPCfg.TswitchSelCal = SWT_AIN0;
    break;

  default:
    return AD5940ERR_PARA;
  }

  return AD5940ERR_OK;
}

/* ===================================================================
 * Adaptive DFT / filter tuning
 *
 * Picks the DFT source, SINC2/SINC3 OSR and DFT length so that the DFT
 * window always captures at least IMP_MIN_CYCLES periods of the excitation
 * signal, while aiming for ~IMP_TARGET_SPC samples per cycle.
 *
 * Sample-rate chain (ADC core clock = 16 MHz -> ADCRATE_800KHZ):
 *     ADC sample rate              = 800 kHz
 *     SINC3 output rate            = 800 kHz / Sinc3Osr
 *     SINC2 output rate (DFT in)   = SINC3 rate / Sinc2Osr   (DFTSRC_SINC2NOTCH)
 *  or DFT input rate               = SINC3 rate              (DFTSRC_SINC3)
 *
 * Cycles captured = freq * DftPoints / Fs_dft, with DftPoints = 2^(DftNum+2).
 *
 * The SINC2 stage always decimates by >= 22, so its fastest output is only
 * SINC3rate/22 (~9 kHz). That is far below Nyquist for higher frequencies,
 * so above ~2 kHz the DFT is fed straight from SINC3 instead.
 * =================================================================== */
// #define IMP_ADC_SAMPLE_RATE 800000.0f /* ADC core rate (ADCRATE_800KHZ, 16MHz) */
#define IMP_TARGET_SPC 18.0f          /* desired samples per signal cycle */
#define IMP_MIN_CYCLES 16.0f          /* capture at least this many cycles */

static const uint16_t imp_sinc2osr_val[] = {22,  44,  89,  178, 267,  533,
                                            640, 667, 800, 889, 1067, 1333};
#define IMP_SINC2OSR_CNT (sizeof(imp_sinc2osr_val) / sizeof(imp_sinc2osr_val[0]))

/* Compute and store DftSrc / Sinc3Osr / Sinc2Osr / DftNum for 'freq'. */
static void AppIMPAdaptiveFilterCfg(float freq) {
  float fs_dft, need_points;
  uint32_t points, dftnum;

  if (freq < 1.0f)
    freq = 1.0f;

  // AppIMPCfg.ADC_Rate = ADCRATE_800KHZ; /* keep the math and HW consistent */
  float ImpADCSampleRate = AppIMPCfg.ADC_Rate == ADCRATE_800KHZ?800000.0f:16000000.0f;
  if (freq <= 600.0f) {
    /* Low frequency: feed DFT from SINC2 and tune its OSR for ~TARGET_SPC. */
    float fs_sinc3 = ImpADCSampleRate / 4.0f; /* SINC3 OSR4 -> 200 kHz */
    float ideal_osr = fs_sinc3 / (IMP_TARGET_SPC * freq);
    uint32_t best_i = 0;
    float best_err = 1e30f;
    for (uint32_t i = 0; i < IMP_SINC2OSR_CNT; i++) {
      float err = fabsf((float)imp_sinc2osr_val[i] - ideal_osr);
      if (err < best_err) {
        best_err = err;
        best_i = i;
      }
    }
    AppIMPCfg.DftSrc = DFTSRC_SINC2NOTCH;
    AppIMPCfg.ADCSinc3Osr = ADCSINC3OSR_4;
    AppIMPCfg.ADCSinc2Osr = (uint8_t)best_i;
    fs_dft = fs_sinc3 / (float)imp_sinc2osr_val[best_i];
  } else if (freq <= 2000.0f) {
    /* Mid-low: SINC2 at its fastest (OSR22) still gives >=4 samples/cycle. */
    AppIMPCfg.DftSrc = DFTSRC_SINC2NOTCH;
    AppIMPCfg.ADCSinc3Osr = ADCSINC3OSR_4;
    AppIMPCfg.ADCSinc2Osr = ADCSINC2OSR_22;
    fs_dft = ImpADCSampleRate / 4.0f / 22.0f; /* ~9.09 kHz */
  } else if (freq < 25000.0f) {
    /* Mid-high: SINC2 too slow, feed DFT from SINC3 (OSR5 -> 160 kHz). */
    AppIMPCfg.DftSrc = DFTSRC_SINC3;
    AppIMPCfg.ADCSinc3Osr = ADCSINC3OSR_5;
    AppIMPCfg.ADCSinc2Osr = ADCSINC2OSR_22; /* unused by DFT, keep valid */
    fs_dft = ImpADCSampleRate / 5.0f;    /* 160 kHz */
  } else {
    /* High frequency: SINC3 OSR2 -> 400 kHz (>=4 samples/cycle up to 100 kHz). */
    AppIMPCfg.DftSrc = DFTSRC_SINC3;
    AppIMPCfg.ADCSinc3Osr = ADCSINC3OSR_2;
    AppIMPCfg.ADCSinc2Osr = ADCSINC2OSR_22; /* unused by DFT, keep valid */
    fs_dft = ImpADCSampleRate / 1.0f;    /* 400 kHz */
  }

  /* Smallest power-of-two DFT (2^(k+2)) covering >= IMP_MIN_CYCLES cycles. */
  need_points = IMP_MIN_CYCLES * fs_dft / freq;
  dftnum = DFTNUM_4; /* index 0 -> 4 points */
  points = 4;
  while ((float)points < need_points && dftnum < DFTNUM_16384) {
    dftnum++;
    points <<= 1;
  }
  AppIMPCfg.DftNum = dftnum;
}
/* ===================================================================
 * HSTIA RTIA auto-ranging
 *
 * The DFT result is 18-bit signed. Keep the measured DFT amplitude
 * comfortably away from clipping, but not too small.
 *
 * Tune these thresholds using your existing RAW packet:
 *   RAW: freq, RzReal, RzImag, rz_mag, HstiaRtiaSel
 * =================================================================== */

#define IMP_AUTORTIA_LOW_COUNTS     12000.0f
#define IMP_AUTORTIA_TARGET_COUNTS  60000.0f
#define IMP_AUTORTIA_HIGH_COUNTS    90000.0f

typedef struct {
  uint32_t sel;
  float ohms;
} AppIMPHsRtiaRange_Type;

static const AppIMPHsRtiaRange_Type AppIMPHsRtiaTable[] = {
    {HSTIARTIA_200,  200.0f},
    {HSTIARTIA_1K,   1000.0f},
    {HSTIARTIA_5K,   5000.0f},
    {HSTIARTIA_10K,  10000.0f},
    {HSTIARTIA_20K,  20000.0f},
    {HSTIARTIA_40K,  40000.0f},
    {HSTIARTIA_80K,  80000.0f},
    {HSTIARTIA_160K, 160000.0f},
};

#define APPIMP_HSRTIA_COUNT \
  (sizeof(AppIMPHsRtiaTable) / sizeof(AppIMPHsRtiaTable[0]))

static int32_t AppIMPFindHsRtiaIndex(uint32_t rtiaSel) {
  for (uint32_t i = 0; i < APPIMP_HSRTIA_COUNT; i++) {
    if (AppIMPHsRtiaTable[i].sel == rtiaSel)
      return (int32_t)i;
  }
  return -1;
}

static int32_t AppIMPSignExtend18(uint32_t x) {
  x &= 0x3ffffUL;
  if (x & (1UL << 17))
    x |= 0xfffc0000UL;
  return (int32_t)x;
}

static float AppIMPDftMagFromPair(const int32_t *pData, uint32_t base) {
  int32_t re = AppIMPSignExtend18((uint32_t)pData[base + 0]);
  int32_t im = AppIMPSignExtend18((uint32_t)pData[base + 1]);

  return sqrtf((float)re * (float)re + (float)im * (float)im);
}

/*
 * Returns bTRUE when RTIA was changed.
 *
 * dataCount is the raw FIFO word count before AppIMPDataProcess().
 * With FifoThresh = 4, one result is:
 *   Rz.Real, Rz.Imag, Rcal.Real, Rcal.Imag
 */
static BoolFlag AppIMPAutoRangeHstia(int32_t *const pData,
                                     uint32_t dataCount) {
  if (AppIMPCfg.HstiaRtiaAutoEn == bFALSE)
    return bFALSE;

  if (dataCount < 4)
    return bFALSE;

  int32_t old_idx = AppIMPFindHsRtiaIndex(AppIMPCfg.HstiaRtiaSel);
  if (old_idx < 0)
    return bFALSE;

  /*
   * Use the latest complete measurement in the FIFO.
   * This makes the function safe even if FIFO contains more than one point.
   */
  uint32_t base = dataCount - 4;

  float rz_mag = AppIMPDftMagFromPair(pData, base + 0);
  float rcal_mag = AppIMPDftMagFromPair(pData, base + 2);

  /*
   * Keep both Rz and RCAL inside range, because your sequence measures both
   * with the same HSTIA RTIA.
   */
  float mag = (rz_mag > rcal_mag) ? rz_mag : rcal_mag;

  int32_t new_idx = old_idx;
  float old_ohms = AppIMPHsRtiaTable[old_idx].ohms;

  if (mag > IMP_AUTORTIA_HIGH_COUNTS) {
    /*
     * Signal too large: decrease RTIA.
     * Predict the new magnitude from RTIA ratio.
     */
    while (new_idx > 0) {
      new_idx--;

      float pred =
          mag * AppIMPHsRtiaTable[new_idx].ohms / old_ohms;

      if (pred <= IMP_AUTORTIA_TARGET_COUNTS)
        break;
    }
  } else if (mag < IMP_AUTORTIA_LOW_COUNTS) {
    /*
     * Signal too small: increase RTIA, but do not predictably exceed
     * the high threshold.
     */
    while (new_idx < (int32_t)APPIMP_HSRTIA_COUNT - 1) {
      float pred =
          mag * AppIMPHsRtiaTable[new_idx + 1].ohms / old_ohms;

      if (pred > IMP_AUTORTIA_HIGH_COUNTS)
        break;

      new_idx++;

      if (pred >= IMP_AUTORTIA_TARGET_COUNTS)
        break;
    }
  }

  if (new_idx == old_idx)
    return bFALSE;

  AppIMPCfg.HstiaRtiaSel = AppIMPHsRtiaTable[new_idx].sel;

  /*
   * Change only HSTIA RTIA. No sequence regeneration is required for RTIA only.
   * The next measurement will power/use HSTIA with this new register value.
   */
  AD5940_HSRTIACfgS(AppIMPCfg.HstiaRtiaSel);

  /*
   * Current FIFO data was acquired with the old range. Drop it and repeat
   * the same frequency.
   */
  AppIMPCfg.HstiaRtiaRangeChanged = bTRUE;

  return bTRUE;
}

AD5940Err AppIMPCtrl(uint32_t Command, void *pPara) {

  switch (Command) {
  case IMPCTRL_START: {
    WUPTCfg_Type wupt_cfg;

    if (AD5940_WakeUp(10) >
        10) /* Wakeup AFE by read register, read 10 times at most */
      return AD5940ERR_WAKEUP; /* Wakeup Failed */
    if (AppIMPCfg.IMPInited == bFALSE)
      return AD5940ERR_APPERROR;
    /* Start it */
    wupt_cfg.WuptEn = bTRUE;
    wupt_cfg.WuptEndSeq = WUPTENDSEQ_A;
    wupt_cfg.WuptOrder[0] = SEQID_0;
    wupt_cfg.SeqxSleepTime[SEQID_0] = 4;
    wupt_cfg.SeqxWakeupTime[SEQID_0] =
        (uint32_t)(AppIMPCfg.WuptClkFreq / AppIMPCfg.ImpODR) - 4;
    AD5940_WUPTCfg(&wupt_cfg);

    AppIMPCfg.FifoDataCount = 0; /* restart */
    AppIMPCfg.bIsRunning = bTRUE;
    break;
  }
  case IMPCTRL_STOPNOW: {
    if (AD5940_WakeUp(10) >
        10) /* Wakeup AFE by read register, read 10 times at most */
      return AD5940ERR_WAKEUP; /* Wakeup Failed */
    /* Start Wupt right now */
    AD5940_WUPTCtrl(bFALSE);
    AD5940_WUPTCtrl(bFALSE);
    AppIMPCfg.bIsRunning = bFALSE;
    break;
  }
  case IMPCTRL_STOPSYNC: {
    AppIMPCfg.StopRequired = bTRUE;
    AppIMPCfg.bIsRunning = bFALSE;
    break;
  }
  case IMPCTRL_GETFREQ: {
    if (pPara == 0)
      return AD5940ERR_PARA;
    if (AppIMPCfg.SweepCfg.SweepEn == bTRUE)
      *(float *)pPara = AppIMPCfg.FreqofData;
    else
      *(float *)pPara = AppIMPCfg.SinFreq;
  } break;
  case IMPCTRL_SHUTDOWN: {
    AppIMPCtrl(IMPCTRL_STOPNOW, 0); /* Stop the measurement if it's running. */
    /* Turn off LPloop related blocks which are not controlled automatically by
     * hibernate operation */
    AFERefCfg_Type aferef_cfg;
    LPLoopCfg_Type lploop_cfg;
    memset(&aferef_cfg, 0, sizeof(aferef_cfg));
    AD5940_REFCfgS(&aferef_cfg);
    memset(&lploop_cfg, 0, sizeof(lploop_cfg));
    AD5940_LPLoopCfgS(&lploop_cfg);
    AD5940_EnterSleepS(); /* Enter Hibernate */
    AppIMPCfg.bIsRunning = bFALSE;
  } break;
  default:
    break;
  }
  return AD5940ERR_OK;
}

/* generated code snnipet */
float AppIMPGetCurrFreq(void) {
  if (AppIMPCfg.SweepCfg.SweepEn == bTRUE)
    return AppIMPCfg.FreqofData;
  else
    return AppIMPCfg.SinFreq;
}

static AD5940Err AppIMPSeqCfgGen(void) {
  AD5940Err error = AD5940ERR_OK;
  const uint32_t *pSeqCmd;
  uint32_t SeqLen;
  AFERefCfg_Type aferef_cfg;
  HSLoopCfg_Type HsLoopCfg;
  LPLoopCfg_Type lploop_cfg;
  DSPCfg_Type dsp_cfg;
  float sin_freq;

  /* Start sequence generator here */
  AD5940_SEQGenCtrl(bTRUE);

  AD5940_AFECtrlS(AFECTRL_ALL, bFALSE); /* Init all to disable state */

  aferef_cfg.HpBandgapEn = bTRUE;
  aferef_cfg.Hp1V1BuffEn = bTRUE;
  aferef_cfg.Hp1V8BuffEn = bTRUE;
  aferef_cfg.Disc1V1Cap = bFALSE;
  aferef_cfg.Disc1V8Cap = bFALSE;
  aferef_cfg.Hp1V8ThemBuff = bFALSE;
  aferef_cfg.Hp1V8Ilimit = bFALSE;
  aferef_cfg.Lp1V1BuffEn = bFALSE;
  aferef_cfg.Lp1V8BuffEn = bFALSE;
  aferef_cfg.LpBandgapEn = bTRUE;
  aferef_cfg.LpRefBufEn = bTRUE;
  aferef_cfg.LpRefBoostEn = bFALSE;
  AD5940_REFCfgS(&aferef_cfg);

  lploop_cfg.LpDacCfg.LpDacSrc = LPDACSRC_MMR;
  lploop_cfg.LpDacCfg.LpDacSW = LPDACSW_VBIAS2LPPA | LPDACSW_VBIAS2PIN |
                                LPDACSW_VZERO2LPTIA | LPDACSW_VZERO2PIN;
  lploop_cfg.LpDacCfg.LpDacVzeroMux = LPDACVZERO_6BIT;
  lploop_cfg.LpDacCfg.LpDacVbiasMux = LPDACVBIAS_12BIT;
  lploop_cfg.LpDacCfg.LpDacRef = LPDACREF_2P5;
  lploop_cfg.LpDacCfg.DataRst = bFALSE;
  lploop_cfg.LpDacCfg.PowerEn = bTRUE;
  lploop_cfg.LpDacCfg.DacData6Bit =
      (uint32_t)((AppIMPCfg.Vzero - 200) / DAC6BITVOLT_1LSB);
  lploop_cfg.LpDacCfg.DacData12Bit =
      (int32_t)((AppIMPCfg.BiasVolt) / DAC12BITVOLT_1LSB) +
      lploop_cfg.LpDacCfg.DacData6Bit * 64;
  if (lploop_cfg.LpDacCfg.DacData12Bit > lploop_cfg.LpDacCfg.DacData6Bit * 64)
    lploop_cfg.LpDacCfg.DacData12Bit--;
  lploop_cfg.LpDacCfg.LpdacSel = LPDAC0;
  lploop_cfg.LpAmpCfg.LpAmpPwrMod = LPAMPPWR_NORM;
  lploop_cfg.LpAmpCfg.LpPaPwrEn = bTRUE;
  lploop_cfg.LpAmpCfg.LpTiaPwrEn = bTRUE;
  lploop_cfg.LpAmpCfg.LpTiaRf = AppIMPCfg.LpTiaRf;
  lploop_cfg.LpAmpCfg.LpTiaRload = AppIMPCfg.LpTiaRl;
  lploop_cfg.LpAmpCfg.LpTiaRtia = AppIMPCfg.LptiaRtiaSel;
  lploop_cfg.LpAmpCfg.LpTiaSW =
      LPTIASW(5) | LPTIASW(2) | LPTIASW(4) | LPTIASW(12) | LPTIASW(13);
  lploop_cfg.LpAmpCfg.LpAmpSel = LPAMP0;
  AD5940_LPLoopCfgS(&lploop_cfg);

  HsLoopCfg.HsDacCfg.ExcitBufGain = AppIMPCfg.ExcitBufGain;
  HsLoopCfg.HsDacCfg.HsDacGain = AppIMPCfg.HsDacGain;
  HsLoopCfg.HsDacCfg.HsDacUpdateRate = AppIMPCfg.HsDacUpdateRate;

  HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;
  HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_1P1;
  HsLoopCfg.HsTiaCfg.HstiaCtia = 31; /* 31pF + 2pF */
  HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;
  HsLoopCfg.HsTiaCfg.HstiaRtiaSel = AppIMPCfg.HstiaRtiaSel;

  HsLoopCfg.SWMatCfg.Dswitch = AppIMPCfg.DswitchSel;
  HsLoopCfg.SWMatCfg.Pswitch = AppIMPCfg.PswitchSel;
  HsLoopCfg.SWMatCfg.Nswitch = AppIMPCfg.NswitchSel;
  HsLoopCfg.SWMatCfg.Tswitch = SWT_TRTIA | AppIMPCfg.TswitchSel;

  HsLoopCfg.WgCfg.WgType = WGTYPE_SIN;
  HsLoopCfg.WgCfg.GainCalEn = bTRUE;
  HsLoopCfg.WgCfg.OffsetCalEn = bTRUE;
  if (AppIMPCfg.SweepCfg.SweepEn == bTRUE) {
    AppIMPCfg.FreqofData = AppIMPCfg.SweepCfg.SweepStart;
    AppIMPCfg.SweepCurrFreq = AppIMPCfg.SweepCfg.SweepStart;
    AD5940_SweepNext(&AppIMPCfg.SweepCfg, &AppIMPCfg.SweepNextFreq);
    sin_freq = AppIMPCfg.SweepCurrFreq;
  } else {
    sin_freq = AppIMPCfg.SinFreq;
    AppIMPCfg.FreqofData = sin_freq;
  }
  HsLoopCfg.WgCfg.SinCfg.SinFreqWord =
      AD5940_WGFreqWordCal(sin_freq, AppIMPCfg.SysClkFreq);
  HsLoopCfg.WgCfg.SinCfg.SinAmplitudeWord =
      (uint32_t)(AppIMPCfg.DacVoltPP / 800.0f * 2047 + 0.5f);
  HsLoopCfg.WgCfg.SinCfg.SinOffsetWord = 0;
  HsLoopCfg.WgCfg.SinCfg.SinPhaseWord = 0;
  AD5940_HSLoopCfgS(&HsLoopCfg);

  /* Adapt DFT source, OSR and DFT length to the excitation frequency so that
   * at least 20 signal cycles are captured. Sets DftSrc/Sinc3Osr/Sinc2Osr/
   * DftNum/ADC_Rate in AppIMPCfg, which the DSP config below picks up. */
  if(AppIMPCfg.AutoFilterEn)
    AppIMPAdaptiveFilterCfg(sin_freq);

  dsp_cfg.ADCBaseCfg.ADCMuxN = ADCMUXN_HSTIA_N;
  dsp_cfg.ADCBaseCfg.ADCMuxP = ADCMUXP_HSTIA_P;
  dsp_cfg.ADCBaseCfg.ADCPga = AppIMPCfg.AdcPgaGain;

  memset(&dsp_cfg.ADCDigCompCfg, 0, sizeof(dsp_cfg.ADCDigCompCfg));

  dsp_cfg.ADCFilterCfg.ADCAvgNum = AppIMPCfg.ADCAvgNum;
  dsp_cfg.ADCFilterCfg.ADCRate =
      AppIMPCfg.ADC_Rate; /* Tell filter block clock rate of ADC */
  dsp_cfg.ADCFilterCfg.ADCSinc2Osr = AppIMPCfg.ADCSinc2Osr;
  dsp_cfg.ADCFilterCfg.ADCSinc3Osr = AppIMPCfg.ADCSinc3Osr;
  dsp_cfg.ADCFilterCfg.BpNotch = bTRUE;
  dsp_cfg.ADCFilterCfg.BpSinc3 = bFALSE;
  dsp_cfg.ADCFilterCfg.Sinc2NotchEnable = bTRUE;
  dsp_cfg.DftCfg.DftNum = AppIMPCfg.DftNum;
  dsp_cfg.DftCfg.DftSrc = AppIMPCfg.DftSrc;
  dsp_cfg.DftCfg.HanWinEn = AppIMPCfg.HanWinEn;

  memset(&dsp_cfg.StatCfg, 0, sizeof(dsp_cfg.StatCfg));
  AD5940_DSPCfgS(&dsp_cfg);

  /* Enable all of them. They are automatically turned off during hibernate mode
   * to save power */
  if (AppIMPCfg.BiasVolt == 0.0f)
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR | AFECTRL_INAMPPWR | AFECTRL_EXTBUFPWR |
                        AFECTRL_WG | AFECTRL_DACREFPWR | AFECTRL_HSDACPWR |
                        AFECTRL_SINC2NOTCH,
                    bTRUE);
  else
    AD5940_AFECtrlS(AFECTRL_HSTIAPWR | AFECTRL_INAMPPWR | AFECTRL_EXTBUFPWR |
                        AFECTRL_WG | AFECTRL_DACREFPWR | AFECTRL_HSDACPWR |
                        AFECTRL_SINC2NOTCH | AFECTRL_DCBUFPWR,
                    bTRUE);
  /* Sequence end. */
  AD5940_SEQGenInsert(SEQ_STOP()); /* Add one extra command to disable sequencer
                                      for initialization sequence because we
                                      only want it to run one time. */

  /* Stop here */
  error = AD5940_SEQGenFetchSeq(&pSeqCmd, &SeqLen);
  AD5940_SEQGenCtrl(bFALSE); /* Stop sequencer generator */
  if (error == AD5940ERR_OK) {
    AppIMPCfg.InitSeqInfo.SeqId = SEQID_1;
    AppIMPCfg.InitSeqInfo.SeqRamAddr = AppIMPCfg.SeqStartAddr;
    AppIMPCfg.InitSeqInfo.pSeqCmd = pSeqCmd;
    AppIMPCfg.InitSeqInfo.SeqLen = SeqLen;
    /* Write command to SRAM */
    AD5940_SEQCmdWrite(AppIMPCfg.InitSeqInfo.SeqRamAddr, pSeqCmd, SeqLen);
  } else
    return error; /* Error */
  return AD5940ERR_OK;
}
static AD5940Err AppIMPSeqMeasureGen(void) {
  AD5940Err error = AD5940ERR_OK;
  const uint32_t *pSeqCmd;
  uint32_t SeqLen;

  uint32_t WaitClks;
  SWMatrixCfg_Type sw_cfg;
  ClksCalInfo_Type clks_cal;
  LPAmpCfg_Type LpAmpCfg;
  /* Calculate number of clocks to get data to FIFO */
  clks_cal.DataType = DATATYPE_DFT;
  clks_cal.DftSrc = AppIMPCfg.DftSrc;
  clks_cal.DataCount = 1L << (AppIMPCfg.DftNum + 2); /* 2^(DFTNUMBER+2) */
  clks_cal.ADCSinc2Osr = AppIMPCfg.ADCSinc2Osr;
  clks_cal.ADCSinc3Osr = AppIMPCfg.ADCSinc3Osr;
  clks_cal.ADCAvgNum = AppIMPCfg.ADCAvgNum;
  clks_cal.RatioSys2AdcClk = AppIMPCfg.SysClkFreq / AppIMPCfg.AdcClkFreq;
  AD5940_ClksCalculate(&clks_cal, &WaitClks);

  /* Start Sequence Generator */
  AD5940_SEQGenCtrl(bTRUE);
  AD5940_SEQGpioCtrlS(
      AGPIO_Pin2); /* Set GPIO1, clear others that under control */
  AD5940_SEQGenInsert(SEQ_WAIT(16 * 250)); /* @todo wait 250us? */
  /* Disconnect SE0 from LPTIA*/
  LpAmpCfg.LpAmpPwrMod = LPAMPPWR_NORM;
  LpAmpCfg.LpPaPwrEn = bTRUE;
  LpAmpCfg.LpTiaPwrEn = bTRUE;
  LpAmpCfg.LpTiaRf = AppIMPCfg.LpTiaRf;
  LpAmpCfg.LpTiaRload = AppIMPCfg.LpTiaRl;
  LpAmpCfg.LpTiaRtia =
      LPTIARTIA_OPEN; /* Disconnect Rtia to avoid RC filter discharge */
  LpAmpCfg.LpTiaSW = LPTIASW(7) | LPTIASW(8) | LPTIASW(12) | LPTIASW(13);
  LpAmpCfg.LpAmpSel = LPAMP0;
  AD5940_LPAMPCfgS(&LpAmpCfg);
  /* Sensor + Rload Measurement */
  sw_cfg.Dswitch = AppIMPCfg.DswitchSel;//|SWD_AFE1;
  sw_cfg.Pswitch = AppIMPCfg.PswitchSel;
  sw_cfg.Nswitch = AppIMPCfg.NswitchSel;//|SWN_AFE3LOAD;
  sw_cfg.Tswitch = SWT_TRTIA | AppIMPCfg.TswitchSel;
  AD5940_SWMatrixCfgS(&sw_cfg);

  AD5940_AFECtrlS(AFECTRL_HSTIAPWR | AFECTRL_INAMPPWR | AFECTRL_EXTBUFPWR |
                      AFECTRL_WG | AFECTRL_DACREFPWR | AFECTRL_HSDACPWR |
                      AFECTRL_SINC2NOTCH,
                  bTRUE);

  AD5940_AFECtrlS(AFECTRL_ADCPWR | AFECTRL_SINC2NOTCH,
                  bTRUE); /* Enable Waveform generator */
  // delay for signal settling DFT_WAIT
  AD5940_SEQGenInsert(SEQ_WAIT(16 * 10));
  AD5940_AFECtrlS(AFECTRL_ADCCNV | AFECTRL_DFT,
                  bTRUE); /* Start ADC convert and DFT */
  AD5940_SEQGenInsert(SEQ_WAIT(WaitClks));
  // wait for first data ready
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR | AFECTRL_INAMPPWR | AFECTRL_EXTBUFPWR |
                      AFECTRL_WG | AFECTRL_DACREFPWR | AFECTRL_HSDACPWR |
                      AFECTRL_SINC2NOTCH | AFECTRL_DFT | AFECTRL_ADCCNV,
                  bFALSE);

  // /* RLOAD Measurement */
  // HsLoopCfg.HsTiaCfg.DiodeClose = bFALSE;
  // HsLoopCfg.HsTiaCfg.HstiaBias = HSTIABIAS_1P1;
  // HsLoopCfg.HsTiaCfg.HstiaCtia = 31; /* 31pF + 2pF */
  // HsLoopCfg.HsTiaCfg.HstiaDeRload = HSTIADERLOAD_OPEN;
  // HsLoopCfg.HsTiaCfg.HstiaDeRtia = HSTIADERTIA_OPEN;
  // HsLoopCfg.HsTiaCfg.HstiaRtiaSel = HSTIARTIA_200;

  // // AD5940_HSTIACfgS(&HsLoopCfg.HsTiaCfg);
  // sw_cfg.Dswitch = SWD_SE0|SWD_AFE1;
  // sw_cfg.Pswitch = SWP_SE0;
  // sw_cfg.Nswitch = SWN_SE0LOAD|SWN_AFE3LOAD;
  // sw_cfg.Tswitch = SWT_SE0LOAD | SWT_TRTIA;
  // AD5940_SWMatrixCfgS(&sw_cfg);
  // AD5940_AFECtrlS(AFECTRL_HSTIAPWR | AFECTRL_INAMPPWR | AFECTRL_EXTBUFPWR |
  //                     AFECTRL_WG | AFECTRL_DACREFPWR | AFECTRL_HSDACPWR |
  //                     AFECTRL_SINC2NOTCH,
  //                 bTRUE);
  // AD5940_SEQGenInsert(SEQ_WAIT(16 * 10)); // delay for signal settling DFT_WAIT
  // AD5940_AFECtrlS(AFECTRL_ADCCNV | AFECTRL_DFT,
  //                 bTRUE);                  /* Start ADC convert and DFT */
  // AD5940_SEQGenInsert(SEQ_WAIT(WaitClks)); /* wait for first data ready */
  // AD5940_AFECtrlS(AFECTRL_HSTIAPWR | AFECTRL_INAMPPWR | AFECTRL_EXTBUFPWR |
  //                     AFECTRL_WG | AFECTRL_DACREFPWR | AFECTRL_HSDACPWR |
  //                     AFECTRL_SINC2NOTCH | AFECTRL_ADCCNV,
  //                 bFALSE);

  /* RCAL Measurement */

  sw_cfg.Dswitch = AppIMPCfg.DswitchSelCal;
  sw_cfg.Pswitch = AppIMPCfg.PswitchSelCal;
  sw_cfg.Nswitch = AppIMPCfg.NswitchSelCal;
  sw_cfg.Tswitch = AppIMPCfg.TswitchSelCal | SWT_TRTIA;
  AD5940_SWMatrixCfgS(&sw_cfg);

  // sw_cfg.Dswitch = SWD_RCAL0;//  SWP_RCAL0 /* Calibration Resistor High Side
  // */ sw_cfg.Pswitch = SWP_RCAL0; sw_cfg.Nswitch = SWN_RCAL1; //|
  // SWN_AFE3LOAD;        /* SWN_RCAL1Calibration Resistor Low Side */
  // sw_cfg.Tswitch = SWT_RCAL1 | SWT_TRTIA; /*SWT_RCAL1 | SWT_TRTIA Return path
  // to HSTIA */
  // AD5940_SWMatrixCfgS(&sw_cfg);
  /* Reconnect LP loop */
  LpAmpCfg.LpTiaRtia =
      AppIMPCfg.LptiaRtiaSel; /* Disconnect Rtia to avoid RC filter discharge */
  LpAmpCfg.LpTiaSW =
      LPTIASW(5) | LPTIASW(2) | LPTIASW(4) | LPTIASW(12) | LPTIASW(13);
  AD5940_LPAMPCfgS(&LpAmpCfg);

  AD5940_AFECtrlS(AFECTRL_HSTIAPWR | AFECTRL_INAMPPWR | AFECTRL_EXTBUFPWR |
                      AFECTRL_WG | AFECTRL_DACREFPWR | AFECTRL_HSDACPWR |
                      AFECTRL_SINC2NOTCH,
                  bTRUE);

  // AD5940_AFECtrlS(AFECTRL_ADCPWR | AFECTRL_SINC2NOTCH,
  //                 bTRUE); /* Enable Waveform generator */
  AD5940_SEQGenInsert(SEQ_WAIT(16 * 10)); // delay for signal settling DFT_WAIT
  AD5940_AFECtrlS(AFECTRL_ADCCNV | AFECTRL_DFT | AFECTRL_SINC2NOTCH,
                  bTRUE);                  /* Start ADC convert and DFT */
  AD5940_SEQGenInsert(SEQ_WAIT(WaitClks)); /* wait for first data ready */
  AD5940_AFECtrlS(AFECTRL_ADCCNV | AFECTRL_DFT | AFECTRL_WG | AFECTRL_ADCPWR,
                  bFALSE); /* Stop ADC convert and DFT */
  AD5940_AFECtrlS(AFECTRL_HSTIAPWR | AFECTRL_INAMPPWR | AFECTRL_EXTBUFPWR |
                      AFECTRL_WG | AFECTRL_DACREFPWR | AFECTRL_HSDACPWR |
                      AFECTRL_SINC2NOTCH,
                  bFALSE);
  AD5940_SEQGpioCtrlS(0); /* Clr GPIO1 */

  sw_cfg.Dswitch = SWD_OPEN;
  sw_cfg.Pswitch = SWP_OPEN;
  sw_cfg.Nswitch = SWN_OPEN;
  sw_cfg.Tswitch = SWT_OPEN;
  AD5940_SWMatrixCfgS(&sw_cfg);

  // AD5940_EnterSleepS();/* Goto hibernate */

  /* Sequence end. */
  error = AD5940_SEQGenFetchSeq(&pSeqCmd, &SeqLen);
  AD5940_SEQGenCtrl(bFALSE); /* Stop sequencer generator */

  if (error == AD5940ERR_OK) {
    AppIMPCfg.MeasureSeqInfo.SeqId = SEQID_0;
    AppIMPCfg.MeasureSeqInfo.SeqRamAddr =
        AppIMPCfg.InitSeqInfo.SeqRamAddr + AppIMPCfg.InitSeqInfo.SeqLen;
    AppIMPCfg.MeasureSeqInfo.pSeqCmd = pSeqCmd;
    AppIMPCfg.MeasureSeqInfo.SeqLen = SeqLen;
    /* Write command to SRAM */
    AD5940_SEQCmdWrite(AppIMPCfg.MeasureSeqInfo.SeqRamAddr, pSeqCmd, SeqLen);
  } else
    return error; /* Error */
  return AD5940ERR_OK;
}

/* Workspace used to re-generate the measurement sequence while a sweep is
 * running. Kept separate from the data buffer so re-generation never clobbers
 * unprocessed FIFO samples. */
static uint32_t AppIMPSeqGenBuff[256];

/* Re-generate the measurement sequence in place. Needed when the DFT length /
 * OSR changes mid-sweep, because the sequence embeds a fixed data-ready wait
 * (WaitClks) derived from those settings. */
static void AppIMPSeqMeasureRegen(void) {
  AD5940_SEQGenInit(AppIMPSeqGenBuff,
                    sizeof(AppIMPSeqGenBuff) / sizeof(AppIMPSeqGenBuff[0]));
  AppIMPSeqMeasureGen();
}

/* Push the current DFT/filter selection to the live AFE registers. The
 * measurement sequence does not re-program these, so they must be written
 * directly when they change between sweep points (AFE is awake here and the
 * settings survive hibernate). */
static void AppIMPApplyFilterRegs(void) {
  ADCFilterCfg_Type filt;
  DFTCfg_Type dft;

  filt.ADCSinc3Osr = AppIMPCfg.ADCSinc3Osr;
  filt.ADCSinc2Osr = AppIMPCfg.ADCSinc2Osr;
  filt.ADCAvgNum = AppIMPCfg.ADCAvgNum;
  filt.ADCRate = AppIMPCfg.ADC_Rate;
  filt.BpNotch = bTRUE;
  filt.BpSinc3 = bFALSE;
  filt.Sinc2NotchEnable = bTRUE;
  AD5940_ADCFilterCfgS(&filt);

  dft.DftNum = AppIMPCfg.DftNum;
  dft.DftSrc = AppIMPCfg.DftSrc;
  dft.HanWinEn = AppIMPCfg.HanWinEn;
  AD5940_DFTCfgS(&dft);
}

/* This function provide application initialize. It can also enable Wupt that
 * will automatically trigger sequence. Or it can configure  */
AD5940Err AppIMPInit(uint32_t *pBuffer, uint32_t BufferSize) {
  AD5940Err error = AD5940ERR_OK;
  SEQCfg_Type seq_cfg;
  FIFOCfg_Type fifo_cfg;

  if (AD5940_WakeUp(10) >
      10) /* Wakeup AFE by read register, read 10 times at most */
    return AD5940ERR_WAKEUP; /* Wakeup Failed */

  /* Configure sequencer and stop it */
  seq_cfg.SeqMemSize =
      SEQMEMSIZE_2KB; /* 2kB SRAM is used for sequencer, others for data FIFO */
  // seq_cfg.SeqBreakEn = bFALSE;
  // seq_cfg.SeqIgnoreEn = bTRUE;
  seq_cfg.SeqCntCRCClr = bTRUE;
  seq_cfg.SeqEnable = bFALSE;
  seq_cfg.SeqWrTimer = 0;
  AD5940_SEQCfg(&seq_cfg);

  /* Reconfigure FIFO */
  AD5940_FIFOCtrlS(FIFOSRC_DFT, bFALSE); /* Disable FIFO firstly */
  fifo_cfg.FIFOEn = bTRUE;
  fifo_cfg.FIFOMode = FIFOMODE_FIFO;
  fifo_cfg.FIFOSize =
      FIFOSIZE_4KB; /* 4kB for FIFO, The reset 2kB for sequencer */
  fifo_cfg.FIFOSrc = FIFOSRC_DFT;
  fifo_cfg.FIFOThresh =
      AppIMPCfg
          .FifoThresh; /* DFT result. One pair for RCAL, another for Rz. One DFT
                          result have real part and imaginary part */
  AD5940_FIFOCfg(&fifo_cfg);
  AD5940_INTCClrFlag(AFEINTSRC_ALLINT);

  /* Start sequence generator */
  /* Initialize sequencer generator */
  if ((AppIMPCfg.IMPInited == bFALSE) || (AppIMPCfg.bParaChanged == bTRUE)) {

    if (pBuffer == 0)
      return AD5940ERR_PARA;
    if (BufferSize == 0)
      return AD5940ERR_PARA;
    AD5940_SEQGenInit(pBuffer, BufferSize);
    /* Generate initialize sequence */
    error = AppIMPSeqCfgGen(); /* Application initialization sequence using
                                  either MCU or sequencer */
    if (error != AD5940ERR_OK)
      return error;

    /* Generate measurement sequence */
    error = AppIMPSeqMeasureGen();
    if (error != AD5940ERR_OK)
      return error;

    AppIMPCfg.bParaChanged = bFALSE; /* Clear this flag as we already
                                        implemented the new configuration */
  }

  /* Initialization sequencer  */
  AppIMPCfg.InitSeqInfo.WriteSRAM = bFALSE;
  AD5940_SEQInfoCfg(&AppIMPCfg.InitSeqInfo);
  seq_cfg.SeqEnable = bTRUE;
  AD5940_SEQCfg(&seq_cfg); /* Enable sequencer */
  AD5940_SEQMmrTrig(AppIMPCfg.InitSeqInfo.SeqId);
  while (AD5940_INTCTestFlag(AFEINTC_1, AFEINTSRC_ENDSEQ) == bFALSE)
    ;

  /* Measurement sequence  */
  AppIMPCfg.MeasureSeqInfo.WriteSRAM = bFALSE;
  AD5940_SEQInfoCfg(&AppIMPCfg.MeasureSeqInfo);

  seq_cfg.SeqEnable = bTRUE;
  AD5940_SEQCfg(&seq_cfg); /* Enable sequencer, and wait for trigger */
  AD5940_ClrMCUIntFlag();  /* Clear interrupt flag generated before */

  AD5940_AFEPwrBW(AppIMPCfg.PwrMod, AFEBW_250KHZ);

  AD5940_WriteReg(REG_AFE_LPTIASW0, 0x3180);
  AppIMPCfg.IMPInited = bTRUE; /* IMP application has been initialized. */
  return AD5940ERR_OK;
}

/* Modify registers when AFE wakeup */
int32_t AppIMPRegModify(int32_t *const pData, uint32_t *pDataCount) {
  if (AppIMPCfg.NumOfData > 0) {
    AppIMPCfg.FifoDataCount += *pDataCount / 4;
    if (AppIMPCfg.FifoDataCount >= AppIMPCfg.NumOfData) {
      AD5940_WUPTCtrl(bFALSE);
      return AD5940ERR_OK;
    }
  }
  if (AppIMPCfg.StopRequired == bTRUE) {
    AD5940_WUPTCtrl(bFALSE);
    return AD5940ERR_OK;
  }
   if (AppIMPAutoRangeHstia(pData, *pDataCount) == bTRUE) {
    /*
     * Do not update WG frequency here. AppIMPDataProcess() will discard this
     * result and will not advance SweepCurrFreq/SweepNextFreq, so the next
     * WUPT trigger repeats the same frequency with the new RTIA.
     */
    return AD5940ERR_OK;
  }
  if (AppIMPCfg.SweepCfg.SweepEn) /* Need to set new frequency and set power mode */
  {
    /* Re-tune DFT length / OSR for the upcoming frequency so it still captures
     * at least 20 cycles, then update the excitation frequency. */
    uint32_t old_dftnum = AppIMPCfg.DftNum;
    uint8_t old_s2 = AppIMPCfg.ADCSinc2Osr;
    uint8_t old_s3 = AppIMPCfg.ADCSinc3Osr;
    uint32_t old_src = AppIMPCfg.DftSrc;

    AppIMPAdaptiveFilterCfg(AppIMPCfg.SweepNextFreq);

    if (AppIMPCfg.DftNum != old_dftnum || AppIMPCfg.ADCSinc2Osr != old_s2 ||
        AppIMPCfg.ADCSinc3Osr != old_s3 || AppIMPCfg.DftSrc != old_src) {
      /* Regenerate the measurement sequence (its data-ready wait depends on the
       * DFT length / OSR) and push the new filter config to live registers. The
       * AFE is awake here and the next Wupt trigger is one ODR period away. */
      AppIMPSeqMeasureRegen();
      AppIMPApplyFilterRegs();
    }

    AD5940_WGFreqCtrlS(AppIMPCfg.SweepNextFreq, AppIMPCfg.SysClkFreq);
  }
  return AD5940ERR_OK;
}

/* Depending on the data type, do appropriate data pre-process before return
 * back to controller */
int32_t AppIMPDataProcess(int32_t *const pData, uint32_t *pDataCount) {
    if (AppIMPCfg.HstiaRtiaRangeChanged == bTRUE) {
    AppIMPCfg.HstiaRtiaRangeChanged = bFALSE;
    *pDataCount = 0;
    return 0;
  }
  uint32_t DataCount = *pDataCount;
  uint32_t ImpResCount = DataCount / 4;

  fImpCalCar_Type *const pOut = (fImpCalCar_Type *)pData;
  iImpCar_Type *pSrcData = (iImpCar_Type *)pData;

  *pDataCount = 0;

  DataCount = (DataCount / 4) *
              4; /* We expect Rz+Rload, Rload and RCAL data, . One DFT result
                    has two data in FIFO, real part and imaginary part.  */

  /* Convert DFT result to int32_t type */
  for (uint32_t i = 0; i < DataCount; i++) {
    pData[i] &= 0x3ffff;
    if (pData[i] & (1L << 17)) /* Bit17 is sign bit */
    {
      pData[i] |= 0xfffc0000; /* Data is 18bit in two's complement, bit17 is the
                                 sign bit */
    }
  }
  fImpCar_Type DftRcal, DftRzRload;
  for (uint32_t i = 0; i < ImpResCount; i++) {
    pOut[i].RzReal = DftRzRload.Real = pSrcData->Real;
    pOut[i].RzImag = DftRzRload.Image = -pSrcData->Image;
    pSrcData++;
    pOut[i].RcalReal = DftRcal.Real = pSrcData->Real;
    pOut[i].RcalImag = DftRcal.Image = -pSrcData->Image;
    pSrcData++;
    // pOut[i].RcalMag = AD5940_ComplexMag(&DftRcal);
    // pOut[i].RzMag = AD5940_ComplexMag(&DftRzRload);
    // pOut[i].RcalPhase = AD5940_ComplexPhase(&DftRcal);
    // pOut[i].RzPhase = AD5940_ComplexPhase(&DftRzRload);
  }
  *pDataCount = ImpResCount;
  AppIMPCfg.FreqofData = AppIMPCfg.SweepCurrFreq;
  /* Calculate next frequency point */
  if (AppIMPCfg.SweepCfg.SweepEn == bTRUE) {
    AppIMPCfg.FreqofData = AppIMPCfg.SweepCurrFreq;
    AppIMPCfg.SweepCurrFreq = AppIMPCfg.SweepNextFreq;
    AD5940_SweepNext(&AppIMPCfg.SweepCfg, &AppIMPCfg.SweepNextFreq);
  }

  return 0;
}

/**

 */
AD5940Err AppIMPISR(void *pBuff, uint32_t *pCount) {
  uint32_t BuffCount;
  uint32_t FifoCnt;
  BuffCount = *pCount;

  *pCount = 0;

  if (AD5940_WakeUp(10) >
      10) /* Wakeup AFE by read register, read 10 times at most */
    return AD5940ERR_WAKEUP;         /* Wakeup Failed */
  AD5940_SleepKeyCtrlS(SLPKEY_LOCK); /* Prohibit AFE to enter sleep mode. */

  if (AD5940_INTCTestFlag(AFEINTC_0, AFEINTSRC_DATAFIFOTHRESH) == bTRUE) {
    /* Now there should be 4 data in FIFO */
    FifoCnt = (AD5940_FIFOGetCnt() / 4) * 4;

    if (FifoCnt > BuffCount) {
      ///@todo buffer is limited.
    }
    AD5940_FIFORd((uint32_t *)pBuff, FifoCnt);
    AD5940_INTCClrFlag(AFEINTSRC_DATAFIFOTHRESH);
    AppIMPRegModify(pBuff,
                    &FifoCnt); /* If there is need to do AFE re-configure, do it
                                  here when AFE is in active state */
    // AD5940_EnterSleepS(); /* Manually put AFE back to hibernate mode. This
    // operation only takes effect when register value is ACTIVE previously */
    AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK); /* Allow AFE to enter sleep mode. */
    /* Process data */
    AppIMPDataProcess((int32_t *)pBuff, &FifoCnt);
    *pCount = FifoCnt;
    return AD5940ERR_OK;
  }

  return AD5940ERR_OK;
}
BoolFlag isRunning(void) { return AppIMPCfg.bIsRunning; }
uint8_t calc_checksum(const char *data) {
    uint8_t chk = 0;
    while (*data) {
        chk ^= (uint8_t)(*data++);
    }
    return chk;
}

