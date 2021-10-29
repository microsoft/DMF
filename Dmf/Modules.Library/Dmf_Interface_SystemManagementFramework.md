## DMF_Interface_SystemManagementFramework

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Summary

-----------------------------------------------------------------------------------------------------------------------------------

Defines Interfaces for all SMF SOC Transports.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Bind Time Data

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_INTERFACE_PROTOCOL_SystemManagementFramework_BIND_DATA
````
typedef struct _DMF_INTERFACE_PROTOCOL_SystemManagementFramework_BIND_DATA
{
    // Stores this Protocol's Id.
    //
    ULONG ProtocolId;
} DMF_INTERFACE_PROTOCOL_SystemManagementFramework_BIND_DATA;

````

##### Remarks

Data provided by the Protocol to the Transport during Bind Operation.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_INTERFACE_TRANSPORT_SystemManagementFramework_BIND_DATA
````
typedef struct _DMF_INTERFACE_TRANSPORT_SystemManagementFramework_BIND_DATA
{
    // Stores this Transport's Id.
    //
    ULONG TransportId;
} DMF_INTERFACE_TRANSPORT_SystemManagementFramework_BIND_DATA;

````

##### Remarks

Data provided by the Transport to the Protocol during Bind Operation.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

-----------------------------------------------------------------------------------------------------------------------------------

#### Protocol Module's Declaration Data

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA
````
typedef struct _DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA
{
    // The Protocol Interface Descriptor. 
    // Every Interface must have this as the first member of its Protocol Declaration Data.
    //
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR DmfProtocolDescriptor;
} DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA;

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA* ProtocolDeclarationData,
    _In_ EVT_DMF_INTERFACE_ProtocolBind* EvtProtocolBind,
    _In_ EVT_DMF_INTERFACE_ProtocolUnbind* EvtProtocolUnbind,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind
    )

````

##### Remarks

Data provided by the Protocol Module to indicate that it implements the Dmf_Interface_SystemManagementFramework.
The DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DESCRIPTOR_INIT is used by the Protocol Module to initialize the Declaration Data.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Methods

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_INTERFACE_SystemManagementFramework_TransportBind
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_SystemManagementFramework_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_SystemManagementFramework_BIND_DATA* TransportBindData
    )
    
````
The Bind method that should be implemented by the Transport Module.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Dmf_Interface_SystemManagementFramework Interface handle.
ProtocolBindData | Bind Data provided by the Protocol Module for the Transport Module.
TransportBindData | Bind Data provided by the Transport Module for the Protocol Module.

##### Remarks

* This method is called by the Protocol Module during the Bind operation.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_INTERFACE_SystemManagementFramework_TransportUnbind
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_INTERFACE_SystemManagementFramework_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
    
````
The Unbind method that should be implemented by the Transport Module.

##### Returns

VOID.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Dmf_Interface_SystemManagementFramework Interface handle.

##### Remarks

* This method is called by the Protocol Module during the Unbind operation.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_INTERFACE_SystemManagementFramework_TransportInitialize
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_ChannelsGet(
    _In_ DMFINTERFACE DmfInterface,
    _Out_ USHORT* NumberOfSensorChannels,
    _Out_ USHORT* NumberOfControlChannels
    )

````
    Returns the number of Sensor and Control channels.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
NumberOfSensorChannels | Number of channels going to SMF Core.
NumberOfControlChannels | Number of channels coming from SMF Core.


##### Remarks


##### DMF_INTERFACE_SystemManagementFramework_TransportInitialize
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportInitialize(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_ SMFSOCPLUGIN_CAPABILITIES* Capabilities,
    _Inout_ BOOLEAN* LimitEnable
    )
    
````
    Initializes SoC related resources and populates SMF Capabilities.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
Capabilities | Array of sensor/control channels supported by this SMF SOC interface.
LimitEnable | Populated by the IHV Module to enable/disable channel limit enforcement.


##### Remarks


##### DMF_INTERFACE_SystemManagementFramework_TransportUninitialize
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportUninitialize(
    _In_ DMFINTERFACE DmfInterface
    )
    
````
    Releases any hardware context required to support SMF.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.

##### Remarks

##### DMF_INTERFACE_SystemManagementFramework_TransportControlSet
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportControlSet(
    _In_ DMFINTERFACE DmfInterface,
    _In_ SHORT ChannelIndex,
    _In_ INT32 ControlData,
    _Out_ UINT8* SamNotification
    )
    
````
    Sets a new value on an output channel.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
ChannelIndex | Index of the output channel.
ControlData | New value to be set on the power control.
SamNotification | Pointer to SAM Notification output.

##### Remarks


##### DMF_INTERFACE_SystemManagementFramework_TransportDataGet
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportDataGet(
    _In_ DMFINTERFACE DmfInterface,
    _In_ SHORT ChannelIndex,
    _Out_ INT32* SensorData
    )
    
````
    This Method provides the current reading for the selected sensor channel.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
ChannelIndex | Index of the Channel to be updated with new configuration.
SensorData | Smf Interface Data structure to be updated with the current sensor value.


##### Remarks



##### DMF_INTERFACE_SystemManagementFramework_TransportResetCauseGet
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportResetCauseGet(
    _In_ DMFINTERFACE DmfInterface,
    _Out_ VOID* Data,
    _In_ UINT16 Size
    )
    
````
    Gets the reset cause from the Cpu.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
Data | Reset cause.
Size | Size of data.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Transport Module's Declaration Data

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA
````
typedef struct _DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA
{
    // The Transport Interface Descriptor.
    // Every Interface must have this as the first member of its Transport Declaration Data.
    //
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR DmfTransportDescriptor;
    // Stores methods implemented by this Interface Transport.
    //
    DMF_INTERFACE_SystemManagementFramework_TransportBind* DMF_SystemManagementFramework_TransportBind;
    DMF_INTERFACE_SystemManagementFramework_TransportUnbind* DMF_SystemManagementFramework_TransportUnbind;
    DMF_INTERFACE_SystemManagementFramework_ChannelsGet* DMF_SystemManagementFramework_ChannelsGet;
    DMF_INTERFACE_SystemManagementFramework_TransportInitialize* DMF_SystemManagementFramework_TransportInitialize;
    DMF_INTERFACE_SystemManagementFramework_TransportUninitialize* DMF_SystemManagementFramework_TransportUninitialize;
    DMF_INTERFACE_SystemManagementFramework_TransportControlSet* DMF_SystemManagementFramework_TransportControlSet;
    DMF_INTERFACE_SystemManagementFramework_TransportDataGet* DMF_SystemManagementFramework_TransportDataGet;
    DMF_INTERFACE_SystemManagementFramework_TransportResetCauseGet* DMF_SystemManagementFramework_TransportResetCauseGet;
} DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA* TransportDeclarationData,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ DMF_INTERFACE_SystemManagementFramework_TransportBind* SystemManagementFrameworkTransportBind,
    _In_ DMF_INTERFACE_SystemManagementFramework_TransportUnbind* SystemManagementFrameworkTransportUnbind,
    _In_ DMF_INTERFACE_SystemManagementFramework_ChannelsGet* SystemManagementFrameworkChannelsGet,
    _In_ DMF_INTERFACE_SystemManagementFramework_TransportInitialize* SystemManagementFrameworkTransportInitialize,
    _In_ DMF_INTERFACE_SystemManagementFramework_TransportUninitialize* SystemManagementFrameworkTransportUninitialize,
    _In_ DMF_INTERFACE_SystemManagementFramework_TransportControlSet* SystemManagementFrameworkTransportControlSet,
    _In_ DMF_INTERFACE_SystemManagementFramework_TransportDataGet* SystemManagementFrameworkTransportDataGet,
    _In_ DMF_INTERFACE_SystemManagementFramework_TransportResetCauseGet* SystemManagementFrameworkTransportResetCauseGet
    )

````

##### Remarks

Data provided by the Transport Module to indicate that it implements the Dmf_Interface_SystemManagementFramework. 
The DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DESCRIPTOR_INIT is used by the Transport Module to initialize the Declaration Data.

-----------------------------------------------------------------------------------------------------------------------------------

##### Methods exposed to Protocol.
````
DMF_INTERFACE_SystemManagementFramework_TransportBind DMF_SystemManagementFramework_TransportBind;
DMF_INTERFACE_SystemManagementFramework_TransportUnbind DMF_SystemManagementFramework_TransportUnbind;
DMF_INTERFACE_SystemManagementFramework_ChannelsGet DMF_SystemManagementFramework_ChannelsGet;
DMF_INTERFACE_SystemManagementFramework_TransportInitialize DMF_SystemManagementFramework_TransportInitialize;
DMF_INTERFACE_SystemManagementFramework_TransportUninitialize DMF_SystemManagementFramework_TransportUninitialize;
DMF_INTERFACE_SystemManagementFramework_TransportControlSet DMF_SystemManagementFramework_TransportControlSet;
DMF_INTERFACE_SystemManagementFramework_TransportDataGet DMF_SystemManagementFramework_TransportDataGet;
DMF_INTERFACE_SystemManagementFramework_TransportResetCauseGet DMF_SystemManagementFramework_TransportResetCauseGet;

````

##### Callbacks exposed to Transport.
````

````

##### Required Macro to be included at the end of the Interface.

````
// This macro defines the SystemManagementFrameworkProtocolDeclarationDataGet and SystemManagementFrameworkTransportDeclarationDataGet.
// Call this macro after the DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA and
// DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA are defined.
//
DECLARE_DMF_INTERFACE(SystemManagementFramework);

````

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Remarks

##### SMF Interface GUIDs
````
// {0B7B5350-BB8C-4459-8D4A-A75C57073801?}
DEFINE_GUID(SMF_POWER_LIMIT_CONTROL_TYPE,
    0xb7b5350, 0xbb8c, 0x4459, 0x8d, 0x4a, 0xa7, 0x5c, 0x57, 0x7, 0x38, 0x1);

// {2E8B14C2-26D6-4119-A62E-9D108C4DDC2A}
DEFINE_GUID(SMF_TEMPERATURE_SENSOR_TYPE,
    0x2e8b14c2, 0x26d6, 0x4119, 0xa6, 0x2e, 0x9d, 0x10, 0x8c, 0x4d, 0xdc, 0x2a);

// {349D0E39-53E2-449C-8067-D690B39E7459}
DEFINE_GUID(SMF_FREQUENCY_LIMIT_CONTROL_TYPE,
    0x349D0E39, 0x53E2, 0x449C, 0x80, 0x67, 0xD6, 0x90, 0xB3, 0x9E, 0x74, 0x59);

// {87008365-EA3A-461C-B83D-6AC6E8DB2F8A}
DEFINE_GUID(SMF_GENERIC_TYPE,
    0x87008365, 0xea3a, 0x461c, 0xb8, 0x3d, 0x6a, 0xc6, 0xe8, 0xdb, 0x2f, 0x8a);

// {CF2F6076-EC8B-4D3D-AF61-CC76B118588A}
DEFINE_GUID(SMF_TIME_LIMIT_CONTROL_OUTPUT_TYPE,
    0xCF2F6076, 0xEC8B, 0x4D3D, 0xaf, 0x61, 0xcc, 0x76, 0xb1, 0x18, 0x58, 0x8a);
````
-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* Dmf_SystemPowerResetCause
* Dmf_SmfSoCInterfacePlugin
* SurfaceSmfSocClientDriver

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

* List possible future work for this Interface.

-----------------------------------------------------------------------------------------------------------------------------------
#### Interface Category

-----------------------------------------------------------------------------------------------------------------------------------

Protocols

-----------------------------------------------------------------------------------------------------------------------------------

