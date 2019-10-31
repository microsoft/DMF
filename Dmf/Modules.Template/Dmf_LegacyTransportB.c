/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_LegacyTransportB.c

Abstract:

    LegacyTransportB a Sample Transport Module (instance "b").

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
// NOTE: If you are using a super set of the DMF Library, then use that Library's include file instead.
//       That include file will also include the Library include files of all Libraries the superset
//       depends on.
//
#include "DmfModule.h"
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"

#include "Dmf_LegacyTransportB.tmh"

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
    // TEMPLATE: Put data needed to support this DMF Module.
    //
    ULONG LegacyTransportB;
} DMF_CONTEXT_LegacyTransportB;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(LegacyTransportB)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(LegacyTransportB)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg()
_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
LegacyTransportB_PrintString(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* String
    )
/*++

Routine Description:

    Prints a given string to trace logger.

Arguments:

    DmfModule - This Module's handle.
    String - The given string.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "From Transport Instance \"B\": %ws", String);

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
DMF_LegacyTransportB_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    LegacyTransportB callback for ModuleD0Entry for a given DMF Module.

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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "From Transport Instance \"b\": PowerUp");

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_LegacyTransportB_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type LegacyTransportB.

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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "From Transport Instance \"b\": Open");

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_LegacyTransportB_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type LegacyTransportB.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_LegacyTransportB;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_LegacyTransportB;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_LegacyTransportB;
    DMF_ModuleTransportMethod DMF_LegacyTransportB_TransportMethod;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_LegacyTransportB);
    dmfCallbacksDmf_LegacyTransportB.DeviceOpen = DMF_LegacyTransportB_Open;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_LegacyTransportB);
    dmfCallbacksWdf_LegacyTransportB.ModuleD0Entry = DMF_LegacyTransportB_ModuleD0Entry;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_LegacyTransportB,
                                            LegacyTransportB,
                                            DMF_CONTEXT_LegacyTransportB,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_LegacyTransportB.CallbacksDmf = &dmfCallbacksDmf_LegacyTransportB;
    dmfModuleDescriptor_LegacyTransportB.CallbacksWdf = &dmfCallbacksWdf_LegacyTransportB;

    // NOTE: This is only used for Transport Modules.
    //
    dmfModuleDescriptor_LegacyTransportB.ModuleTransportMethod = DMF_LegacyTransportB_TransportMethod;
    dmfModuleDescriptor_LegacyTransportB.SupportedTransportInterfaceGuid = LegacyProtocol_Interface_Guid;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_LegacyTransportB,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// NOTE: It is still possible to define Module Methods. Those can be called by the Transport Method.
//       Thus, it is possible to use any Module as a Transport Module as long as it has a Transport Method.
//
NTSTATUS
DMF_LegacyTransportB_TransportMethod(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG Message,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize
    )
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    DMF_ObjectValidate(DmfModule);

    ntStatus = STATUS_SUCCESS;

    switch (Message)
    {
        case LegacyProtocol_TransportMessage_StringPrint:
        {
            WCHAR* string;

            string = (WCHAR*)InputBuffer;
            LegacyTransportB_PrintString(DmfModule, string);
            break;
        }
        default:
        {
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }
    }

    return ntStatus;
}

// eof: Dmf_LegacyTransportB.c
//
