# UART Commands Reference

## Quick Command List

| Command | Description | Example |
|---------|-------------|---------|
| `help` or `?` | Show all available commands | `help` |
| `setcfg` | Set configuration parameter | `setcfg RcalVal 100000` |
| `getcfg` | Get configuration parameter(s) | `getcfg RcalVal` |
| `setrcal` | Select calibration resistor | `setrcal 100k` |
| `EISStart` | Start EIS measurement | `EISStart` |
| `EISStop` | Stop EIS measurement | `EISStop` |
| `*IDN?` | Get device ID | `*IDN?` |

---

## Detailed Commands

### 1. RCAL Selection - `setrcal` ⭐ NEW

**Purpose:** Select calibration resistor without code changes

**Syntax:**
```
setrcal <resistor_value>
```

**Values:**
| Value | Resistor | Pin |
|-------|----------|-----|
| `10` | 10 Ω | AIN2 |
| `25.5k` | 25.5 kΩ | AIN1 |
| `100k` | 100 kΩ | AIN3 |
| `1m` | 1 MΩ | Internal |

**Examples:**
```
setrcal 10
setrcal 25.5K       (case-insensitive)
setrcal 100k
setrcal 1M
```

**Response:**
```
RCAL=100k Ohm
```

**Notes:**
- Automatically stops current measurement
- Re-initializes sequencer with new settings
- Confirmation sent via UART

---

### 2. Configuration - `setcfg` & `getcfg`

**Get Current Configuration:**
```
getcfg              # Show all parameters
getcfg RcalVal      # Show specific parameter
```

**Set Configuration:**
```
setcfg RcalVal 100000    # Set RCAL value to 100k
setcfg SinFreq 50000     # Set frequency to 50kHz
setcfg DacVoltPP 300     # Set DAC voltage
```

**Common Parameters:**
- `RcalVal` - Calibration resistor value (Ohm)
- `SinFreq` - Excitation frequency (Hz)
- `DacVoltPP` - DAC voltage peak-to-peak (mV)
- `BiasVolt` - DC bias voltage (mV)
- `ImpODR` - Output data rate (Hz)

---

### 3. Measurement Control

**Start Measurement:**
```
EISStart
```

**Stop Measurement:**
```
EISStop
```

**Get Current Frequency (during sweep):**
```
# Sent automatically with each data point
# Format: FREQ=50000
```

---

### 4. Device Information

**Get Device ID:**
```
*IDN?
```

**Response:**
```
IDN=ADI,AD5940-EIS,STM32,v1.0
```

---

## Typical Workflow

### Workflow 1: Simple Measurement
```
1. setrcal 100k         # Select 100k resistor
2. EISStart             # Start measurement
3. (measurement runs)
4. EISStop              # Stop measurement
```

### Workflow 2: Frequency Sweep
```
1. setrcal 100k                         # Select calibration resistor
2. setcfg SweepEn 1                    # Enable sweep
3. setcfg SweepStart 1000               # Start at 1kHz
4. setcfg SweepStop 100000              # Stop at 100kHz
5. setcfg SweepPoints 50                # 50 frequency points
6. EISStart                             # Start sweep
7. (data streaming)
8. EISStop                              # Stop when done
```

### Workflow 3: Multiple Resistor Measurements
```
1. setrcal 10        # Select 10 ohm for very low impedance
2. EISStart
3. (measurement 1)
4. EISStop

5. setrcal 100k      # Switch to 100k for general impedance
6. EISStart
7. (measurement 2)
8. EISStop

9. setrcal 1m        # Switch to 1M for high impedance
10. EISStart
11. (measurement 3)
12. EISStop
```

---

## Response Packet Format

### Data Packets
```
IMP=<frequency>,<magnitude>,<phase>
```
Example:
```
IMP=10000.00,1234.56,0.123456
```

### Configuration Packets
```
CFG=<parameter>=<value>
```
Example:
```
CFG=RcalVal=100000.000000
```

### RCAL Selection Packets
```
RCAL=<resistor_value>
```
Example:
```
RCAL=100k Ohm
```

### Error Packets
```
ERR=<error_message>
```
Example:
```
ERR=Failed to set RCAL
```

### Device ID Packets
```
IDN=ADI,AD5940-EIS,STM32,v1.0
```

---

## Serial Port Settings

- **Port:** /dev/ttyUSB0 (Linux) or COM3 (Windows)
- **Baud Rate:** 230400 bps
- **Data Bits:** 8
- **Stop Bits:** 1
- **Parity:** None
- **Flow Control:** None
- **Line Ending:** `\n` (LF)

---

## Common Use Cases

### Use Case 1: Sensor Calibration
```
# Start with 10 Ohm for calibration
setrcal 10
EISStart
# Collect reference measurement
# ...
EISStop
```

### Use Case 2: Impedance Range Test
```
# Test across impedance range using different RCAL values
setrcal 10      # For Z < 100 Ohm
setrcal 25.5k   # For 100 Ohm - 10k Ohm
setrcal 100k    # For 10k - 100k Ohm
setrcal 1m      # For Z > 100k Ohm
```

### Use Case 3: Frequency Response Sweep
```
setrcal 100k
setcfg SweepEn 1
setcfg SweepStart 100
setcfg SweepStop 1000000
setcfg SweepPoints 100
EISStart
# Collect 100-point frequency sweep
```

---

## Tips & Tricks

1. **Always select appropriate RCAL** - Choose resistor close to your sensor impedance
2. **Stop before switching RCAL** - Use `EISStop` before `setrcal`
3. **Check RcalVal after selection** - Verify with `getcfg RcalVal`
4. **Use help command** - Type `help` to see all commands anytime
5. **Case insensitive** - Commands work in any case (SETRCAL, SetRcal, etc.)

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No response to commands | Check UART connection and baud rate (230400) |
| "Invalid resistor value" | Use values: 10, 25.5k, 100k, 1m |
| Measurement doesn't change | Verify selection with `getcfg RcalVal` |
| Command not recognized | Check spelling and use `help` |
| Data not flowing | Ensure `EISStart` was sent |

---

## File References

- Implementation: `Core/Src/AD5940Main.c`, `Core/Src/UARTCmd.c`
- API Docs: `CALIBRATION_RESISTOR_USAGE.md`
- UART Details: `UART_RCAL_COMMAND.md`
- Integration: `UART_INTEGRATION_SUMMARY.md`
