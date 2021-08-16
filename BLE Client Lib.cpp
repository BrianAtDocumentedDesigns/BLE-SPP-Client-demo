/*

MIT License

Copyright(c) 2021 Brian Piccioni brian@documenteddesigns.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this softwareand associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright noticeand this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

this is adapted from
https://stackoverflow.com/questions/67934095/trying-to-create-a-gatt-client-application-for-windows-c-that-doesnt-fail-whe

to set up

Use Visual studio 2019
create a new console (.exe) solution,
    Platform in Project -> Properties to Win32 and
    Project -> Properties -> C/C++ -> Command Line and add these options: / std:c++17 / await

    Note: to compile you have to Project->Manage NuGet Packages-> and add CppWinRT

*/
#include "BLE CLient Lib.h"

#pragma region STRUCS AND ENUMS


#define LOG_ERROR(e) cout << e << endl;

mutex statusLock;
condition_variable statusSignal;

mutex serviceLock;
condition_variable serviceSignal;

mutex subscribeLock;
condition_variable subscribeSignal;

mutex writeLock;
condition_variable signalWrite;

mutex deviceListLock;
condition_variable deviceListSignal;

struct ScanForBLE {
    static void ScanDevices();
    static void StopDeviceScan();
};

struct Subscription {
    GattCharacteristic::ValueChanged_revoker revoker;
};


struct BLEDeviceData {
    wstring id;
    wstring name;
    bool isConnected = false;
    GattDeviceService service = NULL;
    Subscription* subscription = NULL;
    IVectorView<GattCharacteristic> characteristics{ nullptr };
    uint16_t serviceUUID = -1, notifyUUID = -1;
};


vector<BLEDeviceData> deviceList{};
BLEDeviceData   ActiveBLEDevice;

struct BLEProperties {
    char type;
    unsigned mask;
};

struct BLEProperties BLEPropertiesTable[] =
{
        { 'B', 0x1 },   //Broadcast
        { 'R', 0x2 },   //Read 
        { 'w', 0x4 },   //WriteWithoutResponse 
        { 'W', 0x8 },   // Write
        { 'N', 0x10 },  //Notify
        { 'I', 0x20 },  //Indicate
        { 'A', 0x40 },  //AuthenticatedSignedWrites
        { 'X', 0x80 },  //ExtendedProperties
        { 'E', 0x100 }, //ReliableWrites
        { 'Z', 0x200 }, //WritableAuxiliaries
};

#pragma endregion

#pragma region SCAN DEVICES FUNCTIONS

DeviceWatcher deviceWatcher{ nullptr };
mutex deviceWatcherLock;
DeviceWatcher::Added_revoker deviceWatcherAddedRevoker;
DeviceWatcher::Updated_revoker deviceWatcherUpdatedRevoker;
DeviceWatcher::Removed_revoker deviceWatcherRemovedRevoker;
DeviceWatcher::EnumerationCompleted_revoker deviceWatcherCompletedRevoker;


//This function would be called when a new BLE device is detected
void DeviceWatcher_Added(DeviceWatcher sender, DeviceInformation deviceInfo) {
    BLEDeviceData deviceData;
    deviceData.id = wstring(deviceInfo.Id().c_str());
    deviceData.name = wstring(deviceInfo.Name().c_str());
    //Only add new/unique items
    for (unsigned i = 0; i < deviceList.size(); i++) {  
        if ((deviceList[i].id == deviceData.id) && (deviceList[i].name == deviceData.name))
            return;
    }
    deviceList.push_back(deviceData);
}

//This function would be called when an existing BLE device is updated
void DeviceWatcher_Updated(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
}

void DeviceWatcher_Removed(DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
}

void DeviceWatcher_EnumerationCompleted(DeviceWatcher sender, IInspectable const&) {
    ScanForBLE::StopDeviceScan();
    ScanForBLE::ScanDevices();
}

//Call this function to scan async all BLE devices
void ScanForBLE::ScanDevices() {
    try {
        lock_guard lock(deviceWatcherLock);
        IVector<hstring> requestedProperties = single_threaded_vector<hstring>({ L"System.Devices.Aep.DeviceAddress", L"System.Devices.Aep.IsConnected", L"System.Devices.Aep.Bluetooth.Le.IsConnectable" });
        hstring aqsFilter = L"(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")"; // list Bluetooth LE devices
        deviceWatcher = DeviceInformation::CreateWatcher(aqsFilter, requestedProperties, DeviceInformationKind::AssociationEndpoint);
        deviceWatcherAddedRevoker = deviceWatcher.Added(auto_revoke, &DeviceWatcher_Added);
        deviceWatcherUpdatedRevoker = deviceWatcher.Updated(auto_revoke, &DeviceWatcher_Updated);
        deviceWatcherRemovedRevoker = deviceWatcher.Removed(auto_revoke, &DeviceWatcher_Removed);
        deviceWatcherCompletedRevoker = deviceWatcher.EnumerationCompleted(auto_revoke, &DeviceWatcher_EnumerationCompleted);
        deviceWatcher.Start();
    }
    catch (exception e) {
        LOG_ERROR(e.what())
    }
}

void ScanForBLE::StopDeviceScan() {
    scoped_lock lock(deviceListLock, deviceWatcherLock);
    if (deviceWatcher != nullptr) {
        deviceWatcherAddedRevoker.revoke();
        deviceWatcherUpdatedRevoker.revoke();
        deviceWatcherRemovedRevoker.revoke();
        deviceWatcherCompletedRevoker.revoke();
        deviceWatcher.Stop();
        deviceWatcher = nullptr;
    }
    deviceListSignal.notify_one();
}

#pragma endregion

#pragma region READ/WRITE FUNCTIONS


struct ReadBuffer
{
    uint8_t* data;
    size_t  length;
};

#define RXDataQueueSize 10

queue<ReadBuffer> RXDataBuffers;

//
// see also https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.datareader?view=winrt-20348#examples
//On this function you can read all data from the specified characteristic
void Characteristic_ValueChanged(GattCharacteristic const& characteristic, GattValueChangedEventArgs args)
{
    if (RXDataBuffers.size() < RXDataQueueSize) //ignore if full
    {
        auto value = args.CharacteristicValue();
        uint16_t numbytes = (uint16_t)value.Length();
        uint8_t* rxdata = (uint8_t *) calloc(numbytes, sizeof(uint8_t));

        if (rxdata != NULL) {
            for (unsigned i = 0; i < numbytes; i++)
                rxdata[i] = value.data()[i];
        }

        ReadBuffer rbuf;
        rbuf.data = rxdata;
        rbuf.length = numbytes;
        RXDataBuffers.push(rbuf);
    }
}

unsigned  GetBLERXData(uint8_t *RXData, unsigned RXMax ) {
    if (RXDataBuffers.empty()) return 0;    //No gots

    ReadBuffer rbuf = RXDataBuffers.front();
    RXDataBuffers.pop();

    unsigned xfer = rbuf.length > RXMax ? RXMax : rbuf.length;
    for (size_t i = 0; i < xfer; i++)
        *RXData++ = rbuf.data[i];

    free(rbuf.data);
    return(xfer);
}

//
//Function used to send data async to the specific device
//
fire_and_forget SendDataAsync(BLEDeviceData& deviceData, uint16_t DestUUID16, uint8_t* data, uint16_t size, bool* result) {

    GattCharacteristic destcharacteristic = NULL;
    if (deviceData.characteristics == NULL)
        printf("\nCharacteristics not set up!");
    else {
        for (unsigned i = 0; i < deviceData.characteristics.Size(); i++) {
            if (deviceData.characteristics.GetAt(i).Uuid().Data1 == DestUUID16) {
                destcharacteristic = deviceData.characteristics.GetAt(i);
                break;
            }
        }
    }

    if (destcharacteristic == NULL)
        printf("\nDestination UUID not found!");
    else {
        try {
                DataWriter writer;
                writer.WriteBytes(array_view<uint8_t const>(data, data + size));
                IBuffer buffer = writer.DetachBuffer();
                auto status = co_await destcharacteristic.WriteValueAsync(buffer, GattWriteOption::WriteWithoutResponse);
                if (status != GattCommunicationStatus::Success) {
                    LOG_ERROR("Error writing value to characteristic. Status: " << (int)status)
                }
                else if (result != 0) {
                    //                LOG_ERROR("Data written succesfully")
                    *result = true;
                }
        }
        catch (hresult_error& ex)
        {
            LOG_ERROR("SendDataAsync error: " << to_string(ex.message().c_str()));
                for (unsigned i = 0; i < deviceList.size(); i++) {
                    if (deviceList[i].id == deviceData.id && deviceList[i].subscription) {
                        delete deviceList[i].subscription;
                        deviceList[i].subscription = NULL;
                        break;
                    }
                } 
        }
    }
    signalWrite.notify_one();
}


//
// I am not sure the posi
bool FindBLEyName(BLEDeviceData& deviceData, char* aBLEName) {
    bool rv = false;
    wstring bledevicename(aBLEName, aBLEName + strlen(aBLEName));

    if (deviceList.size() != 0)
    {
        for( unsigned i = 0; i < deviceList.size(); i++)
            if( deviceList[i].name == bledevicename ){
                deviceData = deviceList[i];
                rv = true;
        }
    }
        
    return rv;
}

//Call this function to write data on the device
bool SendData(uint16_t DestUUID16, uint8_t* data, uint16_t size) {
    bool result = false;
    unique_lock<mutex> lock(writeLock);
    SendDataAsync(ActiveBLEDevice, DestUUID16, data, size, &result);
    signalWrite.wait(lock);
    return result;
}

#pragma endregion

#pragma region BLE UTILITY FUNCTIONS

//Call this function to see if the device is still connected
fire_and_forget AsyncCheckConnectionStatus(BLEDeviceData& aBLEDevice ) {
    auto device = co_await BluetoothLEDevice::FromIdAsync(aBLEDevice.id);
    if (device != nullptr)
        aBLEDevice.isConnected = device.ConnectionStatus() == BluetoothConnectionStatus::Connected;
    else
        aBLEDevice.isConnected = false;

    statusSignal.notify_one();
}

//
// It takes about 10 seconds before the PC realizes the BLE is no longer connected 
// so only check every few seconds or so
// Note: this routine takes about 33mSecs so it is best not to hammer it
//
bool CheckConnectionStatus(BLEDeviceData& aBLEDevice) {
    unique_lock<mutex> lock(statusLock);
    AsyncCheckConnectionStatus(aBLEDevice);
    statusSignal.wait(lock);
    return aBLEDevice.isConnected;
}

//
//Call this function to get a service if it wasn't found
//
IAsyncOperation<GattDeviceService> GetService( BLEDeviceData& deviceData, uint16_t aServiceUUID, uint16_t aNotifyUUID) {
    auto device = co_await BluetoothLEDevice::FromIdAsync( deviceData.id );
    GattDeviceServicesResult serviceresult = co_await device.GetGattServicesAsync(BluetoothCacheMode::Cached);

    if (serviceresult.Status() != GattCommunicationStatus::Success) {
        LOG_ERROR("Failed getting services. Status: " << (int)serviceresult.Status())
            co_return nullptr;
    }
    else if (serviceresult.Services().Size() == 0) {
        LOG_ERROR("\nNo service found!")
            co_return nullptr;
    }

    else {
        int uuid, i, numservices = serviceresult.Services().Size();
        for (i = 0; i < numservices; i++) {
            uuid = serviceresult.Services().GetAt(i).Uuid().Data1;
            if (uuid == aServiceUUID) break;
        }

        if (uuid != aServiceUUID) {
            printf("\nExpected UUID %x not found!", aServiceUUID);
        }

        else { //Now look for characteristics
            deviceData.service = serviceresult.Services().GetAt(i);
            deviceData.serviceUUID = aServiceUUID;

            try
            {  // Ensure we have access to the device.
                auto accessStatus = co_await deviceData.service.RequestAccessAsync();

                if (accessStatus == DeviceAccessStatus::Allowed) {
                    // BT_Code: Get all the child ESP32Characteristics of a service. Use the cache mode to specify uncached characterstics only 
                    // and the new Async functions to get the characteristics of unpaired devices as well. 
                    GattCharacteristicsResult result = co_await deviceData.service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
                    auto characteristics = result.Characteristics();
                    if ((characteristics.Size() != 0)
                        && (result.Status() == GattCommunicationStatus::Success)) {
                        deviceData.characteristics = characteristics;

                        int numcharacteristics = (int)deviceData.characteristics.Size();
                        bool found = false;

                        uint16_t thisUUID;
                        GattCharacteristic thischar = NULL;
                        GattCharacteristicProperties thisprop;

                        for (int jj = 0; jj < numcharacteristics; jj++)
                        {
                            thischar = deviceData.characteristics.GetAt(jj);
                            thisprop = thischar.CharacteristicProperties();
                            thisUUID = thischar.Uuid().Data1;

                            if (thisUUID == aNotifyUUID) {
                                deviceData.notifyUUID = aNotifyUUID;
                                found = true;
                                try {
                                    if (thischar != nullptr) {
                                        auto status = co_await thischar.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::Notify);
                                        if (status != GattCommunicationStatus::Success) {
                                            LOG_ERROR("Error subscribing to characteristic. Status: " << (int)status)
                                        }
                                        else {
                                            for (unsigned i = 0; i < deviceList.size(); i++) {
                                                if (deviceList[i].id == deviceData.id) {
                                                    deviceList[i].subscription = new Subscription();
                                                    deviceList[i].subscription->revoker = thischar.ValueChanged(auto_revoke, &Characteristic_ValueChanged);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                                catch (hresult_error& ex)
                                {
                                    LOG_ERROR("\nSubscribeCharacteristicAsync error: " << to_string(ex.message().c_str()))
                                        for (unsigned i = 0; i < deviceList.size(); i++) {
                                            if (deviceList[i].id == deviceData.id && deviceList[i].subscription) {
                                                delete deviceList[i].subscription;
                                                deviceList[i].subscription = NULL;
                                                break;
                                            }
                                        }
                                }
                            }
                        }

                        if (!found)
                            printf("\nNotify characteristic %x not found!", aNotifyUUID);


                    }
                    else {
                        printf("\nError accessing service.");
                    }
                }
                else {
                    printf("\nNo access to service.");
                }
            }
            catch (hresult_error& ex)
            {
                LOG_ERROR("\nSubscribeCharacteristicAsync error: " << to_string(ex.message().c_str()))
            }
        }
    }
    serviceSignal.notify_one();
    co_return nullptr;
}

//
// Have to move async outside of main()
//
bool   GetServiceandCharacteristics(BLEDeviceData& deviceData, uint16_t aServiceUUID, uint16_t aNotifyUUID) {
    unique_lock<mutex> lock(serviceLock);
    bool result = false;
    GetService( deviceData, aServiceUUID, aNotifyUUID);
    serviceSignal.wait(lock);
    return result;
}

#pragma endregion

#pragma region C INTERFACE FUNCTIONS 

unsigned ListDevices( bool show )
{
    if (show) {
        for (unsigned j = 0; j < deviceList.size(); j++)
        {
            BLEDeviceData& deviceData = deviceList[j];
            if (deviceData.name.size() != 0) {
                string mac = "NA";
                wstring macheader = L"BluetoothLE#BluetoothLE";
                size_t wheremac = deviceData.id.find(macheader);

                if (wheremac != string::npos)
                    mac = to_string(deviceData.id.substr(wheremac + macheader.length()));

                printf("\nDevice #%d %s %s ", j,
                    to_string(deviceData.name).c_str(),
                    mac.c_str());

                if (deviceData.service != NULL) {

                    unsigned numcharacteristics = deviceData.characteristics.Size();
                    printf("Service UUID %x\nCharacteristics:", deviceData.serviceUUID);

                    if (numcharacteristics != 0) {
                        string propstring;
                        uint16_t thisUUID;
                        GattCharacteristic thischar = NULL;
                        GattCharacteristicProperties thisprop;

                        for (unsigned i = 0; i < numcharacteristics; i++)
                        {
                            thischar = deviceData.characteristics.GetAt(i);
                            thisprop = thischar.CharacteristicProperties();
                            thisUUID = thischar.Uuid().Data1,
                                propstring.clear();

                            for (unsigned pp = 0; pp < (sizeof(BLEPropertiesTable) / sizeof(BLEProperties)); pp++) {
                                if ((unsigned)thisprop & BLEPropertiesTable[pp].mask)
                                    propstring += BLEPropertiesTable[pp].type;
                            }
                            printf("\n# %d %x %s ", i, thisUUID, propstring.c_str());
                            if (deviceData.notifyUUID == thisUUID)
                                printf(" <- subscribed");
                        }
                    }
                }
            }
        }
    }
    return(deviceList.size());
}

bool CheckActiveBLEConnectionStatus(void) {
    return(CheckConnectionStatus(ActiveBLEDevice));
}


int StartBLEDeviceServices(char* BLEDeviceName,
    uint16_t ServiceUUID, uint16_t WriteUUID, uint16_t ReadUUID,
    uint16_t MaxWait)
{
    ScanForBLE::ScanDevices(); //Start scanning devices
    printf("\nWaiting for BLE scan ...");

    do{
        Sleep(1000);
    } while (deviceList.empty() && (MaxWait-- > 0));

    if (MaxWait == 0) return(-2);

    wstring bledevicename(BLEDeviceName, BLEDeviceName + strlen(BLEDeviceName));
    do {
        //Then every device and their info updated would be in this vector
        for (unsigned i = 0; i < deviceList.size(); i++) {
            BLEDeviceData& deviceData = deviceList[i];

            if (deviceData.name == bledevicename)
            {
                if (!CheckConnectionStatus(deviceData)) {
                    printf("\nNot Connected!");
                    Sleep(1000);
                }

                if (deviceData.characteristics == NULL) {
                    GetServiceandCharacteristics(deviceData, ServiceUUID, ReadUUID );
                }
                else {
                    ActiveBLEDevice = deviceData;
                    return(0);
                }
            }
        }
    } while (MaxWait--);
    return(-1);
}

#pragma endregion

