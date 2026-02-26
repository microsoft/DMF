VirtualHidDeviceVhfDmfK VirtualHidDeviceVhfDmfU Sample Drivers
==============================================================
These samples show how to write **VHF** Virtual HID Mini-drivers that are compatible with both Kernel and User-mode using DMF.

***This is also a good sample of a basic DMF User-mode driver. (All the other samples are Kernel-mode drivers.
This sample can be used for drivers that are not doing HID related work.)***

These samples use the DMF_VirtualHidDeviceVhf Module. This Module contains all the *generic* code listed in the [MSDN VHIDMINI2
sample](https://github.com/microsoft/Windows-driver-samples/tree/master/hid/vhidmini2). The *non-generic* code from VHIDMINI2 is in the DMF_VirtualHidDeviceVhfSample. The best practice is for the code that
is specific to the device the driver is written for is in a Module similar to DMF_VirtualHidDeviceVhfSample. That Module will,
in turn use DMF_VirtualHidDeviceVhf Module as a Child Module.

Like other DMF drivers, the DmfInterface.c file instantiates DMF_VirtualHidDeviceVhf. This Module is compatible with both Kernel and User-modes.

This sample is similar to other samples so a code tour is not present. However, note the differences between the default 
Driver Entry macro for User-mode DMF drivers. This change is a result of how event logging happens in DMF drivers.

```
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        VHidMini2DmfEvtDriverContextCleanup,
                        VHidMini2DmfEvtDeviceAdd,
                        L"VirtualHidDeviceVhfDmfU")
````

Also, note that these two APIs are not used in the User-mode driver because they are not present:

```
    // Set any device attributes needed.
    //
    WdfDeviceInitSetDeviceType(DeviceInit,
                               FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetExclusive(DeviceInit,
                              FALSE);
```

Aside from that, there is no difference between the Kernel-mode and User-mode drivers. The same Module is instantiated and works
in both Kernel and User-modes.

**IMPORTANT**
VHF drivers, by design, do not support HID_SET_OUTPUT_REPORT and HID Device Strings. If you need that support, use DMF_VirtualHidMini instead of DMF_VIrtualHidDeviceVhf as the Child Module for your virtual HID device Module.

**IMPORTANT**
Note the registry entries in the .inf file for both Kernel and User-mode drivers.

Testing the driver
==================

1. Build either or both drivers. Copy the .inf and .dll/.sys files to the target machine.
2. To test the Kernel-mode driver execute this command: `devcon install VirtualHidDeviceVhfDmfK.inf root\VirtualHidDeviceVhfDmfK`.
3. To test the User-mode driver execute this command: `devcon install VirtualHidDeviceVhfDmfU.inf root\VirtualHidDeviceVhfDmfU`.
4. Modify the TestVHid.exe sample in MSDN under VHIDMINI2 to delete the following calls:
   ````
   bSuccess = SetOutputReport(file);
   ````
   and 
   ````
   bSuccess = GetIndexedString(file);
   ````
5. Compile the TestVHid.exe program in MSDN Samples (under VHIDMINI2).
6. Execute TestVHid.exe. You can now see code in both Parent and Child Module execute.

