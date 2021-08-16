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

to set up

Use Visual studio 2019
create a new console (.exe) solution,
    Platform in Project -> Properties to Win32 and
    Project -> Properties -> C/C++ -> Command Line and add these options: / std:c++17 / await

    Note: to compile you have to Project->Manage NuGet Packages-> and add CppWinRT

*/

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

    numdevices = ListDevices(true);
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
    
