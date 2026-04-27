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
#define EEPROM_POLY_DEGREE 21
#define EEPROM_NORM_MIN 22
#define EEPROM_NORM_RANGE 26

// Magic number to verify valid calibration data
#define CALIB_MAGIC 0x43414C49 // "CALI" in hex

// Calibration data structure
struct CalibrationCoefficients {
    float coeff3;  // Tilt^3 coefficient
    float coeff2;  // Tilt^2 coefficient
    float coeff1;  // Tilt coefficient
    float coeff0;  // Constant term
    bool isValid;  // Flag indicating if calibration is valid
    uint8_t poly_degree;  // Polynomial degree (1=linear, 2=quadratic, 3=cubic)
    float norm_min;  // Normalization minimum tilt
    float norm_range;  // Normalization range
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
        
        // Load version and normalization parameters
        uint8_t version;
        EEPROM.get(EEPROM_CALIB_VERSION, version);
        
        if (version >= 2) {
            EEPROM.get(EEPROM_POLY_DEGREE, calibCoeffs.poly_degree);
            EEPROM.get(EEPROM_NORM_MIN, calibCoeffs.norm_min);
            EEPROM.get(EEPROM_NORM_RANGE, calibCoeffs.norm_range);
        } else {
            // Version 1: assume cubic, no normalization
            calibCoeffs.poly_degree = 3;
            calibCoeffs.norm_min = 0.0;
            calibCoeffs.norm_range = 1.0;
        }
        
        calibCoeffs.isValid = true;
        
        Serial.println("Calibration coefficients loaded from EEPROM");
        Serial.printf("Version: %d, Degree: %d\n", version, calibCoeffs.poly_degree);
        if (version >= 2) {
            Serial.printf("Normalization: norm(Tilt) = (Tilt - %.2f) / %.2f\n", 
                         calibCoeffs.norm_min, calibCoeffs.norm_range);
        }
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
        calibCoeffs.poly_degree = 1;
        calibCoeffs.norm_min = 0.0;
        calibCoeffs.norm_range = 1.0;
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
    if (numCalibPoints < 2) {
        Serial.println("Need at least 2 calibration points");
        return false;
    }
    
    // Determine polynomial degree based on number of points
    int degree = (numCalibPoints >= 4) ? 3 : (numCalibPoints == 3) ? 2 : 1;
    Serial.printf("Calculating %d degree polynomial with %d points...\n", degree, numCalibPoints);
    
    // Normalize tilt input to improve numerical stability
    float min_tilt = calibPoints[0].tilt;
    float max_tilt = calibPoints[0].tilt;
    for (int i = 1; i < numCalibPoints; i++) {
        if (calibPoints[i].tilt < min_tilt) min_tilt = calibPoints[i].tilt;
        if (calibPoints[i].tilt > max_tilt) max_tilt = calibPoints[i].tilt;
    }
    float tilt_range = max_tilt - min_tilt;
    if (tilt_range == 0) tilt_range = 1.0; // Prevent division by zero
    
    // Build matrices for least squares solution
    // Linear (degree 1): 2x2 matrix
    // Quadratic (degree 2): 3x3 matrix
    // Cubic (degree 3): 4x4 matrix
    
    float sum_x = 0, sum_x2 = 0, sum_x3 = 0, sum_x4 = 0, sum_x5 = 0, sum_x6 = 0;
    float sum_y = 0, sum_xy = 0, sum_x2y = 0, sum_x3y = 0;
    
    // Calculate sums with normalized tilt
    for (int i = 0; i < numCalibPoints; i++) {
        float x = (calibPoints[i].tilt - min_tilt) / tilt_range; // Normalize to [0,1]
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
    
    // Build matrix based on degree
    float matrix[4][5] = {0};
    
    if (degree == 1) {
        // Linear: [x, 1] * [a, b] = y
        matrix[0][0] = sum_x2; matrix[0][1] = sum_x;  matrix[0][2] = sum_xy;
        matrix[1][0] = sum_x;  matrix[1][1] = n;     matrix[1][2] = sum_y;
    } else if (degree == 2) {
        // Quadratic: [x^2, x, 1] * [a, b, c] = y
        matrix[0][0] = sum_x4; matrix[0][1] = sum_x3; matrix[0][2] = sum_x2; matrix[0][3] = sum_x2y;
        matrix[1][0] = sum_x3; matrix[1][1] = sum_x2; matrix[1][2] = sum_x;  matrix[1][3] = sum_xy;
        matrix[2][0] = sum_x2; matrix[2][1] = sum_x;  matrix[2][2] = n;     matrix[2][3] = sum_y;
    } else {
        // Cubic: [x^3, x^2, x, 1] * [a, b, c, d] = y
        matrix[0][0] = sum_x6; matrix[0][1] = sum_x5; matrix[0][2] = sum_x4; matrix[0][3] = sum_x3; matrix[0][4] = sum_x3y;
        matrix[1][0] = sum_x5; matrix[1][1] = sum_x4; matrix[1][2] = sum_x3; matrix[1][3] = sum_x2; matrix[1][4] = sum_x2y;
        matrix[2][0] = sum_x4; matrix[2][1] = sum_x3; matrix[2][2] = sum_x2; matrix[2][3] = sum_x;  matrix[2][4] = sum_xy;
        matrix[3][0] = sum_x3; matrix[3][1] = sum_x2; matrix[3][2] = sum_x;  matrix[3][3] = n;     matrix[3][4] = sum_y;
    }
    
    // Solve using Gaussian elimination
    for (int col = 0; col < degree; col++) {
        // Feed watchdog to prevent timeout during calculation
        esp_task_wdt_reset();
        
        // Find pivot row
        int pivot = col;
        for (int row = col + 1; row < degree; row++) {
            if (fabs(matrix[row][col]) > fabs(matrix[pivot][col])) {
                pivot = row;
            }
        }
        
        // Check for near-zero pivot (singular matrix)
        if (fabs(matrix[pivot][col]) < 1e-10) {
            Serial.println("Warning: Near-zero pivot detected, matrix may be singular");
            // Continue anyway, but results may be unreliable
        }
        
        // Swap rows if needed
        if (pivot != col) {
            for (int i = col; i <= degree; i++) {
                float temp = matrix[col][i];
                matrix[col][i] = matrix[pivot][i];
                matrix[pivot][i] = temp;
            }
        }
        
        // Eliminate column
        for (int row = col + 1; row < degree; row++) {
            float factor = matrix[row][col] / matrix[col][col];
            for (int i = col; i <= degree; i++) {
                matrix[row][i] -= factor * matrix[col][i];
            }
        }
    }
    
    // Back substitution
    float solution[4] = {0};
    for (int row = degree - 1; row >= 0; row--) {
        solution[row] = matrix[row][degree];
        for (int col = row + 1; col < degree; col++) {
            solution[row] -= matrix[row][col] * solution[col];
        }
        solution[row] /= matrix[row][row];
    }
    
    // Store coefficients and normalization parameters
    calibCoeffs.coeff3 = (degree >= 3) ? solution[0] : 0;
    calibCoeffs.coeff2 = (degree >= 2) ? solution[degree == 3 ? 1 : 0] : 0;
    calibCoeffs.coeff1 = (degree >= 1) ? solution[degree == 3 ? 2 : (degree == 2 ? 1 : 0)] : 0;
    calibCoeffs.coeff0 = solution[degree - 1];
    
    // Validate coefficients (check for NaN or infinity)
    if (isnan(calibCoeffs.coeff3) || isinf(calibCoeffs.coeff3) ||
        isnan(calibCoeffs.coeff2) || isinf(calibCoeffs.coeff2) ||
        isnan(calibCoeffs.coeff1) || isinf(calibCoeffs.coeff1) ||
        isnan(calibCoeffs.coeff0) || isinf(calibCoeffs.coeff0)) {
        Serial.println("ERROR: Invalid coefficients detected (NaN or infinity)");
        calibCoeffs.isValid = false;
        return false;
    }
    
    calibCoeffs.isValid = true;
    calibCoeffs.poly_degree = degree;
    calibCoeffs.norm_min = min_tilt;
    calibCoeffs.norm_range = tilt_range;
    
    Serial.printf("%d Degree Polynomial Coefficients Calculated:\n", degree);
    if (degree == 3) {
        Serial.printf("GRAVITY = %.12e * norm(Tilt)^3 + %.12e * norm(Tilt)^2 + %.12e * norm(Tilt) + %.12e\n",
                     calibCoeffs.coeff3, calibCoeffs.coeff2, calibCoeffs.coeff1, calibCoeffs.coeff0);
    } else if (degree == 2) {
        Serial.printf("GRAVITY = %.12e * norm(Tilt)^2 + %.12e * norm(Tilt) + %.12e\n",
                     calibCoeffs.coeff2, calibCoeffs.coeff1, calibCoeffs.coeff0);
    } else {
        Serial.printf("GRAVITY = %.12e * norm(Tilt) + %.12e\n",
                     calibCoeffs.coeff1, calibCoeffs.coeff0);
    }
    Serial.printf("Normalization: norm(Tilt) = (Tilt - %.2f) / %.2f\n", min_tilt, tilt_range);
    
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
    
    // Write version and normalization parameters
    uint8_t version = 2;  // Version 2 includes normalization
    EEPROM.put(EEPROM_CALIB_VERSION, version);
    EEPROM.put(EEPROM_POLY_DEGREE, calibCoeffs.poly_degree);
    EEPROM.put(EEPROM_NORM_MIN, calibCoeffs.norm_min);
    EEPROM.put(EEPROM_NORM_RANGE, calibCoeffs.norm_range);
    
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
    
    // Normalize tilt input using stored normalization parameters
    float norm_tilt = (tilt - calibCoeffs.norm_min) / calibCoeffs.norm_range;
    
    // Calculate based on polynomial degree
    if (calibCoeffs.poly_degree == 3) {
        float norm_tilt2 = norm_tilt * norm_tilt;
        float norm_tilt3 = norm_tilt2 * norm_tilt;
        return calibCoeffs.coeff3 * norm_tilt3 + 
               calibCoeffs.coeff2 * norm_tilt2 + 
               calibCoeffs.coeff1 * norm_tilt + 
               calibCoeffs.coeff0;
    } else if (calibCoeffs.poly_degree == 2) {
        float norm_tilt2 = norm_tilt * norm_tilt;
        return calibCoeffs.coeff2 * norm_tilt2 + 
               calibCoeffs.coeff1 * norm_tilt + 
               calibCoeffs.coeff0;
    } else {
        // Linear (degree 1)
        return calibCoeffs.coeff1 * norm_tilt + calibCoeffs.coeff0;
    }
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
