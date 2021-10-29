## DMF_AcpiPepDeviceFan

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module creates Definition and Match tables for an ACPI-compliant Fan device that uses AcpiPepDevice Module to register
with OS PEP framework. This Module handles EvaluateMethod callback for servicing _FST and _DSM requests as well as sending 
ACPI Notify on behalf of Fan device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_AcpiPepDeviceFan
Client uses this structure to configure the Module specific parameters.

````
typedef struct
{
    // Instance index used by SAM to identify this fan.
    //
    ULONG FanInstanceIndex;
    // GUID defined for FAN DSMs.
    //
    GUID FanDsmGuid;
    // Unique string assigned to this fan.
    //
    CHAR* FanInstanceName;
    // Fan's name as Wchar string as specified in ACPI.
    //
    WCHAR* FanInstanceNameWchar;
    // Fan's name as Char string as specified in ACPI.
    //
    CHAR* FanInstanceNameChar;
    // Ulong containing Fan name as specified in ACPI.
    //
    ULONG FanInstanceRealName;
    // HWID of the fan corresponding to the one in ACPI.
    //
    CHAR* FanInstanceHardwareId;
    // Callback invoked upon _FST function call.
    //
    EVT_DMF_AcpiPepDeviceFan_FanSpeedGet* FanSpeedGet;
    // Callback invoked upon _DSM function call with
    // function index FAN_DSM_TRIPPOINT_FUNCTION_INDEX.
    //
    EVT_DMF_AcpiPepDeviceFan_FanTripPointsSet* FanTripPointsSet;
    // Callback invoked upon _DSM function call with
    // function index FAN_DSM_RANGE_FUNCTION_INDEX.
    //
    EVT_DMF_AcpiPepDeviceFan_DsmFanRangeGet* DsmFanRangeGet;
    // Client can specify Fan resolution to be returned as a result of _DSM call.
    //
    ULONG DsmFanCapabilityResolution;
    // Client can specify Fan support index to be returned as a result of _DSM call.
    //
    UCHAR DsmFunctionSupportIndex;
} DMF_CONFIG_AcpiPepDeviceFan;
````
Member | Description
----|----
FanInstanceIndex | Instance index used by SAM to identify this fan.
FanDsmGuid | GUID defined for FAN DSMs.
FanInstanceName | Unique string assigned to this fan.
FanInstanceNameWchar | Fan's name as Wchar string as specified in ACPI.
FanInstanceNameChar | Fan's name as Char string as specified in ACPI.
FanInstanceRealName | Ulong containing Fan name as specified in ACPI.
FanInstanceHardwareId | HWID of the fan corresponding to the one in ACPI.
FanSpeedGet | Callback invoked upon _FST function call.
FanTripPointsSet | Callback invoked upon _DSM function call with function index FAN_DSM_TRIPPOINT_FUNCTION_INDEX.
DsmFanRangeGet | Callback invoked upon _DSM function call with function index FAN_DSM_RANGE_FUNCTION_INDEX.
DsmFanCapabilityResolution | Client can specify Fan resolution to be returned as a result of _DSM call.
DsmFunctionSupportIndex | Client can specify Fan support index to be returned as a result of _DSM call.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### AcpiPepDeviceFan_FAN_RANGE_INDEX
````
typedef enum _AcpiPepDeviceFan_FAN_RANGE_INDEX
{
    AcpiPepDeviceFan_FanRangeIndex0 = 0,
    AcpiPepDeviceFan_FanRangeIndex1,
    AcpiPepDeviceFan_FanRangeIndex2,
    AcpiPepDeviceFan_FanRangeIndex3,
    AcpiPepDeviceFan_NumberOfFanRanges,
} AcpiPepDeviceFan_FAN_RANGE_INDEX;
````
Enumerator specifying fan range indexes.

-----------------------------------------------------------------------------------------------------------------------------------


-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------
##### AcpiPepDeviceFan_TRIP_POINT
````
typedef struct
{
    UINT16 High;
    UINT16 Low;
} AcpiPepDeviceFan_TRIP_POINT;
````
Specifies high and low trip point for Fan device.

-----------------------------------------------------------------------------------------------------------------------------------


-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_AcpiPepDeviceFan_FanSpeedGet
````
_Function_class_(EVT_DMF_AcpiPepDeviceFan_FanSpeedGet)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_AcpiPepDeviceFan_FanSpeedGet(_In_ DMFMODULE DmfModule,
                                     _In_ ULONG FanInstanceIndex,
                                     _Out_ VOID* Data,
                                     _In_ size_t DataSize);
````

Upon receiving _FST call from OS, the Module calls in Client supplied Fan speed get call to receive fan speed information.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiPepDeviceFan Module handle.
FanInstanceIndex | Fan index as specified in Module config.
Data | The speed information filled in.
DataSize | Size of data.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_AcpiPepDeviceFan_FanTripPointsSet
````
_Function_class_(EVT_DMF_AcpiPepDeviceFan_FanTripPointsSet)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_AcpiPepDeviceFan_FanTripPointsSet(_In_ DMFMODULE DmfModule,
                                          _In_ ULONG FanInstanceIndex,
                                          _In_ AcpiPepDeviceFan_TRIP_POINT TripPoint);
````

The OS sends in new Trip points through _DSM call which is intercepted and new trip points are sent to Client.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiPepDeviceFan Module handle.
FanInstanceIndex | Fan index as specified in Module config.
TripPoint | Trip points specified for fan operation.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_AcpiPepDeviceFan_DsmFanRangeGet
````
_Function_class_(EVT_DMF_AcpiPepDeviceFan_DsmFanRangeGet)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_AcpiPepDeviceFan_DsmFanRangeGet(_In_ DMFMODULE DmfModule,
                                        _Out_ ULONG DsmFanRange[AcpiPepDeviceFan_NumberOfFanRanges]);
````

The OS sends in request for fan range, and this callback should return the appropriate fan range array containing fan annoyance levels

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiPepDeviceFan Module handle.
DsmFanRange | Fan range information from hardware.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_AcpiPepDeviceFan_AcpiDeviceTableGet

````
NTSTATUS
DMF_AcpiPepDeviceFan_AcpiDeviceTableGet(
    _In_ DMFMODULE DmfModule,
    _Out_ PEP_ACPI_REGISTRATION_TABLES* PepAcpiRegistrationTables
    );
````

This Method is used to obtain necessary fan tables to be passed into AcpiPepDevice Module. This is done explicitly because due to the scalability
requirements the Modules don't share the usual parent-child relationship with each other.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiPepDeviceFan Module handle.
PepAcpiRegistrationTables | Tables containing information on Fan device's HW ID, name, and supported callbacks.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_AcpiPepDeviceFan_NotifyRequestSchedule

````
VOID
DMF_AcpiPepDeviceFan_NotifyRequestSchedule(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG NotifyCode
    );
````

Allows the Client to send an Acpi Notify call up the stack.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiPepDeviceFan Module handle.
NotifyCode | The code that is to be passed to the OS.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module is used with one instance of AcpiPepDevice to provide support for an ACPI compliant fan at kernel level. Due to there
being no parent-child relationship between these Modules the user has to explicitly fetch tables using supplied Method and supply
them to AcpiPepDevice Module. See Windows-driver-samples\pofx\PEP\pepsamples for usage example.
-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

