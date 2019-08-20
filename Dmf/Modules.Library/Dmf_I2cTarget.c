/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_I2cTarget.c

Abstract:

    Supports I2C requests via SPB.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_I2cTarget.tmh"

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
    // Resources assigned.
    //
    BOOLEAN I2cConnectionAssigned;
    // Underlying I2C device.
    //
    WDFIOTARGET I2cTarget;
    // Resource information for I2C device.
    //
    CM_PARTIAL_RESOURCE_DESCRIPTOR I2cConnection;
    // Resource Index.
    //
    ULONG ResourceIndex;
} DMF_CONTEXT_I2cTarget;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(I2cTarget)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(I2cTarget)

// Memory Pool Tag.
//
#define MemoryTag 'Mc2I'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define NUMBER_OF_TRANSFERS_IN_A_WRITE_READ                         2

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
I2cTarget_SpbSequence(
    _In_ WDFIOTARGET IoTarget,
    _In_reads_(SequenceLength) VOID* Sequence,
    _In_ SIZE_T SequenceLength,
    _Out_ ULONG* BytesReturned,
    _In_ ULONGLONG ReadTimeoutMs
    )
/*++

Routine Description:

    Send a given SPB sequence to a target device (synchronously). This code is used
    for reading/writing to an SPB target.

Arguments:

    IoTarget - The target device to send the given SPB sequence to.
    Sequence - The given sequence to send.
    SequenceLength - The number of entries in the given sequence table.
    BytesReturned - The number of bytes returned by the target.
    ReadTimeoutMs - Timeout for SPB Read [ in ms].

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG_PTR bytes;
    WDFMEMORY memorySequence;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    PAGED_CODE();

    memorySequence = NULL;

    if (0 == SequenceLength)
    {
        ASSERT(FALSE);
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid Sequence Length ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    ntStatus = WdfMemoryCreatePreallocated(&attributes,
                                           Sequence,
                                           SequenceLength,
                                           &memorySequence);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreatePreallocated fails: ntStatus=%!STATUS!", ntStatus);
        memorySequence = NULL;
        goto Exit;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&memoryDescriptor,
                                      memorySequence,
                                      NULL);

    // Set a request timeout.
    //
    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    if (0 != ReadTimeoutMs)
    {
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions,
                                             WDF_REL_TIMEOUT_IN_MS(ReadTimeoutMs));
    }

    // Send the SPB sequence IOCTL.
    //  
    ntStatus = WdfIoTargetSendIoctlSynchronously(IoTarget,
                                                 NULL,
                                                 IOCTL_SPB_EXECUTE_SEQUENCE,
                                                 &memoryDescriptor,
                                                 NULL,
                                                 &sendOptions,
                                                 &bytes);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetSendIoctlSynchronously fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Copy the number of bytes transmitted over the bus.
    // The controller needs to support querying for actual bytes 
    // for each transaction.
    //   
    *BytesReturned = (ULONG)bytes;

    ASSERT(NT_SUCCESS(ntStatus));

Exit:

    if (memorySequence != NULL)
    {
        WdfObjectDelete(memorySequence);
        memorySequence = NULL;
    }

    return ntStatus;
}
#pragma code_seg()

// TODO: Rename DelayUs to DelayMicroseconds.
//
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
I2cTarget_SpbRead(
    _In_ WDFIOTARGET IoTarget,
    _In_reads_(RegisterAddressLength) UCHAR* RegisterAddress,
    _In_ ULONG RegisterAddressLength,
    _Out_writes_(DataLength) BYTE* Data,
    _In_ ULONG DataLength,
    _In_ ULONG DelayUs,
    _In_ ULONGLONG ReadTimeout
    )
/*++

Routine Description:

    Perform a Read operation on a given SPB target device at a given register address.

Arguments:

    IoTarget - The target device to read from.
    RegisterAddress - The buffer which contains the register of the target device to read from.
    RegisterAddressLength - The size of RegisterAddress buffer.
    Data - Where the read data is written.
    DataLength - Size of the buffer pointed to by Data.
    DelayUs - SPB device delay.
    ReadTimeout - Timeout for SPB Read [ in ms].

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bytesReturned;
    ULONG expectedLength;

    PAGED_CODE();

    expectedLength = sizeof(UCHAR);

    ASSERT(IoTarget != NULL);
    ASSERT(Data != NULL);
    ASSERT(DataLength > 0);

    // Build the SPB sequence.
    //
    SPB_TRANSFER_LIST_AND_ENTRIES(NUMBER_OF_TRANSFERS_IN_A_WRITE_READ) sequence;
    SPB_TRANSFER_LIST_INIT(&(sequence.List), NUMBER_OF_TRANSFERS_IN_A_WRITE_READ);

    // PreFAST cannot figure out the SPB_TRANSFER_LIST_ENTRY
    // struct size but using an index variable quiets 
    // the warning. This is a false positive from OACR.
    // 
    sequence.List.Transfers[0] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionToDevice,
                                                                     DelayUs,
                                                                     RegisterAddress,
                                                                     RegisterAddressLength);
    ASSERT(2 == sequence.List.TransferCount);
    // This code is correct, per the ASSERT.
    //
    #pragma warning(suppress: 6201)
    sequence.List.Transfers[1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionFromDevice,
                                                                     DelayUs,
                                                                     Data,
                                                                     DataLength);

    // Send the read as a Sequence request to the SPB target.
    //
    ntStatus = I2cTarget_SpbSequence(IoTarget,
                                     &sequence,
                                     sizeof(sequence),
                                     &bytesReturned,
                                     ReadTimeout);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Check if this is a "short transaction" i.e. the sequence
    // resulted in lesser bytes read than expected.
    // NOTE: Since there can be multiple TLCs with variable report sizes
    // and inputReportMaxLength is read, ensure it is a non-zero report 
    // i.e. it at least has the 2 bytes for report length.
    //
    if (bytesReturned < expectedLength)
    {
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        goto Exit;
    }

    ASSERT(NT_SUCCESS(ntStatus));

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
I2cTarget_SpbWrite(
    _In_ WDFIOTARGET IoTarget,
    _In_reads_(RegisterAddressLength) UCHAR* RegisterAddress,
    _In_ ULONG RegisterAddressLength,
    _In_reads_(DataLength) BYTE* Data,
    _In_ ULONG DataLength,
    _In_ ULONGLONG WriteTimeoutMs
    )
/*++

Routine Description:

    Perform a Write operation on a given SPB target device at a given register address.

Arguments:

    IoTarget - The target device to write to.
    RegisterAddress - The buffer which contains the register of the target device to write to.
    RegisterAddressLength - The size of RegisterAddress buffer.
    Data - The address of the data to write..
    DataLength - Size of the buffer pointed to by Data.
    WriteTimeoutMs - Timeout for SPB Write [ in ms].

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    UCHAR* buffer;
    ULONG bufferLength;
    ULONG expectedLength;
    ULONG_PTR bytesWritten;
    WDFMEMORY memory;
    WDF_MEMORY_DESCRIPTOR memoryDescriptor;
    WDF_REQUEST_SEND_OPTIONS sendOptions;

    PAGED_CODE();

    ASSERT(IoTarget != NULL);
    ASSERT(Data != NULL);
    ASSERT(DataLength > 0);

    memory = NULL;

    // A SPB write-write is a single write request with the register
    // and data combined in one buffer. Allocate memory for the size
    // of a register + data length.
    //
    bufferLength = RegisterAddressLength + DataLength;
    if (0 == bufferLength)
    {
        // This is only for prefeast.
        //
        ASSERT(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    ASSERT(bufferLength != 0);
    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               NonPagedPoolNx,
                               MemoryTag,
                               bufferLength,
                               &memory,
                               (VOID**)&buffer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate failed allocating memory buffer for SpbWrite ntStatus=%!STATUS!", ntStatus);
        memory = NULL;
        goto Exit;
    }

    RtlZeroMemory(buffer,
                  bufferLength);

    WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&memoryDescriptor,
                                      memory,
                                      NULL);

    // Fill in the buffer: address followed by data.
    //
    RtlCopyMemory(buffer,
                  RegisterAddress,
                  RegisterAddressLength);
    RtlCopyMemory((buffer + RegisterAddressLength),
                  Data,
                  DataLength);

    // Set a request timeout.
    //
    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);

    // Add request timeout.
    //
    if (0 != WriteTimeoutMs)
    {
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions,
                                             WDF_REL_TIMEOUT_IN_MS(WriteTimeoutMs));
    }
    // Send the request synchronously.
    //
    ntStatus = WdfIoTargetSendWriteSynchronously(IoTarget,
                                                 NULL,
                                                 &memoryDescriptor,
                                                 NULL,
                                                 &sendOptions,
                                                 &bytesWritten);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetSendWriteSynchronously fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Check if this is a "short transaction" i.e. the sequence
    // resulted in lesser bytes returned than expected.
    // NOTE: For a Write, a short transaction is not expected.
    //
    expectedLength = RegisterAddressLength + DataLength;
    if (bytesWritten != expectedLength)
    {
        ntStatus = STATUS_DEVICE_PROTOCOL_ERROR;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetSendWriteSynchronously returned with 0x%lu bytes expected:0x%lu bytes ntStatus=%!STATUS!", (ULONG)bytesWritten, expectedLength, ntStatus);
        goto Exit;
    }

    ASSERT(NT_SUCCESS(ntStatus));

Exit:

    if (memory != NULL)
    {
        WdfObjectDelete(memory);
        memory = NULL;
    }

    return ntStatus;
}
#pragma code_seg()

typedef enum
{
    I2CTARGET_DEVICE_BUFFER_READ = 1,
    I2CTARGET_DEVICE_BUFFER_WRITE,
} I2cTarget_Operation;

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
I2cTarget_BufferReadWrite(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferLength) VOID* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMs,
    _In_ I2cTarget_Operation Operation
    )
/*++

Routine Description:

    Transfers bytes to/from the underlying device.

Arguments:

    DmfModule - This Module's handle.
    Buffer - Address of bytes to write or address where read bytes are stored for Client.
    BufferLength - The size of Buffer in bytes.
    TimeoutMs - How long to wait for the transaction to happen.
    Operation - Indicates read or write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_I2cTarget* moduleContext;
    WDFREQUEST request;
    WDF_OBJECT_ATTRIBUTES requestAttributes;
    WDF_REQUEST_SEND_OPTIONS requestOptions;
    WDFMEMORY memory;
    WDF_OBJECT_ATTRIBUTES memoryAttributes;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(TimeoutMs);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    request = NULL;
    memory = NULL;

    if (0 == BufferLength)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&requestAttributes);

    ntStatus = WdfRequestCreate(&requestAttributes,
                                moduleContext->I2cTarget,
                                &request);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestCreate fails: ntStatus=%!STATUS!", ntStatus);
        request = NULL;
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&memoryAttributes);
    memoryAttributes.ParentObject = request;

    ntStatus = WdfMemoryCreatePreallocated(&memoryAttributes,
                                           Buffer,
                                           BufferLength,
                                           &memory);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreatePreallocated fails: ntStatus=%!STATUS!", ntStatus);
        memory = NULL;
        goto Exit;
    }

    if (I2CTARGET_DEVICE_BUFFER_READ == Operation)
    {
        ntStatus = WdfIoTargetFormatRequestForRead(moduleContext->I2cTarget,
                                                   request,
                                                   memory,
                                                   0,
                                                   0);
    }
    else if (I2CTARGET_DEVICE_BUFFER_WRITE == Operation)
    {
        ntStatus = WdfIoTargetFormatRequestForWrite(moduleContext->I2cTarget,
                                                    request,
                                                    memory,
                                                    0,
                                                    0);
    }
    else
    {
        ASSERT(FALSE);
        ntStatus = STATUS_INTERNAL_ERROR;
    }

    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfIoTargetFormatRequestForIoctl fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&requestOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    if (TimeoutMs > 0)
    {
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&requestOptions, WDF_REL_TIMEOUT_IN_SEC(TimeoutMs));
    }
    if (! WdfRequestSend(request,
                         moduleContext->I2cTarget,
                         &requestOptions))
    {
        ntStatus = WdfRequestGetStatus(request);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestSend fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    if (memory != NULL)
    {
        WdfObjectDelete(memory);
        memory = NULL;
    }

    if (request != NULL)
    {
        WdfObjectDelete(request);
        request = NULL;
    }

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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_I2cTarget_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type I2cTarget.

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
    DMF_CONTEXT_I2cTarget* moduleContext;
    DMF_CONFIG_I2cTarget* moduleConfig;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (! moduleContext->I2cConnectionAssigned)
    {
        // In some cases, the minimum number of resources is zero because the same driver
        // is used on different platforms. In that case, this Module still loads and opens
        // but it does nothing.
        //
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "No I2C Resources Found");
        ntStatus = STATUS_SUCCESS;
        goto Exit;
    }

    device = DMF_ParentDeviceGet(DmfModule);

    RtlInitEmptyUnicodeString(&resourcePathString,
                              resourcePathBuffer,
                              sizeof(resourcePathBuffer));

    ntStatus = RESOURCE_HUB_CREATE_PATH_FROM_ID(&resourcePathString,
                                                moduleContext->I2cConnection.u.Connection.IdLowPart,
                                                moduleContext->I2cConnection.u.Connection.IdHighPart);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RESOURCE_HUB_CREATE_PATH_FROM_ID fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;

    ntStatus = WdfIoTargetCreate(device,
                                 &objectAttributes,
                                 &moduleContext->I2cTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RESOURCE_HUB_CREATE_PATH_FROM_ID fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                &resourcePathString,
                                                FILE_GENERIC_READ | FILE_GENERIC_WRITE);

    //  Open the IoTarget for I/O operation.
    //
    ntStatus = WdfIoTargetOpen(moduleContext->I2cTarget,
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        ASSERT(NT_SUCCESS(ntStatus));
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
DMF_I2cTarget_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type I2cTarget.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_I2cTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->I2cTarget != NULL)
    {
        WdfIoTargetClose(moduleContext->I2cTarget);
        WdfObjectDelete(moduleContext->I2cTarget);
        moduleContext->I2cTarget = NULL;
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
DMF_I2cTarget_ResourcesAssign(
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
    DMF_CONTEXT_I2cTarget* moduleContext;
    ULONG i2cResourceCount;
    ULONG resourceCount;
    ULONG resourceIndex;
    NTSTATUS ntStatus;
    DMF_CONFIG_I2cTarget* moduleConfig;
    BOOLEAN resourceAssigned;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(ResourcesRaw);

    ASSERT(ResourcesRaw != NULL);
    ASSERT(ResourcesTranslated != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Number of valid resources.
    //
    i2cResourceCount = 0;
    resourceAssigned = FALSE;

    // Check the number of resources for the button device.
    //
    resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);
    if (resourceCount == 0)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "I2C resources not found");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        ASSERT(FALSE);
        goto Exit;
    }

    // Parse the resources.
    //
    for (resourceIndex = 0; resourceIndex < resourceCount && (! resourceAssigned); resourceIndex++)
    {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR resource;

        resource = WdfCmResourceListGetDescriptor(ResourcesTranslated, resourceIndex);
        if (NULL == resource)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No resources found");
            goto Exit;
        }

        if (resource->Type == CmResourceTypeConnection)
        {
            if ((resource->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_SERIAL) &&
                (resource->u.Connection.Type == CM_RESOURCE_CONNECTION_TYPE_SERIAL_I2C))
            {
                if (moduleConfig->I2cResourceIndex == i2cResourceCount)
                {
                    moduleContext->ResourceIndex = i2cResourceCount;
                    moduleContext->I2cConnection = *resource;
                    moduleContext->I2cConnectionAssigned = TRUE;
                    resourceAssigned = TRUE;
                }
                i2cResourceCount++;
            }
        }
    }

    //  Validate the configuration parameters.
    //
    if ((moduleConfig->I2cConnectionMandatory) &&
        (0 == i2cResourceCount || (! resourceAssigned)))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "I2C Resources not assigned");
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        ASSERT(FALSE);
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
DMF_I2cTarget_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type I2cTarget.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_I2cTarget;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_I2cTarget;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_I2cTarget);
    dmfCallbacksDmf_I2cTarget.DeviceOpen = DMF_I2cTarget_Open;
    dmfCallbacksDmf_I2cTarget.DeviceClose = DMF_I2cTarget_Close;
    dmfCallbacksDmf_I2cTarget.DeviceResourcesAssign = DMF_I2cTarget_ResourcesAssign;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_I2cTarget,
                                            I2cTarget,
                                            DMF_CONTEXT_I2cTarget,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_D0Entry);

    dmfModuleDescriptor_I2cTarget.CallbacksDmf = &dmfCallbacksDmf_I2cTarget;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_I2cTarget,
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
DMF_I2cTarget_AddressRead(
    _In_ DMFMODULE DmfModule,
    _In_reads_(AddressLength) UCHAR* Address,
    _In_ ULONG AddressLength,
    _Out_writes_(BufferLength) VOID* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Invoke the AddressRead Callback for this Module.

Arguments:

    DmfModule - This Module's handle.
    Address - The address to read from.
    AddressLength - The number of bytes that make up the Address.
    Buffer - The address where the bytes that are read should be written.
    BufferLength - The number of bytes to read.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_I2cTarget* moduleContext;
    DMF_CONFIG_I2cTarget* moduleConfig;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = I2cTarget_SpbRead(moduleContext->I2cTarget,
                                 Address,
                                 AddressLength,
                                 (BYTE *)Buffer,
                                 BufferLength,
                                 moduleConfig->ReadDelayUs,
                                 moduleConfig->ReadTimeoutMs);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_AddressWrite(
    _In_ DMFMODULE DmfModule,
    _In_reads_(AddressLength) UCHAR* Address,
    _In_ ULONG AddressLength,
    _In_reads_(BufferLength) VOID* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Invoke the AddressWrite Callback for this Module.

Arguments:

    DmfModule - This Module's handle.
    Address - The address to read from.
    AddressLength - The number of bytes that make up the Address.
    Buffer - The address of the bytes to write.
    BufferLength - The number of bytes to write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_I2cTarget* moduleContext;
    DMF_CONFIG_I2cTarget* moduleConfig;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = I2cTarget_SpbWrite(moduleContext->I2cTarget,
                                  Address,
                                  AddressLength,
                                  (BYTE *)Buffer,
                                  BufferLength,
                                  moduleConfig->WriteTimeoutMs);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_BufferRead(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferLength) VOID* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMs
    )
/*++

Routine Description:

    Invoke the BufferRead Callback for this Module.

Arguments:

    DmfModule - This Module's handle.
    Buffer - The address where the read bytes should be written.
    BufferLength - The number of bytes to read.
    TimeoutMs - Timeout value in milliseconds.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_I2cTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = I2cTarget_BufferReadWrite(DmfModule,
                                         Buffer,
                                         BufferLength,
                                         TimeoutMs,
                                         I2CTARGET_DEVICE_BUFFER_READ);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_BufferWrite(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferLength) VOID* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMs
    )
/*++

Routine Description:

    Invoke the BufferWrite Callback for this Module.

Arguments:

    DmfModule - This Module's handle.
    Buffer - The address of the bytes that should be written.
    BufferLength - The number of bytes to write.
    TimeoutMs - Timeout value in milliseconds.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_I2cTarget* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = I2cTarget_BufferReadWrite(DmfModule,
                                         Buffer,
                                         BufferLength,
                                         TimeoutMs,
                                         I2CTARGET_DEVICE_BUFFER_WRITE);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_I2cTarget_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_opt_ BOOLEAN* I2cConnectionAssigned
    )
/*++

Routine Description:

    I2c resources may or may not be present on some systems. This function returns a flag
    indicating that the I2c resource requested was found.

Arguments:

    DmfModule - This Module's handle.
    I2xConnectionAssigned - Is I2x connection assigned to this Module instance.

Return Value:

    None

--*/
{
    DMF_CONTEXT_I2cTarget* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 I2cTarget);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (I2cConnectionAssigned != NULL)
    {
        *I2cConnectionAssigned = moduleContext->I2cConnectionAssigned;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

// eof: Dmf_I2cTarget.c
//
