/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_NonPnp.c

Abstract:

    This Module is used by the NonPnp sample. There is nothing special about this Module that makes
    it work in a NonPnp driver. This Module just exposes and IOCTL to show how a NonPnp driver can
    handle an IOCTL in a Module.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_NonPnp.tmh"
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
    // Handles IOCTLs for NonPnp.
    //
    DMFMODULE DmfModuleIoctlHandler;
} DMF_CONTEXT_NonPnp;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(NonPnp)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_NO_CONFIG(NonPnp)

// Memory Pool Tag.
//
#define MemoryTag 'pnPN'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
static
NTSTATUS
NonPnp_IoctlHandler(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    This callback is called when the Child Module (Dmf_IoctlHandler) has validated the IOCTL and the
    input/output buffer sizes per the table supplied.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Queue - The WDFQUEUE associated with Request.
    Request - Request data, not used.
    IoctlCode - IOCTL that has been validated to be supported by this Module.
    InputBuffer - Input data buffer.
    InputBufferSize - Input data buffer size.
    OutputBuffer - Output data buffer.
    OutputBufferSize - Output data buffer size.
    BytesReturned - Amount of data to be sent back.

Returns:

    STATUS_PENDING - This Module owns the given Request. It will not be completed by the Child Module. This
                     Module must complete the request eventually.
    Any other NTSTATUS - The given request will be completed with this NTSTATUS.

--*/
{
    DMFMODULE dmfModuleNonPnp;
    size_t bytesReturned;
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(OutputBufferSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    //
    // DMF: A frequent programming DMF programming pattern is that callbacks made by DMF Modules
    //      pass the corresponding DMFMODULE handle. From that handle, it is possible to get the
    //      Client driver's WDFDEVICE and device context.
    //
    dmfModuleNonPnp = DMF_ParentModuleGet(DmfModule);

    bytesReturned = 0;

    switch(IoControlCode) 
    {
        case IOCTL_NonPnp_MESSAGE_TRANSFER: 
        {
            WCHAR bufferFromApplication[NonPnp_BufferSize + 1];
            WCHAR bufferToSendToApplication[] =  L"This is a buffer from the NonPnp Module.";

            RtlCopyMemory(bufferFromApplication,
                          InputBuffer,
                          InputBufferSize);
            // Make sure string is zero terminated.
            //
            bufferFromApplication[_countof(bufferFromApplication) - 1] = L'\0';
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Message from application: %S", bufferFromApplication);;

            // Send a message to the application.
            //
            RtlCopyMemory(OutputBuffer,
                          bufferToSendToApplication,
                          sizeof(bufferToSendToApplication));
            bytesReturned = (wcslen(bufferToSendToApplication) + 1) * sizeof(WCHAR);
            ntStatus = STATUS_SUCCESS;
            break;
        }
        default:
        {
            // Unnecessary because the Module does this. This is for SAL only.
            //
            ntStatus = STATUS_NOT_SUPPORTED;
            break;
        }
    }

    // DMF: Dmf_IoctlHandler will return this information with the request if it completes it.
    //      Local variable is not necessary, of course. It is left here to reduce changes.
    //
    *BytesReturned = bytesReturned;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// The table of IOCTLS that this Module supports.
//
IoctlHandler_IoctlRecord NonPnp_IoctlHandlerTable[] =
{
    { (LONG)IOCTL_NonPnp_MESSAGE_TRANSFER, NonPnp_BufferSize, NonPnp_BufferSize, NonPnp_IoctlHandler, FALSE },
};

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NonPnp_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONTEXT_NonPnp* moduleContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
    moduleConfigIoctlHandler.IoctlRecordCount = _countof(NonPnp_IoctlHandlerTable);
    moduleConfigIoctlHandler.IoctlRecords = NonPnp_IoctlHandlerTable;
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeDefault;
    moduleConfigIoctlHandler.IsRestricted = DEVPROP_TRUE;
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleIoctlHandler);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_NonPnp_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type NonPnp.

Arguments:

    DmfModule - This Module's handle.

Returns:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    DECLARE_CONST_UNICODE_STRING(symbolicLinkName,
                                 NonPnp_SymbolicLinkName);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = WdfDeviceCreateSymbolicLink(device,
                                           &symbolicLinkName);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceCreateSymbolicLink fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

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
DMF_NonPnp_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type NonPnp.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_NonPnp;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_NonPnp;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_NonPnp);
    dmfCallbacksDmf_NonPnp.ChildModulesAdd = DMF_NonPnp_ChildModulesAdd;
    dmfCallbacksDmf_NonPnp.DeviceOpen = DMF_NonPnp_Open;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_NonPnp,
                                            NonPnp,
                                            DMF_CONTEXT_NonPnp,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_NonPnp.CallbacksDmf = &dmfCallbacksDmf_NonPnp;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_NonPnp,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_NonPnp.c
//
