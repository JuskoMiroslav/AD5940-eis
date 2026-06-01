# Resistor Calibration Functionality Implementation Summary

## Overview
Added comprehensive calibration resistor selection functionality to the AD5940 impedance measurement system. Users can now easily switch between four different calibration resistors (10Ω, 25.5kΩ, 100kΩ, and 1MΩ) without manually configuring switch matrices.

## Changes Made

### 1. Header File: `Core/Inc/Impedance.h`

#### New Enumeration
```c
typedef enum {
    RCAL_10OHM = 10,           // 10 ohm resistor (AIN2)
    RCAL_25P5K = 25500,        // 25.5k ohm resistor (AIN1)
    RCAL_100K = 100000,        // 100k ohm resistor (AIN3) - Default
    RCAL_1M = 1000000          // 1M ohm resistor (Internal RCALO)
} AppIMPRcalSelection;
```

#### Updated Structure Fields
Added to `AppIMPCfg_Type`:
- `uint32_t DswitchSelCal` - D-switch configuration for calibration resistor
- `uint32_t PswitchSelCal` - P-switch configuration for calibration resistor
- `uint32_t NswitchSelCal` - N-switch configuration for calibration resistor
- `uint32_t TswitchSelCal` - T-switch configuration for calibration resistor
- `AppIMPRcalSelection RcalSelection` - Currently selected calibration resistor

#### New Function Declaration
```c
AD5940Err AppIMPSetCalibrationResistor(AppIMPRcalSelection RcalSelect);
```

#### New Control Command
```c
#define IMPCTRL_RCAL_SELECT    5   /* Select calibration resistor */
```

### 2. Implementation File: `Core/Src/Impedance.c`

#### Default Configuration Update
Updated `AppIMPCfg` initialization:
- Default calibration resistor: 100kΩ (RCAL_100K)
- Default RcalVal: 100000.0
- Default calibration switches configured for AIN3

#### Function Implementation: `AppIMPSetCalibrationResistor()`
New function that:
1. Takes a calibration resistor selection as parameter
2. Configures appropriate switch settings based on selection:
   - 10Ω: Uses SWD_AIN2, SWP_AIN2, SWN_AIN0, SWT_AIN0
   - 25.5kΩ: Uses SWD_AIN1, SWP_AIN1, SWN_AIN0, SWT_AIN0
   - 100kΩ: Uses SWD_AIN3, SWP_AIN3, SWN_AIN0, SWT_AIN0
   - 1MΩ: Uses SWD_RCAL0, SWP_RCAL0, SWN_RCAL1, SWT_RCAL1
3. Updates RcalVal to the corresponding resistor value
4. Sets bParaChanged flag to trigger sequence regeneration
5. Returns AD5940ERR_OK on success, AD5940ERR_PARA on invalid input

#### Measurement Sequence Update
Modified `AppIMPSeqMeasureGen()` function:
- RCAL measurement now uses configurable calibration switch values:
  ```c
  sw_cfg.Dswitch = AppIMPCfg.DswitchSelCal;
  sw_cfg.Pswitch = AppIMPCfg.PswitchSelCal;
  sw_cfg.Nswitch = AppIMPCfg.NswitchSelCal;
  sw_cfg.Tswitch = AppIMPCfg.TswitchSelCal | SWT_TRTIA;
  ```

## Hardware Configuration
The calibration resistors must be connected to the AD5940 as follows:
- **Pin AIN2**: 10Ω calibration resistor
- **Pin AIN1**: 25.5kΩ calibration resistor
- **Pin AIN3**: 100kΩ calibration resistor
- **Internal RCAL**: 1MΩ resistor (on-chip)
- **Common Ground**: All resistors connect to AIN0 (AINO)

## Usage Examples

### Basic Usage
```c
// Select 10Ω calibration resistor
AppIMPSetCalibrationResistor(RCAL_10OHM);

// Select 25.5kΩ calibration resistor
AppIMPSetCalibrationResistor(RCAL_25P5K);

// Select 100kΩ calibration resistor (default)
AppIMPSetCalibrationResistor(RCAL_100K);

// Select 1MΩ calibration resistor
AppIMPSetCalibrationResistor(RCAL_1M);
```

### With Error Checking
```c
AD5940Err result = AppIMPSetCalibrationResistor(RCAL_100K);
if (result == AD5940ERR_OK) {
    printf("Calibration resistor changed successfully\n");
} else {
    printf("Failed to change calibration resistor\n");
}
```

## Benefits
1. **Easy Selection**: Simple function call to switch between resistors
2. **Automatic Configuration**: Switch matrix settings are handled automatically
3. **Flexible Calibration**: Supports four different impedance ranges
4. **Error Handling**: Returns error codes for invalid selections
5. **Automatic Sequence Regeneration**: Sets bParaChanged flag for next initialization

## Backward Compatibility
- Default behavior unchanged (uses 100kΩ resistor)
- Existing code continues to work without modification
- New functionality is purely additive

## Files Modified
1. `Core/Inc/Impedance.h` - Added enum, struct fields, and function declaration
2. `Core/Src/Impedance.c` - Added function implementation and default configuration

## Files Created
1. `CALIBRATION_RESISTOR_USAGE.md` - Usage documentation with examples

## Testing Recommendations
1. Verify each resistor value can be selected without errors
2. Check that RcalVal is correctly updated for each selection
3. Verify switch configuration is correctly applied in measurement sequence
4. Test sequence regeneration after resistor change
5. Measure impedance with each calibration resistor to verify accuracy
