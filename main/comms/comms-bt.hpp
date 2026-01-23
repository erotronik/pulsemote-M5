#pragma once

#include <NimBLEDevice.h>

void TaskCommsBT(void *pvParameters);
bool ble_get_service( NimBLERemoteService*& service, NimBLEClient* bleClient, NimBLEUUID uuid);
bool ble_get_characteristic( NimBLERemoteService* service, NimBLERemoteCharacteristic*& c,  NimBLEUUID uuid, NimBLERemoteCharacteristic::notify_callback notifyCallback = nullptr, bool response = false);

const int scanTime = 30;  // Duration is in seconds in NimBLE
