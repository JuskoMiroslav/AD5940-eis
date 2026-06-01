# Calibration Resistor Selection - Quick Reference

## Function Call
```c
AD5940Err AppIMPSetCalibrationResistor(AppIMPRcalSelection RcalSelect);
```

## Available Resistors
| Constant | Value | Pin | Resistance |
|----------|-------|-----|-----------|
| `RCAL_10OHM` | 10 | AIN2 | 10 Ω |
| `RCAL_25P5K` | 25500 | AIN1 | 25.5 kΩ |
| `RCAL_100K` | 100000 | AIN3 | 100 kΩ (default) |
| `RCAL_1M` | 1000000 | Internal | 1 MΩ |

## Quick Examples

### Select 10Ω
```c
AppIMPSetCalibrationResistor(RCAL_10OHM);
```

### Select 25.5kΩ
```c
AppIMPSetCalibrationResistor(RCAL_25P5K);
```

### Select 100kΩ
```c
AppIMPSetCalibrationResistor(RCAL_100K);
```

### Select 1MΩ
```c
AppIMPSetCalibrationResistor(RCAL_1M);
```

## Return Values
- `AD5940ERR_OK` - Success
- `AD5940ERR_PARA` - Invalid parameter

## Step-by-Step Integration

1. **Stop Current Measurement** (if running)
   ```c
   AppIMPCtrl(IMPCTRL_STOPNOW, 0);
   ```

2. **Select New Resistor**
   ```c
   AppIMPSetCalibrationResistor(RCAL_100K);
   ```

3. **Re-initialize**
   ```c
   AppIMPInit(AppBuff, BufferSize);
   ```

4. **Start Measurement**
   ```c
   AppIMPCtrl(IMPCTRL_START, 0);
   ```

## Hardware Setup
All resistors must be connected with common ground at **AIN0 (AINO)**:
- **10Ω resistor** → AIN2
- **25.5kΩ resistor** → AIN1
- **100kΩ resistor** → AIN3
- **1MΩ** → Internal RCAL (on-chip)

## Key Implementation Details

### Default Configuration
- Resistor: 100kΩ (RCAL_100K)
- Pin: AIN3
- Value: 100000.0 Ohms

### Switch Settings (Automatically Configured)
The function automatically sets these for each resistor:

**10Ω (AIN2):**
- D: SWD_AIN2
- P: SWP_AIN2
- N: SWN_AIN0
- T: SWT_AIN0

**25.5kΩ (AIN1):**
- D: SWD_AIN1
- P: SWP_AIN1
- N: SWN_AIN0
- T: SWT_AIN0

**100kΩ (AIN3):**
- D: SWD_AIN3
- P: SWP_AIN3
- N: SWN_AIN0
- T: SWT_AIN0

**1MΩ (Internal):**
- D: SWD_RCAL0
- P: SWP_RCAL0
- N: SWN_RCAL1
- T: SWT_RCAL1

## Important Reminders
✓ Call AppIMPInit() after changing resistor
✓ Stop measurement before switching resistors
✓ Ensure hardware has all resistors connected
✓ Select resistor close to sensor impedance for best accuracy
✓ Default is 100kΩ - suitable for most applications

## Related Files
- Implementation: `Core/Src/Impedance.c`
- Header: `Core/Inc/Impedance.h`
- Full Examples: `USAGE_EXAMPLES.c`
- Detailed Docs: `CALIBRATION_RESISTOR_USAGE.md`
