#ifndef POLYNOMIAL_CALIBRATION_H
#define POLYNOMIAL_CALIBRATION_H
#define DEBUG_CALIBRATION 0
#if DEBUG_CALIBRATION
  #define DBG_PRINT(...)   Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
  #define DBG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
  #define DBG_PRINTF(...)
#endif
#include <Arduino.h>
#include <EEPROM.h>

// 3rd Degree Polynomial Calibration System
// GRAVITY = coeff3 * Tilt^3 + coeff2 * Tilt^2 + coeff1 * Tilt + coeff0

// EEPROM addresses for storing calibration coefficients
#define EEPROM_CALIB_MAGIC 0
#define EEPROM_COEFF3 4
#define EEPROM_COEFF2 8
#define EEPROM_COEFF1 12
#define EEPROM_COEFF0 16
#define EEPROM_CALIB_VERSION 20
#define EEPROM_NORM_OFFSET 21
#define EEPROM_NORM_SCALE 25

// Magic number to verify valid calibration data
#define CALIB_MAGIC 0x43414C49 // "CALI" in hex

// Calibration data structure
struct CalibrationCoefficients {
    float coeff3;  // Tilt^3 coefficient
    float coeff2;  // Tilt^2 coefficient
    float coeff1;  // Tilt coefficient
    float coeff0;  // Constant term
    bool isValid;  // Flag indicating if calibration is valid
};

// Global calibration coefficients
CalibrationCoefficients calibCoeffs;

// Normalization parameters for polynomial calculation
float normOffset = 0.0;
float normScale = 1.0;

// Calibration measurement points
struct CalibrationPoint {
    float tilt;
    float gravity;
    float temperature;  // Temperature at measurement point
};

// Maximum calibration points (for robust fitting)
#define MAX_CALIB_POINTS 10
CalibrationPoint calibPoints[MAX_CALIB_POINTS];
int numCalibPoints = 0;

/**
 * Initialize calibration system
 * Load coefficients from EEPROM if available
 */
void initCalibration() {
    DBG_PRINTLN("=== INITIALIZING CALIBRATION SYSTEM ===");
    EEPROM.begin(64); // Reserve 64 bytes for calibration data
    
    // Check if valid calibration data exists
    uint32_t magic;
    EEPROM.get(EEPROM_CALIB_MAGIC, magic);
    DBG_PRINTF("EEPROM magic value: 0x%08X (expected: 0x%08X)\n", magic, CALIB_MAGIC);
    
    if (magic == CALIB_MAGIC) {
        DBG_PRINTLN("✓ Valid calibration data found in EEPROM");
        
        // Load coefficients from EEPROM
        EEPROM.get(EEPROM_COEFF3, calibCoeffs.coeff3);
        EEPROM.get(EEPROM_COEFF2, calibCoeffs.coeff2);
        EEPROM.get(EEPROM_COEFF1, calibCoeffs.coeff1);
        EEPROM.get(EEPROM_COEFF0, calibCoeffs.coeff0);
        calibCoeffs.isValid = true;
        
        // Load normalization parameters from EEPROM
        EEPROM.get(EEPROM_NORM_OFFSET, normOffset);
        EEPROM.get(EEPROM_NORM_SCALE, normScale);
        
        // Validate normalization parameters - reset to defaults if invalid (NaN or zero)
        bool normParamsValid = true;
        if (isnan(normOffset) || isnan(normScale) || normScale == 0.0) {
            DBG_PRINTLN("⚠ Invalid normalization parameters detected in EEPROM");
            normOffset = 0.0;
            normScale = 1.0;
            normParamsValid = false;
            
            // Save the corrected values back to EEPROM
            EEPROM.put(EEPROM_NORM_OFFSET, normOffset);
            EEPROM.put(EEPROM_NORM_SCALE, normScale);
            EEPROM.commit();
            DBG_PRINTLN("✓ Normalization parameters reset to defaults and saved to EEPROM");
        }
        
        DBG_PRINTLN("=== CALIBRATION COEFFICIENTS LOADED ===");
        DBG_PRINTF("coeff3: %.12e\n", calibCoeffs.coeff3);
        DBG_PRINTF("coeff2: %.12e\n", calibCoeffs.coeff2);
        DBG_PRINTF("coeff1: %.12e\n", calibCoeffs.coeff1);
        DBG_PRINTF("coeff0: %.12e\n", calibCoeffs.coeff0);
        DBG_PRINTLN("=== NORMALIZATION PARAMETERS LOADED ===");
        DBG_PRINTF("normOffset: %.6f %s\n", normOffset, normParamsValid ? "" : "(reset to default)");
        DBG_PRINTF("normScale: %.6f %s\n", normScale, normParamsValid ? "" : "(reset to default)");
        
        // Show normalized values for typical tilt ranges
        DBG_PRINTLN("=== NORMALIZATION RANGE ANALYSIS ===");
        DBG_PRINTF("norm(Tilt) = (Tilt - %.2f) / %.2f\n", normOffset, normScale);
        DBG_PRINTLN("Tilt Range | Normalized Value");
        DBG_PRINTLN("-----------|-----------------");
        DBG_PRINTF("0°         | %+.3f\n", (0.0 - normOffset) / normScale);
        DBG_PRINTF("32°        | %+.3f\n", (32.0 - normOffset) / normScale);
        DBG_PRINTF("44.72°     | %+.3f (center)\n", (44.72 - normOffset) / normScale);
        DBG_PRINTF("75°        | %+.3f\n", (75.0 - normOffset) / normScale);
        DBG_PRINTF("90°        | %+.3f\n", (90.0 - normOffset) / normScale);
        
        // Check if typical ranges are within reasonable bounds (-2 to +2)
        float norm0 = (0.0 - normOffset) / normScale;
        float norm32 = (32.0 - normOffset) / normScale;
        float norm75 = (75.0 - normOffset) / normScale;
        float norm90 = (90.0 - normOffset) / normScale;
        
        if (fabs(norm0) > 3.0 || fabs(norm32) > 3.0 || fabs(norm75) > 3.0 || fabs(norm90) > 3.0) {
            DBG_PRINTLN("⚠ WARNING: Some tilt angles produce extreme normalized values (> ±3.0)");
            DBG_PRINTLN("  This may cause polynomial instability or unexpected behavior");
            DBG_PRINTLN("  Consider recalibrating with tilt angles closer to your usage range");
        } else {
            DBG_PRINTLN("✓ Normalization range appears reasonable");
        }
        DBG_PRINTLN("=== CALIBRATION LOADING COMPLETE ===");
    } else {
        DBG_PRINTLN("✗ No valid calibration found in EEPROM");
        
        // Initialize with default coefficients (linear approximation)
        calibCoeffs.coeff3 = 0.0;
        calibCoeffs.coeff2 = 0.0;
        calibCoeffs.coeff1 = 0.01;  // Default linear coefficient
        calibCoeffs.coeff0 = 1.0;   // Default offset
        calibCoeffs.isValid = false;
        
        // Initialize default normalization parameters
        normOffset = 0.0;
        normScale = 1.0;
        
        DBG_PRINTLN("=== USING DEFAULT CALIBRATION ===");
        DBG_PRINTF("coeff3: %.12e\n", calibCoeffs.coeff3);
        DBG_PRINTF("coeff2: %.12e\n", calibCoeffs.coeff2);
        DBG_PRINTF("coeff1: %.12e\n", calibCoeffs.coeff1);
        DBG_PRINTF("coeff0: %.12e\n", calibCoeffs.coeff0);
        DBG_PRINTLN("=== DEFAULT NORMALIZATION PARAMETERS ===");
        DBG_PRINTF("normOffset: %.6f\n", normOffset);
        DBG_PRINTF("normScale: %.6f\n", normScale);
        DBG_PRINTLN("=== DEFAULT CALIBRATION COMPLETE ===");
    }
}

/**
 * Add a calibration measurement point
 * @param tilt Measured tilt angle
 * @param gravity Known gravity value
 * @param temperature Temperature at measurement (optional, defaults to 20.0°C)
 */
void addCalibrationPoint(float tilt, float gravity, float temperature = 20.0) {
    if (numCalibPoints < MAX_CALIB_POINTS) {
        calibPoints[numCalibPoints].tilt = tilt;
        calibPoints[numCalibPoints].gravity = gravity;
        calibPoints[numCalibPoints].temperature = temperature;
        numCalibPoints++;
        
        DBG_PRINTF("Added calibration point %d: Tilt=%.2f°, Gravity=%.3f, Temp=%.1f°C\n", 
                     numCalibPoints, tilt, gravity, temperature);
    } else {
        DBG_PRINTLN("Maximum calibration points reached");
    }
}

/**
 * Calculate 3rd degree polynomial coefficients using least squares method
 * @return true if calculation successful, false otherwise
 */
bool calculatePolynomialCoefficients() {
    if (numCalibPoints < 4) {
        DBG_PRINTLN("Need at least 4 calibration points for 3rd degree polynomial");
        return false;
    }
    
    DBG_PRINTLN("Calculating 3rd degree polynomial coefficients...");
    
    // Build matrices for least squares solution: [X][a] = [y]
    // where X is the Vandermonde matrix [x^3, x^2, x, 1]
    
    float sum_x = 0, sum_x2 = 0, sum_x3 = 0, sum_x4 = 0, sum_x5 = 0, sum_x6 = 0;
    float sum_y = 0, sum_xy = 0, sum_x2y = 0, sum_x3y = 0;
    
    // Calculate sums
    for (int i = 0; i < numCalibPoints; i++) {
        float x = calibPoints[i].tilt;
        float y = calibPoints[i].gravity;
        float x2 = x * x;
        float x3 = x2 * x;
        float x4 = x3 * x;
        float x5 = x4 * x;
        float x6 = x5 * x;
        
        sum_x += x;
        sum_x2 += x2;
        sum_x3 += x3;
        sum_x4 += x4;
        sum_x5 += x5;
        sum_x6 += x6;
        
        sum_y += y;
        sum_xy += x * y;
        sum_x2y += x2 * y;
        sum_x3y += x3 * y;
    }
    
    int n = numCalibPoints;
    
    // Build the normal equations matrix: [X^T * X] * a = [X^T * y]
    // For cubic polynomial: 4x4 matrix
    
    float matrix[4][5] = {
        {sum_x6, sum_x5, sum_x4, sum_x3, sum_x3y},
        {sum_x5, sum_x4, sum_x3, sum_x2, sum_x2y},
        {sum_x4, sum_x3, sum_x2, sum_x,  sum_xy},
        {sum_x3, sum_x2, sum_x,  n,      sum_y}
    };
    
    // Solve using Gaussian elimination
    for (int col = 0; col < 4; col++) {
        // Polynomial calculation - no watchdog needed for this operation
        
        // Find pivot row
        int pivot = col;
        for (int row = col + 1; row < 4; row++) {
            if (fabs(matrix[row][col]) > fabs(matrix[pivot][col])) {
                pivot = row;
            }
        }
        
        // Swap rows if needed
        if (pivot != col) {
            for (int i = col; i < 5; i++) {
                float temp = matrix[col][i];
                matrix[col][i] = matrix[pivot][i];
                matrix[pivot][i] = temp;
            }
        }
        
        // Eliminate column
        for (int row = col + 1; row < 4; row++) {
            float factor = matrix[row][col] / matrix[col][col];
            for (int i = col; i < 5; i++) {
                matrix[row][i] -= factor * matrix[col][i];
            }
        }
    }
    
    // Back substitution
    float solution[4];
    for (int row = 3; row >= 0; row--) {
        solution[row] = matrix[row][4];
        for (int col = row + 1; col < 4; col++) {
            solution[row] -= matrix[row][col] * solution[col];
        }
        solution[row] /= matrix[row][row];
    }
    
    // Store coefficients
    calibCoeffs.coeff3 = solution[0];
    calibCoeffs.coeff2 = solution[1];
    calibCoeffs.coeff1 = solution[2];
    calibCoeffs.coeff0 = solution[3];
    calibCoeffs.isValid = true;
    
    DBG_PRINTLN("3rd Degree Polynomial Coefficients Calculated:");
    DBG_PRINTF("GRAVITY = %.12e * Tilt^3 + %.12e * Tilt^2 + %.12e * Tilt + %.12e\n",
                 calibCoeffs.coeff3, calibCoeffs.coeff2, calibCoeffs.coeff1, calibCoeffs.coeff0);
    
    return true;
}

/**
 * Save calibration coefficients to EEPROM
 */
void saveCalibrationCoefficients() {
    if (!calibCoeffs.isValid) {
        DBG_PRINTLN("Cannot save invalid calibration coefficients");
        return;
    }
    
    DBG_PRINTLN("=== SAVING CALIBRATION TO EEPROM ===");
    
    // Write magic number
    EEPROM.put(EEPROM_CALIB_MAGIC, CALIB_MAGIC);
    
    // Write coefficients
    EEPROM.put(EEPROM_COEFF3, calibCoeffs.coeff3);
    EEPROM.put(EEPROM_COEFF2, calibCoeffs.coeff2);
    EEPROM.put(EEPROM_COEFF1, calibCoeffs.coeff1);
    EEPROM.put(EEPROM_COEFF0, calibCoeffs.coeff0);
    
    // Write normalization parameters
    EEPROM.put(EEPROM_NORM_OFFSET, normOffset);
    EEPROM.put(EEPROM_NORM_SCALE, normScale);
    
    // Write version
    uint8_t version = 1;
    EEPROM.put(EEPROM_CALIB_VERSION, version);
    
    bool success = EEPROM.commit();
    
    if (success) {
        DBG_PRINTLN("✓ Calibration coefficients and normalization parameters saved to EEPROM");
        DBG_PRINTF("Saved normOffset: %.6f\n", normOffset);
        DBG_PRINTF("Saved normScale: %.6f\n", normScale);
        DBG_PRINTLN("=== CALIBRATION SAVE COMPLETE ===");
    } else {
        DBG_PRINTLN("✗ ERROR: Failed to save calibration data to EEPROM");
    }
}

/**
 * Calculate gravity from tilt using calibrated 3rd degree polynomial
 * @param tilt Tilt angle in degrees
 * @return Calculated gravity value
 */
float calculateGravity(float tilt) {
    if (!calibCoeffs.isValid) {
        // Fallback to linear approximation
        return 0.01 * tilt + 1.0;
    }
    
    // Apply normalization: norm(Tilt) = (Tilt - offset) / scale
    float normTilt = (tilt - normOffset) / normScale;
    
    float normTilt2 = normTilt * normTilt;
    float normTilt3 = normTilt2 * normTilt;
    
    return calibCoeffs.coeff3 * normTilt3 + 
           calibCoeffs.coeff2 * normTilt2 + 
           calibCoeffs.coeff1 * normTilt + 
           calibCoeffs.coeff0;
}

/**
 * Clear all calibration points and reset coefficients
 */
void resetCalibration() {
    numCalibPoints = 0;
    calibCoeffs.isValid = false;
    calibCoeffs.coeff3 = 0.0;
    calibCoeffs.coeff2 = 0.0;
    calibCoeffs.coeff1 = 0.01;
    calibCoeffs.coeff0 = 1.0;
    
    // Clear EEPROM
    EEPROM.put(EEPROM_CALIB_MAGIC, 0);
    EEPROM.commit();
    
    DBG_PRINTLN("Calibration reset to defaults");
}

/**
 * Test calibration accuracy with stored points
 */
void testCalibrationAccuracy() {
    if (!calibCoeffs.isValid) {
        DBG_PRINTLN("No valid calibration to test");
        return;
    }
    
    DBG_PRINTLN("Calibration Accuracy Test:");
    DBG_PRINTLN("Point | Tilt(°) | Actual | Calculated | Error");
    DBG_PRINTLN("-------------------------------------------");
    
    float totalError = 0;
    for (int i = 0; i < numCalibPoints; i++) {
        float calculated = calculateGravity(calibPoints[i].tilt);
        float error = fabs(calculated - calibPoints[i].gravity);
        totalError += error;
        
        DBG_PRINTF("%4d  | %6.2f | %6.3f | %10.3f | %6.6f\n",
                     i + 1, calibPoints[i].tilt, calibPoints[i].gravity, calculated, error);
    }
    
    float avgError = totalError / numCalibPoints;
    DBG_PRINTF("Average Error: %.6f\n", avgError);
}

/**
 * Test polynomial behavior across typical tilt ranges
 */
void testPolynomialRange() {
    if (!calibCoeffs.isValid) {
        DBG_PRINTLN("No valid calibration to test");
        return;
    }
    
    DBG_PRINTLN("=== POLYNOMIAL BEHAVIOR TEST ===");
    DBG_PRINTLN("Testing gravity calculation across typical tilt ranges:");
    DBG_PRINTLN("Tilt(°) | Normalized | Calculated SG");
    DBG_PRINTLN("--------|------------|--------------");
    
    // Test key points in your usage ranges
    float testAngles[] = {0.0, 10.0, 20.0, 32.0, 44.72, 75.0, 85.0, 90.0};
    int numTests = sizeof(testAngles) / sizeof(testAngles[0]);
    
    for (int i = 0; i < numTests; i++) {
        float tilt = testAngles[i];
        float normTilt = (tilt - normOffset) / normScale;
        float gravity = calculateGravity(tilt);
        
        DBG_PRINTF("%6.1f  | %10.3f | %12.6f\n", tilt, normTilt, gravity);
        
        // Check for obviously wrong values
        if (gravity < 0.5 || gravity > 2.0) {
            DBG_PRINTF("  ⚠ WARNING: Unreasonable gravity value at %.1f°\n", tilt);
        }
    }
    
    // Test for monotonic behavior in expected ranges
    DBG_PRINTLN("\n=== MONOTONIC BEHAVIOR CHECK ===");
    
    // Check 0-32° range (should be increasing: higher tilt = higher SG)
    DBG_PRINTLN("Checking 0-32° range:");
    float prevGravity = calculateGravity(0.0);
    bool monotonic32 = true;
    for (float tilt = 5.0; tilt <= 32.0; tilt += 5.0) {
        float currentGravity = calculateGravity(tilt);
        if (currentGravity < prevGravity) {  // If gravity decreases as tilt increases (wrong direction)
            DBG_PRINTF("  ⚠ Non-monotonic at %.1f°: %.6f -> %.6f (SG should increase with tilt)\n", tilt, prevGravity, currentGravity);
            monotonic32 = false;
        }
        prevGravity = currentGravity;
    }
    if (monotonic32) {
        DBG_PRINTLN("  ✓ 0-32° range shows expected monotonic behavior (SG increases with tilt)");
    }
    
    // Check 75-90° range (should be increasing: higher tilt = higher SG)
    DBG_PRINTLN("Checking 75-90° range:");
    prevGravity = calculateGravity(75.0);
    bool monotonic90 = true;
    for (float tilt = 80.0; tilt <= 90.0; tilt += 5.0) {
        float currentGravity = calculateGravity(tilt);
        if (currentGravity < prevGravity) {  // If gravity decreases as tilt increases (wrong direction)
            DBG_PRINTF("  ⚠ Non-monotonic at %.1f°: %.6f -> %.6f (SG should increase with tilt)\n", tilt, prevGravity, currentGravity);
            monotonic90 = false;
        }
        prevGravity = currentGravity;
    }
    if (monotonic90) {
        DBG_PRINTLN("  ✓ 75-90° range shows expected monotonic behavior");
    }
    
    DBG_PRINTLN("=== POLYNOMIAL TEST COMPLETE ===");
}

/**
 * Get calibration status
 * @return true if calibration is valid and loaded
 */
bool isCalibrationValid() {
    return calibCoeffs.isValid;
}

/**
 * Print current calibration coefficients
 */
void printCalibrationCoefficients() {
    if (calibCoeffs.isValid) {
        DBG_PRINTLN("Current Calibration Coefficients:");
        DBG_PRINTF("GRAVITY = %.12e * Tilt^3 + %.12e * Tilt^2 + %.12e * Tilt + %.12e\n",
                     calibCoeffs.coeff3, calibCoeffs.coeff2, calibCoeffs.coeff1, calibCoeffs.coeff0);
    } else {
        DBG_PRINTLN("No valid calibration coefficients available");
    }
}

#endif // POLYNOMIAL_CALIBRATION_H
