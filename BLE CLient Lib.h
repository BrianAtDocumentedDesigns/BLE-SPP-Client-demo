#pragma once

#include <iostream>
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <mutex>
#include <string>
#include "time.h"
#include <queue>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Web.Syndication.h>
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Storage.Streams.h"


using namespace std;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Web::Syndication;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Storage::Streams;


int StartBLEDeviceServices(char* BLEDeviceName,
    uint16_t ServiceUUID, uint16_t WriteUUID, uint16_t ReadUUID,
    uint16_t MaxWait);

bool CheckActiveBLEConnectionStatus(void);
bool SendData(uint16_t DestUUID16, uint8_t* data, uint16_t size);
unsigned  GetBLERXData(uint8_t* RXData, unsigned RXMax);
unsigned ListDevices(bool show);




