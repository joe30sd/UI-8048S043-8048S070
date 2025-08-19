#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include "config.h"

/**
 * @brief Communication module for device protocol handling
 */
class Communication {
private:
    HardwareSerial* device_serial;
    HardwareSerial* console_serial;

public:
    /**
     * @brief Initialize communication module
     * @param device_ser Reference to device serial interface
     * @param console_ser Reference to console serial interface
     */
    void init(HardwareSerial& device_ser, HardwareSerial& console_ser);

    /**
     * @brief Send command to device
     * @param devAddr Device address
     * @param command Command byte
     * @param args Pointer to 16-byte argument array
     */
    void sendCommand(uint8_t devAddr, uint8_t command, const uint8_t *args);

    /**
     * @brief Set device to remote mode
     * @param devAddr Device address
     */
    void setRemoteModeOn(uint8_t devAddr);

    /**
     * @brief Set device output on/off
     * @param devAddr Device address
     * @param on_off True for on, false for off
     */
    void setOutput(uint8_t devAddr, bool on_off);

    /**
     * @brief Set voltage and current parameters
     * @param devAddr Device address
     * @param voltage Target voltage
     * @param current Target current
     * @param ovp Over-voltage protection
     * @param ocp Over-current protection
     */
    void setVoltageCurrent(uint8_t devAddr, float voltage, float current, float ovp, float ocp);

    /**
     * @brief Set current value
     * @param s_current Current in hundredths (e.g., 250 = 2.50A)
     */
    void setCurrent(uint32_t s_current);

    /**
     * @brief Set voltage value
     * @param s_voltage Voltage in hundredths (e.g., 2250 = 22.50V)
     */
    void setVoltage(uint32_t s_voltage);

    /**
     * @brief Check if device serial has data available
     * @return True if data is available
     */
    bool isDataAvailable();

    /**
     * @brief Read command from device serial
     * @return Command string
     */
    String readCommand();

    /**
     * @brief Print message to console
     * @param message Message to print
     */
    void consolePrint(const String& message);

    /**
     * @brief Print line to console
     * @param message Message to print with newline
     */
    void consolePrintln(const String& message);

    /**
     * @brief Dump last 4 data files to console
     */
    void dumpLast4DataFiles();
};

extern Communication comm;

#endif // COMMUNICATION_H