VHidMini2DmfK and VHidMini2DmfU Sample Drivers
=============================================
These samples show how to write Virtual HID Mini-drivers that are compatible with both Kernel and User-mode using DMF.

***VHidMini2DmfU is also a good sample of a basic DMF User-mode driver. (All the other samples are Kernel-mode drivers.
This sample can be used for drivers that are not doing HID related work.)***

These samples use the DMF_VirtualHidMini Module. This Module contains all the *generic* code listed in the MSDN VHIDMINI2
sample. The *non-generic* code from VHIDMINI2 is in the DMF_VirtualHidMiniSample. The best practice is for the code that
is specific to the device the driver is written for is in a Module similar to DMF_VirtualHidMiniSample. That Module will,
in turn use DMF_VirtualHidMini Module as a Child Module.

Like other DMF drivers, the DmfInterface.c file instantiates DMF_VirtualHidMiniSample.

This sample is similar to other samples so a code tour is not present. However, note the differences between the default 
Driver Entry macro for User-mode DMF drivers. This change is a result of how event logging happens in DMF drivers.

```
DMF_DEFAULT_DRIVERENTRY(DriverEntry,
                        VHidMini2DmfEvtDriverContextCleanup,
                        VHidMini2DmfEvtDeviceAdd,
                        L"VHidMini2DmfU")
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


Testing the driver
==================

1. Build either or both drivers. Copy the .inf and .dll/.sys files to the target machine.
2. To test the Kernel-mode driver execute this command: `devcon install VHidMIni2DmfK.inf root\vhidmini2dmfk`.
3. To test the User-mode driver execute this command: `devcon install VHidMIni2DmfU.inf root\vhidmini2dmuk`.
4. Compile the TestVHid.exe program in MSDN Samples (under VHIDMINI2).
5. Execute TestVHid.exe. You can now see code in both Parent and Child Module execute.

