## DMF_MobileBroadband

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module allows the Client to monitor MobileBroadband interfaces and control the transmit power and antenna configuration of MobileBroadband hardware.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_MobileBroadband
````
// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Callback when a TransmissionStateChanged event is received.
    //
    EVT_DMF_MobileBroadband_WirelessStateChangeCallback* EVTMobileBroadbandWirelessStateChangeCallback;
} DMF_CONFIG_MobileBroadband;
````
Member | Description
----|----
EVTMobileBroadbandWirelessStateChangeCallback | Allows the Client to get the status of MobileBroadband when the transmission state changes.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

##### MobileBroadband_WIRELESS_STATE
````
typedef struct _MobileBroadband_WIRELESS_STATE
{
    // Indicates whether modem is valid or not.
    //
    BOOLEAN IsModemValid;
    // Indicates whether device is connected to a MobileBroadband network.
    //
    BOOLEAN IsNetworkConnected;
    // Indicates whether device is transmitting data or not.
    //
    BOOLEAN IsTransmitting;
} MobileBroadband_WIRELESS_STATE;
````
Member | Description
----|----
IsModemValid | Indicates whether modem is valid or not.
IsNetworkConnected | Indicates whether device is connected to a MobileBroadband network.
IsTransmitting | Indicates whether device is transmitting data or not.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_MobileBroadband_MobileBroadbandReadingChangeCallback
````
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
EVT_DMF_MobileBroadband_WirelessStateChangeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ MobileBroadband_WIRELESS_STATE* WirelessState
    );
````

Client specific callback that allows the Client to receive status of MobileBroadband when its state changes.

##### Returns

None

##### Parameters
Member | Description
----|----
DmfModule | An open DMF_MobileBroadband Module handle.
MobileBroadbandState | Structure of MobileBroadband state.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_MobileBroadband_AntennaBackOffTableIndexGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_AntennaBackOffTableIndexGet(
    _In_ DMFMODULE DmfModule,
    _In_ INT32 AntennaIndex,
    _Out_ INT32* AntennaBackOffTableIndex
    );
````

Get the current power back-off table index of the given antenna of device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_MobileBroadband Module handle.
AntennaIndex | The given antenna of the device.
AntennaBackOffTableIndex | Receives the power back-off table index.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_MobileBroadband_AntennaBackOffTableIndexSet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_AntennaBackOffTableIndexSet(
    _In_ DMFMODULE DmfModule,
    _In_ INT32* AntennaIndex,
    _In_ INT32* AntennaBackOffTableIndex,
    _In_ INT32 AntennaCount,
    _In_ BOOLEAN AbsoluteAntennaIndexMode
    );
````

Set the desired power back-off table index of the given antenna of device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_MobileBroadband Module handle.
AntennaIndex | Index of antenna to set.
AntennaBackOffTableIndex | Index into the power back-off table to set.
AntennaCount | Number of antennas present.
AbsoluteAntennaIndexMode | Indicates whether the input antenna index is an absolute index or relative index.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_MobileBroadband_MccMncGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_MccMncGet(
    _In_ DMFMODULE DmfModule,
    _Out_ DWORD* MobileCountryCode,
    _Out_ DWORD* MobileNetworkCode
    );
````

Get the Mobile Country Code and Mobile Network Code of the mobile broadband network to which the device is attached from the modem.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_MobileBroadband Module handle.
MobileCountryCode | The three digit code corresponding to the country where the device is connected.
MobileNetworkCode | The three digit code corresponding to the network where the device is connected.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_MobileBroadband_SarBackOffDisable

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_SarBackOffDisable(
    _In_ DMFMODULE DmfModule
    );
````

Allows Client to disable MobileBroadband Sar back-off functionality. 

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_MobileBroadband Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_MobileBroadband_SarBackOffEnable

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_SarBackOffEnable(
    _In_ DMFMODULE DmfModule
    );
````

Allows Client to enable MobileBroadband Sar back-off functionality. 
NOTE: Caller should call DMF_MobileBroadband_AntennaBackOffTableIndexSet to set antenna back-off index after enable SAR back-off.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_MobileBroadband Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_MobileBroadband_WirelessStateGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_WirelessStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ MobileBroadband_WIRELESS_STATE* MobileBroadbandWirelessState
    );
````          

Allows Client to get the current MobileBroadband wireless state.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_MobileBroadband Module handle.
MobileBroadbandWirelessState | MobileBroadband wireless state to return.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module uses C++/WinRT, so it needs RS5+ support.
  Module specific code will not be compiled in RS4 and below.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Network

-----------------------------------------------------------------------------------------------------------------------------------

