# UART Command for RCAL Selection

## Overview
A new UART command `setrcal` has been added to allow remote selection of the calibration resistor without recompilation.

## Command Syntax

```
setrcal <resistor_value>
```

## Parameters

| Value | Resistor | Pin | Description |
|-------|----------|-----|-------------|
| `10` | 10 Ω | AIN2 | 10 ohm resistor |
| `25.5k` | 25.5 kΩ | AIN1 | 25.5k ohm resistor |
| `100k` | 100 kΩ | AIN3 | 100k ohm resistor (default) |
| `1m` | 1 MΩ | Internal | 1M ohm internal resistor |

**Note:** Values are case-insensitive (e.g., `25.5K` or `25.5k` both work)

## Examples

### Select 10 Ω Resistor
```
setrcal 10
```

### Select 25.5 kΩ Resistor
```
setrcal 25.5k
```

### Select 100 kΩ Resistor (Default)
```
setrcal 100k
```

### Select 1 MΩ Resistor
```
setrcal 1m
```

## Response Format

### Success Response
The system responds with a packet formatted as:
```
RCAL=<resistor_value>
```

Example:
```
RCAL=100k Ohm
```

### Error Response
If an invalid value is provided:
```
ERR=Failed to set RCAL
```

### Usage Help
If no parameters provided:
```
Usage: setrcal <10|25.5k|100k|1m>
  10   - 10 ohm resistor (AIN2)
  25.5k - 25.5k ohm resistor (AIN1)
  100k - 100k ohm resistor (AIN3)
  1m   - 1M ohm resistor (internal)
```

## What the Command Does

1. **Stops current measurement** - Gracefully halts any ongoing impedance measurement
2. **Selects resistor** - Configures the AD5940 switch matrix for the selected resistor
3. **Updates RcalVal** - Sets the calibration value used in impedance calculations
4. **Re-initializes** - Regenerates measurement sequences with the new configuration
5. **Confirms selection** - Sends a confirmation packet back via UART

## Integration in Terminal/Python

### UART Terminal Example
```
$ setrcal 100k
RCAL=100k Ohm

$ setrcal 1m
RCAL=1M Ohm
```

### Python Script Example
```python
import serial
import time

# Open serial connection
ser = serial.Serial('/dev/ttyUSB0', 230400, timeout=1)

# Select 100k resistor
ser.write(b'setrcal 100k\n')
time.sleep(0.5)
response = ser.readline().decode()
print(response)

# Select 1M resistor
ser.write(b'setrcal 1m\n')
time.sleep(0.5)
response = ser.readline().decode()
print(response)

ser.close()
```

## Implementation Details

### Command Function
```c
uint32_t command_rcal_select(char *param1_str, double para2)
```

Located in: `Core/Src/AD5940Main.c`

### Registered Command
Command table entry:
- **Name:** `setrcal`
- **Description:** Select calibration resistor (10, 25.5k, 100k, 1m)
- **Handler:** `command_rcal_select`

### Related Functions
- `AppIMPSetCalibrationResistor()` - Sets resistor configuration
- `AppIMPInit()` - Re-initializes measurement with new settings
- `AppIMPCtrl(IMPCTRL_STOPNOW, 0)` - Stops current measurement

## Serial Port Configuration

- **Baud Rate:** 230400
- **Data Bits:** 8
- **Stop Bits:** 1
- **Parity:** None
- **Flow Control:** None

## Recommendations

1. **Always stop measurement first** - Command automatically handles this
2. **Wait for confirmation** - Allow 100-200ms for response
3. **Close to sensor impedance** - Select RCAL resistor close to expected sensor impedance
4. **Default is 100k** - Suitable for most electrochemical sensors

## Files Modified

- `Core/Src/AD5940Main.c` - Added `command_rcal_select()` function
- `Core/Src/UARTCmd.c` - Registered command in table and updated CMDTABLE_SIZE

## Related Commands

- `setcfg` - Set individual configuration parameters
- `getcfg` - Get current configuration
- `help` - Display available commands
