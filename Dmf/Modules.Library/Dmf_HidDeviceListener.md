## DMF_HidDeviceListener

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

HidDeviceListener notifies client of arrival and removal of HID devices specified in the Module's configuration.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_HidDeviceListener
````
typedef struct
{
    // The Vendor Id of the HID device.
    //
    UINT16 VendorId;
    // List of HID Product Ids.
    //
    UINT16 ProductIds[DMF_HidDeviceListener_Maximum_Pid_Count];
    // Number of entries in the above array.
    //
    ULONG ProductIdsCount;
    // Information needed to idenitify the right HID device.
    //
    UINT16 Usage;
    UINT16 UsagePage;
    // Client callback for matching HID device arrival.
    //
    EVT_DMF_HidDeviceListener_DeviceArrivalCallback* EvtHidTargetDeviceArrivalCallback;

    // Client callback for matching HID device removal.
    //
    EVT_DMF_HidDeviceListener_DeviceRemovalCallback* EvtHidTargetDeviceRemovalCallback;
} DMF_CONFIG_HidDeviceListener;
````
Member | Description
----|----
VendorId | The Vendor Id of the HID devices the Client wants to be notified about.
ProductIds | List of HID Product Ids of the HID device the Client wants to be notified about.
ProductIdsCount | The number of entries in ProductIds.
Usage | The Usage of the HID device the client wants to be notified about.
UsagePage | The Usage Page of the HID device the client wants to be notified about.
EvtHidTargetDeviceArrivalCallback | Client callback for matching HID device arrival.
EvtHidTargetDeviceRemovalCallback | Client callback for matching HID device removal.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

##### Remarks

This section lists all the Enumeration Types specific to this Module that are accessible to Clients.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
````
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
EVT_DMF_HidDeviceListener_DeviceArrivalCallback(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING SymbolicLinkName,
    _In_ WDFIOTARGET IoTarget,
    _In_ PHIDP_PREPARSED_DATA PreparsedHidData,
    _In_ HID_COLLECTION_INFORMATION* HidCollectionInformation
    );
````

Notifies the client of matching HID device arrival.

##### Returns

VOID

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidDeviceListener Module handle.
SymbolicLinkName | The symbolic link name of the matching HID device that has arrived.
IoTarget | A WDFIOTARGET for the arrived HID device.
PreparsedHidData | Allows the Client to access the HID API to determine more HID specific information about the given WDFIOTARGET.
HidCollectionInformation | Allows the Client to access the HID API to determine more HID specific information about the given WDFIOTARGET.
-----------------------------------------------------------------------------------------------------------------------------------
````
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
EVT_DMF_HidDeviceListener_DeviceRemovalCallback(
    _In_ DMFMODULE DmfModule,
    _In_ PUNICODE_STRING SymbolicLinkName);
````

Notifies the client of matching HID device removal.

##### Returns

VOID

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HidDeviceListener Module handle.
SymbolicLinkName | The symbolic link name of the matching HID device that has been removed.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* SurfaceHotPlug

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Hardware

-----------------------------------------------------------------------------------------------------------------------------------

