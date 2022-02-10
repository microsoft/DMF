/*++

    Copyright (c) Microsoft Corporation. All Rights Reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Interface_SystemManagementFramework.h

Abstract:

    Companion file to Dmf_Interface_SystemManagementFramework.h.
    Defines a SoC Interface Contract between SMF protocol and transport Module.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

// SMF Interface GUIDs.
//
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

// Redefined here temporarily.
//
typedef struct
{
    // This flag identifies if the data in the structure is valid. This is needed as all physical channels of
    // a single interface driver share same IRPs.
    //
    BOOLEAN IsValid;
    // This GUID defines the channel type. Framework uses the value with instance ID to connect data channels to
    // math functions and send related configurations to interfaces.
    //
    GUID ChannelType;
    // This instance must be unique across the product. Value 0 is means invalid, values above that are allowed.
    //
    USHORT ChannelInstance;
} SMFSOCPLUGIN_CAPABILITIES;

// Redefined here temporarily.
//
typedef struct
{
    // This flag identifies if the data in the structure is valid. This is needed as all physical channels of
    // a single interface driver share same IRPs.
    //
    BOOLEAN IsValid;
    // Data value.
    //
    INT32 DataValue;
} SMFSOCPLUGIN_DATA;

// Enumeration used to expose Sensor Channels to SMF.
// Only one channel - SmfSoCSensorChannelCpuTemperatureAverageId -  is used for control, the others are optional and only for reporting.
//
typedef enum
{
    SmfSoCSensorChannelInvalidInstanceId = 0,
    // These sensors will be used to query CPU and System energy counter.
    // Channel type is SMF_SOC_ENERGY_INPUT_TYPE.
    //
    SmfSoCSensorChannelSocEnergyInputCpuPowerId = 1,
    SmfSoCSensorChannelSocEnergyInputSystemPowerId = 2,
    // This sensor will be used to query Prochot status.
    // Channel type is SMF_SOC_POWER_INPUT_TYPE.
    //
    SmfSoCSensorChannelSocPowerInputProcHotStatusId = 4,
    // These type contains the version number of the module.
    // Channel type is SMF_GENERIC_TYPE.
    //
    SmfSoCSensorChannelModuleVersion = 115,
    // This sensor will be used to calculate temperature average and will be used
    // for active cooling controls. 
    // Channel type is SMF_GENERIC_TYPE.
    //
    SmfSoCSensorChannelCpuTemperatureAverageId = 112,
    // These sensors are reported as is to the framework.
    // Channel type is SMF_TEMPERATURE_SENSOR_TYPE.
    //
    SmfSoCSensorChannelCpuTemperatureId =  102,
    SmfSoCSensorChannelCpuTemperature0Id = 180,
    SmfSoCSensorChannelCpuTemperature1Id = 181,
    SmfSoCSensorChannelCpuTemperature2Id = 182,
    SmfSoCSensorChannelCpuTemperature3Id = 183,
    SmfSoCSensorChannelCpuTemperature4Id = 184,
    SmfSoCSensorChannelCpuTemperature5Id = 185,
    SmfSoCSensorChannelCpuTemperature6Id = 186,
    SmfSoCSensorChannelCpuTemperature7Id = 187,
    SmfSoCSensorChannelCpuTemperature8Id = 188,
    SmfSoCSensorChannelCpuTemperature9Id = 189,
    // These sensors are reported as is to the framework.
    // Channel type is SMF_SILICON_TELEMETRY_INPUT_TYPE.
    //
    SmfSoCSensorChanneCpuTelemetryBitsId = 199,
    SmfSoCSensorChanneGpuTelemetryBitsId = 299
} SMFSOC_SENSORCHANNEL;

// Enumeration used to expose Control Channels to SMF.
// 
// Only a few of these channels are used as there are many ways to control the system. 
// Set to start from is:
//      SmfSoCControlChannelCpuAverageInstanceId (thermal control)
//      SmfSoCControlChannelCpuMaximumInstanceId (performance control)
//      SmfSoCControlChannelCpuPeakInstanceId (brownout control)
//      SmfSoCBatteryModeInstanceId (if SOC has an IO to force it to minimum power state)
//
typedef enum
{
    // These channels are minimum and maximum frequency control channels for SOC.
    // Channel type is SMF_FREQUENCY_LIMIT_CONTROL_TYPE.
    //
    SmfSoCControlChannelCpuFrequencyPeakInstanceId = 1,
    SmfSoCControlChannelCpuFrequencyMinInstanceId = 2,
    // These channels are minimum enable channels for SOC frequency. Usage of these channels can be defined on per SOC basis.
    // Channel type is SMF_FREQUENCY_LIMIT_CONTROL_TYPE.
    //
    SmfSoCControlChannelCpuFrequencyMinEnable1InstanceId = 3,
    SmfSoCControlChannelCpuFrequencyMinEnable2InstanceId = 4,
    // This is a SOC sustained power limit and time constant for calculations. Optional secondary control is marked with 2 in the name.
    // Channel types are SMF_POWER_LIMIT_CONTROL_TYPE and SMF_TIME_LIMIT_CONTROL_OUTPUT_TYPE.
    //
    SmfSoCControlChannelCpuAverageInstanceId = 51,
    SmfSoCControlChannelCpuAverage2InstanceId = 56,
    SmfSoCControlChannelAverageTimeInstanceId = 61,
    SmfSoCControlChannelAverage2TimeInstanceId = 62,
    // This is a SOC maximum power control and duration for short term higher performance. Optional secondary control is marked with 2 in the name.
    // Channel type is SMF_POWER_LIMIT_CONTROL_TYPE.
    //
    SmfSoCControlChannelCpuMaximumInstanceId = 52,
    SmfSoCControlChannelCpuMaximum2InstanceId = 57,
    // This is a pre-emptive SOC peak power which is not allowed to be exceeded - not even for milliseconds.
    // Channel type is SMF_POWER_LIMIT_CONTROL_TYPE.
    //
    SmfSoCControlChannelCpuPeakInstanceId = 53,
    // System power limit controls.
    //      Average: CPU will throttle to meet system power limits. 
    //      Time   : CPU time coefficient that can be used for estimating system power. 
    //      Maximum: This is a system level maximum power control and duration for short term higher performance.
    //      Peak   : This is the system peak power lmit which can't be exceeded even for milliseconds. 
    // Channel types are SMF_POWER_LIMIT_CONTROL_TYPE and SMF_TIME_LIMIT_CONTROL_OUTPUT_TYPE.
    //
    SmfSoCControlChannelSystemAverageInstanceId = 54,
    SmfSoCControlChannelSystemAverageTimeInstanceId = 63,
    SmfSoCControlChannelSystemMaximumInstanceId = 55,
    SmfSoCControlChannelSystemPeakInstanceId = 59,
    // This sets the maximum SOC temperature. Time coefficient can be used for dynamic behavior calculations.
    // Channel type is SMF_TIME_LIMIT_CONTROL_OUTPUT_TYPE.
    //
    SmfSoCControlChannelCpuTcInstanceId = 11,
    SmfSoCControlChannelCpuTcTimeInstanceId = 64,
    // Virtual control channels. These channels are not used for SOC control but are used by SMF to notify external state changes to transport module. 
    //      Battery: This is an indicator that states that all controls have been reconfigured to support DC mode after switching from AC mode.
    //      Minimum: This is an indicator that states that SMF has throttled SOC to minimum power levels. This channel is optional.
    // Channel type is SMF_POWER_LIMIT_CONTROL_TYPE.
    //
    SmfSoCBatteryModeInstanceId = 70,
    SmfSoCControlChannelMinimumPowerInstanceId = 71,
} SMFSOC_CONTROLCHANNEL;

// Enumeration for SAM/EC communication.
//
typedef enum
{
    // Default result for control access. This means that no communication is required towards MCU.
    //
    NoCommunicationNeeded,
    // "Processor Hot"-signal removal control. 
    // This is used when SmfSoCBatteryModeInstanceId-channel indicates that reconfiguration is complete.
    //
    ClearProcessorHotSignal,
} SMFSOCPLUGIN_SAM_EC_COMMUNICATION;

// Enumeration for the transport to send operation information to protocol.
//
typedef enum
{
    ProtocolNotifyOperation_Invalid = 0,
    // This indicates the data variable contains channel information.
    //
    ProtocolNotifyOperation_ChannelData,
    // This indicates the data variable contains SAM reset reason.
    //
    ProtocolNotifyOperation_SamResetReasonTelemetry,
    // This indicates the data variable contains SoC reset reason.
    //
    ProtocolNotifyOperation_SocResetReasonTelemetry,
    ProtocolNotifyOperation_Max
} SMFSOC_PROTOCOL_NOTIFY_OPERATION;

// Bind Time Data.
// ---------------
//
// Data provided by the Protocol Module.
//
typedef struct _DMF_INTERFACE_PROTOCOL_SystemManagementFramework_BIND_DATA
{
    // Stores this Protocol's Id.
    //
    ULONG ProtocolId;
} DMF_INTERFACE_PROTOCOL_SystemManagementFramework_BIND_DATA;

// Data provided by the Transport Module.
//
typedef struct _DMF_INTERFACE_TRANSPORT_SystemManagementFramework_BIND_DATA
{
    // Stores this Transport's Id.
    //
    ULONG TransportId;
} DMF_INTERFACE_TRANSPORT_SystemManagementFramework_BIND_DATA;

// Declaration Time Data.
// ----------------------
//
// Callbacks provided by Protocol Module.
//

// Callback to indicate information that can be reported to protocol Module.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_SystemManagementFramework_ProtocolNotify(
    _In_ DMFINTERFACE DmfInterface,
    _In_ SMFSOC_PROTOCOL_NOTIFY_OPERATION Operation,
    _In_ USHORT Channel,
    _In_ VOID* Data,
    _In_ size_t DataSize
    );

// Data that fully describes this Protocol.
//
typedef struct _DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA
{
    // The Protocol Interface Descriptor. 
    // Every Interface must have this as the first member of its Protocol Declaration Data.
    //
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR DmfProtocolDescriptor;
    // Stores callbacks implemented by this Interface Protocol.
    //
    EVT_DMF_INTERFACE_SystemManagementFramework_ProtocolNotify* EVT_SystemManagementFramework_ProtocolNotify;
} DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA;

// Methods used to initialize Protocol's Declaration Data.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA* ProtocolDeclarationData,
    _In_ EVT_DMF_INTERFACE_ProtocolBind* EvtProtocolBind,
    _In_ EVT_DMF_INTERFACE_ProtocolUnbind* EvtProtocolUnbind,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_opt_ EVT_DMF_INTERFACE_SystemManagementFramework_ProtocolNotify* EvtSystemManagementFramework_ProtocolNotify
    );

// Methods provided by Transport Module.
//
// Bind
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_SystemManagementFramework_BIND_DATA* ProtocolBindData,
    _Inout_opt_ DMF_INTERFACE_TRANSPORT_SystemManagementFramework_BIND_DATA* TransportBindData
    );

// Unbind
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_INTERFACE_SystemManagementFramework_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    );

/*++

Routine Description:

    Callback exposed via Transport Message to parent Module.
    Returns the number of Sensor and Control channels.

Arguments:

    DmfInterface - This Module's interface handle.
    NumberOfSensorChannels - Number of channels going to SMF Core.
    NumberOfControlChannels - Number of channels coming from SMF Core.
    VersionChannelIndex - Index of the channel that provides the version information. If -1 is given, no version channel exists.

Return Value:

   NTSTATUS

--*/
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_ChannelsGet(
    _In_ DMFINTERFACE DmfInterface,
    _Out_ USHORT* NumberOfSensorChannels,
    _Out_ USHORT* NumberOfControlChannels,
    _Out_ SHORT* VersionChannelIndex
    );

/*++

Routine Description:

    Callback exposed via Transport Message to parent Module.
    Initializes SoC related resources and populates SMF Capabilities.

Arguments:

    DmfInterface - This Module's interface handle.
    Capabilities - Interface capabilities.
    LimitEnable - Limit enable flag.

Return Value:

   NTSTATUS

--*/
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportInitialize(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_ SMFSOCPLUGIN_CAPABILITIES* Capabilities,
    _Inout_ BOOLEAN* LimitEnable
    );

/*++

Routine Description:

    Callback exposed via Transport Message to parent Module.
    Releases any hardware context required to support SMF.

Arguments:

    DmfInterface - This Module's interface handle.

Return Value:

    NTSTATUS

--*/
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportUninitialize(
    _In_ DMFINTERFACE DmfInterface
    );

/*++

Routine Description:

    Callback exposed via Transport Message to parent Module.
    Sets a new value on an output channel.

Arguments:

    DmfInterface - This Module's interface handle.
    ChannelIndex - Index of the output channel.
    ControlData - New value to be set on the power control.
    SamNotification - Pointer to SAM Notification output.

Return Value:

   NTSTATUS

--*/
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportControlSet(
    _In_ DMFINTERFACE DmfInterface,
    _In_ SHORT ChannelIndex,
    _In_ INT32 ControlData,
    _Out_ UINT8* SamNotification
    );

/*++

Routine Description:

    Callback exposed via Transport Message to parent Module.
    This Method provides the current reading for the selected sensor channel.

Arguments:

    DmfInterface - This Module's interface handle.
    ChannelIndex - Index of the Channel to be updated with new configuration.
    SensorData - Smf Interface Data structure to be updated with the current sensor value.

Return Value:

   NTSTATUS

--*/
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportDataGet(
    _In_ DMFINTERFACE DmfInterface,
    _In_ SHORT ChannelIndex,
    _Out_ INT32* SensorData
    );

/*++

Routine Description:

    Callback exposed via Transport Message to parent Module.
    Gets the reset cause from the Cpu.

Arguments:

    DmfInterface - This Module's interface handle.
    Data - Reset cause.
    Size - Size of data.

Return Value:

   NTSTATUS

--*/
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_INTERFACE_SystemManagementFramework_TransportResetCauseGet(
    _In_ DMFINTERFACE DmfInterface,
    _Out_ VOID* Data,
    _In_ UINT16 Size
    );

// Data that fully describes this Transport.
//
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
} DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA;

// Methods used to initialize Transport's Declaration Data.
//
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA* TransportDeclarationData,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ DMF_INTERFACE_SystemManagementFramework_TransportBind* SystemManagementFrameworkTransportBind,
    _In_ DMF_INTERFACE_SystemManagementFramework_TransportUnbind* SystemManagementFrameworkTransportUnbind,
    _In_opt_ DMF_INTERFACE_SystemManagementFramework_ChannelsGet* SystemManagementFrameworkChannelsGet,
    _In_opt_ DMF_INTERFACE_SystemManagementFramework_TransportInitialize* SystemManagementFrameworkTransportInitialize,
    _In_opt_ DMF_INTERFACE_SystemManagementFramework_TransportUninitialize* SystemManagementFrameworkTransportUninitialize,
    _In_opt_ DMF_INTERFACE_SystemManagementFramework_TransportControlSet* SystemManagementFrameworkTransportControlSet,
    _In_opt_ DMF_INTERFACE_SystemManagementFramework_TransportDataGet* SystemManagementFrameworkTransportDataGet,
    _In_opt_ DMF_INTERFACE_SystemManagementFramework_TransportResetCauseGet* SystemManagementFrameworkTransportResetCauseGet
    );

// Methods exposed to Protocol.
// ----------------------------
//
DMF_INTERFACE_SystemManagementFramework_TransportBind DMF_SystemManagementFramework_TransportBind;
DMF_INTERFACE_SystemManagementFramework_TransportUnbind DMF_SystemManagementFramework_TransportUnbind;
DMF_INTERFACE_SystemManagementFramework_ChannelsGet DMF_SystemManagementFramework_ChannelsGet;
DMF_INTERFACE_SystemManagementFramework_TransportInitialize DMF_SystemManagementFramework_TransportInitialize;
DMF_INTERFACE_SystemManagementFramework_TransportUninitialize DMF_SystemManagementFramework_TransportUninitialize;
DMF_INTERFACE_SystemManagementFramework_TransportControlSet DMF_SystemManagementFramework_TransportControlSet;
DMF_INTERFACE_SystemManagementFramework_TransportDataGet DMF_SystemManagementFramework_TransportDataGet;
DMF_INTERFACE_SystemManagementFramework_TransportResetCauseGet DMF_SystemManagementFramework_TransportResetCauseGet;

// Callbacks exposed to Transport.
// -------------------------------
//
EVT_DMF_INTERFACE_SystemManagementFramework_ProtocolNotify EVT_SystemManagementFramework_ProtocolNotify;

// This macro defines the SystemManagementFrameworkProtocolDeclarationDataGet and SystemManagementFrameworkTransportDeclarationDataGet.
// Call this macro after the DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA and
// DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA are defined.
//
DECLARE_DMF_INTERFACE(SystemManagementFramework);

// eof: Dmf_Interface_SystemManagementFramework.h
//