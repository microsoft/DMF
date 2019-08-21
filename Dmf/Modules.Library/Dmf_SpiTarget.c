/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SpiTarget.c

Abstract:

    Supports SPI requests via Simple Peripheral Bus (SPB).

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_SpiTarget.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Specific external includes for this DMF Module.
//
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include <gpio.h>
#include <spb.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Underlying SPI device.
    //
    WDFIOTARGET Target;
    // Resource information for SPI device.
    //
    CM_PARTIAL_RESOURCE_DESCRIPTOR Connection;
    // Resource Index.
    //
    ULONG ResourceIndex;
} DMF_CONTEXT_SpiTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(SpiTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(SpiTarget)

// Memory Pool Tag.
//
#define MemoryTag 'MipS'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define NUMBER_OF_TRANSFERS_IN_A_WRITE_READ                         2

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
SpiTarget_SpbWrite(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMilliseconds
    )
/*++

Routine Description:

    This routine forwards a write request to the SPB I/O target
    It is assumed that the address to write to is already sent to device

Arguments:

    Buffer - The output byte buffer containing the data to be written.
    BufferLength - The length of the byte buffer data.
    TimeoutMilliseconds - Not used.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_SpiTarget* moduleContext;
    NTSTATUS ntStatus;
    WDFMEMORY memorySequence;
    WDFMEMORY memoryInData;
    UCHAR* inData;
    WDFMEMORY memoryOutData;
    UCHAR* outData;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;

    UNREFERENCED_PARAMETER(TimeoutMilliseconds);

    FuncEntry(DMF_TRACE);

    memorySequence = NULL;
    memoryInData = NULL;
    inData = NULL;
    memoryOutData = NULL;
    outData = NULL;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(moduleContext->Target != NULL);

    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               NonPagedPoolNx,
                               MemoryTag,
                               BufferLength,
                               &memoryInData,
                               (VOID**)&inData);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate(memoryInData) fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(inData,
                  BufferLength);

    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               NonPagedPoolNx,
                               MemoryTag,
                               BufferLength,
                               &memoryOutData,
                               (VOID**)&outData);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate(memoryOutData) fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // TODO: This is not necessary. Instead preallocate memory over the formal parameter.
    //
    RtlCopyMemory(outData,
                  Buffer,
                  BufferLength);

    // Build the SPB sequence.
    // In order to read from an address it is first necessary to send the address
    // to read from. Then send the buffer, which will contain the data read from that address.
    //
    const ULONG transfers = NUMBER_OF_TRANSFERS_IN_A_WRITE_READ;
    SPB_TRANSFER_LIST_AND_ENTRIES(transfers) sequence;
    SPB_TRANSFER_LIST_INIT(&(sequence.List),
                           transfers);

    // PreFAST cannot figure out the SPB_TRANSFER_LIST_ENTRY
    // "struct hack" size but using an index variable quiets 
    // the warning. This is a false positive from OACR.
    // 

    ULONG transferIndex = 0;
    sequence.List.Transfers[transferIndex + 0] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionToDevice,
                                                                                     0,
                                                                                     outData,
                                                                                     BufferLength);

    sequence.List.Transfers[transferIndex + 1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionFromDevice,
                                                                                     0,
                                                                                     inData,
                                                                                     BufferLength);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    ntStatus = WdfMemoryCreatePreallocated(&attributes,
                                           (VOID*)&sequence,
                                           sizeof(sequence),
                                           &memorySequence);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreatePreallocated fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&memoryDescriptor,
                                      memorySequence,
                                      NULL);

    ULONG_PTR bytesReturned = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "WdfIoTargetSendIoctlSynchronously to SPI Controller BufferLength=%ld", BufferLength);

    // Send IOCTL to SPB SPI Driver.
    //
    ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->Target,
                                                 NULL,
                                                 IOCTL_SPB_FULL_DUPLEX,
                                                 &memoryDescriptor,
                                                 NULL,
                                                 NULL,
                                                 &bytesReturned);
    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE,
                "WdfIoTargetSendIoctlSynchronously bytesReturned=%llu ntStatus=%!STATUS!",
                bytesReturned,
                ntStatus);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "WdfIoTargetSendIoctlSynchronously fails: bytesReturned=%llu ntStatus=%!STATUS!",
                    bytesReturned,
                    ntStatus);
        goto Exit;
    }

    if (bytesReturned < BufferLength)
    {
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "SpbSequence fails: bytesReturned=%llu BufferLength=%lu ntStatus=%!STATUS!",
                    bytesReturned,
                    BufferLength,
                    ntStatus);
        goto Exit;
    }

Exit:

    if (memorySequence != NULL)
    {
        WdfObjectDelete(memorySequence);
        memorySequence = NULL;
    }

    if (memoryInData != NULL)
    {
        WdfObjectDelete(memoryInData);
        memoryInData = NULL;
    }

    if (memoryOutData != NULL)
    {
        WdfObjectDelete(memoryOutData);
        memoryOutData = NULL;
    }

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
SpiTarget_SpbWriteRead(
    _In_ DMFMODULE DmfModule,
    _Inout_ UCHAR* OutData,
    _In_ ULONG OutDataLength,
    _Inout_ UCHAR* InData,
    _In_ ULONG InDataLength,
    _In_ ULONG Timeout
    )
/*++

Routine Description:

    Sends a Write-Read sequence to the SPB I/O target defined by the given
    DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    OutData - The output byte buffer containing the data to be sent on MOSI
    OutDataLength - The length of the byte buffer data
    InData - The input byte buffer containing the data read from MISO
    InDataLength - The length of the byte buffer data
    Timeout - Not used.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_SpiTarget* moduleContext;
    DMF_CONFIG_SpiTarget* moduleConfig;
    NTSTATUS ntStatus;
    WDFMEMORY memorySequence;
    WDFMEMORY memoryInData;
    UCHAR* inData;
    WDFMEMORY memoryOutData;
    UCHAR* outData;
    ULONG transferLength;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    ULONG_PTR bytesReturned;

    UNREFERENCED_PARAMETER(Timeout);

    FuncEntry(DMF_TRACE);

    memorySequence = NULL;
    memoryInData = NULL;
    inData = NULL;
    memoryOutData = NULL;
    outData = NULL;
    transferLength = 0;
    bytesReturned = 0;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ASSERT(moduleContext->Target != NULL);

    transferLength = InDataLength + OutDataLength;

    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               NonPagedPoolNx,
                               MemoryTag,
                               transferLength,
                               &memoryInData,
                               (VOID**)&inData);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate(inData) fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(inData,
                  transferLength);

    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               NonPagedPoolNx,
                               MemoryTag,
                               transferLength,
                               &memoryOutData,
                               (VOID**)&outData);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate(outData) fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    RtlZeroMemory(outData,
                  transferLength);
    RtlCopyMemory(outData,
                  OutData,
                  OutDataLength);

    // Build the SPB sequence.
    //
    const ULONG transfers = NUMBER_OF_TRANSFERS_IN_A_WRITE_READ;
    SPB_TRANSFER_LIST_AND_ENTRIES(transfers) sequence;
    SPB_TRANSFER_LIST_INIT(&(sequence.List), transfers);

    // PreFAST cannot figure out the SPB_TRANSFER_LIST_ENTRY
    // "struct hack" size but using an index variable quiets 
    // the warning. This is a false positive from OACR.
    // 
    ULONG transferIndex = 0;
    sequence.List.Transfers[transferIndex + 0] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionToDevice,
                                                                                     0,
                                                                                     outData,
                                                                                     transferLength);

    sequence.List.Transfers[transferIndex + 1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionFromDevice,
                                                                                     0,
                                                                                     inData,
                                                                                     transferLength);

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    ntStatus = WdfMemoryCreatePreallocated(&attributes,
                                           (VOID*)&sequence,
                                           sizeof(sequence),
                                           &memorySequence);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreatePreallocated(memorySequence) fails ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&memoryDescriptor,
                                      memorySequence,
                                      NULL);

    // Perform optional latency calculations in the Client.
    //
    if (moduleConfig->LatencyCalculationCallback)
    {
        moduleConfig->LatencyCalculationCallback(DmfModule,
                                                 SpiTarget_LatencyCalculation_Message_Start,
                                                 OutData,
                                                 OutDataLength);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "WdfIoTargetSendIoctlSynchronously OutDataLength=%ld InDataLength=%ld",
                OutDataLength, InDataLength);

    // Send IOCTL to SPB SPI Driver.
    //
    ntStatus = WdfIoTargetSendIoctlSynchronously(moduleContext->Target,
                                                 NULL,
                                                 IOCTL_SPB_FULL_DUPLEX,
                                                 &memoryDescriptor,
                                                 NULL,
                                                 NULL,
                                                 &bytesReturned);
    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "WdfIoTargetSendIoctlSynchronously bytesReturned=%llu ntStatus=%!STATUS!)",
                bytesReturned, ntStatus);

    // Perform optional latency calculations in the Client.
    //
    if (moduleConfig->LatencyCalculationCallback)
    {
        moduleConfig->LatencyCalculationCallback(DmfModule,
                                                 SpiTarget_LatencyCalculation_Message_End,
                                                 OutData,
                                                 OutDataLength);
    }

    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetSendIoctlSynchronously fails: bytes:%llu ntStatus=%!STATUS!",
                    bytesReturned,
                    ntStatus);
        goto Exit;
    }

    if (bytesReturned < transferLength)
    {
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "SpbSequence fails: bytesReturned=%llu transferLength=%lu ntStatus=%!STATUS!",
                    bytesReturned,
                    transferLength,
                    ntStatus);
        goto Exit;
    }

    RtlCopyMemory(InData,
                  (inData + OutDataLength),
                  InDataLength);

Exit:

    if (memorySequence != NULL)
    {
        WdfObjectDelete(memorySequence);
        memorySequence = NULL;
    }

    if (memoryInData != NULL)
    {
        WdfObjectDelete(memoryInData);
        memoryInData = NULL;
    }

    if (memoryOutData != NULL)
    {
        WdfObjectDelete(memoryOutData);
        memoryOutData = NULL;
    }

    FuncExitVoid(DMF_TRACE);

    return ntStatus;
}

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
DMF_SpiTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type SpiTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    UNICODE_STRING resourcePathString;
    WCHAR resourcePathBuffer[RESOURCE_HUB_PATH_SIZE];
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    DMF_CONTEXT_SpiTarget* moduleContext;
    DMF_CONFIG_SpiTarget* moduleConfig;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    RtlInitEmptyUnicodeString(&resourcePathString,
                              resourcePathBuffer,
                              sizeof(resourcePathBuffer));

    ntStatus = RESOURCE_HUB_CREATE_PATH_FROM_ID(&resourcePathString,
                                                moduleContext->Connection.u.Connection.IdLowPart,
                                                moduleContext->Connection.u.Connection.IdHighPart);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RESOURCE_HUB_CREATE_PATH_FROM_ID fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = device;

    ntStatus = WdfIoTargetCreate(device,
                                 &objectAttributes,
                                 &moduleContext->Target);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RESOURCE_HUB_CREATE_PATH_FROM_ID fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                &resourcePathString,
                                                FILE_GENERIC_READ | FILE_GENERIC_WRITE);

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                &resourcePathString,
                                                (GENERIC_READ | GENERIC_WRITE));

    openParams.ShareAccess = 0;
    openParams.CreateDisposition = FILE_OPEN;
    openParams.FileAttributes = FILE_ATTRIBUTE_NORMAL;

    //  Open the IoTarget for I/O operation.
    //
    ntStatus = WdfIoTargetOpen(moduleContext->Target,
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        ASSERT(NT_SUCCESS(ntStatus));
        WdfObjectDelete(moduleContext->Target);
        moduleContext->Target = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetOpen fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_SpiTarget_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type SpiTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_SpiTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->Target != NULL)
    {
        WdfIoTargetClose(moduleContext->Target);
        WdfObjectDelete(moduleContext->Target);
        moduleContext->Target = NULL;
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_SpiTarget_ResourcesAssign(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
/*++

Routine Description:

    Tells this Module instance what Resources are available. This Module then extracts
    the needed Resources and uses them as needed.

Arguments:

    DmfModule - This Module's handle.
    ResourcesRaw - WDF Resource Raw parameter that is passed to the given
                   DMF Module callback.
    ResourcesTranslated - WDF Resources Translated parameter that is passed to the given
                          DMF Module callback.

Return Value:

    STATUS_SUCCESS.

--*/
{
    DMF_CONTEXT_SpiTarget* moduleContext;
    ULONG spiResourceCount;
    ULONG resourceCount;
    ULONG resourceIndex;
    NTSTATUS ntStatus;
    DMF_CONFIG_SpiTarget* moduleConfig;
    BOOLEAN resourceAssigned;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(ResourcesRaw);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ASSERT(ResourcesRaw != NULL);
    ASSERT(ResourcesTranslated != NULL);

    // Number of valid resources.
    //
    spiResourceCount = 0;
    resourceAssigned = FALSE;

    // Check the number of resources for the button device.
    //
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);
    if (resourceCount == 0)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "No resources found");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        NT_ASSERT(FALSE);
        goto Exit;
    }

    // Parse the resources.
    //
    for (resourceIndex = 0; resourceIndex < resourceCount && (! resourceAssigned); resourceIndex++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR resource;

        resource = WdfCmResourceListGetDescriptor(ResourcesTranslated,
                                                  resourceIndex);
        if (NULL == resource)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfCmResourceListGetDescriptor fails");
            goto Exit;
        }

        if (resource->Type == CmResourceTypeConnection)
        {
            if ((resource->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_SERIAL) &&
                (resource->u.Connection.Type == CM_RESOURCE_CONNECTION_TYPE_SERIAL_SPI))
            {
                if (moduleConfig->ResourceIndex == spiResourceCount)
                {
                    moduleContext->ResourceIndex = spiResourceCount;
                    moduleContext->Connection = *resource;
                    resourceAssigned = TRUE;
                }
                spiResourceCount++;
            }
        }
    }

    //  Validate the configuration parameters.
    //
    if (0 == spiResourceCount || (! resourceAssigned))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "No SPI Resources assigned");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        NT_ASSERT(FALSE);
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

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
DMF_SpiTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type SpiTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_SpiTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_SpiTarget;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_SpiTarget);
    dmfCallbacksDmf_SpiTarget.DeviceOpen = DMF_SpiTarget_Open;
    dmfCallbacksDmf_SpiTarget.DeviceClose = DMF_SpiTarget_Close;
    dmfCallbacksDmf_SpiTarget.DeviceResourcesAssign = DMF_SpiTarget_ResourcesAssign;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_SpiTarget,
                                            SpiTarget,
                                            DMF_CONTEXT_SpiTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_D0Entry);

    dmfModuleDescriptor_SpiTarget.CallbacksDmf = &dmfCallbacksDmf_SpiTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_SpiTarget,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpiTarget_Write(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferLength) UCHAR* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMilliseconds
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SpiTarget* moduleContext;
    DMF_CONFIG_SpiTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = SpiTarget_SpbWrite(DmfModule,
                                  Buffer,
                                  BufferLength,
                                  TimeoutMilliseconds);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpiTarget_WriteRead(
    _In_ DMFMODULE DmfModule,
    _In_reads_(OutDataLength) UCHAR* OutData,
    _In_ ULONG OutDataLength,
    _Inout_updates_(InDataLength) UCHAR* InData,
    _In_ ULONG InDataLength,
    _In_ ULONG TimeoutMilliseconds
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SpiTarget* moduleContext;
    DMF_CONFIG_SpiTarget* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = SpiTarget_SpbWriteRead(DmfModule,
                                      OutData,
                                      OutDataLength,
                                      InData,
                                      InDataLength,
                                      TimeoutMilliseconds);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_SpiTarget.c
//
