#include <BLEDevice.h>
#include <BLE2902.h>
#include "Adafruit_seesaw.h"

#include <M5Core2.h>
#include <M5Touch.h>

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
static BLERemoteCharacteristic *remoteRemoteCharacteristic;
static BLEAdvertisedDevice *bleRemoteServer;
static boolean doConnect = false;
static boolean doScan = false;
bool deviceConnected = false;
int volume = 1;
int playState = 0;
int songChange = 1;

// Variable to track button states
bool volUpPressed = false;
bool volDownPressed = false;
bool playPausePressed = false;
bool songForward = false;
bool songBackward = false;

// Enum for button types
enum ButtonType {
    VOL_UP,
    VOL_DOWN,
    PLAY_PAUSE,
    SONG_FORWARD,
    SONG_BACKWARD
};

// forward declarations
void drawScreenTextWithBackground(String text, int backgroundColor);
void drawMainScreen();
void drawLoadingScreen(String text);

static BLEUUID SERVICE_UUID ("e3ec124a-bb7b-459e-ae2a-fd6ad5cafff6");
static BLEUUID REMOTE_CHARACTERISTIC_UUID("9d4e5db5-bc04-47fa-a4c9-88987b927f70");

// BLE Broadcast Name
static String BLE_BROADCAST_NAME = "Installation04";

///////////////////////////////////////////////////////////////
// BLE Client Callback Methods
// This method is called when the server that this client is
// connected to NOTIFIES this client (or any client listening)
// that it has changed the remote characteristic
///////////////////////////////////////////////////////////////
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
    Serial.printf("Notify callback for characteristic %s of data length %d\n", pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
    Serial.printf("\tData: %.*s", length, pData);
    // Save the received data to a string (copy up to length bytes from pData to value string)
    String value = String((char *)pData).substring(0, length);
    Serial.printf("\tValue was: %s\n", value.c_str());
}

///////////////////////////////////////////////////////////////
// BLE Server Callback Method
// These methods are called upon connection and disconnection
// to BLE service.
///////////////////////////////////////////////////////////////
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        deviceConnected = true;
        Serial.println("Device connected...");
    }

    void onDisconnect(BLEClient *pclient)
    {
        deviceConnected = false;
        Serial.println("Device disconnected...");
        drawScreenTextWithBackground("LOST connection to device.\n\nAttempting re-connection...", TFT_RED);
    }
};

///////////////////////////////////////////////////////////////
// Method is called to connect to server
///////////////////////////////////////////////////////////////
bool connectToServer()
{
    // Create the client
    Serial.printf("Forming a connection to %s\n", bleRemoteServer->getName().c_str());
    BLEClient *bleClient = BLEDevice::createClient();
    bleClient->setClientCallbacks(new MyClientCallback());
    Serial.println("\tClient connected");

    // Connect to the remote BLE Server.
    if (!bleClient->connect(bleRemoteServer))
    Serial.printf("FAILED to connect to server (%s)\n", bleRemoteServer->getName().c_str());
    Serial.printf("\tConnected to server (%s)\n", bleRemoteServer->getName().c_str());

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *bleRemoteService = bleClient->getService(SERVICE_UUID);

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    remoteRemoteCharacteristic = bleRemoteService->getCharacteristic(REMOTE_CHARACTERISTIC_UUID);
    // DTG - ^^^ These were both server characteristics, so I changed the second one to client characteristic
    // and the same error was repeated below


    /////////////////////////////////////////CHECK TO SEE IF SERVICE/CHARACTERISTIC WAS FOUND////////////////////////////////////////
    if (bleRemoteService == nullptr) {
        Serial.printf("Failed to find our service UUID: %s\n", SERVICE_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our service UUID: %s\n", SERVICE_UUID.toString().c_str());

    if (remoteRemoteCharacteristic == nullptr) {
        Serial.printf("Failed to find the CLIENT characteristic UUID: %s\n", REMOTE_CHARACTERISTIC_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound the CLIENT characteristic UUID: %s\n", REMOTE_CHARACTERISTIC_UUID.toString().c_str());
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    //deviceConnected = true;
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Scan for BLE servers and find the first one that advertises the service we are looking for.
///////////////////////////////////////////////////////////////////////////////////////////////
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    /**
     * Called for each advertising BLE server.
     */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // Print device found
        Serial.print("BLE Advertised Device found:");
        Serial.printf("\tName: %s\n", advertisedDevice.getName().c_str());
        
        // We have found a device, let us now see if it contains the service we are looking for.
        if (advertisedDevice.haveServiceUUID() && 
                advertisedDevice.isAdvertisingService(SERVICE_UUID) && 
                advertisedDevice.getName() == BLE_BROADCAST_NAME.c_str()) {
            BLEDevice::getScan()->stop();
            bleRemoteServer = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
        }

    }     
};        



///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup()
{
    // Init device
    M5.begin();
    M5.Lcd.setFont(&FreeSans9pt7b);

    M5.Lcd.setTextSize(3);
    drawLoadingScreen("Scanning...");

    BLEDevice::init("");
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(0, false);

    drawLoadingScreen("Connecting...");
    drawMainScreen();

}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop() {
    if (doConnect == true) {
        if (connectToServer()) {
            Serial.println("We are now connected to the BLE Server.");
            doConnect = false;
            delay(1000);
        }
        else {
            Serial.println("We have failed to connect to the server; there is nothin more we will do.");
            delay(1000);
        }
    }

    if (deviceConnected) { 
    if (M5.Touch.ispressed()) {
        Point touchPoint = M5.Touch.getPressPoint();
        int touchX = touchPoint.x;
        int touchY = touchPoint.y;

        // If point is in the Volume Up Button
        if (touchX >= 20 && touchX <= 95 && touchY >= 30 && touchY <= 105) {
                if (remoteRemoteCharacteristic->canWrite()) {
                volume = 2;
                // Make a string called data which is playstate + "," + volume + "," + songChange
                String data = String(playState) + "," + String(volume) + "," + String(songChange);
                remoteRemoteCharacteristic->writeValue(data.c_str());
                volume = 1;
                Serial.println("Volume up was pressed");
                Serial.println(data);
            }
        }
        // If point is in the Volume Down Button
        else if (touchX >= 20 && touchX <= 95 && touchY >= 100 && touchY <= 175) {
            if (remoteRemoteCharacteristic->canWrite()) {
                volume = 0;
                // Make a string called data which is playstate + "," + volume + "," + songChange
                String data = String(playState) + "," + String(volume) + "," + String(songChange);
                remoteRemoteCharacteristic->writeValue(data.c_str());
                volume = 1;
                Serial.println("Volume down was pressed");
                Serial.println(data);
            }
        }
        // If point is in the Play/Pause Button
        else if (touchX >= 120 && touchX <= 310 && touchY >= 30 && touchY <= 180) {
            if (remoteRemoteCharacteristic->canWrite()) {
                 playState = (playState == 0) ? 1 : 0; // Toggle play/pause state
                // Make a string called data which is playstate + "," + volume + "," + songChange
                String data = String(playState) + "," + String(volume) + "," + String(songChange);
                remoteRemoteCharacteristic->writeValue(data.c_str());
                 Serial.println(playState == 0 ? "Stop was pressed" : "Start was pressed");
                 Serial.println(data);
            }
        }
        // If point is in the Previous Song Button
        else if (touchX >= 170 && touchX <= 320 && touchY >= 190 && touchY <= 240) {
            if (remoteRemoteCharacteristic->canWrite()) {
                songChange = 2;
                // Make a string called data which is playstate + "," + volume + "," + songChange
                String data = String(playState) + "," + String(volume) + "," + String(songChange);
                remoteRemoteCharacteristic->writeValue(data.c_str());
                songChange = 1;
                Serial.println("Next song was pressed");
                Serial.println(data);
            }
        }
        // If point is in the Next Song Button
        else if (touchX >= 0 && touchX <= 150 && touchY >= 190 && touchY <= 240) {
            if (remoteRemoteCharacteristic->canWrite()) {
                songChange = 0;
                // Make a string called data which is playstate + "," + volume + "," + songChange
                String data = String(playState) + "," + String(volume) + "," + String(songChange);
                remoteRemoteCharacteristic->writeValue(data.c_str());
                songChange = 1;
                Serial.println("Previous song was pressed");
                Serial.println(data);
            }
        }
    }
    delay(150); // Delay a second between loops.
    } else {
        drawScreenTextWithBackground("Lost connection to device.\n\nAttempting re-connection...", TFT_RED);
        delay(1000);
    }
}

void drawScreenTextWithBackground(String text, int backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    M5.Lcd.setCursor(0,0);
    M5.Lcd.println(text);
}

/*draws screen with 5 buttons on it, and a title at the top of the screen that says "Remote Control"
on the middle left on the screen, there will be a green button that says "+" stacked on top of a red button that says "-", both with small buttons sizes
on the middle right on the screen, there will be a blue button that says "Play/Pause" that is bigger than the other two buttons
below all three of these buttonss, on the bottom left, there will be a purple button that says "Previous Song",
on the bottom right, there will be a yellow button that says "Next Song"
both of these buttons will be long horizontally and skinny to fit at the bottom*/

void drawMainScreen() {
    M5.Lcd.setFont(NULL);
    M5.Lcd.fillScreen(BLACK);
    // Text Centered at the top of the screen, write Remote Control
    M5.Lcd.setCursor(80, 0);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("Remote Control");

    // Draw the first column of rectangles
    M5.Lcd.fillRect(20, 30, 75, 75, GREEN); // Volume Up Button
    // Text Centered Inside the first rectangle, write +
    M5.Lcd.setCursor(50, 60);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setTextSize(3);
    M5.Lcd.print("+");

    M5.Lcd.fillRect(20, 100, 75, 75, RED); // Volume Down Button
    // Text Centered Inside the second rectangle, write -
    M5.Lcd.setCursor(50, 130);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.print("-");

    M5.Lcd.fillRect(120, 30, 190, 150, BLUE); // Play/Pause Button
    // Text Centered Inside the third rectangle, write Play/Pause
    M5.Lcd.setCursor(140, 90);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.print("Play/Pause");

    M5.Lcd.fillRect(0, 190, 150, 50, PURPLE); // Previous Song Button
    // Text Centered Inside the fourth rectangle, write Previous Song
    M5.Lcd.setCursor(20, 200);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.print("Previous");

    M5.Lcd.fillRect(170, 190, 150, 50, YELLOW); // Next Song Button
    // Text Centered Inside the fifth rectangle, write Next Song
    M5.Lcd.setCursor(190, 200);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.print("Next");
}

void drawLoadingScreen(String text) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillCircle(160, 120, 50, BLUE);
    //medium size equilateral triangle in the center of the blue circle with two corners aligned vertically and the third corner pointing to the right
    M5.Lcd.fillTriangle(140, 95, 140, 135, 190, 115, WHITE);
    M5.Lcd.setCursor(80, 35);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println(text);
}