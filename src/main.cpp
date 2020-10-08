/**
 * Initial code for BLE with PIN bonding was found here https://postpop.tistory.com/20
 */
#include "Arduino.h"
#include "TFMiniS.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <EEPROM.h>

#include "state_machine.h"

#define SERVICE_UUID "2e9c238e-0913-4ee5-9051-d7b5b01c8858"

#define SET_HEIGHT_TO_PRESET_CHARACTERISTIC_UUID "299b4934-0ee9-41c8-a339-884adebbba3d"

#define MOVE_DESK_UP_CHARACTERISTIC_UUID "b8c2948b-0d32-4c3f-bc0c-5223736f0328"
#define MOVE_DESK_DOWN_CHARACTERISTIC_UUID "58a3df93-88e1-4c66-b27a-00b7f8ab9bc4"

#define SAVE_CURRENT_HEIGHT_AS_HEIGHT_1_CHARACTERISTIC_UUID "48b4f12d-3f7c-47bd-97c2-30f622ab88c9"
#define GET_HEIGHT_1_CHARACTERISTIC_UUID "6f78df56-5f26-432e-a434-7cff8522ecde"

#define SAVE_CURRENT_HEIGHT_AS_HEIGHT_2_CHARACTERISTIC_UUID "aaf4b243-c375-4cbf-8a00-cc6e3ddb9df6"
#define GET_HEIGHT_2_CHARACTERISTIC_UUID "f120482d-693e-4076-a3ae-5edbe6e27edb"

#define SAVE_CURRENT_HEIGHT_AS_HEIGHT_3_CHARACTERISTIC_UUID "aeb81129-bc41-4bd7-b831-e1d4b5c609ea"
#define GET_HEIGHT_3_CHARACTERISTIC_UUID "adc4457b-7811-429f-bd70-f5c0b2452a74"

#define ULTRA_SONIC_PIN 14
#define UP_PWM_PIN 26
#define UP_PWM_CHANNEL 0
#define DOWN_PWM_PIN 25
#define DOWN_PWM_CHANNEL 1
#define PWM_FREQUENCY 5000
#define PWM_RESOLUTION 8

StateMachine *state_machine;
TFMiniS tfminis;
uint8_t current_desk_height;
uint32_t pass_key = 349349;

class MySecurity : public BLESecurityCallbacks
{
    uint32_t onPassKeyRequest()
    {
        Serial.println("PassKeyRequest");
        return pass_key;
    }

    void onPassKeyNotify(uint32_t input_pass_key)
    {
        Serial.print("The passkey input Notify number: ");
        Serial.println(input_pass_key);
    }

    bool onConfirmPIN(uint32_t input_pass_key)
    {
        Serial.print("The passkey YES/NO number: ");
        Serial.println(input_pass_key);

        vTaskDelay(5000);

        return pass_key == input_pass_key;
    }

    bool onSecurityRequest()
    {
        Serial.println("SecurityRequest");
        return true;
    }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl)
    {
        Serial.println("Starting BLE work!");
    }
};

class SetHeightToPresetCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *p_characteristic)
    {
        uint8_t* value = p_characteristic->getData();
        switch(value[0]) {
            case 0x00:
                Serial.println("Adjusting to preset 1 height state");
                state_machine->requestStateChange(ADJUST_TO_PRESET_1_HEIGHT_STATE);
                break;
            case 0x01:
                Serial.println("Adjusting to preset 2 height state");
                state_machine->requestStateChange(ADJUST_TO_PRESET_2_HEIGHT_STATE);
                break;
            case 0x02:
                Serial.println("Adjusting to preset 3 height state");
                state_machine->requestStateChange(ADJUST_TO_PRESET_3_HEIGHT_STATE);
                break;
            default:
                break;
        }
    }
};

class MoveDeskUpCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *p_characteristic)
    {
        state_machine->requestStateChange(ADJUST_UP_STATE);
    }
};

class MoveDeskDownCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *p_characteristic)
    {
        state_machine->requestStateChange(ADJUST_DOWN_STATE);
    }
};

class SaveCurrentHeightAsHeight1Callbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *p_characteristic)
    {
        state_machine->requestStateChange(SAVE_CURRENT_HEIGHT_TO_PRESET_1_STATE);
    }
};

class SaveCurrentHeightAsHeight2Callbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *p_characteristic)
    {
        state_machine->requestStateChange(SAVE_CURRENT_HEIGHT_TO_PRESET_2_STATE);
    }
};

class SaveCurrentHeightAsHeight3Callbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *p_characteristic)
    {
        state_machine->requestStateChange(SAVE_CURRENT_HEIGHT_TO_PRESET_3_STATE);
    }
};

void updateDeskHeight(void* parameter) {
    for(;;) {
        Measurement m = tfminis.triggerMeasurement();

        current_desk_height = m.distance;

        delay(1000);
    }
}

void setup()
{
    Serial.begin(115200);
    Serial2.begin(TFMINIS_BAUDRATE);
    EEPROM.begin(3);

    tfminis.begin(&Serial2);
    tfminis.setFrameRate(0);

    ledcSetup(UP_PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(UP_PWM_PIN, UP_PWM_CHANNEL);

    ledcSetup(DOWN_PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(DOWN_PWM_PIN, DOWN_PWM_CHANNEL);

    state_machine = new StateMachine();
    state_machine->begin(&current_desk_height, UP_PWM_CHANNEL, DOWN_PWM_CHANNEL);

    BLEDevice::init("ESP32_Desk");
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(new MySecurity());

    BLEServer *p_server = BLEDevice::createServer();
    BLEService *p_service = p_server->createService(BLEUUID(SERVICE_UUID), 20);

    /* ------------------- SET HEIGHT TO PRESET CHARACTERISTIC -------------------------------------- */

    BLECharacteristic *p_set_height_to_preset_characteristic = p_service->createCharacteristic(
        SET_HEIGHT_TO_PRESET_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );

    p_set_height_to_preset_characteristic->setCallbacks(new SetHeightToPresetCallbacks());

    /* ------------------- MOVE DESK UP CHARACTERISTIC ---------------------------------------------- */

    BLECharacteristic *p_move_desk_up_characteristic = p_service->createCharacteristic(
        MOVE_DESK_UP_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );

    p_move_desk_up_characteristic->setCallbacks(new MoveDeskUpCallbacks());

    /* ------------------- MOVE DESK UP CHARACTERISTIC ---------------------------------------------- */

    BLECharacteristic *p_move_desk_down_characteristic = p_service->createCharacteristic(
        MOVE_DESK_DOWN_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );

    p_move_desk_down_characteristic->setCallbacks(new MoveDeskDownCallbacks());

    /* ------------------- GET/SET HEIGHT 1 CHARACTERISTIC ------------------------------------------ */

    BLECharacteristic *p_get_height_1_characteristic = p_service->createCharacteristic(
        GET_HEIGHT_1_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ
    );

    p_get_height_1_characteristic->setValue(state_machine->getHeightPreset1(), 1);

    BLECharacteristic *p_save_current_height_as_height_1_characteristic = p_service->createCharacteristic(
        SAVE_CURRENT_HEIGHT_AS_HEIGHT_1_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );

    p_save_current_height_as_height_1_characteristic->setCallbacks(new SaveCurrentHeightAsHeight1Callbacks());

    /* ------------------- GET/SET HEIGHT 2 CHARACTERISTIC ------------------------------------------ */

    BLECharacteristic *p_get_height_2_characteristic = p_service->createCharacteristic(
        GET_HEIGHT_2_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ
    );

    p_get_height_2_characteristic->setValue(state_machine->getHeightPreset2(), 1);

    BLECharacteristic *p_save_current_height_as_height_2_characteristic = p_service->createCharacteristic(
        SAVE_CURRENT_HEIGHT_AS_HEIGHT_2_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );

    p_save_current_height_as_height_2_characteristic->setCallbacks(new SaveCurrentHeightAsHeight2Callbacks());

    /* ------------------- GET/SET HEIGHT 3 CHARACTERISTIC ------------------------------------------ */

    BLECharacteristic *p_get_height_3_characteristic = p_service->createCharacteristic(
        GET_HEIGHT_3_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ
    );

    p_get_height_3_characteristic->setValue(state_machine->getHeightPreset3(), 1);

    BLECharacteristic *p_save_current_height_as_height_3_characteristic = p_service->createCharacteristic(
        SAVE_CURRENT_HEIGHT_AS_HEIGHT_3_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );

    p_save_current_height_as_height_3_characteristic->setCallbacks(new SaveCurrentHeightAsHeight3Callbacks());

    /* ------------------- END CHARACTERISTIC DEFINITIONS ------------------------------------------ */

    p_service->start();

    BLEAdvertising *p_advertising = p_server->getAdvertising();
    p_advertising->start();

    BLESecurity *p_security = new BLESecurity();

    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;

    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &pass_key, sizeof(uint32_t));

    p_security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    p_security->setCapability(ESP_IO_CAP_OUT);
    p_security->setKeySize(16);

    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));

    p_security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    Serial.println("Characteristic defined! Now you can read it in your phone!");

    xTaskCreate(
        updateDeskHeight,     // Function that should be called
        "Update Desk Height", // Name of the task (for debugging)
        1024,                 // Stack size
        NULL,                 // Parameter to pass
        5,                    // Task priority
        NULL                  // Task handle
    );
}

void loop()
{
    state_machine->processCurrentState();
}
