/*
 * EXAMPLE: How to Use the Calibration Resistor Selection Function
 * 
 * This example shows how to integrate the new calibration resistor
 * selection functionality into your main application.
 */

#include "Impedance.h"

// Example 1: Simple resistor selection before starting measurement
void example_basic_usage(void)
{
    uint32_t AppBuff[512];
    
    // Select 100k calibration resistor (default)
    AppIMPSetCalibrationResistor(RCAL_100K);
    
    // Initialize the impedance measurement system
    AppIMPInit(AppBuff, 512);
    
    // Start the measurement
    AppIMPCtrl(IMPCTRL_START, 0);
}

// Example 2: Switching between different calibration resistors
void example_resistor_switching(void)
{
    uint32_t AppBuff[512];
    
    // Initialize with default (100k) resistor
    AppIMPInit(AppBuff, 512);
    AppIMPCtrl(IMPCTRL_START, 0);
    
    // ... perform measurements for a while ...
    
    // Switch to 10 ohm resistor for high impedance measurements
    AppIMPCtrl(IMPCTRL_STOPNOW, 0);
    AppIMPSetCalibrationResistor(RCAL_10OHM);
    AppIMPInit(AppBuff, 512);  // Re-initialize with new settings
    AppIMPCtrl(IMPCTRL_START, 0);
    
    // ... perform measurements ...
    
    // Switch to 1M ohm resistor for very high impedance
    AppIMPCtrl(IMPCTRL_STOPNOW, 0);
    AppIMPSetCalibrationResistor(RCAL_1M);
    AppIMPInit(AppBuff, 512);
    AppIMPCtrl(IMPCTRL_START, 0);
}

// Example 3: With error checking
void example_with_error_checking(void)
{
    AD5940Err result;
    uint32_t AppBuff[512];
    
    // Try to select a specific resistor
    result = AppIMPSetCalibrationResistor(RCAL_25P5K);
    
    if (result == AD5940ERR_OK)
    {
        printf("Successfully selected 25.5k calibration resistor\n");
        printf("RcalVal = %.0f Ohm\n", ((AppIMPCfg_Type*)&AppIMPCfg)->RcalVal);
        
        // Initialize and start measurement
        AppIMPInit(AppBuff, 512);
        AppIMPCtrl(IMPCTRL_START, 0);
    }
    else if (result == AD5940ERR_PARA)
    {
        printf("ERROR: Invalid calibration resistor selection\n");
    }
    else
    {
        printf("ERROR: Failed to set calibration resistor\n");
    }
}

// Example 4: Programmatic resistor selection based on impedance range
void example_automatic_resistor_selection(float expected_impedance)
{
    uint32_t AppBuff[512];
    AppIMPRcalSelection selected_resistor = RCAL_100K;  // default
    
    // Select appropriate calibration resistor based on expected impedance
    if (expected_impedance < 50)
    {
        selected_resistor = RCAL_10OHM;
        printf("Selecting 10 ohm calibration resistor\n");
    }
    else if (expected_impedance < 1000)
    {
        selected_resistor = RCAL_25P5K;
        printf("Selecting 25.5k ohm calibration resistor\n");
    }
    else if (expected_impedance < 100000)
    {
        selected_resistor = RCAL_100K;
        printf("Selecting 100k ohm calibration resistor\n");
    }
    else
    {
        selected_resistor = RCAL_1M;
        printf("Selecting 1M ohm calibration resistor\n");
    }
    
    // Apply the selection
    AppIMPSetCalibrationResistor(selected_resistor);
    AppIMPInit(AppBuff, 512);
    AppIMPCtrl(IMPCTRL_START, 0);
}

// Example 5: Complete measurement sequence with multiple resistor ranges
void example_full_sweep(void)
{
    uint32_t AppBuff[512];
    AppIMPRcalSelection resistors[] = {RCAL_10OHM, RCAL_25P5K, RCAL_100K, RCAL_1M};
    const char* resistor_names[] = {"10 Ohm", "25.5k Ohm", "100k Ohm", "1M Ohm"};
    
    for (int i = 0; i < 4; i++)
    {
        printf("\n--- Measuring with %s calibration resistor ---\n", resistor_names[i]);
        
        // Select resistor
        AppIMPSetCalibrationResistor(resistors[i]);
        
        // Re-initialize system
        AppIMPCtrl(IMPCTRL_STOPNOW, 0);
        AppIMPInit(AppBuff, 512);
        AppIMPCtrl(IMPCTRL_START, 0);
        
        // Perform measurements (in real application, use interrupt/timer)
        // ...
        
        // Stop measurement
        AppIMPCtrl(IMPCTRL_STOPNOW, 0);
    }
}

/*
 * IMPORTANT NOTES:
 * 
 * 1. After calling AppIMPSetCalibrationResistor(), you MUST call AppIMPInit()
 *    again to regenerate the measurement sequence with the new settings.
 * 
 * 2. Always call AppIMPCtrl(IMPCTRL_STOPNOW, 0) before changing the
 *    calibration resistor if measurements are in progress.
 * 
 * 3. The RcalVal is automatically updated when you change the resistor.
 *    This value is used in impedance calculations:
 *    Impedance = RcalVal * (DFT_Result / DFT_Rcal)
 * 
 * 4. Default calibration resistor is 100kΩ (RCAL_100K).
 * 
 * 5. For best results, select a calibration resistor that is close in
 *    magnitude to your sensor's expected impedance.
 * 
 * 6. Hardware must have resistors connected to the appropriate pins:
 *    - 10Ω to AIN2
 *    - 25.5kΩ to AIN1
 *    - 100kΩ to AIN3
 *    - 1MΩ (internal RCAL)
 *    All connected to common ground (AIN0)
 */
