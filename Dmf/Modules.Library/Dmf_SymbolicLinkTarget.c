/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SymbolicLinkTarget.c

Abstract:

    Creates a stream of asynchronous requests to a Serial IO Target. Also, there is support
    for sending synchronous requests to the same IO Target.

Environment:

    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_SymbolicLinkTarget.tmh"
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
    // Underlying Device Target.
    //
    HANDLE IoTarget;
} DMF_CONTEXT_SymbolicLinkTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(SymbolicLinkTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(SymbolicLinkTarget)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Destroy the Device IoTarget.
//
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
SymbolicLinkTarget_IoTargetDestroy(
    _Inout_ DMF_CONTEXT_SymbolicLinkTarget* ModuleContext
    )
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    if (ModuleContext->IoTarget != NULL)
    {
        CloseHandle(ModuleContext->IoTarget);
        ModuleContext->IoTarget = NULL;
    }

    FuncExitVoid(DMF_TRACE);
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

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_SymbolicLinkTarget_Open(
    _In_ DMFMODULE DmfModule
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SymbolicLinkTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleContext->IoTarget = NULL;

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
DMF_SymbolicLinkTarget_Close(
    _In_ DMFMODULE DmfModule
    )
{
    DMF_CONTEXT_SymbolicLinkTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Close the associated target.
    //
    SymbolicLinkTarget_IoTargetDestroy(moduleContext);

    FuncExitVoid(DMF_TRACE);
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
DMF_SymbolicLinkTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type SymbolicLinkTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_SymbolicLinkTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_SymbolicLinkTarget;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_SymbolicLinkTarget);
    dmfCallbacksDmf_SymbolicLinkTarget.DeviceOpen = DMF_SymbolicLinkTarget_Open;
    dmfCallbacksDmf_SymbolicLinkTarget.DeviceClose = DMF_SymbolicLinkTarget_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_SymbolicLinkTarget,
                                            SymbolicLinkTarget,
                                            DMF_CONTEXT_SymbolicLinkTarget,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_SymbolicLinkTarget.CallbacksDmf = &dmfCallbacksDmf_SymbolicLinkTarget;

    // ObjectAttributes must be initialized and
    // ParentObject attribute must be set to either WDFDEVICE or DMFMODULE.
    //
    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_SymbolicLinkTarget,
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

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SymbolicLinkTarget_DeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ DWORD dwIoControlCode,
    _In_reads_bytes_opt_(nInBufferSize) LPVOID lpInBuffer,
    _In_ DWORD nInBufferSize,
    _Out_writes_bytes_to_opt_(nOutBufferSize, *lpBytesReturned) LPVOID lpOutBuffer,
    _In_ DWORD nOutBufferSize,
    _Out_opt_ LPDWORD lpBytesReturned,
    _Inout_opt_ LPOVERLAPPED lpOverlapped
    )
/*++

Routine Description:

    Creates and sends a synchronous request to the IoTarget given a buffer, IOCTL and other information.

Arguments:

    DmfObject - This Module's DMF Object.
    RequestBuffer - Buffer of data to attach to request to be sent.
    RequestLength - Number of bytes to in RequestBuffer to send.
    ResponseBuffer - Buffer of data that is returned by the request.
    ResponseLength - Size of Response Buffer in bytes.
    RequestType - Read or Write or Ioctl
    RequestIoctl - The given IOCTL.
    RequestTimeout - A timeout if desired.
    BytesWritten - Bytes returned by the transaction.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    BOOL success;
    DMF_CONTEXT_SymbolicLinkTarget* moduleContext;

    FuncEntry(DMF_TRACE);

    success = FALSE;

    // It can be called from Close callback.
    // TODO: Correct DMF Framework to set IsClosing flag correctly when there
    //       are Child Modules.
    //
    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 SymbolicLinkTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_CONFIG_SymbolicLinkTarget* moduleConfig;
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "moduleConfig->SymbolikcLinkName %S", moduleConfig->SymbolicLinkName);

    HANDLE myIoTarget;
    myIoTarget = CreateFile(moduleConfig->SymbolicLinkName,
                            moduleConfig->OpenMode,
                            moduleConfig->ShareAccess,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            NULL);
    if (INVALID_HANDLE_VALUE == myIoTarget)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        LPWSTR messageBuffer;

        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      GetLastError(),
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPWSTR)&messageBuffer,
                      0,
                      NULL);

        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "%S", messageBuffer);

        LocalFree(messageBuffer);

        goto Exit;
    }

    success = DeviceIoControl(myIoTarget,
                              dwIoControlCode,
                              lpInBuffer,
                              nInBufferSize,
                              lpOutBuffer,
                              nOutBufferSize,
                              lpBytesReturned,
                              lpOverlapped);

Exit:
    CloseHandle(myIoTarget);

    ntStatus = (TRUE == success) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_SymbolicLinkTarget.c
//
