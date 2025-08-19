#include "device_controller.h"
#include "communication.h"
#include "ui/ui.h"
#include <EEPROM.h>

DeviceController deviceController;

void DeviceController::init() {
    resetVariables();
    self_tester = 17;
    self_tester_counter = 0;
    display_mode_flag = 0;
    rdf_flag = 0;
    diagnostic_mode = false;
    diag_x = 0;
    diag_y = 0;
    diag_t = 0;
    diag_counter = 0;
    diag_target = 0;
}

void DeviceController::resetVariables() {
    minutesPassed = 0;
    o_timer = 0;
    live_magnet_current = 0;
    live_magnet_voltage = 0;
    magnet_current = 0;
    magnet_voltage = 0;
    resistor_value = 0;
    main_voltage = 0;
    rda_voltage = 0;
    att_voltage = 0;
    ts_voltage = 0;
    
    up_mode = 0;
    confirm = 0;
    up_done = 0;
    d_start_current = 0;
    d_dec_current = 0;
    
    att_relay = 0;
    rda_relay = 0;
    main_relay = 0;
    probe_check_counter = 0;
    d_mode = 0;
    G_MODE = 0;
    m_instant_mode = 0;
    rda_checked = 0;
    probe_checker = 0;
    
    // Reset UI elements
    lv_spinbox_set_value(ui_mcd, 0);
    lv_spinbox_set_value(ui_mvd, 0);
    lv_spinbox_set_value(ui_mscd, 0);
    lv_spinbox_set_value(ui_msvd, 0);
    lv_spinbox_set_value(ui_attvd, 0);
    lv_spinbox_set_value(ui_mainvd, 0);
    lv_spinbox_set_value(ui_oattvd, 0);
    lv_spinbox_set_value(ui_omainvd, 0);
    lv_spinbox_set_value(ui_rdavd, 0);
    lv_spinbox_set_value(ui_ocd, 0);
    lv_spinbox_set_value(ui_tovd, 0);
    lv_spinbox_set_value(ui_ovd, 0);
    lv_spinbox_set_value(ui_otd, 0);
    lv_spinbox_set_value(ui_utcd, 0);
    
    lv_obj_clear_state(ui_mainof, LV_STATE_CHECKED);
    lv_obj_clear_state(ui_axof, LV_STATE_CHECKED);
    lv_obj_clear_state(ui_rdacb, LV_STATE_CHECKED);
    
    lv_label_set_text(ui_Label37, "Reads from magnet, only for Philips.");
    lv_label_set_text(ui_Label54, "Reads from magnet, only for Philips.");
    lv_label_set_text(ui_Label24, "Main Heater                                                  Axial Coil                                      Time");
    lv_label_set_text(ui_Label25, "ON/OFF                                                      ON/OFF");
    lv_label_set_text(ui_Label26, "V                                     V                            Min");
    
    lv_obj_clear_flag(ui_axof, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_oattvd, LV_OBJ_FLAG_HIDDEN);
}

DeviceData DeviceController::getCurrentData() {
    DeviceData data;
    data.o_timer = o_timer;
    data.live_magnet_current = live_magnet_current;
    data.live_magnet_voltage = live_magnet_voltage;
    data.magnet_current = magnet_current;
    data.magnet_voltage = magnet_voltage;
    data.resistor_value = resistor_value;
    data.main_voltage = main_voltage;
    data.rda_voltage = rda_voltage;
    data.att_voltage = att_voltage;
    data.ts_voltage = ts_voltage;
    data.att_relay = att_relay;
    data.rda_relay = rda_relay;
    data.main_relay = main_relay;
    data.display_mode = display_mode;
    data.device_footprint = device_footprint;
    return data;
}

void DeviceController::updateFromCommand(const String& command) {
    if (command.startsWith("LV")) {
        live_magnet_voltage = command.substring(2).toFloat();
    }
    else if (command.startsWith("LC")) {
        live_magnet_current = command.substring(2).toFloat();
        
        if (probe_checker) {
            comm.setCurrent(30000); // 300.0A * 100
            comm.setVoltage(60000); // 600.0V * 100
            probe_check_counter++;
            
            if (live_magnet_current > PROBE_CURRENT_THRESHOLD && 
                live_magnet_voltage < PROBE_VOLTAGE_THRESHOLD) {
                lv_obj_clear_flag(ui_Checkbox7, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_Spinner3, LV_OBJ_FLAG_HIDDEN);
                probe_checker = false;
                comm.setCurrent(0);
                comm.setVoltage(0);
                probe_check_counter = 0;
            } else if (probe_check_counter > PROBE_MAX_ATTEMPTS) {
                comm.setCurrent(0);
                comm.setVoltage(0);
                lv_scr_load(ui_Screen10);
                probe_checker = true;
            }
        }
    }
    else if (command.startsWith("AC")) {
        att_current = command.substring(2).toFloat();
        att_current = att_current + c_clb1;
    }
    else if (command.startsWith("AV")) {
        att_voltage = command.substring(2).toFloat();
        if (att_relay == 0) {
            att_voltage = 0;
        }
        if (att_voltage > 22.55) {
            att_voltage = att_voltage + v_clb1;
        }
    }
    else if (command.startsWith("RC")) {
        rda_current = command.substring(2).toFloat();
        rda_current = rda_current + c_clb2;
    }
    else if (command.startsWith("RV")) {
        rda_voltage = command.substring(2).toFloat();
        if (rda_voltage > 22.55) {
            rda_voltage = rda_voltage + v_clb1;
        }
        if (rda_relay == 0) {
            rda_voltage = 0;
        }
    }
    else if (command.startsWith("CC")) {
        main_current = command.substring(2).toFloat();
        main_current = main_current + c_clb3;
    }
    else if (command.startsWith("CV")) {
        main_voltage = command.substring(2).toFloat();
        if (main_voltage > 22.55) {
            main_voltage = main_voltage + v_clb1;
        }
        if (main_relay == 0) {
            main_voltage = 0;
        }
    }
    else if (command.startsWith("OV")) {
        ts_voltage = command.substring(2).toFloat();
    }
    else if (command.startsWith("MC")) {
        if (up_mode != 6) {
            magnet_current = command.substring(2).toFloat();
        }
    }
    else if (command.startsWith("MV")) {
        magnet_voltage = command.substring(2).toFloat();
    }
    else if (command.startsWith("RA")) {
        att_relay = command.substring(2).toInt();
    }
    else if (command.startsWith("RM")) {
        main_relay = command.substring(2).toInt();
    }
    else if (command.startsWith("RR")) {
        rda_relay = command.substring(2).toInt();
    }
    else if (command.startsWith("TP")) {
        minutesPassed = command.substring(2).toInt();
    }
    else if (command.startsWith("UP")) {
        up_mode = command.substring(2).toInt();
    }
    else if (command.startsWith("UD")) {
        up_done = command.substring(2).toInt();
    }
    else if (command.startsWith("DM")) {
        d_mode = command.substring(2).toInt();
    }
    else if (command.startsWith("OR")) {
        resistor_value = command.substring(2).toFloat();
    }
    else if (command.startsWith("YD")) {
        diag_y = command.substring(2).toFloat();
        handleDiagnosticControl(diag_y);
    }
    else if (command.startsWith("ID")) {
        PSU_ID = command.substring(2);
    }
}

void DeviceController::handleDiagnosticTimer(uint8_t timer_mode) {
    if (timer_mode == 1) {
        diag_counter = millis();
        diag_t = 1;
        diag_target = DIAGNOSTIC_TIMEOUT_SHORT_MS;
    } else if (timer_mode == 2) {
        diag_counter = millis();
        diag_t = 1;
        diag_target = DIAGNOSTIC_TIMEOUT_MEDIUM_MS;
    } else if (timer_mode == 3) {
        diag_counter = millis();
        diag_t = 1;
        diag_target = DIAGNOSTIC_TIMEOUT_LONG_MS;
    } else if (timer_mode == 4) {
        diag_t = 0;
        lv_textarea_add_text(ui_dbta, "\n Test Fail: 42 !");
        lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
        diag_x = 0;
        diag_y = 0;
    } else if (timer_mode == 5) {
        diag_t = 0;
        diag_x = 0;
        diag_y = 0;
    }
}

void DeviceController::handleDiagnosticControl(uint8_t dy_command) {
    handleDiagnosticTimer(5);
    
    switch (dy_command) {
        case 2:
        case 3:
            lv_textarea_add_text(ui_dbta, ("\n Test Fail: " + String(dy_command)).c_str());
            lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
            break;
        case 4:
        case 9:
        case 14:
        case 19:
        case 24:
        case 29:
            lv_textarea_add_text(ui_dbta, "\n Test Pass!");
            if (dy_command == 19) {
                lv_textarea_add_text(ui_dbta, "\n");
                lv_textarea_add_text(ui_dbta, PSU_ID.c_str());
            }
            lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
            break;
        case 6:
        case 7:
        case 11:
        case 12:
        case 16:
        case 17:
        case 21:
        case 22:
        case 26:
        case 27:
            lv_textarea_add_text(ui_dbta, ("\n Test Fail: " + String(dy_command)).c_str());
            lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
            break;
        case 51:
            lv_textarea_add_text(ui_dbta, "\n ****WIFI UPDATE START**** ");
            lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
            break;
        case 52:
            lv_textarea_add_text(ui_dbta, "\n ****WIFI UPDATE DONE**** ");
            lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
            break;
        case 53:
            lv_textarea_add_text(ui_dbta, "\n ****WIFI UPDATE ERROR**** ");
            lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
            break;
    }
    
    diag_x = 0;
    diag_y = 0;
}

void DeviceController::processMainLoop() {
    // Handle diagnostic timeout
    if (diag_t == 1) {
        if (millis() - diag_counter > diag_target) {
            handleDiagnosticTimer(4);
        }
    }
    
    // Handle display mode changes
    if (display_mode == 0 && display_mode_flag == 0) {
        display_mode_flag = 1;
        device_footprint = FOOTPRINT_DISPLAY_IDLE;
        resetVariables();
        device_footprint = FOOTPRINT_DISPLAY_RESET;
    } else if (display_mode > 0 && display_mode_flag == 1) {
        display_mode_flag = 0;
    }
}