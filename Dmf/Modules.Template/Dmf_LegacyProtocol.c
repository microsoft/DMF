/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_LegacyProtocol.c

Abstract:

    LegacyProtocol is a sample Protocol Module using the Legacy DMF Protocol/Transport support.

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

#include "Dmf_LegacyProtocol.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This Module has no private context.
// NOTE: There is nothing about a Protocol Module that prevents the use of a private context. The
// lack of context is for demonstration purposes.
//
DMF_MODULE_DECLARE_NO_CONTEXT(LegacyProtocol)

// This Module has no Config.
// NOTE: There is nothing about a Protocol Module that prevents the use of a Config. The
// lack of context is for demonstration purposes.
//
DMF_MODULE_DECLARE_NO_CONFIG(LegacyProtocol)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_LegacyProtocol_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type LegacyProtocol.

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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_LegacyProtocol_Open() executes");

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_LegacyProtocol;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_LegacyProtocol;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_LegacyProtocol_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type LegacyProtocol.

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

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_LegacyProtocol);
    DmfCallbacksDmf_LegacyProtocol.DeviceOpen = DMF_LegacyProtocol_Open;

    DMF_MODULE_DESCRIPTOR_INIT(DmfModuleDescriptor_LegacyProtocol,
                               LegacyProtocol,
                               DMF_MODULE_OPTIONS_PASSIVE |
                                   DMF_MODULE_OPTIONS_TRANSPORT_REQUIRED,
                               DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_LegacyProtocol.CallbacksDmf = &DmfCallbacksDmf_LegacyProtocol;
    DmfModuleDescriptor_LegacyProtocol.RequiredTransportInterfaceGuid = LegacyProtocol_Interface_Guid;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_LegacyProtocol,
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

VOID
DMF_LegacyProtocol_StringDisplay(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* String
    )
/*++

Routine Description:

    Outputs a given string in the Traceview log. This Method simply calls the underlying Transport
    Module to actually output the string. To do so, this Method calls a DMF function that retrieves
    the underlying Transport's Module. Then, it calls its Transport Method with the same parameters
    it receives.

Arguments:

    DmfModule - This Module's handle.
    String - The given string to display.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_LegacyProtocol);

    ntStatus = DMF_ModuleTransportCall(DmfModule,
                                       LegacyProtocol_TransportMessage_StringPrint,
                                       (VOID*)String,
                                       0,
                                       NULL,
                                       0);

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_LegacyProtocol.c
//
