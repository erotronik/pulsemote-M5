#pragma once

#include <NimBLEDevice.h>

void TaskCommsBT(void *pvParameters);
bool ble_get_service( NimBLERemoteService*& service, NimBLEClient* bleClient, NimBLEUUID uuid);
// Delete a device's client and set it to nullptr. Use this rather than NimBLEDevice::deleteClient()
void ble_delete_client(NimBLEClient*& bleClient);
bool ble_get_characteristic( NimBLERemoteService* service, NimBLERemoteCharacteristic*& c,  NimBLEUUID uuid, NimBLERemoteCharacteristic::notify_callback notifyCallback = nullptr, bool response = false);

// Scanning for devices runs in sessions: at power-on, when a device goes away, and from a Scan button.
// A session ends this long after it started, or after the last device it connected
const uint32_t scan_session_ms = 2 * 60 * 1000;

void ble_start_scan(void);  // start a scan session, or give the one running its full time again
bool ble_is_scanning(void);
