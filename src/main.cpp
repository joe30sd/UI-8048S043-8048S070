#include <Arduino.h>
#include <lv_conf.h>
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/ui_events.h"
#include "gui/gui.h"
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <EEPROM.h>

#define EEPROM_SIZE 1 

const char* ssid = "Modric";
const char* password = "arctic9744";
 

#define FOUR_ON

#ifdef FOUR_ON
    // If FOUR_ON is defined, this section is compiled.
HardwareSerial& device_m = Serial;
HardwareSerial& console_m = Serial2;
#else
    // FOUR_ON is NOT defined, this section is compiled.
HardwareSerial& device_m = Serial2;
HardwareSerial& console_m = Serial;
#endif



int update_counter = 0 , display_updater = 0 ;
unsigned long update_timer = 0;

// Firmware URL (raw GitHub link for direct binary download)
const char* firmwareUrl = "https://raw.githubusercontent.com/byronin/rnn-bins/main/firmware.bin";

const int chipSelect = 10;

//#define SELF_TEST_ON

// Ramp Down Flag States
enum RampDownState {
    RAMPDOWN_INIT = 0,           // Initial state - show "Wait 3 min to engage power"
    RAMPDOWN_POWER_WAIT = 1,     // Waiting for main heater to become active
    RAMPDOWN_HEATER_ACTIVE = 2,  // Main heater active - 3 minute wait before ramp down
    RAMPDOWN_ACTIVE = 3,         // Ramp down process started
    RAMPDOWN_COMPLETE = 4        // Operation complete
};

int minutesPassed = 0, o_timer = 0 ;
float live_magnet_current = 0, live_magnet_voltage = 0, magnet_current = 0, magnet_voltage = 0, resistor_value = 0;
float main_voltage = 0, main_current = 0, att_current = 0, rda_current = 0, rda_voltage = 0, att_voltage = 0,ts_voltage = 0, c_clb1 = 0, c_clb2 = 0 , c_clb3 = 0, v_clb1 = 0;
int self_tester = 17,self_tester_counter = 0, display_mode_flag = 0;
RampDownState rdf_flag = RAMPDOWN_INIT;
float u_target_current, u_inc_current;
uint8_t up_mode = 0, confirm = 0, up_done = 0, G_MODE = 0;
char stc_buffer[100];
float d_start_current, d_dec_current;
uint8_t d_mode = 0;

uint8_t att_relay = 0, rda_relay = 0, main_relay = 0, display_mode = 0,probe_check_counter = 0;
String s_cmd, PSU_ID;

boolean m_instant_mode = 0, rda_checked = 0, probe_checker = 0, diagnostic_mode = 0 ;
uint8_t diag_x = 0, diag_y = 0, diag_t = 0;
float u_increasing_mode[] = {0.5, 0.6, 0.7, 0.8, 0.9, 1.00, 1.50, 2.00, 2.50, 3.00, 5.00, 10.00};


int  last_o_timer = -1, device_footprint = 0, last_device_footprint = 0;
float last_live_magnet_current = -1, last_live_magnet_voltage = -1,  last_magnet_current = -1, last_magnet_voltage = -1, last_resistor_value = -1;
float last_main_voltage = -1, last_rda_voltage = -1, last_att_voltage = -1, last_ts_voltage = -1;
float last_att_relay = -1, last_rda_relay = -1, last_main_relay = -1, last_display_mode = -1;


// Data line counter for JSON entries
int dataLine = 1;

unsigned long diag_counter = 0, diag_target = 0  ;

String currentDataFile;     // file name to be written in that session

String nextDataFilename()   // returns the name /dataN.txt
{
  uint16_t maxIdx = 0;
  File root = SD.open("/");
  for (File e = root.openNextFile(); e; e = root.openNextFile()) {
    String n = e.name();            // e.g., "data3.txt"
    if (!e.isDirectory() &&
        n.startsWith("data") && n.endsWith(".txt")) {
      uint16_t idx = n.substring(4, n.length() - 4).toInt(); // 4 = "data"
      if (idx > maxIdx) maxIdx = idx;
    }
    e.close();
  }
  root.close();
  return String("/data") + (maxIdx + 1) + ".txt";
}



void performOTAUpdate() {
  // Step 1: Connect to WiFi
  console_m.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    console_m.print(".");
  }
  console_m.println("\nConnected to WiFi");

  // Step 2: Download and apply firmware
  HTTPClient http;
  
  // Initialize HTTP client with the firmware URL
  http.begin(firmwareUrl);
  
  // Send GET request
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) { // HTTP 200
    int contentLength = http.getSize();
    
    if (contentLength > 0) {
      // Prepare OTA update with the firmware size
      bool canBegin = Update.begin(contentLength);
      
      if (canBegin) {
        console_m.println("Starting OTA update...");
        WiFiClient* stream = http.getStreamPtr();
        
        // Write the firmware stream to the Update library
        size_t written = Update.writeStream(*stream);
        
        if (written == contentLength) {
          console_m.println("Written : " + String(written) + " bytes successfully");
        } else {
          console_m.println("Written only : " + String(written) + "/" + String(contentLength) + " bytes");
          http.end();
          return;
        }
        
        // Finalize the update
        if (Update.end()) {      
        console_m.println("OTA update completed!");
          if (Update.isFinished()) {
            console_m.println("Update successful. Rebooting...");
            display_updater = 8; 
            EEPROM.write(0, display_updater);
            EEPROM.commit();
            ESP.restart(); // Restart ESP32 to boot into new firmware
          } else {
            console_m.println("Update not finished. Something went wrong!");
            display_updater = 7; 
            EEPROM.write(0, display_updater);
            EEPROM.commit();
          }
        } else {
          console_m.println("Update error: #" + String(Update.getError()));
        }
      } else {
        console_m.println("Not enough space to begin OTA update");
        display_updater = 6; 
        EEPROM.write(0, display_updater);
        EEPROM.commit();
      }
    } else {
      console_m.println("Firmware size is invalid (<= 0)");
    }
  } else {
    console_m.println("HTTP error: " + String(httpCode));
    display_updater = 4; 
    EEPROM.write(0, display_updater);
    EEPROM.commit();
  }
  
  // Clean up
  http.end();
}

uint8_t calcChecksum(const uint8_t *frame19) {
  uint16_t sum = 0;
  for (int i = 0; i < 19; i++) {
    sum += frame19[i];
  }
  return (uint8_t)(sum & 0xFF);
}

void sendCommand(uint8_t devAddr, uint8_t command, const uint8_t *args) {
  uint8_t frame[20];
  frame[0] = 0xAA;
  frame[1] = devAddr;
  frame[2] = command;
  for (int i = 0; i < 16; i++) {
    frame[3 + i] = args[i];
  }
  frame[19] = calcChecksum(frame);
  console_m.write(frame, 20);
}

void setRemoteModeOn(uint8_t devAddr) {
  uint8_t args[16];
  args[0] = 0x01;
  for (int i = 1; i < 16; i++) args[i] = 0x00;
  sendCommand(devAddr, 0x20, args);
}

void setOutput(uint8_t devAddr, bool on_off) {
  uint8_t args[16];
  args[0] = on_off ? 0x01 : 0x00;
  for (int i = 1; i < 16; i++) args[i] = 0x00;
  sendCommand(devAddr, 0x22, args);
}

void setVoltageCurrent(uint8_t devAddr, float voltage, float current, float ovp, float ocp) {
  uint16_t ovp_val = (uint16_t)(ovp * 100.0f);
  uint16_t ocp_val = (uint16_t)(ocp * 100.0f);
  uint16_t v_val = (uint16_t)(voltage * 100.0f);
  uint16_t i_val = (uint16_t)(current * 1000.0f);
  uint8_t args[16];
  args[0] = (ovp_val >> 8) & 0xFF;
  args[1] = ovp_val & 0xFF;
  args[2] = (ocp_val >> 8) & 0xFF;
  args[3] = ocp_val & 0xFF;
  args[4] = (v_val >> 8) & 0xFF;
  args[5] = v_val & 0xFF;
  args[6] = (i_val >> 8) & 0xFF;
  args[7] = i_val & 0xFF;
  for (int i = 8; i < 16; i++) args[i] = 0x00;
  sendCommand(devAddr, 0x2C, args);
}


void setup() {
 /* console_m.begin(9600);
  delay(2000);
  setRemoteModeOn(1);
  delay(100);
  setRemoteModeOn(2);
  delay(100);
  setVoltageCurrent(1, 22.0, 0.85, 30.0, 10.0);
  delay(100);
  setVoltageCurrent(2, 22.0, 0.75, 30.0, 10.0);
  delay(100);
  setOutput(1, true);
  delay(100);
  setOutput(2, true);
  delay(100);
  console_m.end();
  delay(300);
*/
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 17, 18);

  EEPROM.begin(EEPROM_SIZE);
  
  
  display_updater  = EEPROM.read(0);

  if(display_updater == 1){
  //lv_label_set_text(ui_mbft, "Uing");
  display_updater = 5; 
  EEPROM.write(0, display_updater);
  EEPROM.commit();
  console_m.println("UPDATE START");
  performOTAUpdate();
  

    if (!SD.begin(chipSelect)) { console_m.println("Card Mount Failed"); return; }

  currentDataFile = nextDataFilename();         
  console_m.print("Log file: ");
  console_m.println(currentDataFile);

  
  }




  gui_start();

  if (!SD.begin(chipSelect)) {
    console_m.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    console_m.println("No SD card attached");
    return;
  }
  console_m.println("SD card initialized.");


}



bool hasChanged() {
  // Returns true if any variable has changed since the last save
  return (device_footprint!= last_device_footprint ||
          live_magnet_current != last_live_magnet_current ||
          live_magnet_voltage != last_live_magnet_voltage ||
          magnet_current != last_magnet_current ||
          magnet_voltage != last_magnet_voltage ||
          resistor_value != last_resistor_value ||
          main_voltage != last_main_voltage ||
          rda_voltage != last_rda_voltage ||
          att_voltage != last_att_voltage ||
          ts_voltage != last_ts_voltage ||
          att_relay != last_att_relay ||
          rda_relay != last_rda_relay ||
          main_relay != last_main_relay ||
          display_mode != last_display_mode);
}


void saving_data() {
  static unsigned long startMillis = millis();   // 00:00:00 referance
  if (!hasChanged()) return;                     // exit if there is no change

  // calculate HH:MM:SS
  unsigned long elapsed = (millis() - startMillis) / 1000;
  int hours = elapsed / 3600;
  int minutes = (elapsed % 3600) / 60;
  int seconds = elapsed % 60;
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);

  StaticJsonDocument<512> doc;
  doc["data_line"] = dataLine;
  doc["time"]      = timeStr;

  doc["o_timer"]             = o_timer;
  doc["live_magnet_current"] = live_magnet_current;
  doc["live_magnet_voltage"] = live_magnet_voltage;
  doc["magnet_current"]      = magnet_current;
  doc["magnet_voltage"]      = magnet_voltage;
  doc["resistor_value"]      = resistor_value;
  doc["main_voltage"]        = main_voltage;
  doc["rda_voltage"]         = rda_voltage;
  doc["att_voltage"]         = att_voltage;
  doc["ts_voltage"]          = ts_voltage;
  doc["att_relay"]           = att_relay;
  doc["rda_relay"]           = rda_relay;
  doc["main_relay"]          = main_relay;
  doc["display_mode"]        = display_mode;
  doc["footprint"]           = device_footprint;

  File file = SD.open(currentDataFile, FILE_APPEND);     //<< changed
  if (file) {
    serializeJson(doc, file);
    file.println();
    file.close();
    console_m.print("Saved to "); console_m.println(currentDataFile);
    dataLine++;

    last_o_timer             = o_timer;
    last_live_magnet_current = live_magnet_current;
    last_live_magnet_voltage = live_magnet_voltage;
    last_magnet_current      = magnet_current;
    last_magnet_voltage      = magnet_voltage;
    last_resistor_value      = resistor_value;
    last_main_voltage        = main_voltage;
    last_rda_voltage         = rda_voltage;
    last_att_voltage         = att_voltage;
    last_ts_voltage          = ts_voltage;
    last_att_relay           = att_relay;
    last_rda_relay           = rda_relay;
    last_main_relay          = main_relay;
    last_display_mode        = display_mode;
  } else {
    console_m.print("Cannot open "); console_m.println(currentDataFile);
  }
}


/*---- Dumps the last 4 data*.txt files ----*/
void dumpLast4DataFiles() {
  struct FileInfo { uint16_t idx; String name; };
  FileInfo files[20];      // we expect at max 20 files
  uint8_t  count = 0;

  /* 1) collect the data*.txt files*/
  File root = SD.open("/");
  for (File e = root.openNextFile(); e; e = root.openNextFile()) {
    if (e.isDirectory()) { e.close(); continue; }

    String n = e.name();                     // "data12.txt"
    if (n.startsWith("data") && n.endsWith(".txt")) {
      uint16_t i = n.substring(4, n.length() - 4).toInt();
      if (count < 20) { files[count++] = { i, n }; }
    }
    e.close();
  }
  root.close();
  if (count == 0) { Serial.println("DUMP ERR"); return; }

  /* 2) sort by index (small to large) */
  std::sort(files, files + count,
            [](const FileInfo& a, const FileInfo& b){ return a.idx < b.idx; });

  /* 3) select the last block of 4 */
  uint8_t start = (count > 4) ? count - 4 : 0;

  Serial.println("DUMP BEGIN");
  for (uint8_t k = start; k < count; ++k) {
    File f = SD.open("/" + files[k].name, FILE_READ);
    if (!f) continue;

    Serial.print("FILE "); Serial.println(files[k].name);
    while (f.available()) {
      Serial.println(f.readStringUntil('\n'));
      delay(2);
    }
    f.close();
    Serial.print("END "); Serial.println(files[k].name);
  }
  Serial.println("DUMP DONE");
}


void set_current(uint32_t s_current)
{

    float formatted_current = s_current / 100.0; // We divide the number by 100 to make it fractional.
    device_m.print("C");                          // Print the start of the SCPI command.
    device_m.println(formatted_current, 2);
    magnet_current = formatted_current;
}
void set_voltage(uint32_t s_voltage)
{

    float formatted_voltage = s_voltage / 100.0; // We divide the number by 100 to make it fractional.
    device_m.print("V");                          // Print the start of the SCPI command.
    device_m.println(formatted_voltage, 2);
    magnet_voltage = formatted_voltage;
}
float intToFloat(uint32_t number, u32_t decimalPlaces)
{
   
    float result = number; 

    
    for (int i = 0; i < decimalPlaces; i++)
    {
        result /= 10.0;
    }

    return result;
}


void reset_variables(){
    #if defined(SELF_TESTER_ON)
    if(self_tester == 13 || self_tester == 17){
    device_m.println("D0");
    }
    #endif
minutesPassed = 0;
o_timer = 0 ;
live_magnet_current = 0;
live_magnet_voltage = 0;
magnet_current = 0;
magnet_voltage = 0;
resistor_value = 0;
main_voltage = 0;
rda_voltage = 0;
att_voltage = 0;
ts_voltage = 0;
u_target_current;
u_inc_current;
up_mode = 0;
confirm = 0;
up_done = 0;
d_start_current=0;
d_dec_current=0;

att_relay = 0;
rda_relay = 0;
main_relay = 0;
probe_check_counter = 0;
d_mode = 0;
G_MODE = 0;
m_instant_mode = 0;
rda_checked = 0;
probe_checker = 0;
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
lv_obj_clear_state(ui_rdacb, LV_STATE_CHECKED);
                lv_label_set_text(ui_Label24, "Main Heater                                                  Axial Coil                                      Time");
        lv_label_set_text(ui_Label25, "ON/OFF                                                      ON/OFF");
        lv_label_set_text(ui_Label26, "V                                     V                            Min");
        lv_obj_clear_state(ui_axof, LV_STATE_CHECKED);
        lv_obj_clear_flag(ui_axof, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(ui_mainof, LV_STATE_CHECKED);
        lv_obj_clear_flag(ui_oattvd, LV_OBJ_FLAG_HIDDEN);
        delay(100);

}

void map_event(lv_event_t *e)
{
    lv_scr_load(ui_Screen4);
    display_mode = 1;
    device_m.println("D1");
    device_footprint = 11;
    saving_data();
}

void rdpb_event(lv_event_t *e)
{
    lv_scr_load(ui_Screen8);
    display_mode = 3;
    device_m.println("D3");
    device_footprint = 14;
      saving_data();
}

void rupb_event(lv_event_t *e)
{
    // Your code here
    lv_scr_load(ui_Screen5);
    display_mode = 2;
    device_m.println("D2");
    device_footprint = 13;
      saving_data();
}

void mscd_event(lv_event_t *e)
{
    // Your code here
    if (m_instant_mode == 1)
    {
        int32_t value = lv_spinbox_get_value(ui_mscd);
        set_current(value);
    }
}

void msvd_event(lv_event_t *e)
{
    // Your code here
    if (m_instant_mode == 1)
    {
        int32_t value = lv_spinbox_get_value(ui_msvd);
        set_voltage(value);
    }
}

void axrb_event(lv_event_t *e)
{
    // Your code here
    device_m.println("A1");
    att_relay = 1;
}

void t1rb_event(lv_event_t *e)
{
    // Your code here
    device_m.println("A2");
    att_relay = 2;
}

void t2rb_event(lv_event_t *e)
{
    // Your code here
    device_m.println("A3");
    att_relay = 3;
}

void rdarb_event(lv_event_t *e)
{
    // Your code here
    if (rda_relay == 0)
    {
        device_m.println("R1");
        rda_relay = 1;
    }
    else
    {
        device_m.println("R0");
        rda_relay = 0;
    }
}

void mainrb_event(lv_event_t *e)
{
    // Your code here
    if (main_relay == 0)
    {
        device_m.println("M1");
        main_relay = 1;
    }
    else
    {
        device_m.println("M0");
        main_relay = 0;
    }
}

void mpsi_event(lv_event_t *e)
{
    // Your code here
    if (m_instant_mode == 0)
    {
        m_instant_mode = 1;
    }
    else
    {
        m_instant_mode = 0;
    }
}

void mpsob_event(lv_event_t *e)
{
    // Your code here
    int32_t value1 = lv_spinbox_get_value(ui_msvd);
    set_voltage(value1);
    int32_t value2 = lv_spinbox_get_value(ui_mscd);
    set_current(value2);
}

void mbb_event(lv_event_t *e)
{
    confirm = 6;
    lv_scr_load(ui_Screen9);
    
   
}

void attob_event(lv_event_t *e)
{
    // Your code here
    device_m.println("A0");
    att_relay = 0;
}

void usb_event(lv_event_t *e)
{
    // Your code here
    if(intToFloat(lv_spinbox_get_value(ui_utcd), 2) < 1){
        lv_spinbox_set_value(ui_utcd, 120 );
    }
    confirm = 1;
    lv_scr_load(ui_Screen9);
}

void ucb_event(lv_event_t *e)
{
    // Your code here
    display_mode = 0;
    lv_scr_load(ui_Screen3);
    reset_variables();
}

void ossb_event(lv_event_t *e)
{
    
     //lv_obj_add_state(ui_axof, LV_STATE_CHECKED); //ronin
    //  Your code here

if(confirm == 1 || confirm == 2 ){

    if (up_mode == 1)  // Skip communication wait - go directly to active mode
    {
        lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0x08FF80), LV_PART_MAIN | LV_STATE_DEFAULT);
        up_mode = 2;
        device_m.println("U2");
        lv_textarea_add_text(ui_ota, "\n Main Heater Active");
        lv_textarea_add_text(ui_ota, "\n Axial Coil Active");
        lv_textarea_add_text(ui_ota, "\n **Please Wait Adjusting Ramp Up**");
    }
    else if (up_mode == 3)
    {
        lv_textarea_add_text(ui_ota, "\n **Wait engage operation**");
    }
    else if (up_mode == 4 && up_done != 4)
    {
        device_m.println("U5");
        up_mode = 5;
        lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else if (up_mode == 5)
    {
        if(G_MODE == 0){
        device_m.println("U4");
        up_mode = 4;
        lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0x08FF80), LV_PART_MAIN | LV_STATE_DEFAULT);
        }else {
            lv_textarea_add_text(ui_ota, "\n Wait for safe setup");
        }
        
    }
    console_m.print("UP");
    console_m.println(up_mode);

    device_footprint = 20;
      saving_data();
} else if(confirm == 4){

if (d_mode == 0){
    device_m.println("U1");
        d_mode = 1;
        lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0x08FF80), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_textarea_add_text(ui_ota, "\n **Ramp Down is Start**");
         if(lv_obj_has_state(ui_rdacb, LV_STATE_CHECKED)){
            lv_textarea_add_text(ui_ota, "\n **RDA Coil Active**");
        }else{
            lv_textarea_add_text(ui_ota, "\n ");
        }
         lv_textarea_add_text(ui_ota, "\n Wait 3 min to engage power");


}else if(d_mode == 4){
    lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_textarea_add_text(ui_ota, "\n **Wait 1 Sec.**");
    device_m.println("U8");
    d_mode = 8;
    delay(1000);
    lv_textarea_add_text(ui_ota, "\n **Stopped**");

}else if(d_mode == 8){
    lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_textarea_add_text(ui_ota, "\n **Wait 1 Sec.**");
    device_m.println("U4");
    d_mode = 8;
    delay(1000);
    lv_textarea_add_text(ui_ota, "\n **Operation Continues**");

}else{
    lv_textarea_add_text(ui_ota, "\n **Can't Stop**");
    
}
device_footprint = 21;
      saving_data();
}

    
}

void oob_event(lv_event_t *e)
{

    if(d_mode > 0){
        if(d_mode == 8 ){
        lv_textarea_add_text(ui_ota, "\n **Unusual Request**");
        }else{
            lv_textarea_add_text(ui_ota, "\n **Stop First**");
        }
device_footprint = 22;
      saving_data();
    }else{
       
if (up_done == 2 && up_mode == 3)
    {
        lv_textarea_add_text(ui_ota, "\n **Wait!**");
    }
    else if (up_done == 3 && up_mode == 4)
    {
        lv_textarea_add_text(ui_ota, "\n **Press Stop**");
    }
    else if (up_mode == 5)
    {
        confirm = 2;
        lv_scr_load(ui_Screen9);
    }
     device_footprint = 23;
      saving_data();
    }
    // Your code here
    
}

void tscd_event(lv_event_t *e)
{
    if(confirm == 3 && (up_mode == 6 || up_mode == 7) ){
   device_footprint = 26;
      saving_data();
    }else{
    int32_t value = lv_spinbox_get_value(ui_tscd);
    set_current(value);
    device_footprint = 27;
    saving_data();
    }
    
}

void tsvd_event(lv_event_t *e)
{
    // Your code here
}

void tpsi_event(lv_event_t *e)
{
    // Your code here
}

void tpsob_event(lv_event_t *e)
{
    // Your code here
}

void tob_event(lv_event_t *e)
{
    if (display_mode == 2)
    {
        if (up_mode != 6)
        {
            lv_textarea_add_text(ui_tta, "\n **Wait**");
        }
        else
        {
            confirm = 3;
            lv_scr_load(ui_Screen9);
        }
    }
    else if (display_mode == 3)
    {
        confirm = 5;
        lv_scr_load(ui_Screen9);
    }

    // Your code here
}

void dsb_event(lv_event_t *e)
{
    // Your code here
    if(intToFloat(lv_spinbox_get_value(ui_dtcd), 2) < 1){
        lv_spinbox_set_value(ui_dtcd, 120 );
    }
    confirm = 4;
    lv_scr_load(ui_Screen9);
}

void dcb_event(lv_event_t *e)
{
    lv_scr_load(ui_Screen3);
    // Your code here
    reset_variables();
    device_footprint = 30;
    saving_data();
}

void mbp_event(lv_event_t *e)
{
    // Your code here
}

void umanic_event(lv_event_t *e)
{
    // Your code here
}

void ut1c_event(lv_event_t *e)
{
    // Your code here
}

void ut2c_event(lv_event_t *e)
{
    // Your code here
}

void uaxc_event(lv_event_t *e)
{
    // Your code here
}

void urdac_event(lv_event_t *e)
{
    // Your code here
}
void rurcb_event(lv_event_t *e)
{
    if(((resistor_value / 5) * 100) < 99900){
    lv_spinbox_set_value(ui_utcd, (resistor_value / 5) * 100 );
    lv_spinbox_set_value(ui_dtcd, (resistor_value / 5) * 100 );

    }else{
        lv_label_set_text(ui_Label37, "**No Connection! Try again**");
        lv_label_set_text(ui_Label54, "**No Connection! Try again**");
    }
    device_footprint = 16;
      saving_data();
}
void t_current_d(lv_event_t *e)
{
    // Your code here
    if (up_mode == 6)
    {
        magnet_current = magnet_current - u_inc_current;
        device_m.print("C");
        device_m.println(magnet_current);
    }
    lv_spinbox_set_value(ui_tscd, magnet_current * 100);
}

void t_current_i(lv_event_t *e)
{
    // Your code here
    if (up_mode == 6)
    {

        magnet_current = magnet_current + u_inc_current;
        device_m.print("C");
        device_m.println(magnet_current);
    }
    lv_spinbox_set_value(ui_tscd, magnet_current * 100);
}


void confirm_yes(lv_event_t *e)
{
    // Your code here
    if (confirm == 1)
    {

        lv_textarea_set_text(ui_ota, " ");
        lv_textarea_set_text(ui_tta, " ");
        console_m.println("usb");
        u_target_current = intToFloat(lv_spinbox_get_value(ui_utcd), 2);
        device_m.print("T");
        device_m.println(u_target_current);
        console_m.print("T");
        console_m.println(u_target_current);
        sprintf(stc_buffer, "                     Ramp Up Operation        Target Current: %.2f A                 ", u_target_current);
        lv_label_set_text(ui_Label22, stc_buffer);
        u_inc_current = u_increasing_mode[lv_dropdown_get_selected(ui_uid)];
        device_m.print("I");
        device_m.println(u_inc_current);
        console_m.print("I");
        console_m.println(u_inc_current);
        device_m.println("A1");
        device_m.println("M1");
        device_m.println("U1");
        up_mode = 1;
        lv_scr_load(ui_Screen6);
        lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_bar_set_range(ui_opb, 0, int(u_target_current));
        lv_bar_set_value(ui_opb, 0, LV_ANIM_ON);
        lv_textarea_add_text(ui_ota, "\n Press Start for operation!");
        device_footprint = 19;
        saving_data();
    }
    else if (confirm == 2)
    {
        if (up_mode == 5)
        {
            device_m.println("U6");
            up_mode = 6;
            lv_scr_load(ui_Screen7);
        }
    }
    else if (confirm == 3)
    {
        if (up_mode == 6)
        {
            lv_scr_load(ui_Screen7);
            device_m.println("U7");
            up_mode = 7;
            lv_textarea_add_text(ui_tta, "\n ***Park Mode is Start***");
            //lv_textarea_add_text(ui_tta, "\n  Main Heater OFF");
            //lv_textarea_add_text(ui_tta, "\n  Axial Coil OFF");
            lv_textarea_add_text(ui_tta, "\n  Wait 3 min for park");
            lv_obj_clear_flag(ui_Spinner1, LV_OBJ_FLAG_HIDDEN);
            
            
        }
    }
    else if (confirm == 4)
    {

        //rampdown icin kur
        lv_textarea_set_text(ui_ota, " ");
       if(rda_checked == 0){
        lv_label_set_text(ui_Label24, "Main Heater                                                                                                  Time");
        lv_label_set_text(ui_Label25, "ON/OFF                                                            ");
        lv_label_set_text(ui_Label26, "V                                                                  Min");
        lv_obj_add_flag(ui_axof, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(ui_oattvd, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(ui_mainof, LV_STATE_CHECKED);
        device_footprint = 28;
    saving_data();
       }else{
        lv_label_set_text(ui_Label24, "Main Heater                                                  RDA Coil                                      Time");
        lv_label_set_text(ui_Label25, "ON/OFF                                                      ON/OFF");
        lv_label_set_text(ui_Label26, "V                                     V                            Min");
        lv_obj_clear_state(ui_axof, LV_STATE_CHECKED);
        lv_obj_clear_flag(ui_axof, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(ui_mainof, LV_STATE_CHECKED);
        lv_obj_clear_flag(ui_oattvd, LV_OBJ_FLAG_HIDDEN);
      device_footprint = 29;
    saving_data();
       }
        

        console_m.println("dsb");
        device_m.println("D3");
        d_start_current = intToFloat(lv_spinbox_get_value(ui_dtcd), 2);
        device_m.print("T");
        device_m.println(d_start_current);
        console_m.print("T");
        console_m.println(d_start_current);
        sprintf(stc_buffer, "                  Ramp Down Operation        Start Current: %.2f A              ", d_start_current);
        lv_label_set_text(ui_Label22, stc_buffer);
        d_dec_current = u_increasing_mode[lv_dropdown_get_selected(ui_did)];
        device_m.print("I");
        device_m.println(d_dec_current);
        console_m.print("I");
        console_m.println(d_dec_current);
        device_m.println("M1");
        if(lv_obj_has_state(ui_rdacb, LV_STATE_CHECKED)){
            device_m.println("R1");
        }else{
            device_m.println("R0");
        }
        
        display_mode =3 ;
        
        // device_m.println("M1");
        
        lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_scr_load(ui_Screen6);
        // lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        // lv_bar_set_range(ui_ob, 0, int(u_target_current));
        // lv_bar_set_value(ui_ob, 0, LV_ANIM_ON);
        lv_textarea_add_text(ui_ota, "\n Press Start for Operation!");
        lv_bar_set_range(ui_opb, 0, int(d_start_current));
        lv_bar_set_value(ui_opb, 0, LV_ANIM_ON);

    }
    else if (confirm == 5)
    {
        d_mode = 0;
        lv_scr_load(ui_Screen3);
    }  else if (confirm == 6)
    {
         lv_scr_load(ui_Screen3);
    display_mode = 0;
    device_m.println("A0");
    device_m.println("M0");
    device_m.println("R0");
    set_current(0.0);
    set_voltage(0.0);
    lv_obj_clear_state(ui_axrb, LV_STATE_CHECKED);
    lv_obj_clear_state(ui_t1rb, LV_STATE_CHECKED);
    lv_obj_clear_state(ui_t2rb, LV_STATE_CHECKED);
    lv_obj_clear_state(ui_rdarb, LV_STATE_CHECKED);
    lv_obj_clear_state(ui_mainrb, LV_STATE_CHECKED);
    device_m.println("D0");
        
        lv_scr_load(ui_Screen3);
        device_footprint = 15;
      saving_data();
    }else if (confirm == 8)
    {
        // main board update
        lv_scr_load(ui_Screen2);
        lv_textarea_add_text(ui_dbta, "\n **** MAIN BOARD GOING UPDATE ****");
        device_m.println("X50");
        device_footprint = 7;
      saving_data();

    } else if (confirm == 9)
    {
        // display update
        lv_scr_load(ui_Screen2);
    lv_textarea_add_text(ui_dbta, "\n **** DISPLAY GOING UPDATE ****");
    lv_textarea_add_text(ui_dbta, "\n **** DISPLAY CAN BE OFF 2 MINUTES ****");
    lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    display_updater = 1; 
    device_footprint = 9 ;
      saving_data();
    
    } 
}

void confirm_no(lv_event_t *e)
{
    // Your code here
    if (confirm == 1)
    {
        lv_scr_load(ui_Screen5);
        confirm = 0;
    }
    else if (confirm == 2)
    {
        lv_scr_load(ui_Screen6);
        confirm = 0;
    }
    else if (confirm == 3)
    {
        lv_scr_load(ui_Screen7);
        confirm = 0;
    }
    else if (confirm == 4)
    {
        lv_scr_load(ui_Screen8);
        confirm = 0;
    }
    else if (confirm == 5)
    {
        lv_scr_load(ui_Screen7);
        confirm = 0;
    }else if (confirm == 6)
    {
        lv_scr_load(ui_Screen4);
        confirm = 0;
    }else if (confirm == 8)
    {
        // main board update
        lv_scr_load(ui_Screen2);
        confirm = 0;
        device_footprint = 8;
      saving_data();
    } else if (confirm == 9)
    {
        // display update
        lv_scr_load(ui_Screen2);
        confirm = 0;
        device_footprint = 10;
      saving_data();
    } 
}

   
void ResetOperation(lv_event_t *e){
    if(probe_checker == 1){
    probe_check_counter = 0;
    probe_checker = 0;
    lv_scr_load(ui_Screen5);
    lv_obj_add_flag(ui_Spinner3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Checkbox7, LV_OBJ_FLAG_HIDDEN);
    }else{

        lv_scr_load(ui_Screen6);
       // lv_textarea_add_text(ui_ota, "\n *** Wait 2 Sec. ***");
        delay(100);
        lv_textarea_add_text(ui_ota, "\n ***Can't Resume Operation!***");
        lv_textarea_add_text(ui_ota, "\n Wait 3min for safe setup");
        lv_obj_clear_state(ui_mainof, LV_STATE_CHECKED);
        lv_obj_clear_state(ui_axof, LV_STATE_CHECKED);
        device_m.println("G1");
        G_MODE = 1;
    }


}
void rub_event(lv_event_t * e)
{
	// Your code here
    if(G_MODE == 1){
    lv_textarea_add_text(ui_ota, "\n Wait for safe setup");
    } else if(up_mode == 5 || (display_mode == 3 && d_mode > 1)){

    att_relay = 0;
    rda_relay = 0; 
    main_relay = 0;
    lv_textarea_add_text(ui_ota, "\n The operation is coming to an end");
    lv_textarea_add_text(ui_ota, "\n *** Wait 5 Sec. ***");
    device_m.println("U0");
    delay(1000);
    up_done = 0;
    up_mode = 0;
    device_m.println("D0");
    delay(300);
    device_m.println("D1");
    delay(300);
    set_current(0.0);
    set_voltage(0.0);
    device_m.println("D0");
    device_m.println("A0");
    delay(300);
    device_m.println("M0");
    delay(300);
    device_m.println("R0");
    delay(400);
    delay(400);
    if(display_mode == 3){
    device_m.println("D3");
    delay(400);
    lv_scr_load(ui_Screen8);

    }else{
    device_m.println("D2");
    delay(400);
    lv_scr_load(ui_Screen5);
    device_m.println("A0");
    delay(300);
    device_m.println("M0");
    delay(300);
    device_m.println("R0");
    delay(400);
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
    }
     device_footprint = 23;
      saving_data();


    }else if(((up_mode == 1 || up_mode == 0 ) && display_mode == 2 )|| (display_mode == 3 && d_mode == 0) ){
   device_m.println("D0");
   device_m.println("U0");
   if(display_mode == 3){
    device_m.println("D3");
    delay(400);
    lv_scr_load(ui_Screen8);
   }else{
    device_m.println("D2");
    delay(400);
    lv_scr_load(ui_Screen5);
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
   }
    
    device_m.println("A0");
    device_m.println("M0");
    device_m.println("U0");
     device_footprint = 24;
      saving_data();}else{
    lv_textarea_add_text(ui_ota, "\n ***Please stop the operation first!***");
    }
}


void probe_event(lv_event_t * e)
{
	// Your code here
    device_m.println("D0");
    delay(100);
    device_m.println("D1");
    set_current(300.0);
    set_voltage(600.0);
    lv_obj_clear_flag(ui_Spinner3, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Checkbox7, LV_OBJ_FLAG_HIDDEN);
    probe_checker = 1;

device_footprint = 17;
      saving_data();
}
void rda_enb_event(lv_event_t * e)
{

    if(rda_checked == 0){
rda_checked = 1;
    }else{
rda_checked = 0;
    }
	
}

void diag_timer(uint8_t timer_mode){
    if(timer_mode == 1){
    diag_counter = millis();
    diag_t = 1;
    diag_target = 20000;
    } else if(timer_mode == 2){
    diag_counter = millis();
    diag_t = 1;
    diag_target = 60000;
    } else if(timer_mode == 3){
    diag_counter = millis();
    diag_t = 1;
    diag_target = 90000;
    } else if(timer_mode == 4){
    diag_t = 0;
     lv_textarea_add_text(ui_dbta, "\n Test Fail: 42 !");  // Board communication fail
     lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
     diag_x = 0; 
     diag_y = 0;
    }else if(timer_mode == 5){
    diag_t = 0;
    diag_x = 0; 
    diag_y = 0;
    }

}


void sccb_event(lv_event_t * e)
{
	// Your code here
    if(diag_x == 0 && diag_y == 0){
    diag_timer(2);
    diag_x = 1; 
    diag_y = 1;
    device_m.println("X1");
    lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_add_text(ui_dbta, "\n Testing Shim Heater...");
    }else{
    lv_textarea_add_text(ui_dbta, "\n Test has already started...");
    }
    device_footprint = 1;
    saving_data();
    
}

void mccb_event(lv_event_t * e)
{
	// Your code here
    if(diag_x == 0 && diag_y == 0){
    diag_timer(1);
    diag_x = 5; 
    diag_y = 5;
    device_m.println("X5");
    lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_add_text(ui_dbta, "\n Testing Main Heater...");
    }else{
    lv_textarea_add_text(ui_dbta, "\n Test has already started...");
    }
    device_footprint = 2;
    saving_data();
}
void rccb_event(lv_event_t * e)
{
	// Your code here
    if(diag_x == 0 && diag_y == 0){
    diag_timer(1);
    diag_x = 10; 
    diag_y = 10;
    device_m.println("X10");
    lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_add_text(ui_dbta, "\n Testing RDA Heater...");
    update_timer = millis();
    }else{
    lv_textarea_add_text(ui_dbta, "\n Test has already started...");
    }
    device_footprint = 3;
    saving_data();
}
void pccb_event(lv_event_t * e)
{
	// Your code here
    if(diag_x == 0 && diag_y == 0){
    diag_timer(1);
    diag_x = 15; 
    diag_y = 15;
    device_m.println("X15");
    lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_add_text(ui_dbta, "\n Testing PSU Communication...");
    update_timer = millis();
    }else{
        
    lv_textarea_add_text(ui_dbta, "\n Test has already started...");
    
    }
    device_footprint = 4;
    saving_data();
}

void pqcb_event(lv_event_t * e)
{
	// Your code here
    if(diag_x == 0 && diag_y == 0){
    diag_timer(2);
    diag_x = 20; 
    diag_y = 20;
    device_m.println("X20");
    lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_add_text(ui_dbta, "\n Checking PSU...");
    }else{
    lv_textarea_add_text(ui_dbta, "\n Test has already started...");
    }
    device_footprint = 5;
    saving_data();
}

void pfcb_event(lv_event_t * e)
{
	// Your code here
    if(diag_x == 0 && diag_y == 0){
    diag_timer(3);
    diag_x = 25; 
    diag_y = 25;
    device_m.println("X25");
    lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_add_text(ui_dbta, "\n Checking PSU...");
    }else{
    lv_textarea_add_text(ui_dbta, "\n Test has already started...");
    }
    device_footprint = 6;
    saving_data();
}



void dpb_event(lv_event_t * e)
{
	// Your code here
    lv_scr_load(ui_Screen2);
    device_m.println("D5");
    diagnostic_mode = 0;
    diag_x = 0;
    diag_y = 0;
    display_mode = 5;
    lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    device_footprint = 13;
      saving_data();
    
}
void dbb_event(lv_event_t * e)
{
    if(diag_x == 0 && diag_y == 0){
    lv_scr_load(ui_Screen3);
    device_m.println("D0");
    display_mode = 0;
    display_mode_flag = 0;
    lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    }else{
        lv_textarea_add_text(ui_dbta, "\n Wait!");
    }
    device_footprint = 7;
    saving_data();
}

void dbub_event(lv_event_t * e)
{
	// Your code here
if(confirm != 8){
confirm = 8;
lv_scr_load(ui_Screen9);
}else{
lv_textarea_add_text(ui_dbta, "\n Wait!");
}
    
}

void mbub_event(lv_event_t * e)
{
	// Your code here
    if(confirm != 9){
confirm = 9;
lv_scr_load(ui_Screen9);
}else{
lv_textarea_add_text(ui_dbta, "\n Wait!");
}
}

void diag_control(uint8_t dy_command){
diag_timer(5);
if(dy_command == 2){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 2"); // shim check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 3){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 3"); // shim check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 4){
 lv_textarea_add_text(ui_dbta, "\n Test Pass!");  // shim check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN); 
}else if(dy_command == 6){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 6"); // main check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 7){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 7"); // main check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 9){
 lv_textarea_add_text(ui_dbta, "\n Test Pass!");  // main check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
} else if(dy_command == 11){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 11"); // rda check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 12){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 12"); // rda check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 14){
 lv_textarea_add_text(ui_dbta, "\n Test Pass!");   // rda check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
} else if(dy_command == 16){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 16");  // PSU ID check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 17){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 17");  // PSU ID check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 19){
 lv_textarea_add_text(ui_dbta, "\n Test Pass!");  // PSU ID check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
 lv_textarea_add_text(ui_dbta, "\n");
 lv_textarea_add_text(ui_dbta, PSU_ID.c_str());
}if(dy_command == 21){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 21");  // PSU quick check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 22){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 22");  // PSU quick check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 24){
 lv_textarea_add_text(ui_dbta, "\n Test Pass!");  // PSU quick check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 26){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 26"); // PSU functional check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 27){
 lv_textarea_add_text(ui_dbta, "\n Test Fail: 27"); // PSU functional check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
}else if(dy_command == 29){
 lv_textarea_add_text(ui_dbta, "\n Test Pass!");  // PSU functional check
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);

}else if(dy_command == 51){
 lv_textarea_add_text(ui_dbta, "\n ****WIFI UPDATE START**** ");  // Wifi Update
 lv_obj_clear_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);

}else if(dy_command == 52){
 lv_textarea_add_text(ui_dbta, "\n ****WIFI UPDATE DONE**** ");  // Wifi Update
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);

}else if(dy_command == 53){
 lv_textarea_add_text(ui_dbta, "\n ****WIFI UPDATE ERROR**** ");  //Wifi Update
 lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);

}else if(dy_command == 40 && display_updater == 0 ){
lv_scr_load(ui_Screen3);
device_m.println("D0");
display_mode = 0;
display_mode_flag = 0;
self_tester = 19;
}

diag_x = 0; 
diag_y = 0;
}




void loop()
{


lv_timer_handler();
delay(1);


if(display_updater > 1){
    
update_counter++;
delay(100);
 if(display_updater == 5 ){
 lv_label_set_text(ui_mbft, "Wi-Fi ERROR FOR UPDATE");
 }
 else if(display_updater == 4){
lv_label_set_text(ui_mbft, "HTTP ERROR FOR UPDATE");
}
 else if(display_updater == 6){
lv_label_set_text(ui_mbft, "STORE ERROR FOR UPDATE");
 }
 else if(display_updater == 7 ){
lv_label_set_text(ui_mbft, "UNKNOW ERROR FOR UPDATE");
 }
 else if (display_updater == 8) {
lv_label_set_text(ui_mbft, "UPDATE SUCCESFUL");
}

lv_obj_clear_flag(ui_mbft, LV_OBJ_FLAG_HIDDEN);
if(update_counter > 70){
display_updater = 0; 
update_counter = 0;
EEPROM.write(0, display_updater);
EEPROM.commit();
lv_scr_load(ui_Screen3);
device_m.println("D0");
display_mode = 0;
display_mode_flag = 0;
self_tester = 19;
}
}




if(diag_t == 1){
 if(millis()- diag_counter > diag_target){
 diag_timer(4);
 }
}

    if(self_tester == 17){
    delay(1000);
    device_m.println("D5");
    delay(1000);
    device_m.println("X40"); // self test
    self_tester = 18;
    diag_counter = millis();
    saving_data();
    } else if (self_tester == 18 && millis() - diag_counter > 15000 ){
    lv_obj_clear_flag(ui_mbft, LV_OBJ_FLAG_HIDDEN);
    self_tester = 19;
device_footprint = 50;


    }
#if defined(SELF_TESTER_ON)
   if(self_tester == 0){
    delay(1000);
    lv_scr_load(ui_Screen2);
    self_tester = 1;
    device_m.println("D1");
    delay(100);
    device_m.println("A1");
    self_tester = 2;
   }else if(self_tester == 12){
    self_tester = 13;
    lv_obj_add_flag(ui_Spinner2, LV_OBJ_FLAG_HIDDEN);
    usleep(1000);
    lv_obj_clear_flag(ui_Checkbox5, LV_OBJ_FLAG_HIDDEN);
    usleep(1000);
    delay(1000);
    lv_scr_load(ui_Screen3);

   }else{
    self_tester_counter++;
    if(self_tester_counter > 2000){
     if(self_tester < 3 ){
     lv_label_set_text(ui_Label44, "Self Test Main Board Fail! Restart System");
     }else if(self_tester < 10){
     lv_label_set_text(ui_Label44, "Self Test Shim Relay Fail! Restart System");
     }else if(self_tester == 10){
     lv_label_set_text(ui_Label44, "Self Test Shim Power Fail! Restart System");
     }
     self_tester = 17;
    }
   }
#endif
   
   if(display_mode == 0 && display_mode_flag == 0){
    display_mode_flag = 1;
    device_footprint = 52;

    reset_variables();
device_footprint = 51;

   }else if(display_mode > 0 && display_mode_flag == 1){
    display_mode_flag = 0;
   }
    
    /*console_m.print("Display: ");
    console_m.println(display_mode);
    console_m.print("UP: ");
    console_m.println(up_mode);
    console_m.print("Time: ");
    console_m.println(minutesPassed);
    console_m.print("Magnet: ");
    console_m.println(magnet_current);*/
    //sleep(10);
    //usleep(3000);
    if (device_m.available() > 0)
    {
        s_cmd = device_m.readStringUntil('\n');
        console_m.println(s_cmd);
        
        if (s_cmd.startsWith("LV")) // live magnet voltage
        {
            live_magnet_voltage = s_cmd.substring(2).toFloat();
            // console_m.println(display_mode);
            
        }
        else if (s_cmd.startsWith("LC")) // live magnet current
        {
            live_magnet_current = s_cmd.substring(2).toFloat();
            if(probe_checker == 1){
                set_current(300.0);
                set_voltage(600.0);
                probe_check_counter++;
                if(live_magnet_current > 1.00 && live_magnet_voltage < 3){
               lv_obj_clear_flag(ui_Checkbox7, LV_OBJ_FLAG_HIDDEN);
               lv_obj_add_flag(ui_Spinner3, LV_OBJ_FLAG_HIDDEN);
               probe_checker = 0;
               set_current(0.0);
               set_voltage(0.0);
               device_m.println("D0");
               delay(100);
               device_m.println("D2");
               probe_check_counter = 0;
                }else if(probe_check_counter > 10){
               set_current(0.0);
               set_voltage(0.0);
               device_m.println("D0");
               delay(100);
               device_m.println("D2");
               lv_scr_load(ui_Screen10) ;
               probe_checker = 1;
               
                }
            }
           
        }
        else if (s_cmd.startsWith("AC")) //att current
        {
            att_current = s_cmd.substring(2).toFloat();
            att_current = att_current + c_clb1;
        }
        else if (s_cmd.startsWith("AV")) //att voltage
        {
            att_voltage = s_cmd.substring(2).toFloat();
            #if defined(SELF_TESTER_ON)
            if(self_tester == 10){
                delay(200);
                if(att_voltage > 10){
                    self_tester = 12;
                }
                lv_obj_add_state(ui_Checkbox4, LV_STATE_CHECKED);
            }
            #endif
            if(att_relay == 0){
                att_voltage = 0;

            }
            if(att_voltage > 22.55){
              att_voltage = att_voltage + v_clb1;
            }
            
        }
        else if (s_cmd.startsWith("RC")) //rda current
        {
            rda_current = s_cmd.substring(2).toFloat();
            rda_current = rda_current + c_clb2;
        }
        else if (s_cmd.startsWith("RV")) //rda voltage
        {
            rda_voltage = s_cmd.substring(2).toFloat();
            if(rda_voltage > 22.55){
              rda_voltage = rda_voltage + v_clb1;
            }

            if(rda_relay == 0){
                rda_voltage = 0;

            }
        }
        else if (s_cmd.startsWith("CC")) //main current
        {
            main_current = s_cmd.substring(2).toFloat();
            main_current = main_current +  c_clb3;
        }
        else if (s_cmd.startsWith("CV")) // main voltage
        {
            main_voltage = s_cmd.substring(2).toFloat();
            if(main_voltage > 22.55){
              main_voltage = main_voltage + v_clb1;
            }
            if(main_relay == 0){
                main_voltage = 0;

            }
        }
        else if (s_cmd.startsWith("OV")) // main voltage
        {
            ts_voltage = s_cmd.substring(2).toFloat();
        }
        else if (s_cmd.startsWith("MC")) // magnet current
        {
            if (up_mode != 6)
            {
                magnet_current = s_cmd.substring(2).toFloat();
            }
        }
        else if (s_cmd.startsWith("MV")) // magnet voltage
        {
            magnet_voltage = s_cmd.substring(2).toFloat();
        }
        else if (s_cmd.startsWith("RA")) // att relay status
        {
            att_relay = s_cmd.substring(2).toInt();
            #if defined(SELF_TESTER_ON)

            if(self_tester < 6){
            if(self_tester == 2 && att_relay == 1){
                delay(200);
              self_tester = 3;
              device_m.println("A2");
              lv_obj_add_state(ui_Checkbox3, LV_STATE_CHECKED);
            } else if(self_tester == 3 && att_relay == 2){
                delay(200);
                self_tester = 4;
                device_m.println("A3");
                
            }else if(self_tester == 4 && att_relay == 3){
                delay(200);
                self_tester = 5;
                device_m.println("A0");
            }else if(self_tester == 5 && att_relay == 0){
                delay(200);
                self_tester = 6;
                device_m.println("M1");
            }
            }
           #endif
        }
        else if (s_cmd.startsWith("RM")) // main relay status
        {
            main_relay = s_cmd.substring(2).toInt();
            #if defined(SELF_TESTER_ON)
            if(self_tester < 9){
                delay(200);
                if(self_tester == 6 && main_relay == 1){
                self_tester = 7;
                device_m.println("M0");
            }else if(self_tester == 7 && main_relay == 0){
                delay(200);
                self_tester = 8;
                device_m.println("R1");
            }
            }
            #endif
            
        }
        else if (s_cmd.startsWith("RR")) // rda relay status
        {
            rda_relay = s_cmd.substring(2).toInt();
            #if defined(SELF_TESTER_ON)
            if(self_tester < 11){
                delay(200);
                if(self_tester == 8 && rda_relay == 1){
                self_tester = 9;
                device_m.println("R0");
            }else if(self_tester == 9 && rda_relay == 0){
                delay(200);
                self_tester = 10;
                lv_obj_add_state(ui_Checkbox2, LV_STATE_CHECKED);
            }
            }
            #endif
            
        }
        else if (s_cmd.startsWith("TP")) // minute passed
        {
            minutesPassed = s_cmd.substring(2).toInt();
        }
        else if (s_cmd.startsWith("UP")) //  current up mode status for ramp-up
        {
            up_mode = s_cmd.substring(2).toInt();
        }
        else if (s_cmd.startsWith("UD")) // up mode done flag
        {
            up_done = s_cmd.substring(2).toInt();
        }
        else if (s_cmd.startsWith("DM")) // display number
        {
            d_mode = s_cmd.substring(2).toInt();
        }
        else if (s_cmd.startsWith("OR")) // display number
        {
            resistor_value = s_cmd.substring(2).toFloat();
        }
        else if (s_cmd.startsWith("YD")) // display number
        {
            diag_y = s_cmd.substring(2).toFloat();
            diag_control(diag_y);
        }
        else if (s_cmd.startsWith("ID")) // display number
        {
            PSU_ID = s_cmd.substring(2);
        }
        #ifdef FOUR_ON  
        else if (s_cmd.startsWith("DWN")) // display number
        {
           s_cmd.trim(); 
           dumpLast4DataFiles(); 
          
        }

    }
#else
    // If FOUR_ON is NOT defined, this section is compiled.
    }
    if (Serial.available()) {
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();                      // clean CR/LF
  if (cmd == "DWN") {              // if the “DWN” command arrived
    dumpLast4DataFiles();                // send the contents of data.txt
  }
}
#endif


 
    o_timer++;
    if (o_timer > 1000)
    {
        o_timer = 0;
        console_m.println("*****o_timer*****");
        if (display_mode == 1)
        {
            lv_spinbox_set_value(ui_mcd, live_magnet_current * 100);
            lv_spinbox_set_value(ui_mvd, live_magnet_voltage * 100);
            lv_spinbox_set_value(ui_attvd, att_voltage * 10);
            lv_spinbox_set_value(ui_mainvd, main_voltage * 10);
            lv_spinbox_set_value(ui_rdavd, rda_voltage * 10);
           
        }
        else if (display_mode == 2)
        {
            
            if (up_done == 2 && up_mode == 2)
            {
                device_m.println("U3");
                lv_textarea_add_text(ui_ota, "\n **Wait 3 min to engage power**");
            }
            else if (up_done == 3 && up_mode == 3)
            {
                device_m.println("U4");
                lv_textarea_add_text(ui_ota, "\n **Power engaged**");
                lv_textarea_add_text(ui_ota, "\n ***Ramp UP is start***");
            }
            else if (up_done == 4 && up_mode == 4)
            {
                // yazbisiler
                lv_bar_set_value(ui_opb, (int)u_target_current, LV_ANIM_ON);
                device_m.println("U6");
                up_mode = 6;
                lv_scr_load(ui_Screen7);
               
                lv_spinbox_set_value(ui_tscd, magnet_current * 100);

               
            }
            else if (up_done == 7 && up_mode == 7){
                device_m.println("U8");
            }
            else if (up_done == 8 && up_mode == 8)
            {
                device_m.println("U9");
            }
            else if (up_done == 9 && up_mode == 9)
            {
                device_m.println("U10");
            }

            if (up_mode == 1)
            {
                /* if (att_relay == 0)
                 {
                     lv_label_set_text(ui_axof, "OFF");
                 }
                 else
                 {
                     lv_label_set_text(ui_axof, "ON");
                 }
                 if (main_relay == 0)
                 {
                     lv_label_set_text(ui_mainof, "OFF");
                 }
                 else
                 {
                     lv_label_set_text(ui_mainof, "ON");
                 }*/
            }
            else if (up_mode > 2 && up_mode < 6)
            { 
                if(live_magnet_current < 20.00 && live_magnet_voltage > 1.00 && up_mode !=  5){
                    
    
                device_m.println("U5");
                up_mode = 5;
                lv_obj_set_style_bg_color(ui_ossb, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_scr_load(ui_Screen10);
                device_m.println("G1");
                G_MODE = 1;
                lv_spinbox_set_value(ui_oattvd, 0 );
                lv_spinbox_set_value(ui_omainvd, 0 ); 

                }
                if(G_MODE == 1 && minutesPassed > 6){
                 reset_variables();
                 device_m.println("D0");
                 delay(100);
                 lv_scr_load(ui_Screen5);
                 delay(100);
                 device_m.println("D2");

                }

                lv_spinbox_set_value(ui_ocd, live_magnet_current * 100);
                lv_spinbox_set_value(ui_ovd, live_magnet_voltage * 100);
                lv_spinbox_set_value(ui_tovd, ts_voltage * 100);
                lv_spinbox_set_value(ui_otd, minutesPassed);

                if(G_MODE == 0){
                lv_spinbox_set_value(ui_oattvd, att_voltage * 10);
                lv_spinbox_set_value(ui_omainvd, main_voltage * 10);
                }else{
                lv_spinbox_set_value(ui_oattvd, 0 );
                lv_spinbox_set_value(ui_omainvd, 0 ); 
                }
                
                lv_bar_set_value(ui_opb, (int)magnet_current, LV_ANIM_ON);

                if (att_relay == 0 )
                {
                   lv_obj_clear_state(ui_axof, LV_STATE_CHECKED);
                }
                else if(G_MODE == 0)
                {
                    lv_obj_add_state(ui_axof, LV_STATE_CHECKED);
                }
                if (main_relay == 0 )
                {
                    lv_obj_clear_state(ui_mainof, LV_STATE_CHECKED);
                }
                else if(G_MODE == 0)
                {
                    lv_obj_add_state(ui_mainof, LV_STATE_CHECKED);
                }
            }
            else if (up_mode > 5 && up_mode < 10)
            {

                lv_spinbox_set_value(ui_tcda, live_magnet_current * 100);
                lv_spinbox_set_value(ui_tvda, live_magnet_voltage * 100);
                lv_spinbox_set_value(ui_tovda, ts_voltage * 100);
                lv_spinbox_set_value(ui_tattvd, att_voltage * 10);
            
                lv_spinbox_set_value(ui_tmainvd, main_voltage * 10);
                
                

                if (att_relay == 0)
                {
                    lv_obj_clear_state(ui_taxof, LV_STATE_CHECKED);
                }
                else
                {
                    lv_obj_add_state(ui_taxof, LV_STATE_CHECKED);
                }
                if (main_relay == 0)
                {
                    lv_obj_clear_state(ui_tmainof, LV_STATE_CHECKED);
                }
                else
                {
                    lv_obj_add_state(ui_tmainof, LV_STATE_CHECKED);
                }
            }
            else if (up_mode == 10)
            {

                lv_obj_add_flag(ui_Spinner1, LV_OBJ_FLAG_HIDDEN);
                usleep(1000);
                lv_obj_clear_flag(ui_Checkbox9, LV_OBJ_FLAG_HIDDEN);
                lv_textarea_add_text(ui_tta, "\n ****PARK IS DONE*****");
                lv_textarea_add_text(ui_tta, "\n  Main Heater OFF");
                lv_textarea_add_text(ui_tta, "\n  Axial Coil OFF");
                lv_textarea_add_text(ui_tta, "\n Wait 3sec");
                delay(1000);
                lv_textarea_add_text(ui_tta, "\n Wait 2sec");
                delay(1000);
                lv_textarea_add_text(ui_tta, "\n Wait 1sec");
                delay(1000);
                lv_obj_clear_flag(ui_Checkbox9, LV_OBJ_FLAG_HIDDEN);

                lv_scr_load(ui_Screen3);
                display_mode = 0;
                up_mode = 0;
                up_done = 0;
                device_m.println("U0");
                
            }
        }
        else if (display_mode == 3)
        {
            
            if(rdf_flag == RAMPDOWN_INIT){
                lv_textarea_add_text(ui_ota, "\n Wait 3 min to engage power");
                rdf_flag = RAMPDOWN_POWER_WAIT;

            }
            lv_spinbox_set_value(ui_ocd, live_magnet_current * 100);
            lv_spinbox_set_value(ui_ovd, live_magnet_voltage * 100);
            lv_spinbox_set_value(ui_tovd, ts_voltage * 100);
            lv_spinbox_set_value(ui_otd, minutesPassed);
            lv_spinbox_set_value(ui_omainvd, main_voltage * 10);
            lv_spinbox_set_value(ui_oattvd, rda_voltage * 10);
            lv_bar_set_value(ui_opb, int(d_start_current) - (int)live_magnet_current, LV_ANIM_ON);
             if (main_relay == 0)
                {
                    lv_obj_clear_state(ui_mainof, LV_STATE_CHECKED);
                }
                else
                {
                    lv_obj_add_state(ui_mainof, LV_STATE_CHECKED);
               if(rdf_flag == RAMPDOWN_POWER_WAIT){
                lv_textarea_add_text(ui_ota, "\n **Main Heater Active**");
                //lv_textarea_add_text(ui_ota, "\n Wait 1 min to engage Heater");
                rdf_flag = RAMPDOWN_HEATER_ACTIVE;

            }
                    
                }

                if(minutesPassed > 3 && rdf_flag == RAMPDOWN_HEATER_ACTIVE ){
                    rdf_flag = RAMPDOWN_ACTIVE;
                    lv_textarea_add_text(ui_ota, "\n Ramp Down Start");
                }
                 if (rda_relay == 0)
                {
                    lv_obj_clear_state(ui_axof, LV_STATE_CHECKED);
                }
                else
                {
                    lv_obj_add_state(ui_axof, LV_STATE_CHECKED);
                }
            if(live_magnet_current < 0.5 && d_mode > 3  && minutesPassed > 3){

                if(rdf_flag == RAMPDOWN_ACTIVE){
             lv_textarea_add_text(ui_ota, "\n Operation is Done Wait 3min!");
             rdf_flag = RAMPDOWN_COMPLETE;
                }
            }
            
            if (d_mode == 5)
            {
                if(rdf_flag == RAMPDOWN_ACTIVE && live_magnet_current < 0.5 && d_mode > 3 ){
             lv_textarea_add_text(ui_ota, "\n Operation is Done Wait !");
             rdf_flag = RAMPDOWN_COMPLETE;
                }
                delay(1000);
                lv_scr_load(ui_Screen3);
                display_mode = 0;
                device_m.println("D0");
                device_m.println("U0");
                rda_checked = 0;
                reset_variables();
                

            }
        }
        saving_data();
       
    }

    if(display_updater == 1){
     update_counter++;
     delay(100);
    if(update_counter == 60){
     lv_textarea_add_text(ui_dbta, "\n **** DISPLAY GOING OFF ****");
     }
     if(update_counter > 80){
    EEPROM.write(0, display_updater);
    EEPROM.commit();
     ESP.restart();
     }
    
    }

}