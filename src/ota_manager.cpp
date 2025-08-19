#include "ota_manager.h"
#include "communication.h"

OTAManager otaManager;

void OTAManager::init(const String& wifi_ssid, const String& wifi_password, const String& firmware_url) {
    ssid = wifi_ssid;
    password = wifi_password;
    firmwareUrl = firmware_url;
}

bool OTAManager::connectToWiFi() {
    comm.consolePrintln("Connecting to WiFi...");
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long startTime = millis();
    const unsigned long timeout = 30000; // 30 second timeout
    
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > timeout) {
            comm.consolePrintln("WiFi connection timeout");
            return false;
        }
        delay(500);
        comm.consolePrint(".");
    }
    comm.consolePrintln("\nConnected to WiFi");
    return true;
}

DisplayUpdateStatus OTAManager::performUpdate() {
    // Connect to WiFi
    if (!connectToWiFi()) {
        return UPDATE_WIFI_ERROR;
    }

    // Download and apply firmware
    HTTPClient http;
    http.begin(firmwareUrl);
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        
        if (contentLength > 0) {
            bool canBegin = Update.begin(contentLength);
            
            if (canBegin) {
                comm.consolePrintln("Starting OTA update...");
                WiFiClient* stream = http.getStreamPtr();
                
                size_t written = Update.writeStream(*stream);
                
                if (written == contentLength) {
                    comm.consolePrintln("Written : " + String(written) + " bytes successfully");
                } else {
                    comm.consolePrintln("Written only : " + String(written) + "/" + String(contentLength) + " bytes");
                    http.end();
                    return UPDATE_UNKNOWN_ERROR;
                }
                
                if (Update.end()) {      
                    comm.consolePrintln("OTA update completed!");
                    if (Update.isFinished()) {
                        comm.consolePrintln("Update successful. Rebooting...");
                        setUpdateStatus(UPDATE_SUCCESSFUL);
                        http.end();
                        ESP.restart();
                        return UPDATE_SUCCESSFUL;
                    } else {
                        comm.consolePrintln("Update not finished. Something went wrong!");
                        return UPDATE_UNKNOWN_ERROR;
                    }
                } else {
                    comm.consolePrintln("Update error: #" + String(Update.getError()));
                    return UPDATE_UNKNOWN_ERROR;
                }
            } else {
                comm.consolePrintln("Not enough space to begin OTA update");
                return UPDATE_STORE_ERROR;
            }
        } else {
            comm.consolePrintln("Firmware size is invalid (<= 0)");
            return UPDATE_HTTP_ERROR;
        }
    } else {
        comm.consolePrintln("HTTP error: " + String(httpCode));
        return UPDATE_HTTP_ERROR;
    }
    
    http.end();
    return UPDATE_UNKNOWN_ERROR;
}

DisplayUpdateStatus OTAManager::getUpdateStatus() {
    return static_cast<DisplayUpdateStatus>(EEPROM.read(0));
}

void OTAManager::setUpdateStatus(DisplayUpdateStatus status) {
    EEPROM.write(0, static_cast<uint8_t>(status));
    EEPROM.commit();
}

void OTAManager::clearUpdateStatus() {
    setUpdateStatus(UPDATE_NONE);
}

bool OTAManager::isUpdateRequested() {
    return getUpdateStatus() == UPDATE_REQUESTED;
}

void OTAManager::requestUpdate() {
    setUpdateStatus(UPDATE_REQUESTED);
}