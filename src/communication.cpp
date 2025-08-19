#include "communication.h"
#include "utils.h"
#include <SD.h>
#include <algorithm>

Communication comm;

void Communication::init(HardwareSerial& device_ser, HardwareSerial& console_ser) {
    device_serial = &device_ser;
    console_serial = &console_ser;
}

void Communication::sendCommand(uint8_t devAddr, uint8_t command, const uint8_t *args) {
    uint8_t frame[FRAME_SIZE];
    frame[0] = FRAME_HEADER;
    frame[1] = devAddr;
    frame[2] = command;
    for (int i = 0; i < ARGS_SIZE; i++) {
        frame[3 + i] = args[i];
    }
    frame[19] = calcChecksum(frame);
    console_serial->write(frame, FRAME_SIZE);
}

void Communication::setRemoteModeOn(uint8_t devAddr) {
    uint8_t args[ARGS_SIZE];
    args[0] = 0x01;
    for (int i = 1; i < ARGS_SIZE; i++) args[i] = 0x00;
    sendCommand(devAddr, 0x20, args);
}

void Communication::setOutput(uint8_t devAddr, bool on_off) {
    uint8_t args[ARGS_SIZE];
    args[0] = on_off ? 0x01 : 0x00;
    for (int i = 1; i < ARGS_SIZE; i++) args[i] = 0x00;
    sendCommand(devAddr, 0x22, args);
}

void Communication::setVoltageCurrent(uint8_t devAddr, float voltage, float current, float ovp, float ocp) {
    uint16_t ovp_val = (uint16_t)(ovp * 100.0f);
    uint16_t ocp_val = (uint16_t)(ocp * 100.0f);
    uint16_t v_val = (uint16_t)(voltage * 100.0f);
    uint16_t i_val = (uint16_t)(current * 1000.0f);
    
    uint8_t args[ARGS_SIZE];
    args[0] = (ovp_val >> 8) & 0xFF;
    args[1] = ovp_val & 0xFF;
    args[2] = (ocp_val >> 8) & 0xFF;
    args[3] = ocp_val & 0xFF;
    args[4] = (v_val >> 8) & 0xFF;
    args[5] = v_val & 0xFF;
    args[6] = (i_val >> 8) & 0xFF;
    args[7] = i_val & 0xFF;
    for (int i = 8; i < ARGS_SIZE; i++) args[i] = 0x00;
    
    sendCommand(devAddr, 0x2C, args);
}

void Communication::setCurrent(uint32_t s_current) {
    float formatted_current = s_current / 100.0;
    device_serial->print("C");
    device_serial->println(formatted_current, 2);
}

void Communication::setVoltage(uint32_t s_voltage) {
    float formatted_voltage = s_voltage / 100.0;
    device_serial->print("V");
    device_serial->println(formatted_voltage, 2);
}

bool Communication::isDataAvailable() {
    return device_serial->available() > 0;
}

String Communication::readCommand() {
    return device_serial->readStringUntil('\n');
}

void Communication::consolePrint(const String& message) {
    console_serial->print(message);
}

void Communication::consolePrintln(const String& message) {
    console_serial->println(message);
}

void Communication::dumpLast4DataFiles() {
    struct FileInfo { uint16_t idx; String name; };
    FileInfo files[20];
    uint8_t count = 0;

    // Collect the data*.txt files
    File root = SD.open("/");
    for (File e = root.openNextFile(); e; e = root.openNextFile()) {
        if (e.isDirectory()) { 
            e.close(); 
            continue; 
        }

        String n = e.name();
        if (n.startsWith("data") && n.endsWith(".txt")) {
            uint16_t i = n.substring(4, n.length() - 4).toInt();
            if (count < 20) { 
                files[count++] = { i, n }; 
            }
        }
        e.close();
    }
    root.close();
    
    if (count == 0) { 
        Serial.println("DUMP ERR"); 
        return; 
    }

    // Sort by index (small to large)
    std::sort(files, files + count,
              [](const FileInfo& a, const FileInfo& b){ 
                  return a.idx < b.idx; 
              });

    // Select the last block of 4
    uint8_t start = (count > 4) ? count - 4 : 0;

    Serial.println("DUMP BEGIN");
    for (uint8_t k = start; k < count; ++k) {
        File f = SD.open("/" + files[k].name, FILE_READ);
        if (!f) continue;

        Serial.print("FILE "); 
        Serial.println(files[k].name);
        while (f.available()) {
            Serial.println(f.readStringUntil('\n'));
            delay(2);
        }
        f.close();
        Serial.print("END "); 
        Serial.println(files[k].name);
    }
    Serial.println("DUMP DONE");
}