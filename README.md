<b>BLE Client Backgrounder</b>

Bluetooth Low Energy (BLE) is different from Bluetooth Classic (BLC) in a number of ways. Windows support for BLE is such that you can set up a BLC serial port and provided your BLC device is running a serial port acceptor you are golden. BLE is not well supported and, to make things worse, much of the documentation around it appears to be wrong. Microsoft’s own sample project, for example, only works if you “pair” the BLE device, however, you can’t pair a BLE device on Windows 10 so the sample project doesn’t work.

Espressif provides all sort of sample code however I am using the ESP32-C3, which is not yet well supported. I can’t get the BLC examples to compile for ESP32-C3 and the BLE samples which do compile are ble_spp_server and ble_spp_client, which are meant to talk to each other. My current project requires 2 ESP32-C3s, GPS, and LORA, with one of the ESP32-C3s acting as a gateway to an Android device. For battery reasons it made a lot more sense to connect to Android using BLE instead of WiFi.

When working with embedded devices I find it much easier to get things working on a PC (hardware permitting) and then port things over to my own hardware. I didn’t want to try and learn/program/debug 2 ESP32-C3 boards + Android, especially since my first task will be to debug the hardware. BLE is apparently well supported under Android and Linux using the Bluez stack. Unfortunately, it does not appear there is a Bluez stack for Msys2, or at least I could not find one. I use my PC for other things and did not want to have to install Linux on it at this time.

That meant that I would have to get a BLE Serial Client going under Windows. Since there is no reason why I’d want a GUI application on Windows (remember my first task is hardware debug) I wanted to use a simple concole application. I figured that would be easy: surely there would be sample code all over the place and, failing that, how hard could it be. It turns out there is very little BLE sample code available, that which is available either does not work (recall, Microsoft’s own example doesn’t work) and/or is written in C#. After a lot of trying, I found two examples:

1) BLEConsole, which works but is written as a debugger tool and is in C# (see https://github.com/sensboston/BLEConsole) and

2) This https://stackoverflow.com/questions/67934095/trying-to-create-a-gatt-client-application-for-windows-c-that-doesnt-fail-whe Stack Overflow post, which doesn’t work, or at least doesn’t work with esp_spp_server even with modification.

Unfortunately, Microsoft’s documentation on its BLE API is of byzantinne complexity and starts with the assumption that you already know how everything works. The few examples it give are of very narrow applicability and, as I noted, their GUI application literally does not work.

I know I am not a particularly competent c++ programmer, and in particular I know next to nothing about Windows API programming. Therefore I am sure this is not an example of how things should be done, but it works and should be useful as a starter.


<b>Known Problems and Limitations:</b> 
1) I haven’t figured out how to re-attach a BLE device if connection is lost. You need to re-start the code. Since this is intended for debugging purposes it should not be a problem;

2) If you stop and then restart the client quickly, it appears the server (ESP32-C3) still thinks there is a connection and the client won’t connect. Usually it figures it out after a while but if it doesn’t, restart the ESP32 and the client.

I would be delighted if anybody could help me solve these problems, especially the first one, because I think it is related to the second.

The software currently supports only a single BLE device, one read and one write UUID (see below). It should be easy to expand this to mulitple BLE devices and/or mulitple read or write UUIDs, however that was not my requirement.


<b><u>BLE Client Outline</b></u>

The ESP32 ble_spp_server_demo sets up a device name "ESP_SPP_SERVER" as well as 

1) BLE Service with a UUID of 0xABF0 and

2) Characteristic UUIDs of
DATA_RECEIVE 0xABF1
DATA_NOTIFY 0xABF2
COMMAND_RECEIVE 0xABF3
COMMAND_NOTIFY 0xABF4

(UUIDs are of the form 0000xxxx-0000-1000-8000-00805f9b34fb where xxxx is the 16 bit UUID)

A scan of the BLE device will report services of
[0] 0x1801 (GenericAttribute)
[1] 0x1800 (GenericAccess)
[2] 0xABF0
The client writes to the DATA_RECEIVE UUID and is notified via the DATA_NOTIFY UUID and the server writes to DATA_NOTIFY and reads from DATA_RECEIVE.

The BLE Client starts up a Windows scanner (TestBLE::ScanDevices()) which populates vector<BLEDeviceData> deviceList{} as it discovers nearby BLE devices. The BLE device (i.e. SPP_SERVER) is identified by a unique name (“ESP_SPP_SERVER”) and MAC (which is 98:3b:8f:dd:f4:d1-7c:df:a1:66:a6:3d in my case). Windows refers to this as deviceList.name and deviceList.id. Only unique deviceList.name and deviceList.id combinations are added to deviceList. If there is more than one device with the same name you will have to come up with a way to select which one(s) you want to access. BLE Client assumes only a single BLE device named ESP_SPP_SERVER.

Once a ESP_SPP_SERVER device is discovered, an effort is made to discover the service with UUID 0xABF0 and if that is found the associated characteristics are collected. The UUIDs of the characteristics and their properties will be listed and once this is complete you should be able to read/write the characteristics. 

Properties abbreviations
B Broadcast
R Read 
w WriteWithoutResponse (used by ESP32 SPP Server for writing)
W Write
N Notify (used by ESP32 SPP Server for reading)
I Indicate
A AuthenticatedSignedWrites
X ExtendedProperties
E ReliableWrites
Z WritableAuxiliaries

You write to the ESP_WRITE_DATA_UUID and (indirectly) read from ESP_READ_DATA_NOTIFY_UUID. The read function is asynchronous (like an interupt) so the data essentially just appears in a queue called RXDataBuffers. The size of the recieved packets is not restricted but the number of packets in RXDataBuffers is limited to 10 at the moment. 


<b>C Language Interface</b>

There are five functions involved.

1) int StartBLEDeviceServices(char* BLEDeviceName, uint16_t ServiceUUID, uint16_t WriteUUID, uint16_t ReadUUID, uint16_t MaxWait);
   returns 0 if successful. 

2) bool SendData(uint16_t DestUUID16, uint8_t* data, uint16_t size);
   returns true if successful;

3) unsigned GetRXData(uint8_t* RXData, unsigned RXMax);
   returns number of bytes received. RX data is copied to a uint8_t array of size RXMax. Note that binary data is 
   exchanged so if you want to print a string you have to zero terminate (or ensure the sender sends the zero).

4) bool CheckActiveBLEConnectionStatus(void);
   returns true if the BLE connection is still active. It takes about 10 seconds for the connection to be lost.

5) unsigned ListDevices(bool show);
   print a list of currently connected BLE devices, service UUIDs, and characteristic UUIDs.
