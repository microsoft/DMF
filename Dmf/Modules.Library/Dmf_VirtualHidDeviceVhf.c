/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_VirtualHidDeviceVhf.c

Abstract:

    Support for creating a Virtual Hid using MS VHF (Virtual Hid Framework).
    NOTE: When using the Module, the Client driver must set Vhf.sys as a Lower Filter driver using the Client driver INF.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_VirtualHidDeviceVhf.tmh"

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
    // Handle to Ms Virtual Hid Framework.
    //
    VHFHANDLE VhfHandle;
    // For validation purposes.
    //
    ULONG Started;
} DMF_CONTEXT_VirtualHidDeviceVhf;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(VirtualHidDeviceVhf)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(VirtualHidDeviceVhf)

// MemoryTag.
//
#define MemoryTag 'MDHV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
VirtualHidDeviceVhf_Start(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts the HID on demand from Client.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_VirtualHidDeviceVhf* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->VhfHandle != NULL);
    ASSERT(! moduleContext->Started);
    ntStatus = VhfStart(moduleContext->VhfHandle);
    if (NT_SUCCESS(ntStatus))
    {
        moduleContext->Started = TRUE;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
VirtualHidDeviceVhf_Stop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stops the HID on demand from Client.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_VirtualHidDeviceVhf* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->VhfHandle != NULL)
    {
        VhfDelete(moduleContext->VhfHandle,
                  TRUE);
        moduleContext->VhfHandle = NULL;
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
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_VirtualHidDeviceVhf_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type VirtualHidDeviceVhf.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualHidDeviceVhf* moduleConfig;
    DMF_CONTEXT_VirtualHidDeviceVhf* moduleContext;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    VHF_CONFIG vhfConfig;

    VHF_CONFIG_INIT(&vhfConfig,
                    WdfDeviceWdmGetDeviceObject(device),
                    (USHORT)(moduleConfig->HidReportDescriptorLength),
                    (UCHAR*)(moduleConfig->HidReportDescriptor));
    vhfConfig.VendorID = moduleConfig->VendorId;
    vhfConfig.ProductID = moduleConfig->ProductId;
    vhfConfig.VersionNumber = moduleConfig->VersionNumber;
    vhfConfig.EvtVhfAsyncOperationGetFeature = moduleConfig->IoctlCallback_IOCTL_HID_GET_FEATURE;
    vhfConfig.EvtVhfAsyncOperationGetInputReport = moduleConfig->IoctlCallback_IOCTL_HID_GET_INPUT_REPORT;
    vhfConfig.EvtVhfAsyncOperationSetFeature = moduleConfig->IoctlCallback_IOCTL_HID_SET_FEATURE;
    vhfConfig.EvtVhfAsyncOperationWriteReport = moduleConfig->IoctlCallback_IOCTL_HID_WRITE_REPORT;
    vhfConfig.EvtVhfReadyForNextReadReport = moduleConfig->IoctlCallback_IOCTL_HID_READ_REPORT;
    vhfConfig.VhfClientContext = moduleConfig->VhfClientContext;
    ntStatus = VhfCreate(&vhfConfig,
                         &moduleContext->VhfHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ASSERT(NULL != moduleContext->VhfHandle);
    if (moduleConfig->StartOnOpen)
    {
        ntStatus = VirtualHidDeviceVhf_Start(DmfModule);
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_VirtualHidDeviceVhf_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type VirtualHidDeviceVhf.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    VirtualHidDeviceVhf_Stop(DmfModule);

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
DMF_VirtualHidDeviceVhf_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type VirtualHidDeviceVhf.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_VirtualHidDeviceVhf;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_VirtualHidDeviceVhf;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_VirtualHidDeviceVhf);
    dmfCallbacksDmf_VirtualHidDeviceVhf.DeviceOpen = DMF_VirtualHidDeviceVhf_Open;
    dmfCallbacksDmf_VirtualHidDeviceVhf.DeviceClose = DMF_VirtualHidDeviceVhf_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_VirtualHidDeviceVhf,
                                            VirtualHidDeviceVhf,
                                            DMF_CONTEXT_VirtualHidDeviceVhf,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_VirtualHidDeviceVhf.CallbacksDmf = &dmfCallbacksDmf_VirtualHidDeviceVhf;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_VirtualHidDeviceVhf,
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
VOID
DMF_VirtualHidDeviceVhf_AsynchronousOperationComplete(
    _In_ DMFMODULE DmfModule,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_ NTSTATUS NtStatus
    )
/*++

Routine Description:

    Indicates to Vhf that the Client has completed an asynchronous operation.

Arguments:

    DmfModule - This Module's handle.
    VhfOperationHandle - The operation handle passed to the caller.
    NtStatus - The return status (completion status) of the operation.

Return Value:

    None

--*/
{
    DMF_CONTEXT_VirtualHidDeviceVhf* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 VirtualHidDeviceVhf);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->VhfHandle != NULL);
    ASSERT(moduleContext->Started);

    VhfAsyncOperationComplete(VhfOperationHandle,
                              NtStatus);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_VirtualHidDeviceVhf_ReadReportSend(
    _In_ DMFMODULE DmfModule,
    _In_ HID_XFER_PACKET* HidTransferPacket
    )
/*++

Routine Description:

    Starts the HID on demand from Client.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_VirtualHidDeviceVhf* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 VirtualHidDeviceVhf);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->VhfHandle != NULL);
    ASSERT(moduleContext->Started);
    ntStatus = VhfReadReportSubmit(moduleContext->VhfHandle,
                                   HidTransferPacket);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "VhfReadReportSubmit fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_VirtualHidDeviceVhf.c
//
