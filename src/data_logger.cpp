#include "data_logger.h"
#include "utils.h"
#include "communication.h"

DataLogger dataLogger;

bool DataLogger::init(int chipSelect) {
    if (!SD.begin(chipSelect)) {
        comm.consolePrintln("Card Mount Failed");
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        comm.consolePrintln("No SD card attached");
        return false;
    }
    
    comm.consolePrintln("SD card initialized.");
    
    currentDataFile = nextDataFilename();
    dataLine = 1;
    startMillis = millis();
    
    // Initialize last data to invalid values to force first save
    memset(&lastData, 0xFF, sizeof(lastData));
    
    comm.consolePrint("Log file: ");
    comm.consolePrintln(currentDataFile);
    
    return true;
}

bool DataLogger::hasChanged(const DeviceData& current) {
    return (current.device_footprint != lastData.device_footprint ||
            current.live_magnet_current != lastData.live_magnet_current ||
            current.live_magnet_voltage != lastData.live_magnet_voltage ||
            current.magnet_current != lastData.magnet_current ||
            current.magnet_voltage != lastData.magnet_voltage ||
            current.resistor_value != lastData.resistor_value ||
            current.main_voltage != lastData.main_voltage ||
            current.rda_voltage != lastData.rda_voltage ||
            current.att_voltage != lastData.att_voltage ||
            current.ts_voltage != lastData.ts_voltage ||
            current.att_relay != lastData.att_relay ||
            current.rda_relay != lastData.rda_relay ||
            current.main_relay != lastData.main_relay ||
            current.display_mode != lastData.display_mode);
}

void DataLogger::saveData(const DeviceData& data) {
    if (!hasChanged(data)) {
        return; // No changes, skip saving
    }
    
    forceSaveData(data);
}

void DataLogger::forceSaveData(const DeviceData& data) {
    // Calculate HH:MM:SS
    unsigned long elapsed = (millis() - startMillis) / 1000;
    int hours = elapsed / 3600;
    int minutes = (elapsed % 3600) / 60;
    int seconds = elapsed % 60;
    char timeStr[9];
    sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);

    JsonDocument doc;
    doc["data_line"] = dataLine;
    doc["time"] = timeStr;
    doc["o_timer"] = data.o_timer;
    doc["live_magnet_current"] = data.live_magnet_current;
    doc["live_magnet_voltage"] = data.live_magnet_voltage;
    doc["magnet_current"] = data.magnet_current;
    doc["magnet_voltage"] = data.magnet_voltage;
    doc["resistor_value"] = data.resistor_value;
    doc["main_voltage"] = data.main_voltage;
    doc["rda_voltage"] = data.rda_voltage;
    doc["att_voltage"] = data.att_voltage;
    doc["ts_voltage"] = data.ts_voltage;
    doc["att_relay"] = data.att_relay;
    doc["rda_relay"] = data.rda_relay;
    doc["main_relay"] = data.main_relay;
    doc["display_mode"] = data.display_mode;
    doc["footprint"] = data.device_footprint;

    File file = SD.open(currentDataFile, FILE_APPEND);
    if (file) {
        serializeJson(doc, file);
        file.println();
        file.close();
        
        comm.consolePrint("Saved to ");
        comm.consolePrintln(currentDataFile);
        
        dataLine++;
        lastData = data; // Update last saved data
    } else {
        comm.consolePrint("Cannot open ");
        comm.consolePrintln(currentDataFile);
    }
}