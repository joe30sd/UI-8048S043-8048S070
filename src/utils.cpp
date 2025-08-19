#include "utils.h"
#include <SD.h>

uint8_t calcChecksum(const uint8_t *frame19) {
    uint16_t sum = 0;
    for (int i = 0; i < 19; i++) {
        sum += frame19[i];
    }
    return (uint8_t)(sum & 0xFF);
}

float intToFloat(uint32_t number, uint32_t decimalPlaces) {
    float result = number; 
    
    for (int i = 0; i < decimalPlaces; i++) {
        result /= 10.0;
    }
    
    return result;
}

String nextDataFilename() {
    uint16_t maxIdx = 0;
    File root = SD.open("/");
    for (File e = root.openNextFile(); e; e = root.openNextFile()) {
        String n = e.name();            // e.g., "data3.txt"
        if (!e.isDirectory() &&
            n.startsWith("data") && n.endsWith(".txt")) {
            uint16_t idx = n.substring(4, n.length() - 4).toInt(); // 4 = "data"
            if (idx > maxIdx) maxIdx = idx;
        }
        e.close();
    }
    root.close();
    return String("/data") + (maxIdx + 1) + ".txt";
}