/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_SampleInterfaceUpperProtocol.c

Abstract:

    This is an example DMF Protocol Module. It is called "Upper" to make the example
    clear, but there is nothing special that makes it "upper". Any Client can instantiate
    this Module.

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
#include "Dmf_SampleInterfaceUpperProtocol.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_SampleInterfaceUpperProtocol
{
    // Stores the DMF Module of the Transport this Module is bound to.
    //
    DMFINTERFACE SampleInterfaceHandle;
} DMF_CONTEXT_SampleInterfaceUpperProtocol;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(SampleInterfaceUpperProtocol)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(SampleInterfaceUpperProtocol)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Private context the Protocol Module associates with an Interface.
//
typedef struct _DMF_INTERFACE_UPPERPROTOCOL1_CONTEXT
{
    // Stores the Id of the Transport Module.
    //
    ULONG TransportId;
} DMF_INTERFACE_UPPERPROTOCOL1_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DMF_INTERFACE_UPPERPROTOCOL1_CONTEXT, DMF_SampleInterfaceUpperProtocolContextGet)

// Protocol Specific Callbacks
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_SampleInterfaceUpperProtocol_Callback1(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Sample Protocol Callback.

Arguments:

    ProtocolModule - This Module's handle. (The given Protocol Module).
    TransportModule - The given Transport Module.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE protocolModule;
    DMF_CONFIG_SampleInterfaceUpperProtocol* moduleConfig;
    DMF_INTERFACE_UPPERPROTOCOL1_CONTEXT* protocolContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    protocolModule = DMF_InterfaceProtocolModuleGet(DmfInterface);
    moduleConfig = DMF_CONFIG_GET(protocolModule);

    // Get the Protocol's Private Context associated with this connection.
    //
    protocolContext = DMF_SampleInterfaceUpperProtocolContextGet(DmfInterface);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                "SampleInterface TestCallback1: ProtocolId=%d ProtocolName=%s TransportId=%d ntStatus=%!STATUS!",
                moduleConfig->ModuleId,
                moduleConfig->ModuleName,
                protocolContext->TransportId,
                ntStatus);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(DMF_ModuleD0Entry)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_SampleInterfaceUpperProtocol_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    SampleInterfaceUpperProtocol callback for ModuleD0Entry for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    PreviousState - The WDF Power State that the given DMF Module should exit from.

Return Value:

   NTSTATUS of either the given DMF Module's Open Callback or STATUS_SUCCESS.

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Function_class_(DMF_ModuleD0Exit)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_SampleInterfaceUpperProtocol_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
/*++

Routine Description:

    SampleInterfaceUpperProtocol callback for ModuleD0Exit for a given DMF Module.

Arguments:

    DmfModule - This Module's handle.
    TargetState - The WDF Power State that the given DMF Module will enter.

Return Value:

    None

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

// Protocol Generic Callbacks.
// (Implementation of publicly accessible callbacks required by the Interface.)
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_SampleInterfaceUpperProtocol_PostBind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    This callback tells the given Protocol Module that it is bound to the given
    Transport Module.

Arguments:

    DmfModule - This Module's handle. (The given Protocol Module).
    TransportModule - The given Transport Module.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfInterface);

    FuncEntry(DMF_TRACE);

    // If the Protocol requires the Transport to allocate resources, send a message
    // to the Transport to allocate resources.
    //

    // It is now possible to use Methods provided by the Transport.
    //

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_SampleInterfaceUpperProtocol_PreUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    This callback tells the given Protocol Module that it is about to be unbound from
    the given Transport Module.

Arguments:

    DmfModule - This Module's handle. (The given Protocol Module).
    TransportModule - The given Transport Module.

Return Value:

    None

--*/
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfInterface);

    FuncEntry(DMF_TRACE);

    // If Protocol requested Transport to allocate resources, send a message to free those resources.
    //

    // Stop using Methods provided by Transport after this callback completes (except for Unbind).
    //

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SampleInterfaceUpperProtocol_Bind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Binds the given Protocol Module to the given Transport Module.

Arguments:

    ProtocolModule - This Module's handle. (The given Protocol Module).
    TransportModule - The given Transport Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_PROTOCOL_SampleInterfaceUpper_BIND_DATA protocolBindData;
    DMF_INTERFACE_TRANSPORT_SampleInterfaceUpper_BIND_DATA transportBindData;
    DMF_CONTEXT_SampleInterfaceUpperProtocol* moduleContext;
    DMF_CONFIG_SampleInterfaceUpperProtocol* moduleConfig;
    DMF_INTERFACE_UPPERPROTOCOL1_CONTEXT* protocolContext;
    DMFMODULE protocolModule;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    protocolModule = DMF_InterfaceProtocolModuleGet(DmfInterface);

    moduleContext = DMF_CONTEXT_GET(protocolModule);
    moduleConfig = DMF_CONFIG_GET(protocolModule);


    // Populate the Protocol Bind Data structure that the Protocol wants to share with the Transport.
    //
    protocolBindData.ProtocolId = moduleConfig->ModuleId;

    // Call the Interface's Bind function.
    //
    ntStatus = DMF_SampleInterfaceUpper_TransportBind(DmfInterface,
                                                 &protocolBindData,
                                                 &transportBindData);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_SampleInterfaceUpper_TransportBind fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Save the Interface handle representing the interface binding.
    //
    moduleContext->SampleInterfaceHandle = DmfInterface;

    protocolContext = DMF_SampleInterfaceUpperProtocolContextGet(DmfInterface);
    protocolContext->TransportId = transportBindData.TransportId;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_SampleInterfaceUpperProtocol_Bind success");

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_SampleInterfaceUpperProtocol_Unbind(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Unbinds the given Protocol Module from the given Transport Module.

Arguments:

    ProtocolModule - This Module's handle. (The given Protocol Module).
    TransportModule - The given Transport Module.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Call the Interface's Unbind function.
    //
    DMF_SampleInterfaceUpper_TransportUnbind(DmfInterface);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_SampleInterfaceUpperProtocol_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type SampleInterfaceUpperProtocol.

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
DMF_SampleInterfaceUpperProtocol_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type SampleInterfaceUpperProtocol.

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

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Protocol
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SampleInterfaceUpperProtocol_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type SampleInterfaceUpperProtocol.

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
    DMF_INTERFACE_PROTOCOL_SampleInterfaceUpper_DECLARATION_DATA protocolDeclarationData;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_SampleInterfaceUpperProtocol;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_SampleInterfaceUpperProtocol;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_SampleInterfaceUpperProtocol;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_SampleInterfaceUpperProtocol);
    dmfCallbacksDmf_SampleInterfaceUpperProtocol.DeviceOpen = DMF_SampleInterfaceUpperProtocol_Open;
    dmfCallbacksDmf_SampleInterfaceUpperProtocol.DeviceClose = DMF_SampleInterfaceUpperProtocol_Close;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_SampleInterfaceUpperProtocol);
    dmfCallbacksWdf_SampleInterfaceUpperProtocol.ModuleD0Entry = DMF_SampleInterfaceUpperProtocol_ModuleD0Entry;
    dmfCallbacksWdf_SampleInterfaceUpperProtocol.ModuleD0Exit = DMF_SampleInterfaceUpperProtocol_ModuleD0Exit;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_SampleInterfaceUpperProtocol,
                                            SampleInterfaceUpperProtocol,
                                            DMF_CONTEXT_SampleInterfaceUpperProtocol,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_SampleInterfaceUpperProtocol.CallbacksDmf = &dmfCallbacksDmf_SampleInterfaceUpperProtocol;
    dmfModuleDescriptor_SampleInterfaceUpperProtocol.CallbacksWdf = &dmfCallbacksWdf_SampleInterfaceUpperProtocol;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_SampleInterfaceUpperProtocol,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Initialize Protocol's declaration data.
    //
    DMF_INTERFACE_PROTOCOL_SampleInterfaceUpper_DESCRIPTOR_INIT(&protocolDeclarationData,
                                                           DMF_SampleInterfaceUpperProtocol_Bind,
                                                           DMF_SampleInterfaceUpperProtocol_Unbind,
                                                           DMF_SampleInterfaceUpperProtocol_PostBind,
                                                           DMF_SampleInterfaceUpperProtocol_PreUnbind,
                                                           DMF_SampleInterfaceUpperProtocol_Callback1);

    // An optional context can be set by the Protocol module on the bind instance
    // This is a unique context for each instance of Protocol Transport binding. 
    // E.g. in case a protocol module is bound to multiple modules, the Protocol 
    // Module will get a unique instance of this context each binding. 
    // 
    DMF_INTERFACE_DESCRIPTOR_SET_CONTEXT_TYPE(&protocolDeclarationData, 
                                              DMF_INTERFACE_UPPERPROTOCOL1_CONTEXT);

    // Add the interface to the Protocol Module.
    //
    ntStatus = DMF_ModuleInterfaceDescriptorAdd(*DmfModule,
                                                (DMF_INTERFACE_DESCRIPTOR*)&protocolDeclarationData);
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

// Protocol Methods
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_SampleInterfaceUpperProtocol_TestMethod(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    A sample Method implemented by this Protocol that invokes the TransportMethod1 specified in the SampleInterface.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_SampleInterfaceUpperProtocol* moduleContext;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->SampleInterfaceHandle != NULL);

    // Call the Interface's Method1.
    //
    ntStatus = DMF_SampleInterfaceUpper_TransportMethod1(moduleContext->SampleInterfaceHandle);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_SampleInterfaceUpper_TransportMethod1 fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "DMF_SampleInterfaceUpper_TransportMethod1 success: ntStatus=%!STATUS!", ntStatus);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;

}
#pragma code_seg()

// eof: Dmf_SampleInterfaceUpperProtocol.c
//
