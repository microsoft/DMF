/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SystemManagementFramework.c

Abstract:

    Implements a SoC Interface Contract between SMF protocol and transport Module.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"
#include "Dmf_Interface_SystemManagementFramework.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Validation
///////////////////////////////////////////////////////////////////////////////////////////////////////
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
    )
/*++

Routine Description:

    Ensures all required callbacks are provided by Protocol Module and populates the Declaration Data structure.

Arguments:

    ProtocolDeclarationData - The Protocol's declaration data.
    EvtProtocolBind - The Bind callback. Must be provided by all Protocol Modules.
    EvtProtocolUnBind - The Unbind callback. Must be provided by all Protocol Modules.
    EvtPostBind - Optional Post Bind callback.
    EvtPreUnbind - Optional Pre Unbind callback.
    EvtSystemManagementFramework_ProtocolNotify - Callback for sending information to protocol Module.

Returns:

    None.
--*/
{
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR_INIT(&(ProtocolDeclarationData->DmfProtocolDescriptor),
                                           "SystemManagementFramework",
                                           DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA,
                                           EvtProtocolBind,
                                           EvtProtocolUnbind,
                                           EvtPostBind,
                                           EvtPreUnbind);

    ProtocolDeclarationData->EVT_SystemManagementFramework_ProtocolNotify = EvtSystemManagementFramework_ProtocolNotify;
}

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
    )
/*++

Routine Description:

    Ensures all required methods are provided by Transport Module and populates the Declaration Data structure.

Arguments:

    TransportDeclarationData - The Transport's declaration data.
    EvtPostBind - Optional Post bind callback.
    EvtPreUnbind - Optional Pre Unbind callback.
    SystemManagementFrameworkTransportBind - Transport's Bind method.
    SystemManagementFrameworkTransportUnbind - Transport's Unbind method.
    SystemManagementFrameworkChannelsGet - Transport's method for retrieving channels.
    SystemManagementFrameworkTransportInitialize - Transport's Initialize method.
    SystemManagementFrameworkTransportUninitialize - Transport's Uninitialize method.
    SystemManagementFrameworkTransportControlSet - Transport's method for setting control.
    SystemManagementFrameworkTransportDataGet - Transport's method for getting data.


Return Value:

    None.
--*/
{
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR_INIT(&(TransportDeclarationData->DmfTransportDescriptor),
                                            "SystemManagementFramework",
                                            DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA,
                                            EvtPostBind,
                                            EvtPreUnbind);

    TransportDeclarationData->DMF_SystemManagementFramework_TransportBind = SystemManagementFrameworkTransportBind;
    TransportDeclarationData->DMF_SystemManagementFramework_TransportUnbind = SystemManagementFrameworkTransportUnbind;
    TransportDeclarationData->DMF_SystemManagementFramework_ChannelsGet = SystemManagementFrameworkChannelsGet;
    TransportDeclarationData->DMF_SystemManagementFramework_TransportInitialize = SystemManagementFrameworkTransportInitialize;
    TransportDeclarationData->DMF_SystemManagementFramework_TransportUninitialize = SystemManagementFrameworkTransportUninitialize;
    TransportDeclarationData->DMF_SystemManagementFramework_TransportControlSet = SystemManagementFrameworkTransportControlSet;
    TransportDeclarationData->DMF_SystemManagementFramework_TransportDataGet = SystemManagementFrameworkTransportDataGet;
    TransportDeclarationData->DMF_SystemManagementFramework_TransportResetCauseGet = SystemManagementFrameworkTransportResetCauseGet;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Protocol Bind/Unbind
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

NTSTATUS
DMF_SystemManagementFramework_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_SystemManagementFramework_BIND_DATA* ProtocolBindData,
    _Inout_opt_ DMF_INTERFACE_TRANSPORT_SystemManagementFramework_BIND_DATA* TransportBindData
    )
/*++

Routine Description:

    Registers Protocol Module with the Transport Module. This is called by Protocol Module.

Arguments:

    DmfInterface - Interface handle.
    ProtocolBindData - Bind time data provided by Protocol to the Transport.
    TransportBindData - Bind time data provided by Transport to the Protocol.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_SystemManagementFramework_TransportBind");

    ntStatus = transportData->DMF_SystemManagementFramework_TransportBind(DmfInterface,
                                                                          ProtocolBindData, 
                                                                          TransportBindData);

    return ntStatus;
}

VOID
DMF_SystemManagementFramework_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Unregisters the given Protocol Module from the Transport Module. This is called by Protocol Module.

Arguments:

    DmfInterface - Interface handle.

Return Value:

    NTSTATUS

--*/
{
    DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    DmfAssert(transportData != NULL);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SystemManagementFramework_UnbindSystemManagementFramework_Unbind");

    transportData->DMF_SystemManagementFramework_TransportUnbind(DmfInterface);

    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Methods
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SystemManagementFramework_ChannelsGet(
    _In_ DMFINTERFACE DmfInterface,
    _Out_ USHORT* NumberOfSensorChannels,
    _Out_ USHORT* NumberOfControlChannels,
    _Out_ SHORT* VersionChannelIndex
    )
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
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_SystemManagementFramework_ChannelsGet");

    ntStatus = transportData->DMF_SystemManagementFramework_ChannelsGet(DmfInterface,
                                                                        NumberOfSensorChannels,
                                                                        NumberOfControlChannels, 
                                                                        VersionChannelIndex);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SystemManagementFramework_TransportInitialize(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_ SMFSOCPLUGIN_CAPABILITIES* Capabilities,
    _Inout_ BOOLEAN* LimitEnable
    )
/*++

Routine Description:

    Callback exposed via Transport Message to parent Module.
    Initializes SoC related resources and populates SMF Capabilities.

Arguments:

    DmfInterface - This Module's interface handle.
    Capabilities - Array of sensor/control channels supported by this SMF SOC interface.
    LimitEnable - Populated by the IHV Module to enable/disable channel limit enforcement.

Return Value:

   NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_SystemManagementFramework_TransportInitialize");

    ntStatus = transportData->DMF_SystemManagementFramework_TransportInitialize(DmfInterface,
                                                                                Capabilities,
                                                                                LimitEnable);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SystemManagementFramework_TransportUninitialize(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Callback exposed via Transport Message to parent Module.
    Releases any hardware context required to support SMF.

Arguments:

    DmfInterface - This Module's interface handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_SystemManagementFramework_TransportUninitialize");

    ntStatus = transportData->DMF_SystemManagementFramework_TransportUninitialize(DmfInterface);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SystemManagementFramework_TransportControlSet(
    _In_ DMFINTERFACE DmfInterface,
    _In_ SHORT ChannelIndex,
    _In_ INT32 ControlData,
    _Out_ UINT8* SamNotification
    )
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
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_SystemManagementFramework_TransportControlSet");

    ntStatus = transportData->DMF_SystemManagementFramework_TransportControlSet(DmfInterface,
                                                                                ChannelIndex,
                                                                                ControlData,
                                                                                SamNotification);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SystemManagementFramework_TransportDataGet(
    _In_ DMFINTERFACE DmfInterface,
    _In_ SHORT ChannelIndex,
    _Out_ INT32* SensorData
    )
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
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_SystemManagementFramework_TransportDataGet");

    ntStatus = transportData->DMF_SystemManagementFramework_TransportDataGet(DmfInterface,
                                                                             ChannelIndex,
                                                                             SensorData);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SystemManagementFramework_TransportResetCauseGet(
    _In_ DMFINTERFACE DmfInterface,
    _Out_ VOID* Data,
    _In_ UINT16 Size
    )
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
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA* transportData;

    transportData = (DMF_INTERFACE_TRANSPORT_SystemManagementFramework_DECLARATION_DATA*)DMF_InterfaceTransportDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_SystemManagementFramework_TransportResetCauseGet");

    ntStatus = transportData->DMF_SystemManagementFramework_TransportResetCauseGet(DmfInterface,
                                                                                   Data,
                                                                                   Size);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_SystemManagementFramework_ProtocolNotify(
    _In_ DMFINTERFACE DmfInterface,
    _In_ SMFSOC_PROTOCOL_NOTIFY_OPERATION Operation,
    _In_ USHORT Channel,
    _In_ VOID* Data,
    _In_ size_t DataSize
    )
/*++

Routine Description:

    Callback to indicate information that can be reported through telemetry.

Arguments:

    DmfInterface - Interface handle.
    Operation - Provides information on the type of data is sent from transport.
    Channel - Channel this operation is targeting.
    Data - Data to be reported.
    DataSize - Size of Data.

Return Value:

    None.

--*/
{
    DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA* protocolData;

    protocolData = (DMF_INTERFACE_PROTOCOL_SystemManagementFramework_DECLARATION_DATA*)DMF_InterfaceProtocolDeclarationDataGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "EVT_SystemManagementFramework_ProtocolNotify");

    protocolData->EVT_SystemManagementFramework_ProtocolNotify(DmfInterface,
                                                               Operation,
                                                               Channel,
                                                               Data,
                                                               DataSize);

    return;
}

// eof: Dmf_Interface_SystemManagementFramework.c
//
