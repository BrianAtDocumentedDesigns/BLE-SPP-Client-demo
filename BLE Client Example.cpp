//
// Adapted from https://stackoverflow.com/questions/67934095/trying-to-create-a-gatt-client-application-for-windows-c-that-doesnt-fail-whe
//
//Use Visual studio 2019 
// create a new console (.exe) solution, 
// Platform in Project -> Properties to Win32 and 
// Project -> Properties -> C/C++ -> Command Line and add these options: / std:c++17 / await
// 
// Note: for this to work, you have to Project->Manage NuGet Packages-> and add CppWinRT
//
#include "BLE CLient Lib.h"

#define ESP_SPP_SERVER "ESP_SPP_SERVER"
#define ESP_SERVICE_UUID 0xabf0
#define ESP_WRITE_DATA_UUID 0xabf1
#define ESP_READ_DATA_NOTIFY_UUID  0xabf2


int main() {

    char outbuf[100];
    uint8_t rxbuf[100];
    unsigned numdevices, count = 0;

    StartBLEDeviceServices((char *) ESP_SPP_SERVER, 
                        ESP_SERVICE_UUID, ESP_WRITE_DATA_UUID, ESP_READ_DATA_NOTIFY_UUID, 10);

    numdevices = ListDevices(false);
    while (true) {

        if (ListDevices(false) != numdevices) {
            numdevices = ListDevices( true );
        }

        snprintf(outbuf, sizeof(outbuf), "Send %d", count);
        printf("\nWrite %d %s", count++, outbuf);
        bool rv = SendData( ESP_WRITE_DATA_UUID, (uint8_t*)outbuf, (uint16_t)strlen(outbuf));
        if (!rv) {
            printf("\nWre failed");
            if(!CheckActiveBLEConnectionStatus()) printf("\nNot connected!");
            }
 
        do {
            unsigned rxdcnt = GetBLERXData(rxbuf, sizeof(rxbuf));
            if (rxdcnt == 0) break;
            rxbuf[rxdcnt] = '\0';
            printf("%s", (char*)rxbuf);
        } while( true );

        printf("\nSleep");
        Sleep(2000);
    }
}
    
