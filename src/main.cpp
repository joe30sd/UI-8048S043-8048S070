#include <Arduino.h>
#include <lv_conf.h>
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/ui_events.h"
#include "gui/gui.h"
#include <EEPROM.h>

// New modular includes
#include "config.h"
#include "communication.h"
#include "ota_manager.h"
#include "data_logger.h"
#include "device_controller.h"
#include "event_handlers.h"
#include "utils.h"

#ifdef FOUR_ON
    // If FOUR_ON is defined, this section is compiled.
HardwareSerial& device_m = Serial;
HardwareSerial& console_m = Serial2;
#else
    // FOUR_ON is NOT defined, this section is compiled.
HardwareSerial& device_m = Serial2;
HardwareSerial& console_m = Serial;
#endif

// Global variables that still need to be accessed from main
int update_counter = 0;
DisplayUpdateStatus display_updater = UPDATE_NONE;
unsigned long update_timer = 0;
String s_cmd;

// Forward declarations
void handleDisplayModeUpdates();
void updateMapModeDisplay();
void updateRampUpModeDisplay();
void updateRampDownModeDisplay();

void setup() {
  // Initialize serial communication
  Serial.begin(CONSOLE_BAUD_RATE);
  Serial2.begin(DEVICE_BAUD_RATE, SERIAL_8N1, CONSOLE_RX_PIN, CONSOLE_TX_PIN);

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize non-UI modules first
  comm.init(device_m, console_m);
  otaManager.init(WIFI_SSID, WIFI_PASSWORD, FIRMWARE_URL);
  
  // Check for OTA update request
  display_updater = otaManager.getUpdateStatus();
  
  if (otaManager.isUpdateRequested()) {
    otaManager.setUpdateStatus(UPDATE_WIFI_ERROR); // Default to error, will be updated if successful
    comm.consolePrintln("UPDATE START");
    DisplayUpdateStatus result = otaManager.performUpdate();
    // If we reach here, update failed
    display_updater = result;
  }

  // Initialize GUI FIRST - critical for display to work
  gui_start();
  
  // NOW initialize device controller (which needs UI elements to exist)
  deviceController.init();

  // Initialize data logger (non-critical - don't fail if SD card missing)
  if (!dataLogger.init(SD_CHIP_SELECT)) {
    comm.consolePrintln("Warning: SD card initialization failed - continuing without data logging");
    // Continue anyway - SD card is not critical for display functionality
  }
}

void loop() {
    lv_timer_handler();
    delay(MAIN_LOOP_DELAY_MS);
    
    // Handle OTA update display status
    if (display_updater > UPDATE_NONE) {
        update_counter++;
        delay(100);
        
        // Display appropriate update message
        switch (display_updater) {
            case UPDATE_WIFI_ERROR:
                lv_label_set_text(ui_mbft, "Wi-Fi ERROR FOR UPDATE");
                break;
            case UPDATE_HTTP_ERROR:
                lv_label_set_text(ui_mbft, "HTTP ERROR FOR UPDATE");
                break;
            case UPDATE_STORE_ERROR:
                lv_label_set_text(ui_mbft, "STORE ERROR FOR UPDATE");
                break;
            case UPDATE_UNKNOWN_ERROR:
                lv_label_set_text(ui_mbft, "UNKNOW ERROR FOR UPDATE");
                break;
            case UPDATE_SUCCESSFUL:
                lv_label_set_text(ui_mbft, "UPDATE SUCCESFUL");
                break;
        }
        
        lv_obj_clear_flag(ui_mbft, LV_OBJ_FLAG_HIDDEN);
        
        if (update_counter > 70) {
            otaManager.clearUpdateStatus();
            update_counter = 0;
            lv_scr_load(ui_Screen3);
            deviceController.setDisplayMode(0);
            deviceController.setSelfTester(19);
        }
    }
    
    // Handle device communication
    if (comm.isDataAvailable()) {
        s_cmd = comm.readCommand();
        comm.consolePrintln(s_cmd);
        
        deviceController.updateFromCommand(s_cmd);
        
        // Handle special commands
        #ifdef FOUR_ON
        if (s_cmd.startsWith("DWN")) {
            s_cmd.trim();
            comm.dumpLast4DataFiles();
        }
        #else
        // Handle DWN command for non-FOUR_ON mode
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            if (cmd == "DWN") {
                comm.dumpLast4DataFiles();
            }
        }
        #endif
    }
    
    // Handle main device processing
    deviceController.processMainLoop();
    
    // Handle o_timer processing and data logging
    static int o_timer = 0;
    o_timer++;
    if (o_timer > O_TIMER_THRESHOLD) {
        o_timer = 0;
        comm.consolePrintln("*****o_timer*****");
        
        // Handle display mode specific updates
        handleDisplayModeUpdates();
        
        // Save data
        dataLogger.saveData(deviceController.getCurrentData());
    }
}

void handleDisplayModeUpdates() {
    uint8_t displayMode = deviceController.getDisplayMode();
    
    switch (displayMode) {
        case 1: // Map mode
            updateMapModeDisplay();
            break;
        case 2: // Ramp up mode
            updateRampUpModeDisplay();
            break;
        case 3: // Ramp down mode
            updateRampDownModeDisplay();
            break;
    }
}

void updateMapModeDisplay() {
    lv_spinbox_set_value(ui_mcd, deviceController.getLiveMagnetCurrent() * 100);
    lv_spinbox_set_value(ui_mvd, deviceController.getLiveMagnetVoltage() * 100);
    // Add other map mode UI updates as needed
}

void updateRampUpModeDisplay() {
    uint8_t upMode = deviceController.getUpMode();
    uint8_t upDone = deviceController.getUpDone();
    
    // Handle ramp up state transitions
    if (upDone == 2 && upMode == 2) {
        lv_textarea_add_text(ui_ota, "\n **Wait 3 min to engage power**");
    } else if (upDone == 3 && upMode == 3) {
        lv_textarea_add_text(ui_ota, "\n **Power engaged**");
        lv_textarea_add_text(ui_ota, "\n ***Ramp UP is start***");
    } else if (upDone == 4 && upMode == 4) {
        lv_bar_set_value(ui_opb, (int)deviceController.getUTargetCurrent(), LV_ANIM_ON);
        deviceController.setUpMode(6);
        lv_scr_load(ui_Screen7);
        lv_spinbox_set_value(ui_tscd, deviceController.getMagnetCurrent() * 100);
    }
    
    // Update display values
    lv_spinbox_set_value(ui_ocd, deviceController.getLiveMagnetCurrent() * 100);
    lv_spinbox_set_value(ui_ovd, deviceController.getLiveMagnetVoltage() * 100);
    lv_bar_set_value(ui_opb, (int)deviceController.getMagnetCurrent(), LV_ANIM_ON);
}

void updateRampDownModeDisplay() {
    // Update ramp down mode display
    lv_spinbox_set_value(ui_ocd, deviceController.getLiveMagnetCurrent() * 100);
    lv_spinbox_set_value(ui_ovd, deviceController.getLiveMagnetVoltage() * 100);
    
    if (deviceController.getLiveMagnetCurrent() < 0.5 && deviceController.getDMode() > 3 && deviceController.getMinutesPassed() > 3) {
        lv_textarea_add_text(ui_ota, "\n Operation is Done Wait 3min!");
    }
    
    if (deviceController.getDMode() == 5) {
        delay(1000);
        lv_scr_load(ui_Screen3);
        deviceController.setDisplayMode(0);
        deviceController.resetVariables();
    }
}