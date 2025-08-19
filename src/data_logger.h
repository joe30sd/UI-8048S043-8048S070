#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include "config.h"

/**
 * @brief Data structure for device state
 */
struct DeviceData {
    int o_timer;
    float live_magnet_current;
    float live_magnet_voltage;
    float magnet_current;
    float magnet_voltage;
    float resistor_value;
    float main_voltage;
    float rda_voltage;
    float att_voltage;
    float ts_voltage;
    uint8_t att_relay;
    uint8_t rda_relay;
    uint8_t main_relay;
    uint8_t display_mode;
    int device_footprint;
};

/**
 * @brief Data logger for JSON logging to SD card
 */
class DataLogger {
private:
    String currentDataFile;
    int dataLine;
    DeviceData lastData;
    unsigned long startMillis;
    
    /**
     * @brief Check if data has changed since last save
     * @param current Current device data
     * @return True if data has changed
     */
    bool hasChanged(const DeviceData& current);

public:
    /**
     * @brief Initialize data logger
     * @param chipSelect SD card chip select pin
     * @return True if initialization successful
     */
    bool init(int chipSelect);

    /**
     * @brief Save device data to SD card
     * @param data Device data to save
     */
    void saveData(const DeviceData& data);

    /**
     * @brief Get current data filename
     * @return Current data filename
     */
    String getCurrentDataFile() const { return currentDataFile; }

    /**
     * @brief Get current data line number
     * @return Current line number
     */
    int getCurrentDataLine() const { return dataLine; }

    /**
     * @brief Force save data even if unchanged
     * @param data Device data to save
     */
    void forceSaveData(const DeviceData& data);
};

extern DataLogger dataLogger;

#endif // DATA_LOGGER_H