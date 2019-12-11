## DMF_VirtualHidMiniSample

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module serves as an example of a virtual HID device that uses DMF_VirtualHidMini as a "base class". See the MSDN sample, VHIDMINI2
for more information.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_VirtualHidMiniSample
Client uses this structure to configure the Module specific parameters.

````
// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    ULONG ReadFromRegistry;
} DMF_CONFIG_VirtualHidMiniSample;
````
Member | Description
----|----
ReadFromRegistry | Indicates that the HID Device Descriptor information should be read from the registry. NOTE: This feature is not currently implemented.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* IMPORTANT: Kernel-mode drivers must set MSHIDMKDF.sys as a Lower Filter driver in the Client driver's INF file using the "LowerFilters" registry entry. See the VHidMini2DmfK.inx for an example.
* IMPORTANT: User-mode drivers must set MSHIDUMDF.sys as a Lower Filter driver in the Client driver's INF file using the "LowerFilters" registry entry. See the VHidMini2DmfU.inx for an example.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* VHidMini2DmfK
* VHidMini2DmfU

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* Implement reading the descriptor from the registry feature or remove it (or possibly move it to DMF_VirtualHidMini.)

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Hid

-----------------------------------------------------------------------------------------------------------------------------------

