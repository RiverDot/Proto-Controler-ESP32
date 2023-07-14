#include "BluetoothSerial.h"
#include <SerialTransfer.h>
#include <ArduinoJson.h>

#define PROTOCONTROLLERV1_1

/* Check if Bluetooth configurations are enabled in the SDK */
/* If not, then you have to recompile the SDK */
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

static struct __attribute__((__packed__)) ESP32Data {
  uint8_t faceState;
  uint8_t brightness;
  uint8_t accentBrightness;
  uint8_t useMic;
  uint8_t micLevel;
  uint8_t useBoop;
  uint8_t spectrumMirror;
  uint8_t faceSize;
  uint8_t faceColor;
  uint8_t use;
} e32DataTX, e32DataRX;

SerialTransfer dataTransfer;

const byte numChars = 200;
char receivedChars[numChars];
char tempChars[numChars];  // temporary array for use when parsing

StaticJsonDocument<256> doc;

boolean newData = false;

void setup() {
  Serial.begin(115200);
  /* If no name is given, default 'ESP32' is applied */
  /* If you want to give your own name to ESP32 Bluetooth device, then */
  /* specify the name as an argument SerialBT.begin("myESP32Bluetooth"); */
  SerialBT.begin("Vesper_Display");
  SerialBT.register_callback(Bt_Status);
  Serial.println("Bluetooth Started! Ready to pair...");

  Serial1.begin(9600, SERIAL_8N1, 12, 13);  //For V1-1
  dataTransfer.begin(Serial1);

  e32DataTX.use = 0;
}

void loop() {
  uint16_t sendSize = 0;
  uint16_t receiveSize = 0;

  recvWithStartEndMarkers();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    // this temporary copy is necessary to protect the original data
    //   because strtok() used in parseData() replaces the commas with \0
    parseData();
    //showParsedData();
    newData = false;
  }

  // TX
  sendSize = dataTransfer.txObj(e32DataTX, sendSize);
  dataTransfer.sendData(sendSize);

  // RX
  if (dataTransfer.available()) {
    receiveSize = dataTransfer.rxObj(e32DataRX, receiveSize);
  }

  delay(20);
}

static uint8_t get_value(char charVal) {
  String stringVal = String(charVal);
  int intVal = stringVal.toInt();
  return intVal;
}

static uint8_t get_bool(char charVal) {
  if (charVal = '1') {
    return 1;
  } else {
    return 0;
  }
}

//============

void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (SerialBT.available() > 0 && newData == false) {
    rc = SerialBT.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      } else {
        receivedChars[ndx] = '\0';  // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

//============

void parseData() {  // split the data into its parts

  DeserializationError error = deserializeJson(doc, String(tempChars));

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  e32DataTX.faceState = doc["faceState"];                // 10
  e32DataTX.brightness = doc["brightness"];              // 10
  e32DataTX.accentBrightness = doc["accentBrightness"];  // 10
  e32DataTX.useMic = boolToint(doc["useMic"]);                      // true
  e32DataTX.micLevel = doc["micLevel"];                  // 10
  e32DataTX.useBoop = boolToint(doc["useBoop"]);                    // true
  e32DataTX.spectrumMirror = boolToint(doc["spectrumMirror"]);      // true
  e32DataTX.faceSize = doc["faceSize"];                  // 10
  e32DataTX.faceColor = doc["faceColor"];                // 10
}

void sendBTData() {
  String output = "";

  doc["faceState"] = e32DataRX.faceState;
  doc["brightness"] = e32DataRX.brightness;
  doc["accentBrightness"] = e32DataRX.accentBrightness;
  doc["useMic"] = intToBool(e32DataRX.useMic);
  doc["micLevel"] = e32DataRX.micLevel;
  doc["useBoop"] = intToBool(e32DataRX.useBoop);
  doc["spectrumMirror"] = intToBool(e32DataRX.spectrumMirror);
  doc["faceSize"] = e32DataRX.faceSize;
  doc["faceColor"] = e32DataRX.faceColor;

  serializeJson(doc, output);
  SerialBT.println(output);
}

static bool intToBool(uint8_t in) {
  if (in == 1) {
    return true;
  } else {
    return false;
  }
}

static uint8_t boolToint(bool in) {
  if (in) {
    return 1;
  } else {
    return 0;
  }
}

// Bt_Status callback function
void Bt_Status(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {

  if (event == ESP_SPP_SRV_OPEN_EVT) {

    Serial.println("Client Connected");
    e32DataTX.use = 1;
    sendBTData();
    // Do stuff if connected
  }

  else if (event == ESP_SPP_CLOSE_EVT) {
    Serial.println("Client Disconnected");
    e32DataTX.use = 0;
    // Do stuff if not connected
  }
}
