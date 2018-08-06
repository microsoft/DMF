/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_LiveKernelDump.c

Abstract:

    Provides support for Live Kernel Dump Management.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModules.Core.h"
#include "DmfModules.Core.Trace.h"

#include "Dmf_LiveKernelDump.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// When adding triage data to Live Kernel Memory Dump, there is an overhead of 8 Bytes to each block that is added.
// This define is used to account for this overhead while tracking the total size of Triage Data.
//
#define TRIAGE_DATA_OVERHEAD_PER_BLOCK  8

// The default data buffer ring buffer size.
//
#define LiveKernelDump_DATA_BUFFER_RingBuffer_SIZE      256

// Format used to store pointers to data - Data Buffers.
//
#pragma pack(push, 1)
typedef struct
{
    // Indicates the address of the data buffer.
    //
    VOID* Address;
    // Indicates the size of the data buffer.
    //
    ULONG Size;
    // Indicates if the data buffer is valid.
    //
    BOOLEAN Valid;
} DATA_BUFFER;
#pragma pack()

// Information for each Live Dump Data Buffer.
// A Data Buffer Source stores location and size of buffers that must be written to the live kernel
// memory dump file.
//
typedef struct
{
    // Ring Buffers for each Data Buffer.
    //
    DMFMODULE DmfModuleRingBuffer;
} DATA_BUFFER_SOURCE;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Information for the Crash Dump Management.
//
typedef struct
{
    // Data buffer source to store pointers to data structures.
    //
    DATA_BUFFER_SOURCE DataBufferSource;
    // Stores the DMF Collection as a bugcheck parameter.
    //
    ULONG_PTR BugCheckParameterDmfCollection;
    // Stores the size of DMF data stored in the LiveKernelDump Module.
    //
    ULONG DmfDataSize;
#if IS_WIN10_RS3_OR_LATER
    // Stores the handle to the IOCTL Handler.
    //
    DMFMODULE LiveKernelDumpIoctlHandler;
    // List used to temporarily store all the buffers from DataBufferSource during live kernel dump generation.
    // This list is required because, the function used to enumerate all entries in DataBufferSource is called at dispatch level.
    // We don't want to dereference the pointers stored in DataBufferSource in dispatch as the memory they are pointing to might be paged.
    // We hence store the buffers in the producer consumer list during enumeration and later copy them over to the LiveKernelDump.
    //
    DMFMODULE BufferQueue;
#endif // IS_WIN10_RS3_OR_LATER
} DMF_CONTEXT_LiveKernelDump;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(LiveKernelDump)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(LiveKernelDump)

// Memory pool tag.
//
#define MemoryTag 'DmKL'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
LiveKernelDump_DataBufferSourceAdd(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Store address and size of a data buffer.
    NOTE:
    This API should be used only if the data buffer is guaranteed to be available while
    it is stored in the Ring Buffer.
    The buffer must be removed from the Ring Buffer using DMF_LiveKernelDump_DataBufferRemove
    before the data buffer is destroyed.

Arguments:

    DmfModule - The LiveKernelDump Module.
    Buffer - Address of the data buffer.
    BufferLength - Size of the data buffer.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_LiveKernelDump* moduleContext;
    DATA_BUFFER_SOURCE* dataBufferSource;
    DATA_BUFFER dataBuffer;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE_LiveKernelDump);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Lock before adding a data buffer to prevent race conditions with invalidation of data buffers and Live Dump generation.
    //
    DMF_ModuleLock(DmfModule);

    dataBufferSource = &(moduleContext->DataBufferSource);

    dataBuffer.Address = Buffer;
    dataBuffer.Size = BufferLength;
    dataBuffer.Valid = TRUE;

    ntStatus = DMF_RingBuffer_Write(dataBufferSource->DmfModuleRingBuffer,
                                    (UCHAR*)&dataBuffer,
                                    sizeof(DATA_BUFFER));

    // There is an overhead of TRIAGE_DATA_OVERHEAD_PER_BLOCK bytes for every Triage block added.
    //
    moduleContext->DmfDataSize += (dataBuffer.Size + TRIAGE_DATA_OVERHEAD_PER_BLOCK);

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE_LiveKernelDump, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
LiveKernelDump_InvalidateDataBuffer(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_ VOID* CallbackContext
    )
/*++

Routine Description:

    This function invalidates the data buffer entry. Data Buffers can be added to the Ring Buffer by DMF Modules.
    The DMF Module should invalidate the data buffer in its cleanup routine before it gets destroyed.
    This ensures that during Live Dump generation, only valid buffers are added to the Live Dump.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Buffer - Address of the data buffer to invalidate.
    BufferSize - Size of the data buffer.
    CallbackContext - Context passed for the callback.

Return Value:

    TRUE so that all items in Ring Buffer are enumerated.

--*/
{
    DATA_BUFFER* dataBuffer;
    DMF_CONTEXT_LiveKernelDump* moduleContext;

    UNREFERENCED_PARAMETER(BufferSize);
    UNREFERENCED_PARAMETER(CallbackContext);

    ASSERT(BufferSize == sizeof(DATA_BUFFER));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    dataBuffer = (DATA_BUFFER*)Buffer;
    dataBuffer->Valid = FALSE;

    // There is an overhead of TRIAGE_DATA_OVERHEAD_PER_BLOCK Bytes for every Triage block added.
    //
    ASSERT(moduleContext->DmfDataSize >= (dataBuffer->Size + TRIAGE_DATA_OVERHEAD_PER_BLOCK));
    moduleContext->DmfDataSize -= (dataBuffer->Size + TRIAGE_DATA_OVERHEAD_PER_BLOCK);

    return TRUE;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
LiveKernelDump_DataBufferSourceRemove(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Remove a data buffer from the ring buffer.
    NOTE:
    This API should be used to remove buffers from the Ring Buffer before the data buffer is destroyed.

Arguments:

    DmfModule - The LiveKernelDump Module.
    Buffer - Address of the data buffer.
    BufferLength - Size of the data buffer.

Return Value:

    None

--*/
{
    DMF_CONTEXT_LiveKernelDump* moduleContext;
    DATA_BUFFER_SOURCE* dataBufferSource;
    DATA_BUFFER dataBuffer;

    FuncEntry(DMF_TRACE_LiveKernelDump);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Lock before invalidating a data buffer to prevent race conditions with addition of new data buffers and Live Dump generation.
    //
    DMF_ModuleLock(DmfModule);

    dataBufferSource = &(moduleContext->DataBufferSource);

    dataBuffer.Address = Buffer;
    dataBuffer.Size = BufferLength;
    dataBuffer.Valid = TRUE;

    // Find the Data Buffer.
    //
    DMF_RingBuffer_EnumerateToFindItem(dataBufferSource->DmfModuleRingBuffer,
                                       LiveKernelDump_InvalidateDataBuffer,
                                       NULL,
                                       (UCHAR*)&dataBuffer,
                                       sizeof(DATA_BUFFER));

    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE_LiveKernelDump);
}

#if IS_WIN10_RS3_OR_LATER

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
LiveKernelDump_InsertDataBufferInLiveDump(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_ VOID* CallbackContext
    )
/*++

Routine Description:

    This function inserts the data buffer entry into the Live Dump telemetry handle.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Buffer - Address of the data buffer to invalidate.
    BufferLength - Size of the data buffer.
    CallbackContext - Context passed for the callback.

Return Value:

    TRUE so that all items in Ring Buffer are enumerated.

--*/
{
    DATA_BUFFER* dataBuffer;
    VOID* producerBuffer;
    VOID* producerBufferContext;
    DMFMODULE liveKernelDumpModule;
    DMF_CONTEXT_LiveKernelDump* liveKernelDumpContext;
    NTSTATUS ntStatus;

    // Static variables used to keep track of number of buffers and total size of data added to the livedump.
    //
    static ULONG numberOfDataBuffers = 0;
    static ULONG telemetryDataSize = 0;

    UNREFERENCED_PARAMETER(BufferSize);
    UNREFERENCED_PARAMETER(CallbackContext);

    FuncEntry(DMF_TRACE_LiveKernelDump);

    ASSERT(BufferSize == sizeof(DATA_BUFFER));

    ntStatus = STATUS_SUCCESS;
    dataBuffer = (DATA_BUFFER*)Buffer;

    liveKernelDumpModule = DMF_ParentModuleGet(DmfModule);
    liveKernelDumpContext = DMF_CONTEXT_GET(liveKernelDumpModule);

    // Check if this Data Buffer is Valid.
    // NOTE: Data Buffers can be added to the Ring Buffer by DMF Modules. The DMF Module should invalidate the data buffer
    // in its cleanup routine before it gets destroyed. The below check is to ensure that only buffers that are Valid during
    // during Live Dump generation are added to the Live Dump.
    //
    if (! dataBuffer->Valid)
    {
        // Cannot insert the data buffer into the telemetry handle.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE_LiveKernelDump, "Invalid parameters: dataBuffer->Valid=%d", dataBuffer->Valid);
        goto Exit;
    }

    // Get a buffer from the Producer List.
    //
    ntStatus = DMF_BufferQueue_Fetch(liveKernelDumpContext->BufferQueue,
                                     &producerBuffer,
                                     &producerBufferContext);
    if (! NT_SUCCESS(ntStatus))
    {
        // Failed to get buffer from producer list.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE_LiveKernelDump, "DMF_BufferQueue_Fetch fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Store the DataBuffer in the buffer we just got.
    //
    RtlCopyMemory(producerBuffer,
                  dataBuffer,
                  sizeof(DATA_BUFFER));

    numberOfDataBuffers++;

    // Move the buffer to the Consumer List.
    //
    DMF_BufferQueue_Enqueue(liveKernelDumpContext->BufferQueue,
                            producerBuffer);

    // There is an overhead of TRIAGE_DATA_OVERHEAD_PER_BLOCK Bytes for Evey Triage block added.
    //
    telemetryDataSize += dataBuffer->Size + TRIAGE_DATA_OVERHEAD_PER_BLOCK;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE_LiveKernelDump, "numberOfDataBuffers=%d, telemetryDataSize so far=%d", numberOfDataBuffers, telemetryDataSize);

Exit:

    FuncExitVoid(DMF_TRACE_LiveKernelDump);

    // Continue enumeration.
    //
    return TRUE;
}

#endif  // IS_WIN10_RS3_OR_LATER

#if IS_WIN10_RS3_OR_LATER
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
LiveKernelDump_InsertDmfTriageDataToLiveDump(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE TelemetryHandle
    )
/*++

Routine Description:

    Inserts DMF triage data to the live kernel mini dump.

Arguments:

    DmfModule - This Module's handle.
    TelemetryHandle - The telemetry handle in which we want to insert the DMF triage data.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS ntStatus;
    DMF_CONTEXT_LiveKernelDump* moduleContext;
    DATA_BUFFER_SOURCE* dataBufferSource;
    DATA_BUFFER* dataBuffer;
    VOID* dataBufferContext;
    ULONG numberOfDataBuffers;
    ULONG index;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_LiveKernelDump);

    ntStatus = STATUS_SUCCESS;
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Store data buffers in the producer consumer list (non paged code called at dispatch level).
    //
    dataBufferSource = &(moduleContext->DataBufferSource);

    // Lock before adding data from the data buffer source to the telemetry handle to prevent race conditions with addition and invalidation of data buffers in the Ring Buffer.
    //
    DMF_ModuleLock(DmfModule);

    DMF_RingBuffer_Enumerate(dataBufferSource->DmfModuleRingBuffer,
                             TRUE,
                             LiveKernelDump_InsertDataBufferInLiveDump,
                             NULL);

    DMF_ModuleUnlock(DmfModule);

    // Store the data buffers from the producer consumer list to the telemetry handle (now we are back in passive level).
    //
    numberOfDataBuffers = DMF_BufferQueue_Count(moduleContext->BufferQueue);
    for (index = 0; index < numberOfDataBuffers; index++)
    {
        ntStatus = DMF_BufferQueue_Dequeue(moduleContext->BufferQueue,
                                           (VOID**)&dataBuffer,
                                           &dataBufferContext);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE_LiveKernelDump, "DMF_BufferQueue_Dequeue fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        ntStatus = LkmdTelInsertTriageDataBlock(TelemetryHandle,
                                                dataBuffer->Address,
                                                dataBuffer->Size);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "LkmdTelInsertTriageDataBlock fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Put the data buffer back in the producer list.
        //
        DMF_BufferQueue_Reuse(moduleContext->BufferQueue,
                              dataBuffer);
    }

    FuncExit(DMF_TRACE_LiveKernelDump, "ntStatus=%!STATUS!", ntStatus);

Exit:

    return ntStatus;
}
#pragma code_seg()
#endif  // IS_WIN10_RS3_OR_LATER

#if IS_WIN10_RS3_OR_LATER
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
LiveKernelDump_InsertClientTriageDataToLiveDump(
    _In_ HANDLE TelemetryHandle,
    _In_ ULONG NumberOfClientStructures,
    _In_ PLIVEKERNELDUMP_CLIENT_STRUCTURE ArrayOfClientStructures
    )
/*++

Routine Description:

    Inserts client's triage data to the live kernel mini dump.

Arguments:

    TelemetryHandle - The telemetry handle in which we want to insert the DMF triage data.
    NumberOfClientStructures - The number of structures that the client want included as part of the mini dump.
    ArrayOfClientStructures - Pointer to an array containing information of structures the client wants included as part of the mini dump.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS ntStatus;
    ULONG index;
    LIVEKERNELDUMP_CLIENT_STRUCTURE clientStructure;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_LiveKernelDump);

    ntStatus = STATUS_SUCCESS;

    for (index = 0; index < NumberOfClientStructures; index++)
    {
        clientStructure = ArrayOfClientStructures[index];
        ntStatus = LkmdTelInsertTriageDataBlock(TelemetryHandle,
                                                clientStructure.Address,
                                                clientStructure.Size);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "LkmdTelInsertTriageDataBlock fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    FuncExit(DMF_TRACE_LiveKernelDump, "ntStatus=%!STATUS!", ntStatus);

Exit:

    return ntStatus;
}
#pragma code_seg()
#endif  // IS_WIN10_RS3_OR_LATER

#if IS_WIN10_RS3_OR_LATER
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
LiveKernelDump_LiveKernelMemoryDumpCreate(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG BugCheckCode,
    _In_ ULONG_PTR BugCheckParameter,
    _In_ BOOLEAN ExcludeDmfData,
    _In_ ULONG NumberOfClientStructures,
    _In_opt_ PLIVEKERNELDUMP_CLIENT_STRUCTURE ArrayOfClientStructures,
    _In_ ULONG SizeOfSecondaryData,
    _In_opt_ VOID* SecondaryDataBuffer
    )
/*++

Routine Description:

    Creates a live kernel memory dump. Includes all the Data Buffers storing various memory buffers
    and data marked by the Client Driver and Modules in the live kernel memory dump.

Arguments:

    DmfModule - This Module's handle.
    BugCheckCode - The bugcheck code.
    BugCheckParameter - Bugcheck parameter defined per component.
    ExcludeDmfData - Indicates if the client wants to exclude DMF data from the mini dump.
    NumberOfClientStructures - The number of structures that the client want included as part of the mini dump.
    ArrayOfClientStructures - An array containing the client data structures that will be included in the mini dump.
    SizeOfSecondaryData - Size of secondary data that needs to be part of the mini dump.
    SecondaryDataBuffer - Pointer to a buffer containing the secondary data.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_LiveKernelDump* moduleContext;
    DMF_CONFIG_LiveKernelDump* moduleConfig;
    HANDLE telemetryHandle;
    ULONG_PTR secondBugcheckParameter;
    GUID nullGuid;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_LiveKernelDump);

    ntStatus = STATUS_SUCCESS;
    telemetryHandle = NULL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Validate input parameters.
    //
    if (((NumberOfClientStructures > 0) && (ArrayOfClientStructures == NULL)) ||
        ((SizeOfSecondaryData > 0) && (SecondaryDataBuffer == NULL)))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "LiveKernelDump_LiveKernelMemoryDumpCreate fails due to invalid parameters.");
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    if (ExcludeDmfData == TRUE)
    {
        secondBugcheckParameter = NULL;
    }
    else
    {
        secondBugcheckParameter = moduleContext->BugCheckParameterDmfCollection;
    }

    // Generate a telemetry handle.
    // NOTE: Among 4 bugcheck parameters, we are reserving 2 for future use. 
    // Parameter 1 is used to store the caller supplied pointer.
    // Parameter 2 is the handle to the DmfCollection if the caller chooses to include the DMF data in the mini dump, and NULL otherwise.
    //
    telemetryHandle = LkmdTelCreateReport(moduleConfig->ReportType,
                                          BugCheckCode,
                                          BugCheckParameter,
                                          secondBugcheckParameter,
                                          0,
                                          0);
    if (telemetryHandle == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "LkmdTelCreateReport fails.");
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    if (ExcludeDmfData == FALSE)
    {
        ntStatus = LiveKernelDump_InsertDmfTriageDataToLiveDump(DmfModule,
                                                                telemetryHandle);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "LiveKernelDump_InsertDmfDataToLiveDump fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    if (NumberOfClientStructures > 0)
    {
        ntStatus = LiveKernelDump_InsertClientTriageDataToLiveDump(telemetryHandle,
                                                                   NumberOfClientStructures,
                                                                   ArrayOfClientStructures);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "LiveKernelDump_InsertClientTriageDataToLiveDump fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    RtlZeroMemory(&nullGuid,
                  sizeof(GUID));

    if ((SizeOfSecondaryData > 0) &&
        !DMF_Utility_IsEqualGUID(&nullGuid,
                                 &moduleConfig->GuidSecondaryData))
    {
        ntStatus = LkmdTelSetSecondaryData(telemetryHandle,
                                           &(moduleConfig->GuidSecondaryData),
                                           SizeOfSecondaryData,
                                           SecondaryDataBuffer);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "LkmdTelSetSecondaryData fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Submit the telemetry report.
    //
    ntStatus = LkmdTelSubmitReport(telemetryHandle);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE_LiveKernelDump, "LkmdTelSubmitReport completed status = %!STATUS!", ntStatus);

Exit:

    if (telemetryHandle != NULL)
    {
        LkmdTelCloseHandle(telemetryHandle);
    }

    FuncExit(DMF_TRACE_LiveKernelDump, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()
#endif  // IS_WIN10_RS3_OR_LATER

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_LiveKernelDump_Destroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy an instance of this Module.
    This callback is used to clear the global pointer.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    PAGED_CODE();

    DMF_ModuleDestroy(DmfModule);
    DmfModule = NULL;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_LiveKernelDump_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type LiveKernelDump.

    NOTE: This function is called during initialization. It initializes the Client Driver's Ring Buffer.
          Since that Ring Buffer will only be used after this function executes, it is not necessary
          to acquire the lock. The lock is only used for accessing the User-mode Crash Dump Data
          Source data structures since they are created dynamically.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_LiveKernelDump* moduleConfig;

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Callback function to allow Client to store the DmfModule.
    //
    if (moduleConfig->LiveKernelDumpFeatureInitialize != NULL)
    {
        moduleConfig->LiveKernelDumpFeatureInitialize(DmfModule);
    }

    return ntStatus;
}
#pragma code_seg()

#if IS_WIN10_RS3_OR_LATER

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
LiveKernelDump_IoctlHandler(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoctlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

   This handles IOCTLS to the LiveKernelDump Module.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Request - Request data, not used
    IoctlCode - IOCTL to be used in the command
    InputBuffer - Input data buffer
    InputBufferSize - Input data buffer size, not used
    OutputBuffer - Output data buffer
    OutputBufferSize - Output data buffer size, not used
    BytesReturned - Amount of data to be sent back

Return Value:

    STATUS_SUCCESS always

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_LiveKernelDump* moduleContext;
    PLIVEKERNELDUMP_INPUT_BUFFER liveDumpInput;
    DMFMODULE liveKernelDumpModule;

    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(BytesReturned);
    UNREFERENCED_PARAMETER(Request);

    PAGED_CODE();

    FuncEntry(DMF_TRACE_LiveKernelDump);

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    liveKernelDumpModule = DMF_ParentModuleGet(DmfModule);

    moduleContext = DMF_CONTEXT_GET(liveKernelDumpModule);

    *BytesReturned = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION,
                DMF_TRACE_LiveKernelDump,
                "Request=0x%p OutputBufferLength=%d InputBufferLength=%d IoControlCode=0x%X",
                Request, (int)OutputBufferSize, (int)InputBufferSize, IoctlCode);

    switch (IoctlCode)
    {
        case IOCTL_LIVEKERNELDUMP_CREATE:
        {
            // It is a request to create a Live Kernel Dump.
            //
            ASSERT(InputBufferSize == sizeof(LIVEKERNELDUMP_INPUT_BUFFER));
            liveDumpInput = (PLIVEKERNELDUMP_INPUT_BUFFER)InputBuffer;
            ntStatus = LiveKernelDump_LiveKernelMemoryDumpCreate(liveKernelDumpModule,
                                                                 liveDumpInput->BugCheckCode,
                                                                 liveDumpInput->BugCheckParameter,
                                                                 liveDumpInput->ExcludeDmfData,
                                                                 liveDumpInput->NumberOfClientStructures,
                                                                 liveDumpInput->ArrayOfClientStructures,
                                                                 liveDumpInput->SizeOfSecondaryData,
                                                                 liveDumpInput->SecondaryDataBuffer);
            break;
        }
        default:
        {
            ASSERT(FALSE);
            ntStatus = STATUS_NOT_SUPPORTED;
            break;
        }
    }

    FuncExit(DMF_TRACE_LiveKernelDump, "%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#endif // IS_WIN10_RS3_OR_LATER

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_LiveKernelDump;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_LiveKernelDump;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if IS_WIN10_RS3_OR_LATER
IoctlHandler_IoctlRecord LiveKernelDump_IoctlSpecification[] =
{
    { IOCTL_LIVEKERNELDUMP_CREATE, sizeof(LIVEKERNELDUMP_INPUT_BUFFER), 0, LiveKernelDump_IoctlHandler, TRUE },
};
#endif // IS_WIN10_RS3_OR_LATER

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_LiveKernelDump_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type LiveKernelDump.

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
    WDF_OBJECT_ATTRIBUTES attributes;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMFMODULE dmfModule;
    DMF_CONTEXT_LiveKernelDump* moduleContext;
    DMF_CONFIG_LiveKernelDump* moduleConfig;
    DMF_CONFIG_RingBuffer ringBufferModuleConfig;
    DATA_BUFFER_SOURCE* dataBufferSource;
#if IS_WIN10_RS3_OR_LATER
    DMF_CONFIG_IoctlHandler ioctlHandlerModuleConfig;
    DMF_CONFIG_BufferQueue bufferQueueModuleConfig;
#endif // IS_WIN10_RS3_OR_LATER
    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_LiveKernelDump);
    DmfCallbacksDmf_LiveKernelDump.ModuleInstanceDestroy = DMF_LiveKernelDump_Destroy;
    DmfCallbacksDmf_LiveKernelDump.DeviceOpen = DMF_LiveKernelDump_Open;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_LiveKernelDump,
                                            LiveKernelDump,
                                            DMF_CONTEXT_LiveKernelDump,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_LiveKernelDump.CallbacksDmf = &DmfCallbacksDmf_LiveKernelDump;
    DmfModuleDescriptor_LiveKernelDump.ModuleConfigSize = sizeof(DMF_CONFIG_LiveKernelDump);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_LiveKernelDump,
                                &dmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    // dmfModule will be set as ParentObject for all child modules.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = dmfModule;

    // Ring Buffer for the data buffer source.
    //
    // RingBuffer
    // ----------
    //
    dataBufferSource = &(moduleContext->DataBufferSource);

    // Zero out the Framework Data Source.
    //
    RtlZeroMemory(dataBufferSource,
                  sizeof(DATA_BUFFER_SOURCE));

    DMF_CONFIG_RingBuffer_AND_ATTRIBUTES_INIT(&ringBufferModuleConfig,
                                              &moduleAttributes);

    ringBufferModuleConfig.ItemCount = LiveKernelDump_DATA_BUFFER_RingBuffer_SIZE;
    ringBufferModuleConfig.ItemSize = sizeof(DATA_BUFFER);
    ringBufferModuleConfig.Mode = RingBuffer_Mode_DeleteOldestIfFullOnWrite;
    moduleAttributes.ClientModuleInstanceName = "DataBufferSource";
    ntStatus = DMF_RingBuffer_Create(Device,
                                     &moduleAttributes,
                                     &attributes,
                                     &dataBufferSource->DmfModuleRingBuffer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "DMF_RingBuffer_Create fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ASSERT(NULL != dataBufferSource->DmfModuleRingBuffer);

#if IS_WIN10_RS3_OR_LATER
    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&ioctlHandlerModuleConfig,
                                                &moduleAttributes);

    ioctlHandlerModuleConfig.DeviceInterfaceGuid = moduleConfig->GuidDeviceInterface;
    ioctlHandlerModuleConfig.AccessModeFilter = IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl;
    ioctlHandlerModuleConfig.EvtIoctlHandlerAccessModeFilter = NULL;
    ioctlHandlerModuleConfig.IoctlRecordCount = ARRAYSIZE(LiveKernelDump_IoctlSpecification);
    ioctlHandlerModuleConfig.IoctlRecords = LiveKernelDump_IoctlSpecification;
    ntStatus = DMF_IoctlHandler_Create(Device,
                                       &moduleAttributes,
                                       &attributes,
                                       &moduleContext->LiveKernelDumpIoctlHandler);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "DMF_IoctlHandler_Create fails: ntStatus=%!STATUS!", ntStatus);
        DMF_Module_Destroy(dmfModule);
        dmfModule = NULL;
        goto Exit;
    }

    // BufferQueue
    // -----------
    //
    DMF_CONFIG_BufferQueue_AND_ATTRIBUTES_INIT(&bufferQueueModuleConfig,
                                               &moduleAttributes);
    bufferQueueModuleConfig.SourceSettings.EnableLookAside = TRUE;
    bufferQueueModuleConfig.SourceSettings.BufferCount = LiveKernelDump_DATA_BUFFER_RingBuffer_SIZE;
    bufferQueueModuleConfig.SourceSettings.BufferSize = sizeof(DATA_BUFFER);
    moduleAttributes.ClientModuleInstanceName = "LiveKernelDumpBufferQueue";
    ntStatus = DMF_BufferQueue_Create(Device,
                                      &moduleAttributes,
                                      &attributes,
                                      &moduleContext->BufferQueue);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE_LiveKernelDump, "DMF_BufferQueue_Create fails: ntStatus=%!STATUS!", ntStatus);
        DMF_Module_Destroy(dmfModule);
        dmfModule = NULL;
        goto Exit;
    }

#endif // IS_WIN10_RS3_OR_LATER

Exit:

    *DmfModule = dmfModule;

    FuncExit(DMF_TRACE_LiveKernelDump, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_LiveKernelDump_DataBufferSourceAdd(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Calls the data buffer source add function if the passed DmfModule is valid.

Arguments:

    DmfModule - The LiveKernelDump Module.
    Buffer - Address of the data buffer.
    BufferLength - Size of the data buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    ntStatus = STATUS_SUCCESS;

    // NOTE: Feature Modules are an exception to the rule in that NULL DMFMODULE may be passed in.
    //       This occurs to support dynamic enable/disable of this feature. If the pointer is NULL
    //       then the function call exits immediately. (This is only allowed for Module Method.)
    //
    if (DmfModule == NULL)
    {
        // Treat this as a NOP.
        //
        goto Exit;
    }

    FuncEntry(DMF_TRACE_LiveKernelDump);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_LiveKernelDump);

    ntStatus = LiveKernelDump_DataBufferSourceAdd(DmfModule,
                                                  Buffer,
                                                  BufferLength);

    FuncExitVoid(DMF_TRACE_LiveKernelDump);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_LiveKernelDump_DataBufferSourceRemove(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Buffer,
    _In_ ULONG BufferLength
    )
/*++

Routine Description:

    Calls the data buffer source remove function if the passed DmfModule is valid.

Arguments:

    DmfModule - The LiveKernelDump Module.
    Buffer - Address of the data buffer.
    BufferLength - Size of the data buffer.

Return Value:

    None

--*/
{
    // NOTE: Feature Modules are an exception to the rule in that NULL DMFMODULE may be passed in.
    //       This occurs to support dynamic enable/disable of this feature. If the pointer is NULL
    //       then the function call exits immediately. (This is only allowed for Module Method.)
    //
    if (DmfModule == NULL)
    {
        // Treat this as a NOP.
        //
        goto Exit;
    }

    FuncEntry(DMF_TRACE_LiveKernelDump);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_LiveKernelDump);

    LiveKernelDump_DataBufferSourceRemove(DmfModule,
                                          Buffer,
                                          BufferLength);

Exit:

    FuncExitVoid(DMF_TRACE_LiveKernelDump);

    return;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_LiveKernelDump_LiveKernelMemoryDumpCreate(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG BugCheckCode,
    _In_ ULONG_PTR BugCheckParameter,
    _In_ BOOLEAN ExcludeDmfData,
    _In_ ULONG NumberOfClientStructures,
    _In_opt_ PLIVEKERNELDUMP_CLIENT_STRUCTURE ArrayOfClientStructures,
    _In_ ULONG SizeOfSecondaryData,
    _In_opt_ VOID* SecondaryDataBuffer
    )
/*++

Routine Description:

    Calls the Live kernel memory dump creation function if the passed DmfModule is valid.
    NOTE: This API can be called from IRQL = passive level only as it references memory pages
    which could be paged out during this call.

Arguments:

    DmfModule - This Module's handle.
    BugCheckCode - The bugcheck code.
    BugCheckParameter - Bugcheck parameter value defined per component. This shows up as the second parameter in the Live Kernel Dump.
    ExcludeDmfData - Indicates if the client wants to exclude DMF data from the mini dump.
    NumberOfClientStructures - Indicates the number of client data structures that the client wants to include to the mini dump.
    ArrayOfClientStructures - An array containing the client data structures that will be included in the mini dump.
    SizeOfSecondaryData - Size of secondary data that needs to be part of the mini dump.
    SecondaryDataBuffer - Pointer to a buffer containing the secondary data.
Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(BugCheckCode);
    UNREFERENCED_PARAMETER(BugCheckParameter);
    UNREFERENCED_PARAMETER(ExcludeDmfData);
    UNREFERENCED_PARAMETER(NumberOfClientStructures);
    UNREFERENCED_PARAMETER(ArrayOfClientStructures);
    UNREFERENCED_PARAMETER(SizeOfSecondaryData);
    UNREFERENCED_PARAMETER(SecondaryDataBuffer);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    // NOTE: Feature Modules are an exception to the rule in that NULL DMFMODULE may be passed in.
    //       This occurs to support dynamic enable/disable of this feature. If the pointer is NULL
    //       then the function call exits immediately. (This is only allowed for Module Method.)
    //
    if (DmfModule == NULL)
    {
        // Treat this as a NOP.
        //
        goto Exit;
    }

    FuncEntry(DMF_TRACE_LiveKernelDump);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_LiveKernelDump);

#if IS_WIN10_RS3_OR_LATER

    ntStatus = LiveKernelDump_LiveKernelMemoryDumpCreate(DmfModule,
                                                         BugCheckCode,
                                                         BugCheckParameter,
                                                         ExcludeDmfData,
                                                         NumberOfClientStructures,
                                                         ArrayOfClientStructures,
                                                         SizeOfSecondaryData,
                                                         SecondaryDataBuffer);

#endif // IS_WIN10_RS3_OR_LATER

    FuncExitVoid(DMF_TRACE_LiveKernelDump);

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_LiveKernelDump_StoreDmfCollectionAsBugcheckParameter(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG_PTR DmfCollection
    )
/*++

Routine Description:

    Helps Store The First Bugcheck Parameter in the Module Context of Live Kernel Dump Module.

Arguments:

    DmfModule - This Module's handle.
    DmfCollection - Handle to the DmfCollection to be stored as a Bugcheck Parameter.

Return Value:

    None

--*/
{
    DMF_CONTEXT_LiveKernelDump* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE_LiveKernelDump);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Lock before updating Module Context to prevent race conditions with live Dump create call.
    //
    DMF_ModuleLock(DmfModule);
    moduleContext->BugCheckParameterDmfCollection = DmfCollection;
    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE_LiveKernelDump);

}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
DMF_LiveKernelDump_DmfDataSizeGet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Returns the size of DMF data stored by the LiveKernelDump Module.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    Size of DMF data.

--*/
{
    DMF_CONTEXT_LiveKernelDump* moduleContext;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    return moduleContext->DmfDataSize;
}
#pragma code_seg()

// eof: Dmf_LiveKernelDump.c
//
