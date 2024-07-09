#include <SD.h>
#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include "Adafruit_seesaw.h"

// Seesaw
Adafruit_seesaw ss;

#define BUTTON_X         6
#define BUTTON_Y         2
#define BUTTON_A         5
#define BUTTON_B         1
#define BUTTON_SELECT    0
#define BUTTON_START    16
  uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);

#include <esp_log.h>

// Define UUIDs
#define SERVICE_UUID "e3ec124a-bb7b-459e-ae2a-fd6ad5cafff6"
#define REMOTE_CHARACTERISTIC_UUID "9d4e5db5-bc04-47fa-a4c9-88987b927f70"

BLEServer *bleServer;
BLEService *bleService;
BLECharacteristic *remoteCharacteristic;

bool deviceConnected = false;
bool previouslyConnected = false;
int playState = 0;
int volume = 135;
bool seek = false;
bool track = false;
int trackNum = 0;
int prievousTrackNum = 0;
int previousVolume = 135;
bool hasChanged = false;

// BLE Broadcast Name
static String BLE_BROADCAST_NAME = "Installation04";

// forward declarations
void setupBLE();
void drawLoadingScreen(String text);


static constexpr const gpio_num_t SDCARD_CSPIN = GPIO_NUM_4;

static constexpr const char* files[] = {
  "/africa-toto.wav",
  "/breakMyStride.wav",
  "/Funkytown.wav",
};

int filesIndexMax = 2;

static constexpr const size_t buf_num = 3;
static constexpr const size_t buf_size = 1024;
static uint8_t wav_data[buf_num][buf_size];

struct __attribute__((packed)) wav_header_t
{
  char RIFF[4];
  uint32_t chunk_size;
  char WAVEfmt[8];
  uint32_t fmt_chunk_size;
  uint16_t audiofmt;
  uint16_t channel;
  uint32_t sample_rate;
  uint32_t byte_per_sec;
  uint16_t block_size;
  uint16_t bit_per_sample;
};

struct __attribute__((packed)) sub_chunk_t
{
  char identifier[4];
  uint32_t chunk_size;
  uint8_t data[1];
};

static bool playSdWav(const char* filename)
{
  auto file = SD.open(filename);

  if (!file) { return false; }

  wav_header_t wav_header;
  file.read((uint8_t*)&wav_header, sizeof(wav_header_t));

  ESP_LOGD("wav", "RIFF           : %.4s" , wav_header.RIFF          );
  ESP_LOGD("wav", "chunk_size     : %d"   , wav_header.chunk_size    );
  ESP_LOGD("wav", "WAVEfmt        : %.8s" , wav_header.WAVEfmt       );
  ESP_LOGD("wav", "fmt_chunk_size : %d"   , wav_header.fmt_chunk_size);
  ESP_LOGD("wav", "audiofmt       : %d"   , wav_header.audiofmt      );
  ESP_LOGD("wav", "channel        : %d"   , wav_header.channel       );
  ESP_LOGD("wav", "sample_rate    : %d"   , wav_header.sample_rate   );
  ESP_LOGD("wav", "byte_per_sec   : %d"   , wav_header.byte_per_sec  );
  ESP_LOGD("wav", "block_size     : %d"   , wav_header.block_size    );
  ESP_LOGD("wav", "bit_per_sample : %d"   , wav_header.bit_per_sample);

  if ( memcmp(wav_header.RIFF,    "RIFF",     4)
    || memcmp(wav_header.WAVEfmt, "WAVEfmt ", 8)
    || wav_header.audiofmt != 1
    || wav_header.bit_per_sample < 8
    || wav_header.bit_per_sample > 16
    || wav_header.channel == 0
    || wav_header.channel > 2
    )
  {
    file.close();
    return false;
  }

  file.seek(offsetof(wav_header_t, audiofmt) + wav_header.fmt_chunk_size);
  sub_chunk_t sub_chunk;

  file.read((uint8_t*)&sub_chunk, 8);

  ESP_LOGD("wav", "sub id         : %.4s" , sub_chunk.identifier);
  ESP_LOGD("wav", "sub chunk_size : %d"   , sub_chunk.chunk_size);

  while(memcmp(sub_chunk.identifier, "data", 4))
  {
    if (!file.seek(sub_chunk.chunk_size, SeekMode::SeekCur)) { break; }
    file.read((uint8_t*)&sub_chunk, 8);

    ESP_LOGD("wav", "sub id         : %.4s" , sub_chunk.identifier);
    ESP_LOGD("wav", "sub chunk_size : %d"   , sub_chunk.chunk_size);
  }

  if (memcmp(sub_chunk.identifier, "data", 4))
  {
    file.close();
    return false;
  }

  int32_t data_len = sub_chunk.chunk_size;
  bool flg_16bit = (wav_header.bit_per_sample >> 4);

  size_t idx = 0;
  while (data_len > 0) {
    if (!ss.digitalRead(BUTTON_START)) {
      // Pause/Play
      playState = 0;
    }
    if (playState == 0) {
      // Stop playing
      M5.Speaker.stop();
      break; // Exit the loop
    }
    size_t len = data_len < buf_size ? data_len : buf_size;
    len = file.read(wav_data[idx], len);
    data_len -= len;

    if (flg_16bit) {
      M5.Speaker.playRaw((const int16_t*)wav_data[idx], len >> 1, wav_header.sample_rate, wav_header.channel > 1, 1, 0);
    } else {
      M5.Speaker.playRaw((const uint8_t*)wav_data[idx], len, wav_header.sample_rate, wav_header.channel > 1, 1, 0);
    }
    idx = idx < (buf_num - 1) ? idx + 1 : 0;
  }
  file.close();
  return true;
}

void setup(void) {
  M5.begin();
  M5.Lcd.setFont(&FreeSans9pt7b);
  drawLoadingScreen("Loading Music Player");
  if (!ss.begin(0x50)) { // Use the seesaw's I2C address
    Serial.println("Seesaw not found!");
    while (1);
  }
  ss.pinModeBulk(button_mask, INPUT_PULLUP);
  ss.setGPIOInterrupts(button_mask, 1);
  setupBLE();
  SD.begin(SDCARD_CSPIN, SPI, 25000000);

  M5.Speaker.setVolume(volume);
  M5.Lcd.fillScreen(TFT_MAROON);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(TFT_WHITE, MAROON);
  M5.Lcd.print("Track Number: ");
  M5.Lcd.print(trackNum);
  String song = files[trackNum];
  song = song.substring(1, song.length());
  song = song.substring(0, song.length() - 4);
  M5.Lcd.setCursor(0, 40);
    if (trackNum == 0) {
      M5.Lcd.println("Current Song: ");
      M5.Lcd.println("Africa\nToto");
    } else if (trackNum == 1) {
      M5.Lcd.println("Current Song: ");
      M5.Lcd.println("Break My Stride\nMatthew Wilder");
    } else if (trackNum == 2) {
      M5.Lcd.println("Current Song: ");
      M5.Lcd.println("Funkytown\nLipps Inc.");
    }
  M5.Lcd.print("Volume: ");
  // Calculate the percentage and round it to the nearest whole number
  int volumePercentage = round(static_cast<float>(volume) / 255 * 100);
  M5.Lcd.print(volumePercentage); // Display the rounded percentage
  M5.Lcd.print("%");
  M5.Lcd.setCursor(0, 140);
  prievousTrackNum = trackNum;
  previousVolume = volume;
}

void loop(void)
{
  // Use I2C Seesaw buttons to control music player
  if (!ss.digitalRead(BUTTON_X) && volume < 255) {
    // Volume Up
    volume += 15;
    Serial.printf("Volume: %d\n", volume);
  } else if (!ss.digitalRead(BUTTON_B) && volume > 0) {
    // Volume Down
    volume -= 15;
    Serial.printf("Volume: %d\n", volume);
  } else if (!ss.digitalRead(BUTTON_Y)) {
    if (trackNum < filesIndexMax)
    {
      trackNum++;
    } else if (trackNum == filesIndexMax)
    {
      trackNum = 0;
    }
  } else if (!ss.digitalRead(BUTTON_A)) {
    // Rewind to previous track
    if (trackNum > 0)
    {
      trackNum--;
    } else if (trackNum == 0)
    {
      trackNum = filesIndexMax;
    }
    
  } else if (!ss.digitalRead(BUTTON_SELECT)) {
    // Pause/Play
    playState = 1;
  } else if (!ss.digitalRead(BUTTON_START)) {
    // Pause/Play
    playState = 0;
  }
  /**
    * 1. Check if the playState is 1
    * 2. If it is, play the music file
    * 3. If it is not, stop the music file
    * 4. Check if the volume has changed
    * 5. If it has, update the volume
    * 6. Check if the track has changed
    * 7. If it has, update the track
  */

  /**
   * If either the track number or volume has changed since the last loop,
   * Print track number to Lcd screen.
   * Print the current song by take the substring of the song name
   * and removing the first and last four characters.
   * Print volume level to the screen
   * Show if song is playing or stopped
  */ 
  hasChanged = (prievousTrackNum != trackNum || previousVolume != volume); // Determine if the track number or volume has changed. If so make hasChanged true
  if (trackNum != prievousTrackNum || volume != previousVolume) {
    hasChanged = true;
  } else {
    hasChanged = false;
  }

  // Draw the screen if the track number, volume, or playState has changed
  if (hasChanged) {
    M5.Lcd.fillScreen(TFT_MAROON);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_MAROON);
    M5.Lcd.print("Track Number: ");
    M5.Lcd.print(trackNum);
    String song = files[trackNum];
    song = song.substring(1, song.length());
    song = song.substring(0, song.length() - 4);
    M5.Lcd.setCursor(0, 40);
    if (trackNum == 0) {
      M5.Lcd.println("Current Song: ");
      M5.Lcd.println("Africa\nToto");
    } else if (trackNum == 1) {
      M5.Lcd.println("Current Song: ");
      M5.Lcd.println("Break My Stride\nMatthew Wilder");
    } else if (trackNum == 2) {
      M5.Lcd.println("Current Song: ");
      M5.Lcd.println("Funkytown\nLipps Inc.");
    }
    M5.Lcd.print("Volume: ");
    // Calculate the percentage and round it to the nearest whole number
    int volumePercentage = round(static_cast<float>(volume) / 255 * 100);
    M5.Lcd.print(volumePercentage); // Display the rounded percentage
    M5.Lcd.print("%");
    M5.Lcd.setCursor(0, 140);

    prievousTrackNum = trackNum;
    previousVolume = volume;
  }

  M5.Speaker.setVolume(volume);

  if (playState == 1) {
    playSdWav(files[trackNum]);
  } else {
    M5.Speaker.stop();
  }
  delay(250);
}

///////////////////////////////////////////////////////////////
//////////BLUETOOTH SETUP//////////////////////////////////////
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods
///////////////////////////////////////////////////////////////
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    previouslyConnected = true;
    Serial.println("Device connected...");
  }
  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    Serial.println("Device disconnected...");
  }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
  // Callback function to support a write request.
  void onWrite(BLECharacteristic *pCharacteristic) {
    String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
    String characteristicValue = pCharacteristic->getValue().c_str();
    Serial.printf("Client JUST wrote to %s: %\n", characteristicUUID.c_str(), characteristicValue.c_str());

    if (characteristicUUID.equalsIgnoreCase(REMOTE_CHARACTERISTIC_UUID))
    {
      // The characteristicUUID that just got written to by a client matches the known
      // Get a string value (3 digits seperated by commas) playstate + "," + volume + "," + songChange
      String value = characteristicValue.c_str();

      // Split the string into playState, volume, and songChange
      String playStateStr = value.substring(0, 1);
      String volumeStr = value.substring(2, 3);
      String songChangeStr = value.substring(4, 5);

      printf("Play State: %s\n", playStateStr.c_str());
      printf("Volume: %s\n", volumeStr.c_str());
      printf("Song Change: %s\n", songChangeStr.c_str());

      playState = playStateStr.toInt();

      // Play the song
      if (playStateStr == "1") {
      } else {
        M5.Speaker.stop();
      }

      //If Volume is 2, increase volume by 15
      if (volumeStr == "2") {
        if (volume < 255) {
          volume += 15;
        }
      } else if (volumeStr == "0") {
        if (volume > 0) {
          volume -= 15;
        }
      }

      // If songChange is 2, change the song
      if (songChangeStr == "2") {
        if (trackNum < filesIndexMax)
        {
          trackNum++;
        } else if (trackNum == filesIndexMax)
        {
          trackNum = 0;
        }
      } else if (songChangeStr == "0") {
        if (trackNum > 0)
        {
          trackNum--;
        } else if (trackNum == 0)
        {
          trackNum = filesIndexMax;
        }
      }
    }
  }

  // Callback function to support a Notify request.
  void onNotify(BLECharacteristic *pCharacteristic)
  {
    String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
    Serial.printf("Client JUST notified about change to %s: %s", characteristicUUID.c_str(), pCharacteristic->getValue().c_str());
  }

  // Callback function to support when a client subscribes to notifications/indications.
  void onSubscribe(BLECharacteristic *pCharacteristic, uint16_t subValue)
  {
    String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
    Serial.printf("Client JUST subscribed to %s: %d", characteristicUUID.c_str(), subValue);
  }

  // Callback function to support a Notify/Indicate Status report.
  void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code)
  {
    // Print appropriate response
    String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
    switch (s)
    {
    case SUCCESS_INDICATE:
      // Serial.printf("Status for %s: Successful Indication", characteristicUUID.c_str());
      break;
    case SUCCESS_NOTIFY:
      Serial.printf("Status for %s: Successful Notification", characteristicUUID.c_str());
      break;
    case ERROR_INDICATE_DISABLED:
      Serial.printf("Status for %s: Failure; Indication Disabled on Client", characteristicUUID.c_str());
      break;
    case ERROR_NOTIFY_DISABLED:
      Serial.printf("Status for %s: Failure; Notification Disabled on Client", characteristicUUID.c_str());
      break;
    case ERROR_GATT:
      Serial.printf("Status for %s: Failure; GATT Issue", characteristicUUID.c_str());
      break;
    case ERROR_NO_CLIENT:
      Serial.printf("Status for %s: Failure; No BLE Client", characteristicUUID.c_str());
      break;
    case ERROR_INDICATE_TIMEOUT:
      Serial.printf("Status for %s: Failure; Indication Timeout", characteristicUUID.c_str());
      break;
    case ERROR_INDICATE_FAILURE:
      Serial.printf("Status for %s: Failure; Indication Failure", characteristicUUID.c_str());
      break;
    }
  }
};

void setupBLE()
{
  // Initialize BLE
  Serial.print("Starting BLE...");
  BLEDevice::init(BLE_BROADCAST_NAME.c_str());

  // Create BLE server
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new MyServerCallbacks());

  // Create BLE service
  bleService = bleServer->createService(SERVICE_UUID);

  /////////////////////////////////////////////////////////////////
  // CHARACTERISTICS
  // Remote (Client) Characteristic
  remoteCharacteristic = bleService->createCharacteristic(
      REMOTE_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  /////////////////////////////////////////////////////////////////

  remoteCharacteristic->setValue("0");

  remoteCharacteristic->setCallbacks(new CharacteristicCallbacks());

  // Start service
  bleService->start();

  // Start broadcasting (advertising) BLE service
  BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
  bleAdvertising->addServiceUUID(BLEUUID(SERVICE_UUID));
  bleAdvertising->setScanResponse(true);
  bleAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined");
  // Draw background Blue with "Connecting to Remote..." displayed on screen
  M5.Lcd.fillScreen(TFT_BLUE);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLUE);
  M5.Lcd.print("Connecting to Remote...");
}

void drawLoadingScreen(String text) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillCircle(160, 120, 50, BLUE);
    //medium size equilateral triangle in the center of the blue circle with two corners aligned vertically and the third corner pointing to the right
    M5.Lcd.fillTriangle(140, 95, 140, 135, 190, 115, WHITE);
    M5.Lcd.setCursor(80, 0);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.println(text);
}