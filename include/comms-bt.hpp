#pragma once

#include <NimBLEDevice.h>

void TaskCommsBT(void *pvParameters);
void comms_init(short myid);
bool ble_get_service(NimBLERemoteService*& service, NimBLEClient* bleClient, NimBLEUUID uuid);
bool ble_get_characteristic(NimBLERemoteService* service, NimBLERemoteCharacteristic*& c, NimBLEUUID uuid, notify_callback notifyCallback = nullptr);
