#ifndef DEVICE_CONTROLLER_H
#define DEVICE_CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "data_logger.h"

/**
 * @brief Device controller for managing device state and operations
 */
class DeviceController {
private:
    // Device state variables
    int minutesPassed;
    int o_timer;
    float live_magnet_current;
    float live_magnet_voltage;
    float magnet_current;
    float magnet_voltage;
    float resistor_value;
    float main_voltage;
    float main_current;
    float att_current;
    float rda_current;
    float rda_voltage;
    float att_voltage;
    float ts_voltage;
    float c_clb1, c_clb2, c_clb3, v_clb1;
    
    uint8_t att_relay;
    uint8_t rda_relay;
    uint8_t main_relay;
    uint8_t display_mode;
    uint8_t probe_check_counter;
    
    // Operation mode variables
    float u_target_current, u_inc_current;
    uint8_t up_mode, confirm, up_done, G_MODE;
    char stc_buffer[100];
    float d_start_current, d_dec_current;
    uint8_t d_mode;
    
    // Flags and counters
    boolean m_instant_mode, rda_checked, probe_checker, diagnostic_mode;
    uint8_t diag_x, diag_y, diag_t;
    int device_footprint;
    int last_device_footprint;
    int display_mode_flag;
    int rdf_flag;
    String PSU_ID;
    
    // Diagnostic timer variables
    unsigned long diag_counter, diag_target;
    
    // Self test variables
    int self_tester, self_tester_counter;

public:
    /**
     * @brief Initialize device controller
     */
    void init();

    /**
     * @brief Reset all variables to default state
     */
    void resetVariables();

    /**
     * @brief Update device state from received data
     * @param command Command string received from device
     */
    void updateFromCommand(const String& command);

    /**
     * @brief Get current device data for logging
     * @return DeviceData structure with current values
     */
    DeviceData getCurrentData();

    /**
     * @brief Handle main loop processing
     */
    void processMainLoop();

    /**
     * @brief Handle diagnostic timer operations
     * @param timer_mode Timer mode (1-5)
     */
    void handleDiagnosticTimer(uint8_t timer_mode);

    /**
     * @brief Handle diagnostic control operations
     * @param dy_command Diagnostic command
     */
    void handleDiagnosticControl(uint8_t dy_command);

    // Getters for important state variables
    uint8_t getDisplayMode() const { return display_mode; }
    uint8_t getUpMode() const { return up_mode; }
    uint8_t getUpDone() const { return up_done; }
    uint8_t getDMode() const { return d_mode; }
    uint8_t getConfirm() const { return confirm; }
    boolean getProbeChecker() const { return probe_checker; }
    uint8_t getGMode() const { return G_MODE; }
    int getMinutesPassed() const { return minutesPassed; }
    float getLiveMagnetCurrent() const { return live_magnet_current; }
    float getLiveMagnetVoltage() const { return live_magnet_voltage; }
    float getMagnetCurrent() const { return magnet_current; }
    float getMagnetVoltage() const { return magnet_voltage; }
    float getUTargetCurrent() const { return u_target_current; }
    float getUIncCurrent() const { return u_inc_current; }
    float getDStartCurrent() const { return d_start_current; }
    int getSelfTester() const { return self_tester; }
    boolean getRdaChecked() const { return rda_checked; }
    String getPSUID() const { return PSU_ID; }
    
    // Additional getters needed by event handlers
    boolean getMInstantMode() const { return m_instant_mode; }
    uint8_t getRdaRelay() const { return rda_relay; }
    uint8_t getMainRelay() const { return main_relay; }
    uint8_t getAttRelay() const { return att_relay; }
    float getResistorValue() const { return resistor_value; }
    
    // Setters for UI interaction
    void setMagnetCurrent(float current) { magnet_current = current; }
    void setMagnetVoltage(float voltage) { magnet_voltage = voltage; }
    void setAttRelay(uint8_t relay) { att_relay = relay; }
    void setRdaRelay(uint8_t relay) { rda_relay = relay; }
    void setMainRelay(uint8_t relay) { main_relay = relay; }
    void setDisplayMode(uint8_t mode) { display_mode = mode; }
    void setUpMode(uint8_t mode) { up_mode = mode; }
    void setUpDone(uint8_t done) { up_done = done; }
    void setDMode(uint8_t mode) { d_mode = mode; }
    void setConfirm(uint8_t conf) { confirm = conf; }
    void setUTargetCurrent(float current) { u_target_current = current; }
    void setUIncCurrent(float current) { u_inc_current = current; }
    void setDStartCurrent(float current) { d_start_current = current; }
    void setDDecCurrent(float current) { d_dec_current = current; }
    void setProbeChecker(boolean checker) { probe_checker = checker; }
    void setProbeCheckCounter(uint8_t counter) { probe_check_counter = counter; }
    void setGMode(uint8_t mode) { G_MODE = mode; }
    void setMInstantMode(boolean mode) { m_instant_mode = mode; }
    void setRdaChecked(boolean checked) { rda_checked = checked; }
    void setSelfTester(int tester) { self_tester = tester; }
    void setDeviceFootprint(int footprint) { device_footprint = footprint; }
    void setRdfFlag(int flag) { rdf_flag = flag; }
};

extern DeviceController deviceController;

#endif // DEVICE_CONTROLLER_H