# Calibration Resistor Selection Guide

## Overview
The impedance measurement system now supports selecting between four different calibration resistors:
- **10 Ω** (connected to AIN2)
- **25.5 kΩ** (connected to AIN1)
- **100 kΩ** (connected to AIN3)
- **1 MΩ** (internal RCAL)

## Function Signature
```c
AD5940Err AppIMPSetCalibrationResistor(AppIMPRcalSelection RcalSelect);
```

## Enumeration Values
```c
typedef enum {
    RCAL_10OHM = 10,           // 10 ohm resistor
    RCAL_25P5K = 25500,        // 25.5k ohm resistor
    RCAL_100K = 100000,        // 100k ohm resistor (default)
    RCAL_1M = 1000000          // 1M ohm resistor
} AppIMPRcalSelection;
```

## Usage Examples

### Select 10 Ω Calibration Resistor
```c
AppIMPSetCalibrationResistor(RCAL_10OHM);
```

### Select 25.5 kΩ Calibration Resistor
```c
AppIMPSetCalibrationResistor(RCAL_25P5K);
```

### Select 100 kΩ Calibration Resistor (Default)
```c
AppIMPSetCalibrationResistor(RCAL_100K);
```

### Select 1 MΩ Calibration Resistor
```c
AppIMPSetCalibrationResistor(RCAL_1M);
```

## What the Function Does
When you call `AppIMPSetCalibrationResistor()`, it:
1. Configures the switch matrix settings (D, P, N, T switches) for the selected resistor
2. Updates the `RcalVal` calibration value accordingly
3. Sets the `bParaChanged` flag to trigger sequence regeneration

## Hardware Configuration
The calibration resistors must be connected to the AD5940 as follows:
- **AIN2**: 10 Ω calibration resistor
- **AIN1**: 25.5 kΩ calibration resistor
- **AIN3**: 100 kΩ calibration resistor
- **RCAL**: 1 MΩ internal resistor (already on chip)

All resistors share a common connection at **AIN0** (AINO).

## Switch Configuration Details

### 10 Ω Resistor
```
D-switch: SWD_AIN2
P-switch: SWP_AIN2
N-switch: SWN_AIN0
T-switch: SWT_AIN0 | SWT_TRTIA
```

### 25.5 kΩ Resistor
```
D-switch: SWD_AIN1
P-switch: SWP_AIN1
N-switch: SWN_AIN0
T-switch: SWT_AIN0 | SWT_TRTIA
```

### 100 kΩ Resistor (Default)
```
D-switch: SWD_AIN3
P-switch: SWP_AIN3
N-switch: SWN_AIN0
T-switch: SWT_AIN0 | SWT_TRTIA
```

### 1 MΩ Resistor
```
D-switch: SWD_RCAL0
P-switch: SWP_RCAL0
N-switch: SWN_RCAL1
T-switch: SWT_RCAL1 | SWT_TRTIA
```

## Return Values
- `AD5940ERR_OK`: Successfully configured the calibration resistor
- `AD5940ERR_PARA`: Invalid resistor selection provided
