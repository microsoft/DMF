## DMF_VirtualHidDeviceVhfSample

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This Module serves as an example of a virtual HID device that uses DMF_VirtualHidDeviceVhf as a "base class". Note: There is no corresponding MSDN sample.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_VirtualHidDeviceVhfSample
Client uses this structure to configure the Module specific parameters.

````
// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    ULONG ReadFromRegistry;
} DMF_CONFIG_VirtualHidDeviceVhfSample;
````
Member | Description
----|----
ReadFromRegistry | Indicates that the HID Device Descriptor information should be read from the registry. NOTE: This feature is not currently implemented.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* To test this sample, use the TestVHid.exe sample application that is part of the MSDN samples.
* VHF does not support SET_OUTPUT_REPORT so this call in the sample application should be removed: ````SetOutputReport(file);````
* VHF does not support device strings, so this call in the sample application should be removed: ````GetIndexedString(file);````

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* VHidDeviceVhfDmfK
* VHidDeviceVhfDmfU

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* Implement reading the descriptor from the registry feature or remove it.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Hid

-----------------------------------------------------------------------------------------------------------------------------------

