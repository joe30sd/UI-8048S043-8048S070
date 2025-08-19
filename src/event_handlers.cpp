#include "event_handlers.h"
#include "device_controller.h"
#include "communication.h"
#include "data_logger.h"
#include "utils.h"
#include "config.h"
#include "ui/ui.h"

// Navigation events
void map_event(lv_event_t *e) {
    lv_scr_load(ui_Screen4);
    deviceController.setDisplayMode(1);
    deviceController.setDeviceFootprint(FOOTPRINT_MAP_MODE);
    dataLogger.saveData(deviceController.getCurrentData());
}

void rdpb_event(lv_event_t *e) {
    lv_scr_load(ui_Screen8);
    deviceController.setDisplayMode(3);
    deviceController.setDeviceFootprint(FOOTPRINT_RAMP_DOWN_MODE);
    dataLogger.saveData(deviceController.getCurrentData());
}

void rupb_event(lv_event_t *e) {
    lv_scr_load(ui_Screen5);
    deviceController.setDisplayMode(2);
    deviceController.setDeviceFootprint(FOOTPRINT_DIAGNOSTIC_MODE);
    dataLogger.saveData(deviceController.getCurrentData());
}

// Manual mode events
void mscd_event(lv_event_t *e) {
    if (deviceController.getMInstantMode()) {
        int32_t value = lv_spinbox_get_value(ui_mscd);
        comm.setCurrent(value);
    }
}

void msvd_event(lv_event_t *e) {
    if (deviceController.getMInstantMode()) {
        int32_t value = lv_spinbox_get_value(ui_msvd);
        comm.setVoltage(value);
    }
}

void mpsi_event(lv_event_t *e) {
    deviceController.setMInstantMode(!deviceController.getMInstantMode());
}

void mpsob_event(lv_event_t *e) {
    int32_t value1 = lv_spinbox_get_value(ui_msvd);
    comm.setVoltage(value1);
    int32_t value2 = lv_spinbox_get_value(ui_mscd);
    comm.setCurrent(value2);
}

void mbb_event(lv_event_t *e) {
    deviceController.setConfirm(6);
    lv_scr_load(ui_Screen9);
}

// Relay control events
void axrb_event(lv_event_t *e) {
    deviceController.setAttRelay(1);
}

void t1rb_event(lv_event_t *e) {
    deviceController.setAttRelay(2);
}

void t2rb_event(lv_event_t *e) {
    deviceController.setAttRelay(3);
}

void rdarb_event(lv_event_t *e) {
    if (deviceController.getRdaRelay() == 0) {
        deviceController.setRdaRelay(1);
    } else {
        deviceController.setRdaRelay(0);
    }
}

void mainrb_event(lv_event_t *e) {
    if (deviceController.getMainRelay() == 0) {
        deviceController.setMainRelay(1);
    } else {
        deviceController.setMainRelay(0);
    }
}

void attob_event(lv_event_t *e) {
    deviceController.setAttRelay(0);
}

// Operation control events  
void usb_event(lv_event_t *e) {
    if (intToFloat(lv_spinbox_get_value(ui_utcd), 2) < 1) {
        lv_spinbox_set_value(ui_utcd, 120);
    }
    deviceController.setConfirm(1);
    lv_scr_load(ui_Screen9);
}

void ucb_event(lv_event_t *e) {
    deviceController.setDisplayMode(0);
    lv_scr_load(ui_Screen3);
    deviceController.resetVariables();
}

void ossb_event(lv_event_t *e) {
    uint8_t confirm = deviceController.getConfirm();
    uint8_t upMode = deviceController.getUpMode();
    uint8_t upDone = deviceController.getUpDone();
    uint8_t dMode = deviceController.getDMode();
    uint8_t gMode = deviceController.getGMode();
    
    if (confirm == 1 || confirm == 2) {
        if (upMode == 1 && upDone == 0) {
            lv_textarea_add_text(ui_ota, "\n **Wait for communication**");
        } else if (upMode == 1 && upDone == 1) {
            lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0x08FF80), LV_PART_MAIN | LV_STATE_DEFAULT);
            deviceController.setUpMode(2);
            lv_textarea_add_text(ui_ota, "\n Main Heater Active");
            lv_textarea_add_text(ui_ota, "\n Axial Coil Active");
            lv_textarea_add_text(ui_ota, "\n **Please Wait Adjusting Ramp Up**");
        } else if (upMode == 5) {
            if (gMode == 0) {
                deviceController.setUpMode(4);
                lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0x08FF80), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else {
                lv_textarea_add_text(ui_ota, "\n Wait for safe setup");
            }
        }
        deviceController.setDeviceFootprint(FOOTPRINT_OPERATION_START);
        dataLogger.saveData(deviceController.getCurrentData());
    } else if (confirm == 4) {
        if (dMode == 0) {
            deviceController.setDMode(1);
            lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0x08FF80), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_textarea_add_text(ui_ota, "\n **Ramp Down is Start**");
            if (lv_obj_has_state(ui_rdacb, LV_STATE_CHECKED)) {
                lv_textarea_add_text(ui_ota, "\n **RDA Coil Active**");
            }
            lv_textarea_add_text(ui_ota, "\n Wait 3 min to engage power");
        }
        deviceController.setDeviceFootprint(FOOTPRINT_OPERATION_STOP);
        dataLogger.saveData(deviceController.getCurrentData());
    }
}

void oob_event(lv_event_t *e) {
    uint8_t dMode = deviceController.getDMode();
    uint8_t upDone = deviceController.getUpDone();
    uint8_t upMode = deviceController.getUpMode();
    
    if (dMode > 0) {
        if (dMode == 8) {
            lv_textarea_add_text(ui_ota, "\n **Unusual Request**");
        } else {
            lv_textarea_add_text(ui_ota, "\n **Stop First**");
        }
        deviceController.setDeviceFootprint(FOOTPRINT_OPERATION_PAUSE);
        dataLogger.saveData(deviceController.getCurrentData());
    } else {
        if (upDone == 2 && upMode == 3) {
            lv_textarea_add_text(ui_ota, "\n **Wait!**");
        } else if (upDone == 3 && upMode == 4) {
            lv_textarea_add_text(ui_ota, "\n **Press Stop**");
        } else if (upMode == 5) {
            deviceController.setConfirm(2);
            lv_scr_load(ui_Screen9);
        }
        deviceController.setDeviceFootprint(FOOTPRINT_OPERATION_RESUME);
        dataLogger.saveData(deviceController.getCurrentData());
    }
}

// Confirmation events
void confirm_yes(lv_event_t *e) {
    uint8_t confirm = deviceController.getConfirm();
    
    switch (confirm) {
        case 1: { // Ramp up operation
            lv_textarea_set_text(ui_ota, " ");
            lv_textarea_set_text(ui_tta, " ");
            comm.consolePrintln("usb");
            
            float targetCurrent = intToFloat(lv_spinbox_get_value(ui_utcd), 2);
            deviceController.setUTargetCurrent(targetCurrent);
            
            char buffer[100];
            sprintf(buffer, "                     Ramp Up Operation        Target Current: %.2f A                 ", targetCurrent);
            lv_label_set_text(ui_Label22, buffer);
            
            float incCurrent = RAMP_UP_STEPS[lv_dropdown_get_selected(ui_uid)];
            deviceController.setUIncCurrent(incCurrent);
            
            deviceController.setUpMode(1);
            lv_scr_load(ui_Screen6);
            lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_bar_set_range(ui_opb, 0, int(targetCurrent));
            lv_bar_set_value(ui_opb, 0, LV_ANIM_ON);
            lv_textarea_add_text(ui_ota, "\n Press Start for operation!");
            
            deviceController.setDeviceFootprint(FOOTPRINT_RAMP_UP_SETUP);
            dataLogger.saveData(deviceController.getCurrentData());
            break;
        }
            
        case 6: // Manual reset
            lv_scr_load(ui_Screen3);
            deviceController.setDisplayMode(0);
            comm.setCurrent(0);
            comm.setVoltage(0);
            lv_scr_load(ui_Screen3);
            deviceController.setDeviceFootprint(FOOTPRINT_MANUAL_RESET);
            dataLogger.saveData(deviceController.getCurrentData());
            break;
            
        // Add other confirmation cases as needed
    }
}

void confirm_no(lv_event_t *e) {
    uint8_t confirm = deviceController.getConfirm();
    
    switch (confirm) {
        case 1:
            lv_scr_load(ui_Screen5);
            deviceController.setConfirm(0);
            break;
        case 6:
            lv_scr_load(ui_Screen4);
            deviceController.setConfirm(0);
            break;
        // Add other cases as needed
    }
}

// Probe and diagnostic events
void probe_event(lv_event_t *e) {
    comm.setCurrent(30000); // 300.0 * 100
    comm.setVoltage(60000); // 600.0 * 100
    lv_obj_clear_flag(ui_Spinner3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Checkbox7, LV_OBJ_FLAG_HIDDEN);
    deviceController.setProbeChecker(true);
    deviceController.setDeviceFootprint(FOOTPRINT_PROBE_TEST);
    dataLogger.saveData(deviceController.getCurrentData());
}

void rda_enb_event(lv_event_t *e) {
    deviceController.setRdaChecked(!deviceController.getRdaChecked());
}

void rurcb_event(lv_event_t *e) {
    float resistorValue = deviceController.getResistorValue();
    if (((resistorValue / 5) * 100) < 99900) {
        lv_spinbox_set_value(ui_utcd, (resistorValue / 5) * 100);
        lv_spinbox_set_value(ui_dtcd, (resistorValue / 5) * 100);
    } else {
        lv_label_set_text(ui_Label37, "**No Connection! Try again**");
        lv_label_set_text(ui_Label54, "**No Connection! Try again**");
    }
    deviceController.setDeviceFootprint(FOOTPRINT_RESISTANCE_READ);
    dataLogger.saveData(deviceController.getCurrentData());
}

// Diagnostic test events
void sccb_event(lv_event_t *e) {
    deviceController.handleDiagnosticTimer(2);
    lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_add_text(ui_dbta, "\n Testing Shim Heater...");
    deviceController.setDeviceFootprint(FOOTPRINT_SHIM_TEST);
    dataLogger.saveData(deviceController.getCurrentData());
}

void mccb_event(lv_event_t *e) {
    deviceController.handleDiagnosticTimer(1);
    lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_add_text(ui_dbta, "\n Testing Main Heater...");
    deviceController.setDeviceFootprint(FOOTPRINT_MAIN_TEST);
    dataLogger.saveData(deviceController.getCurrentData());
}

// Stub implementations for remaining events
void tscd_event(lv_event_t *e) { /* Implementation needed */ }
void tsvd_event(lv_event_t *e) { /* Implementation needed */ }
void tpsi_event(lv_event_t *e) { /* Implementation needed */ }
void tpsob_event(lv_event_t *e) { /* Implementation needed */ }
void tob_event(lv_event_t *e) { /* Implementation needed */ }
void dsb_event(lv_event_t *e) { /* Implementation needed */ }
void dcb_event(lv_event_t *e) { /* Implementation needed */ }
void mbp_event(lv_event_t *e) { /* Implementation needed */ }
void umanic_event(lv_event_t *e) { /* Implementation needed */ }
void ut1c_event(lv_event_t *e) { /* Implementation needed */ }
void ut2c_event(lv_event_t *e) { /* Implementation needed */ }
void uaxc_event(lv_event_t *e) { /* Implementation needed */ }
void urdac_event(lv_event_t *e) { /* Implementation needed */ }
void ResetOperation(lv_event_t *e) { /* Implementation needed */ }
void rub_event(lv_event_t *e) { /* Implementation needed */ }
void t_current_d(lv_event_t *e) { /* Implementation needed */ }
void t_current_i(lv_event_t *e) { /* Implementation needed */ }
void rccb_event(lv_event_t *e) { /* Implementation needed */ }
void pccb_event(lv_event_t *e) { /* Implementation needed */ }
void pqcb_event(lv_event_t *e) { /* Implementation needed */ }
void pfcb_event(lv_event_t *e) { /* Implementation needed */ }
void dpb_event(lv_event_t *e) { /* Implementation needed */ }
void dbb_event(lv_event_t *e) { /* Implementation needed */ }
void dbub_event(lv_event_t *e) { /* Implementation needed */ }
void mbub_event(lv_event_t *e) { /* Implementation needed */ }