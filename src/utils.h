#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

/**
 * @brief Calculate checksum for communication frame
 * @param frame19 Pointer to 19-byte frame data
 * @return Calculated checksum
 */
uint8_t calcChecksum(const uint8_t *frame19);

/**
 * @brief Convert integer to float with specified decimal places
 * @param number Integer to convert
 * @param decimalPlaces Number of decimal places to shift
 * @return Converted float value
 */
float intToFloat(uint32_t number, uint32_t decimalPlaces);

/**
 * @brief Generate next available data filename
 * @return String containing next filename (e.g., "/data1.txt")
 */
String nextDataFilename();

#endif // UTILS_H