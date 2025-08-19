#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <EEPROM.h>
#include "config.h"

/**
 * @brief OTA (Over-The-Air) update manager
 */
class OTAManager {
private:
    String ssid;
    String password;
    String firmwareUrl;
    
    /**
     * @brief Connect to WiFi network
     * @return True if connection successful
     */
    bool connectToWiFi();

public:
    /**
     * @brief Initialize OTA manager
     * @param wifi_ssid WiFi network name
     * @param wifi_password WiFi password
     * @param firmware_url URL to firmware binary
     */
    void init(const String& wifi_ssid, const String& wifi_password, const String& firmware_url);

    /**
     * @brief Perform OTA update
     * @return Update status code
     */
    DisplayUpdateStatus performUpdate();

    /**
     * @brief Get current update status from EEPROM
     * @return Current update status
     */
    DisplayUpdateStatus getUpdateStatus();

    /**
     * @brief Set update status in EEPROM
     * @param status Status to save
     */
    void setUpdateStatus(DisplayUpdateStatus status);

    /**
     * @brief Clear update status (set to none)
     */
    void clearUpdateStatus();

    /**
     * @brief Check if update was requested
     * @return True if update is pending
     */
    bool isUpdateRequested();

    /**
     * @brief Request update on next reboot
     */
    void requestUpdate();
};

extern OTAManager otaManager;

#endif // OTA_MANAGER_H