/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterfaceTransport1.c

Abstract:

    Interface Transport (1) for "Sample Interface".

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_SampleInterfaceTransport1.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Stores the DMF Module of the Protocol this Module is bound to.
    //
    DMFINTERFACE SampleInterfaceHandle;
} DMF_CONTEXT_SampleInterfaceTransport1;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(SampleInterfaceTransport1)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(SampleInterfaceTransport1)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Private context the Protocol Module associates with an Interface.
//
typedef struct _DMF_INTERFACE_TRANSPORT1_CONTEXT
{
    // Stores the Id of the Protocol Module.
    //
    ULONG ProtocolId;
}DMF_INTERFACE_TRANSPORT1_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DMF_INTERFACE_TRANSPORT1_CONTEXT, DMF_SampleInterfaceTransport1ContextGet)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(DMF_ModuleD0Entry)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_SampleInterfaceTransport1_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    SampleInterfaceTransport1 callback for ModuleD0Entry for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(PreviousState);
    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
 
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Function_class_(DMF_ModuleD0Exit)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_SampleInterfaceTransport1_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    SampleInterfaceTransport1 callback for ModuleD0Exit for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(TargetState);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Transport Generic Callbacks.
// (Implementation of publicly accessible callbacks required by the Interface.)
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_SampleInterfaceTransport1_PostBind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    This callback tells the given Transport Module that it is bound to the given
    Protocol Module.

Arguments:

    DmfModule - This Module's handle. (The given Transport Module).
    ProtocolModule - The given Protocol Module.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfInterface);

    FuncEntry(DMF_TRACE);

    // It is now possible to use Methods provided by the Protocol.
    //

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_SampleInterfaceTransport1_PreUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    This callback tells the given Transport Module that it is about to be unbound from
    the given Protocol Module.

Arguments:

    DmfModule - This Module's handle. (The given Transport Module).
    ProtocolModule - The given Transport Module.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfInterface);

    FuncEntry(DMF_TRACE);

    // Free any resources allocated during Bind.
    //

    // Stop using Methods provided by Protocol after this callback completes (except for Unbind).
    //

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SampleInterfaceTransport1_Bind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_SampleInterface_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_SampleInterface_BIND_DATA* TransportBindData
    )
/*++

Routine Description:

    Binds the given Transport Module to the given Protocol Module.

Arguments:

    TransportModule - The given Transport Module.
    ProtocolModule - This Module's handle. (The given Protocol Module).
    ProtocolBindData - Bind data provided by Protocol for the Transport.
    TransportBindData - Bind data provided by Transport for the Protocol.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE transportModule;
    DMF_CONTEXT_SampleInterfaceTransport1* moduleContext;
    DMF_INTERFACE_TRANSPORT1_CONTEXT* transportContext;
    DMF_CONFIG_SampleInterfaceTransport1* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(ProtocolBindData);

    ntStatus = STATUS_SUCCESS;
    transportModule = DMF_InterfaceTransportModuleGet(DmfInterface);
    moduleContext = DMF_CONTEXT_GET(transportModule);
    moduleConfig = DMF_CONFIG_GET(transportModule);

    // Save the Bind Data provided by the Protocol in Transport1's Context
    // associated with this Protocol.
    //
    transportContext = DMF_SampleInterfaceTransport1ContextGet(DmfInterface);
    transportContext->ProtocolId = ProtocolBindData->ProtocolId;

    // Save the Interface Handle representing the Interface binding.
    //
    moduleContext->SampleInterfaceHandle = DmfInterface;

    // Populate the Transport Bind Data structure that the Protocol is requesting for.
    //
    TransportBindData->TransportId = moduleConfig->ModuleId;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_INTERFACE_TRANSPORT_SampleInterfaceTransport1_Bind success");

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_SampleInterfaceTransport1_Unbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Unbinds the given Transport Module from the given Protocol Module.

Arguments:

    TransportModule - The given Transport Module.
    ProtocolModule - This Module's handle. (The given Protocol Module).

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfInterface);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_INTERFACE_PROTOCOL_SampleInterfaceTransport1_Unbind success");

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_SampleInterfaceTransport1_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type SampleInterfaceTransport1.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_SampleInterfaceTransport1_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type SampleInterfaceTransport1.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

// Interface Specific Transport Module Methods
//
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SampleInterfaceTransport1_Method1(
    _In_ DMFINTERFACE DmfInterface
    )
{
    NTSTATUS ntStatus;
    DMFMODULE transportModule;
    DMF_CONFIG_SampleInterfaceTransport1* moduleConfig;
    DMF_INTERFACE_TRANSPORT1_CONTEXT* transportContext;

    PAGED_CODE()

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    transportModule = DMF_InterfaceTransportModuleGet(DmfInterface);
    moduleConfig = DMF_CONFIG_GET(transportModule);

    transportContext = DMF_SampleInterfaceTransport1ContextGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                "SampleInterface Method1: TransportId=%d TransportName=%s ProtocolId=%d ntStatus=%!STATUS!",
                moduleConfig->ModuleId,
                moduleConfig->ModuleName,
                transportContext->ProtocolId,
                ntStatus);

    EVT_SampleInterface_ProtocolCallback1(DmfInterface);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Protocol
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SampleInterfaceTransport1_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type SampleInterfaceTransport1.

Arguments:

    Device - Protocol driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA transportDeclarationData;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_SampleInterfaceTransport1;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_SampleInterfaceTransport1;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_SampleInterfaceTransport1;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_SampleInterfaceTransport1);
    dmfCallbacksDmf_SampleInterfaceTransport1.DeviceOpen = DMF_SampleInterfaceTransport1_Open;
    dmfCallbacksDmf_SampleInterfaceTransport1.DeviceClose = DMF_SampleInterfaceTransport1_Close;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_SampleInterfaceTransport1);
    dmfCallbacksWdf_SampleInterfaceTransport1.ModuleD0Entry = DMF_SampleInterfaceTransport1_ModuleD0Entry;
    dmfCallbacksWdf_SampleInterfaceTransport1.ModuleD0Exit = DMF_SampleInterfaceTransport1_ModuleD0Exit;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_SampleInterfaceTransport1,
                                            SampleInterfaceTransport1,
                                            DMF_CONTEXT_SampleInterfaceTransport1,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_SampleInterfaceTransport1.CallbacksDmf = &dmfCallbacksDmf_SampleInterfaceTransport1;
    dmfModuleDescriptor_SampleInterfaceTransport1.CallbacksWdf = &dmfCallbacksWdf_SampleInterfaceTransport1;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_SampleInterfaceTransport1,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Initialize the Transport Declaration Data.
    //
    DMF_INTERFACE_TRANSPORT_SampleInterface_DESCRIPTOR_INIT(&transportDeclarationData,
                                                            DMF_SampleInterfaceTransport1_PostBind,
                                                            DMF_SampleInterfaceTransport1_PreUnbind,
                                                            DMF_SampleInterfaceTransport1_Bind,
                                                            DMF_SampleInterfaceTransport1_Unbind,
                                                            DMF_SampleInterfaceTransport1_Method1);

    
    // An optional context can be set by the Transport module on the bind instance.
    // This is a unique context for each instance of Protocol Transport binding. 
    // E.g. in case a transport module is bound to multiple protocol modules, the transport 
    // module will get a unique instance of this context each binding. 
    // 
    DMF_INTERFACE_DESCRIPTOR_SET_CONTEXT_TYPE(&transportDeclarationData, 
                                              DMF_INTERFACE_TRANSPORT1_CONTEXT);

    // Add the interface to the Transport Module.
    //
    ntStatus = DMF_ModuleInterfaceDescriptorAdd(*DmfModule,
                                                (DMF_INTERFACE_DESCRIPTOR*)&transportDeclarationData);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleInterfaceDescriptorAdd fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// eof: Dmf_SampleInterfaceTransport1.c
//
