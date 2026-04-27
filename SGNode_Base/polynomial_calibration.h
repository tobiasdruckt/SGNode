#ifndef POLYNOMIAL_CALIBRATION_H
#define POLYNOMIAL_CALIBRATION_H

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
    EEPROM.begin(64); // Reserve 64 bytes for calibration data
    
    // Check if valid calibration data exists
    uint32_t magic;
    EEPROM.get(EEPROM_CALIB_MAGIC, magic);
    
    if (magic == CALIB_MAGIC) {
        // Load coefficients from EEPROM
        EEPROM.get(EEPROM_COEFF3, calibCoeffs.coeff3);
        EEPROM.get(EEPROM_COEFF2, calibCoeffs.coeff2);
        EEPROM.get(EEPROM_COEFF1, calibCoeffs.coeff1);
        EEPROM.get(EEPROM_COEFF0, calibCoeffs.coeff0);
        calibCoeffs.isValid = true;
        
        Serial.println("Calibration coefficients loaded from EEPROM");
        Serial.printf("coeff3: %.12e\n", calibCoeffs.coeff3);
        Serial.printf("coeff2: %.12e\n", calibCoeffs.coeff2);
        Serial.printf("coeff1: %.12e\n", calibCoeffs.coeff1);
        Serial.printf("coeff0: %.12e\n", calibCoeffs.coeff0);
    } else {
        // Initialize with default coefficients (linear approximation)
        calibCoeffs.coeff3 = 0.0;
        calibCoeffs.coeff2 = 0.0;
        calibCoeffs.coeff1 = 0.01;  // Default linear coefficient
        calibCoeffs.coeff0 = 1.0;   // Default offset
        calibCoeffs.isValid = false;
        
        Serial.println("No valid calibration found in EEPROM");
        Serial.println("Using default linear calibration");
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
        
        Serial.printf("Added calibration point %d: Tilt=%.2f°, Gravity=%.3f, Temp=%.1f°C\n", 
                     numCalibPoints, tilt, gravity, temperature);
    } else {
        Serial.println("Maximum calibration points reached");
    }
}

/**
 * Calculate 3rd degree polynomial coefficients using least squares method
 * @return true if calculation successful, false otherwise
 */
bool calculatePolynomialCoefficients() {
    if (numCalibPoints < 4) {
        Serial.println("Need at least 4 calibration points for 3rd degree polynomial");
        return false;
    }
    
    Serial.println("Calculating 3rd degree polynomial coefficients...");
    
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
        // Feed watchdog to prevent timeout during calculation
        esp_task_wdt_reset(NULL);
        
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
    
    Serial.println("3rd Degree Polynomial Coefficients Calculated:");
    Serial.printf("GRAVITY = %.12e * Tilt^3 + %.12e * Tilt^2 + %.12e * Tilt + %.12e\n",
                 calibCoeffs.coeff3, calibCoeffs.coeff2, calibCoeffs.coeff1, calibCoeffs.coeff0);
    
    return true;
}

/**
 * Save calibration coefficients to EEPROM
 */
void saveCalibrationCoefficients() {
    if (!calibCoeffs.isValid) {
        Serial.println("Cannot save invalid calibration coefficients");
        return;
    }
    
    // Write magic number
    EEPROM.put(EEPROM_CALIB_MAGIC, CALIB_MAGIC);
    
    // Write coefficients
    EEPROM.put(EEPROM_COEFF3, calibCoeffs.coeff3);
    EEPROM.put(EEPROM_COEFF2, calibCoeffs.coeff2);
    EEPROM.put(EEPROM_COEFF1, calibCoeffs.coeff1);
    EEPROM.put(EEPROM_COEFF0, calibCoeffs.coeff0);
    
    // Write version
    uint8_t version = 1;
    EEPROM.put(EEPROM_CALIB_VERSION, version);
    
    bool success = EEPROM.commit();
    
    if (success) {
        Serial.println("Calibration coefficients saved to EEPROM");
    } else {
        Serial.println("ERROR: Failed to save calibration coefficients to EEPROM");
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
    
    float tilt2 = tilt * tilt;
    float tilt3 = tilt2 * tilt;
    
    return calibCoeffs.coeff3 * tilt3 + 
           calibCoeffs.coeff2 * tilt2 + 
           calibCoeffs.coeff1 * tilt + 
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
    
    Serial.println("Calibration reset to defaults");
}

/**
 * Test calibration accuracy with stored points
 */
void testCalibrationAccuracy() {
    if (!calibCoeffs.isValid) {
        Serial.println("No valid calibration to test");
        return;
    }
    
    Serial.println("Calibration Accuracy Test:");
    Serial.println("Point | Tilt(°) | Actual | Calculated | Error");
    Serial.println("-------------------------------------------");
    
    float totalError = 0;
    for (int i = 0; i < numCalibPoints; i++) {
        float calculated = calculateGravity(calibPoints[i].tilt);
        float error = fabs(calculated - calibPoints[i].gravity);
        totalError += error;
        
        Serial.printf("%4d  | %6.2f | %6.3f | %10.3f | %6.6f\n",
                     i + 1, calibPoints[i].tilt, calibPoints[i].gravity, calculated, error);
    }
    
    float avgError = totalError / numCalibPoints;
    Serial.printf("Average Error: %.6f\n", avgError);
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
        Serial.println("Current Calibration Coefficients:");
        Serial.printf("GRAVITY = %.12e * Tilt^3 + %.12e * Tilt^2 + %.12e * Tilt + %.12e\n",
                     calibCoeffs.coeff3, calibCoeffs.coeff2, calibCoeffs.coeff1, calibCoeffs.coeff0);
    } else {
        Serial.println("No valid calibration coefficients available");
    }
}

#endif // POLYNOMIAL_CALIBRATION_H
