/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_RingBuffer.c

Abstract:

    Ring Buffer Implementation using Ring Buffer in Memory.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Core.h"
#include "DmfModules.Core.Trace.h"

#include "Dmf_RingBuffer.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef DECLSPEC_ALIGN(32) PLONG PLONG_A32;

typedef struct
{
    // Memory handle or memory that store the item data.
    //
    WDFMEMORY MemoryRing;
    // Size of each item.
    //
    ULONG ItemSize;
    // The entries in the Ring Buffer.
    //
    UCHAR* Items;
    // Current Read Address.
    //
    UCHAR* ReadPointer;
    // Current Write Address.
    //
    UCHAR* WritePointer;
    // End of Buffer.
    //
    UCHAR* BufferEnd;
    // Total size of the Ring Buffer.
    //
    ULONG TotalSize;
    // Indicates the mode of the Ring Buffer.
    //
    RingBuffer_ModeType Mode;
    // Number of items Ring Buffer can hold.
    //
    ULONG ItemsCount;
    // Items present in Ring Buffer.
    //
    ULONG ItemsPresentCount;
} RING_BUFFER;

typedef struct
{
    // Pointer to the Item being searched.
    //
    UCHAR* Item;
    // Size of the Item being searched.
    //
    ULONG ItemSize;
    // Callback  function to call if the Item is found.
    //
    EVT_DMF_RingBuffer_Enumeration* CallbackIfFound;
    // Context to pass to the callback function when it is called.
    //
    VOID* CallbackContextIfFound;
} BUFFER_TO_FIND;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Contains all the Ring Buffer management constructs.
    //
    RING_BUFFER RingBuffer;
} DMF_CONTEXT_RingBuffer;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(RingBuffer)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(RingBuffer)

// Memory Pool Tag.
//
#define MemoryTag 'oMBR'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
RingBuffer_ReadPointerIncrement(
    _Inout_ RING_BUFFER* RingBuffer
    )
/*++

Routine Description:

    Increment the Read Pointer, properly wrapping around when necessary. Incrementing the Read Pointer happens
    from two different locations.

Arguments:

    RingBuffer - The Ring Buffer management data.

Return Value:

    None

--*/
{
    ASSERT(RingBuffer != NULL);
    ASSERT(RingBuffer->ItemSize > 0);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                "ReadPointer=%d", (LONG)((RingBuffer->ReadPointer - RingBuffer->Items) / RingBuffer->ItemSize));

    RingBuffer->ReadPointer += RingBuffer->ItemSize;
    ASSERT(RingBuffer->ReadPointer <= RingBuffer->BufferEnd);
    ASSERT(RingBuffer->ReadPointer >= RingBuffer->Items);
    if (RingBuffer->ReadPointer == RingBuffer->BufferEnd)
    {
        RingBuffer->ReadPointer = RingBuffer->Items;
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Wrap Read RingBuffer->ReadPointer");
    }

    // An item has been read. There is now one less item.
    //
    RingBuffer->ItemsPresentCount--;
    ASSERT(RingBuffer->ItemsPresentCount < RingBuffer->ItemsCount);
}

// Callback that allows client to copy into the Ring Buffer item in a way that
// the client wants (not just full buffer overwrite).
//
typedef void (*RingBuffer_ItemProcessCallbackType)(_Inout_ VOID* Context,
                                                   _Inout_updates_(BufferSize) UCHAR* Buffer,
                                                   _In_ ULONG BufferSize);

void
RingBuffer_ItemProcessCallbackRead(
    _Inout_ VOID* Context,
    _Inout_updates_(ItemSize) UCHAR* Item,
    _In_ ULONG ItemSize
    )
/*++

Routine Description:

    Read a certain number of bytes from the Ring Buffer entry. This callback is basically
    the same as RtlCopyMemory (default callback).

Arguments:

    Context - Caller context (target buffer).
    Item - Address of beginning of buffer to read from.
    ItemSize - Number of bytes to read from the buffer.

Return Value:

    None

--*/
{
    UCHAR* targetBuffer;

    targetBuffer = (UCHAR*)Context;
    RtlCopyMemory(targetBuffer,
                  Item,
                  ItemSize);
}

void
RingBuffer_ItemProcessCallbackWrite(
    _Inout_ VOID* Context,
    _Inout_updates_(ItemSize) UCHAR* Item,
    _In_ ULONG ItemSize
)
/*++

Routine Description:

    Write a certain number of bytes to the Ring Buffer entry.This callback is basically
    the same as RtlCopyMemory (default callback).

Arguments:

    Context - Caller context (source buffer).
    Item - Address of beginning of buffer to write to.
    ItemSize - Number of bytes to write to the buffer.

Return Value:

    None

--*/
{
    UCHAR* sourceBuffer;

    sourceBuffer = (UCHAR*)Context;
    RtlCopyMemory(Item,
                  sourceBuffer,
                  ItemSize);
}

// Context that contains a map of the data in a Ring Buffer entry.
//
typedef struct
{
    // Pointer to each segment of the buffer.
    //
    UCHAR** Segments;
    // Offset for each segment.
    //
    ULONG* SegmentOffsets;
    // Size of each segment.
    //
    ULONG* SegmentSizes;
    // Number of segments.
    //
    ULONG NumberOfSegments;
    // Data copy function.
    //
    RingBuffer_ItemProcessCallbackType DataCopy;
} RingBuffer_CustomItemProcessContext;

void
RingBuffer_ItemProcessCallbackSegments(
    _Inout_ VOID* Context,
    _Inout_updates_(ItemSize) UCHAR* Item,
    _In_ ULONG ItemSize
    )
/*++

Routine Description:

    Read/Write from/to a Ring Buffer entry using a map that maps the contents
    of a Ring Buffer entry. This callback receives a table of offsets and lengths
    to direct how memory is read from the buffer. This allows the Ring Buffer to
    have entries that contain sub buffers with headers/payloads.

Arguments:

    Context - Caller context (target buffer).
    Item - Address of beginning of buffer to read from.
    ItemSize - Number of bytes to read from the buffer.

Return Value:

    None

--*/
{
    RingBuffer_CustomItemProcessContext* writeContext;
    ULONG bufferIndex;

    UNREFERENCED_PARAMETER(ItemSize);

    writeContext = (RingBuffer_CustomItemProcessContext*)Context;
    ASSERT(writeContext->DataCopy != NULL);

    for (bufferIndex = 0; bufferIndex < writeContext->NumberOfSegments; bufferIndex++)
    {
        // Client data buffer.
        //
        UCHAR* clientSegmentBuffer = writeContext->Segments[bufferIndex];
        // Offset in the Ring Buffer entry that where data transfer happens.
        //
        ULONG segmentOffset = writeContext->SegmentOffsets[bufferIndex];
        ASSERT(segmentOffset < ItemSize);
        // Address in Ring Buffer entry where the transfer happens.
        //
        UCHAR* ringBufferSegmentBuffer = &Item[segmentOffset];
        // Size of the data transfer.
        //
        ULONG segmentSize = writeContext->SegmentSizes[bufferIndex];

        ASSERT(segmentSize > 0);
        ASSERT(segmentOffset + segmentSize <= ItemSize);

        // Transfer the data to/from the Ring Buffer.
        //
        (*writeContext->DataCopy)(clientSegmentBuffer,
                                  ringBufferSegmentBuffer,
                                  segmentSize);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
static
NTSTATUS
RingBuffer_Write(
    _Inout_ RING_BUFFER* RingBuffer,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_ RingBuffer_ItemProcessCallbackType ItemProcessCallback
    )
/*++

Routine Description:

    Write data to the Ring Buffer.

Arguments:

    RingBuffer - The Ring Buffer management data.
    Buffer - Address of data to write to the Write Pointer.
    BufferSize - Amount of data in bytes to write to the Write Pointer.
    ItemProcessCallback - Callback function that writes into the ring buffer entry.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    ntStatus = STATUS_SUCCESS;

    ASSERT(RingBuffer != NULL);
    ASSERT(Buffer != NULL);
    ASSERT(RingBuffer->ItemSize > 0);
    ASSERT(RingBuffer->ItemsPresentCount <= RingBuffer->ItemsCount);

    if (RingBuffer->ItemsPresentCount == RingBuffer->ItemsCount)
    {
        ASSERT(RingBuffer->ReadPointer == RingBuffer->WritePointer);
        // It means the buffer is full.
        //
        if (RingBuffer->Mode == RingBuffer_Mode_FailIfFullOnWrite)
        {
            // Ring Buffer is Full. This is an error condition.
            //
            ntStatus = STATUS_UNSUCCESSFUL;
            goto Exit;
        }
        else if (RingBuffer->Mode == RingBuffer_Mode_DeleteOldestIfFullOnWrite)
        {
            // Ring Buffer is full, but it is infinite. So, just throw away the oldest pending Read
            // to make space for this Write.
            //
            RingBuffer_ReadPointerIncrement(RingBuffer);
        }
        else
        {
            ASSERT(FALSE);
        }
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                "WritePointer=%d BufferSize=%d",
                (LONG)((RingBuffer->WritePointer - RingBuffer->Items) / RingBuffer->ItemSize),
                BufferSize);

    // Although everything has been validated by this point, both trusted and untrusted callers,
    // make a run time check to make sure BufferSize is equal to the size of each entry.
    // NOTE: This should *never* happen because trusted callers only call this function.
    //
    if (BufferSize != RingBuffer->ItemSize)
    {
        ASSERT(FALSE);
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    // Although everything has been validated by this point, both trusted and untrusted callers,
    // make a run time check to make sure the write does not exceed the size of the Ring Buffer.
    // NOTE: This should *never* happen because trusted callers only call this function.
    //
    if ((RingBuffer->WritePointer + BufferSize) > RingBuffer->BufferEnd)
    {
        ASSERT(FALSE);
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    // Write to the Ring Buffer entry in a caller specific manner.
    //
    (*ItemProcessCallback)(Buffer,
                           RingBuffer->WritePointer,
                           RingBuffer->ItemSize);

    // Move the Write Pointer to the next proper location.
    //
    RingBuffer->WritePointer += RingBuffer->ItemSize;
    ASSERT(RingBuffer->WritePointer <= RingBuffer->BufferEnd);
    ASSERT(RingBuffer->WritePointer >= RingBuffer->Items);
    if (RingBuffer->WritePointer == RingBuffer->BufferEnd)
    {
        RingBuffer->WritePointer = RingBuffer->Items;
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Wrap Read RingBuffer->WritePointer");
    }

    // An item has just been written (added), so increment the number of items in the buffer.
    //
    RingBuffer->ItemsPresentCount++;
    ASSERT(RingBuffer->ItemsPresentCount <= RingBuffer->ItemsCount);

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
static
NTSTATUS
RingBuffer_Read(
    _Inout_ RING_BUFFER* RingBuffer,
    _Out_writes_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_ RingBuffer_ItemProcessCallbackType ItemProcessCallback
    )
/*++

Routine Description:

    Read data from the Ring Buffer.

Arguments:

    RingBuffer - The Ring Buffer management data.
    Buffer - Address of data to copy data read from the Read Pointer.
    BufferSize - Amount of data in bytes to read from the Read Pointer.
    ItemProcessCallback - Callback function that reads from the ring buffer entry.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(BufferSize);

    ASSERT(RingBuffer != NULL);
    ASSERT(RingBuffer->ItemSize > 0);
    ASSERT(Buffer != NULL);
    ASSERT(RingBuffer->ItemsPresentCount <= RingBuffer->ItemsCount);

    ntStatus = STATUS_SUCCESS;

    if (0 == RingBuffer->ItemsPresentCount)
    {
        // There are no items in the buffer to read.
        //
        ASSERT(RingBuffer->ReadPointer == RingBuffer->WritePointer);
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                "ReadPointer=%d", (LONG)((RingBuffer->ReadPointer - RingBuffer->Items) / RingBuffer->ItemSize));

    ASSERT(BufferSize == RingBuffer->ItemSize);

    // Read from the Ring Buffer entry in a caller specific manner.
    // Suppress 6001: "*Buffer not initialized." It is because Buffer is either pointer or table to callback.
    //
    #pragma warning(suppress: 6001)
    (ItemProcessCallback)(Buffer,
                          RingBuffer->ReadPointer,
                          RingBuffer->ItemSize);

    // Now, increment the read pointer.
    //
    RingBuffer_ReadPointerIncrement(RingBuffer);
    ASSERT(RingBuffer->ItemsPresentCount < RingBuffer->ItemsCount);

Exit:

    return ntStatus;
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
RingBuffer_Create(
    _In_ DMFMODULE DmfModule,
    _Inout_ RING_BUFFER* RingBuffer,
    _In_ ULONG ItemCount,
    _In_ ULONG ItemSize,
    _In_ RingBuffer_ModeType Mode
    )
/*++

Routine Description:

    Create a Ring Buffer using parameters from the caller.

Arguments:

    DmfModule - This Module's handle.
    RingBuffer - The Ring Buffer management data that will be initialized.
    ItemCount - Number of entries in the Ring Buffer.
    ItemSize - Size in bytes of each entry in the Ring Buffer.
    Mode - Indicates the mode of Ring Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    ASSERT(RingBuffer != NULL);
    ASSERT(NULL == RingBuffer->Items);

    if (0 == ItemSize)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        ASSERT(FALSE);
        goto Exit;
    }

    if (0 == ItemCount)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        ASSERT(FALSE);
        goto Exit;
    }

    // Create space for the Ring Buffer entries.
    // The +1 is for extra swap space used only by this object.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    size_t sizeToAllocate = ((ItemCount + 1) * ItemSize);
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               sizeToAllocate,
                               &RingBuffer->MemoryRing,
                               (VOID**)&RingBuffer->Items);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ASSERT(RingBuffer->MemoryRing != NULL);
    ASSERT(RingBuffer->Items != NULL);

    // Initialize the space to zero in case not all space is used up.
    //
    RtlZeroMemory(RingBuffer->Items,
                  sizeToAllocate);

    // Initialize the Ring Buffer management entries.
    //
    RingBuffer->ReadPointer = RingBuffer->Items;
    RingBuffer->WritePointer = RingBuffer->Items;
    RingBuffer->ItemSize = ItemSize;
    RingBuffer->BufferEnd = RingBuffer->Items + (RingBuffer->ItemSize * ItemCount);
    RingBuffer->TotalSize = RingBuffer->ItemSize * ItemCount;
    RingBuffer->Mode = Mode;
    RingBuffer->ItemsCount = ItemCount;
    RingBuffer->ItemsPresentCount = 0;

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
NTSTATUS
RingBuffer_Delete(
    _In_ RING_BUFFER* RingBuffer
    )
/*++

Routine Description:

    Destroy a Ring Buffer.

Arguments:

    RingBuffer - The Ring Buffer management data that will be destroyed.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    ASSERT(RingBuffer != NULL);

    if (RingBuffer->MemoryRing != NULL)
    {
        ASSERT(RingBuffer->Items != NULL);
        WdfObjectDelete(RingBuffer->MemoryRing);
        RingBuffer->MemoryRing = NULL;
        RingBuffer->Items = NULL;
    }

    return STATUS_SUCCESS;
}
#pragma code_seg()

BOOLEAN
RingBuffer_ItemMatch(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _In_ VOID* BufferToFind
    )
/*++

Routine Description:

    This function compares two buffers and calls client specified callback if they match.

Arguments:

    DmfModule - This Module's handle.
    Buffer - Pointer to a ring buffer entry.
    BufferSize - Size of the Buffer.
    BufferToFind - Pointer to a structure containing details of the Buffer to find.

Return Value:

    TRUE so that all items in Ring Buffer are enumerated.

--*/
{
    BUFFER_TO_FIND* bufferToFind;

    bufferToFind = (BUFFER_TO_FIND*)BufferToFind;

    ASSERT(bufferToFind->ItemSize <= BufferSize);

    // Check if this Buffer matches the bufferToFind.
    //
    if (RtlCompareMemory(Buffer,
                         bufferToFind->Item,
                         bufferToFind->ItemSize) ==
        bufferToFind->ItemSize)
    {
        // Found a matching buffer, call the callback supplied by Client.
        //
        bufferToFind->CallbackIfFound(DmfModule,
                                      Buffer,
                                      BufferSize,
                                      bufferToFind->CallbackContextIfFound);
    }

    // Continue enumeration.
    //
    return TRUE;
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
DMF_RingBuffer_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type RingBuffer.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_RingBuffer* moduleContext;
    DMF_CONFIG_RingBuffer* moduleConfig;

    PAGED_CODE();

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = RingBuffer_Create(DmfModule,
                                 &moduleContext->RingBuffer,
                                 moduleConfig->ItemCount,
                                 moduleConfig->ItemSize,
                                 moduleConfig->Mode);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_RingBuffer_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type RingBuffer.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    DMF_CONTEXT_RingBuffer* moduleContext;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RingBuffer_Delete(&moduleContext->RingBuffer);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_RingBuffer;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_RingBuffer;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
)
/*++

Routine Description:

    Create an instance of a DMF Module of type RingBuffer.

Arguments:

    Device - Client Driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_RingBuffer);
    DmfCallbacksDmf_RingBuffer.DeviceOpen = DMF_RingBuffer_Open;
    DmfCallbacksDmf_RingBuffer.DeviceClose = DMF_RingBuffer_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_RingBuffer,
                                            RingBuffer,
                                            DMF_CONTEXT_RingBuffer,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_RingBuffer.CallbacksDmf = &DmfCallbacksDmf_RingBuffer;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_RingBuffer,
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

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Lock,
    _In_ EVT_DMF_RingBuffer_Enumeration* RingBufferItemCallback,
    _In_opt_ VOID* RingBufferItemCallbackContext
    )
/*++

Routine Description:

    Calls Client provided function for all ring buffer entries.

Arguments:

    DmfModule - This Module's handle.
    Lock - Indicates if this Method should lock the Ring Buffer during enumeration.
    RingBufferItemCallback - Client provided callback function.
    RingBufferItemCallbackContext - Context passed to the callback.

Return Value:

    None

--*/
{
    DMF_CONTEXT_RingBuffer* moduleContext;
    RING_BUFFER* ringBuffer;
    UCHAR* readPointer;
    UCHAR* writePointer;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_RingBuffer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // When this Method is executed from a Crash Dump Handler, it must not lock since
    // the lock may already be held.
    //
    if (Lock)
    {
        DMF_ModuleLock(DmfModule);
    }

    ringBuffer = &(moduleContext->RingBuffer);

    readPointer = ringBuffer->ReadPointer;
    writePointer = ringBuffer->WritePointer;

    // Check if ring buffer is empty.
    //
    ASSERT(ringBuffer->ItemsPresentCount <= ringBuffer->ItemsCount);
    if (0 == ringBuffer->ItemsPresentCount)
    {
        ASSERT(readPointer == writePointer);
        goto Exit;
    }

    ASSERT(ringBuffer->ItemSize > 0);

    BOOLEAN continueEnumeration;

    do
    {
        // Enumerate each entry and call the client supplied callback.
        //
        continueEnumeration = RingBufferItemCallback(DmfModule,
                                                     readPointer,
                                                     ringBuffer->ItemSize,
                                                     RingBufferItemCallbackContext);
        // Update the read pointer.
        //
        readPointer += ringBuffer->ItemSize;

        if (readPointer == ringBuffer->BufferEnd)
        {
            readPointer = ringBuffer->Items;
        }
    } 
    while (continueEnumeration &&
           (readPointer != writePointer));

Exit:

    if (Lock)
    {
        DMF_ModuleUnlock(DmfModule);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_EnumerateToFindItem(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_RingBuffer_Enumeration* RingBufferItemCallback,
    _In_opt_ VOID* RingBufferItemCallbackContext,
    _In_ UCHAR* Item,
    _In_ ULONG ItemSize
    )
/*++

Routine Description:

    Calls Client provided function for ring buffer entries that match the client provided buffer.

Arguments:

    DmfModule - This Module's handle.
    RingBufferItemCallback - Client provided callback function.
    RingBufferItemCallbackContext - Context passed to the callback.
    Item - A buffer with data that the client is looking for.
    ItemSize - Size of the data that the client is looking for.

Return Value:

    None

--*/
{
    BUFFER_TO_FIND bufferToFind;
    DMF_CONTEXT_RingBuffer* moduleContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_RingBuffer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    bufferToFind.Item = Item;
    bufferToFind.ItemSize = ItemSize;
    bufferToFind.CallbackIfFound = RingBufferItemCallback;
    bufferToFind.CallbackContextIfFound = RingBufferItemCallbackContext;

    ASSERT(bufferToFind.ItemSize <= moduleContext->RingBuffer.ItemSize);

    DMF_RingBuffer_Enumerate(DmfModule,
                             TRUE,
                             RingBuffer_ItemMatch,
                             &bufferToFind);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_Read(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(TargetBufferSize) UCHAR* TargetBuffer,
    _In_ ULONG TargetBufferSize
    )
/*++

Routine Description:

    Read data from the Ring Buffer. (Reads the whole entry.)

Arguments:

    DmfModule - This Module's handle.
    TargetBuffer - Address of data to copy data read from the Read Pointer.
    TargetBufferSize - Amount of data in bytes to read from the Read Pointer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_RingBuffer* moduleContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_RingBuffer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    ASSERT(TargetBufferSize == moduleContext->RingBuffer.ItemSize);
    ntStatus = RingBuffer_Read(&moduleContext->RingBuffer,
                               TargetBuffer,
                               TargetBufferSize,
                               RingBuffer_ItemProcessCallbackRead);

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_ReadAll(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(TargetBufferSize) UCHAR* TargetBuffer,
    _In_ ULONG TargetBufferSize,
    _Out_ ULONG* BytesWritten
    )
/*++

Routine Description:

    Capture data from the Ring Buffer. (Reads the full Ring Buffer.)

Arguments:

    DmfModule - This Module's handle.
    TargetBuffer - Address of buffer to store data in.
    TargetBufferSize - Size of Buffer.
    BytesWritten - Number of actual bytes written.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_RingBuffer* moduleContext;
    NTSTATUS ntStatus;
    ULONG entriesRead;
    ULONG sizeOfEachItem;

    UNREFERENCED_PARAMETER(TargetBufferSize);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_RingBuffer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext != NULL);
    ASSERT(NULL != TargetBuffer);
    ASSERT(TargetBufferSize >= moduleContext->RingBuffer.TotalSize);

    ntStatus = STATUS_UNSUCCESSFUL;

    DMF_ModuleLock(DmfModule);

    entriesRead = 0;
    sizeOfEachItem = moduleContext->RingBuffer.ItemSize;
    ASSERT(sizeOfEachItem > 0);
    do
    {
        ntStatus = RingBuffer_Read(&moduleContext->RingBuffer,
                                   TargetBuffer,
                                   sizeOfEachItem,
                                   RingBuffer_ItemProcessCallbackRead);
        if (! NT_SUCCESS(ntStatus))
        {
            break;
        }
        TargetBuffer += sizeOfEachItem;
        entriesRead++;
    } while (NT_SUCCESS(ntStatus));

    ASSERT(BytesWritten != NULL);
    *BytesWritten = entriesRead * sizeOfEachItem;

    DMF_ModuleUnlock(DmfModule);

    return STATUS_SUCCESS;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_Reorder(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Lock
    )
/*++

Routine Description:

    Reorder the elements of the Ring Buffer data so that elements are sorted by
    oldest at the beginning of the buffer to the newest at the end.
    NOTE: This function is called in unlocked state since it is designed to be used by 
          crash dump processing. If you need to use this for other purpose, be sure
          to acquire this Module's lock!.

Arguments:

    DmfModule - This Module's handle.
    Lock - Set to TRUE if the this Method should lock the Module while performing the
           reorder operation.

Return Value:

    None

--*/
{
    DMF_CONTEXT_RingBuffer* moduleContext;
    UCHAR* addressToOverwrite;
    UCHAR* addressOfDataToMove;
    UCHAR* endOfRingBuffer;
    UCHAR* addressForSwap;
    ULONG swapsNeeded;
    RING_BUFFER* ringBuffer;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_RingBuffer);

    // When this Method is executed from a Crash Dump Handler, it must not lock since
    // the lock may already be held.
    //
    if (Lock)
    {
        DMF_ModuleLock(DmfModule);
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ringBuffer = &moduleContext->RingBuffer;

    // The beginning of the Ring Buffer's memory. This is the address of the first
    // byte that will be output during a crash dump.
    //
    addressToOverwrite = ringBuffer->Items;
    // The location of Read Pointer.
    //
    addressOfDataToMove = ringBuffer->ReadPointer;
    // The end of the Ring Buffer data area.
    //
    endOfRingBuffer = ringBuffer->BufferEnd;
    addressForSwap = endOfRingBuffer;

    ASSERT(ringBuffer->ItemsPresentCount <= ringBuffer->ItemsCount);
    if (ringBuffer->ItemsPresentCount == 0)
    {
        // The buffer is empty. Nothing to reorder.
        //
        ASSERT(ringBuffer->ReadPointer == ringBuffer->WritePointer);
        goto Exit;
    }

    // Copy all the items from the read pointer until the write pointer or until
    // the end of the ring buffer.
    //
    // TODO: Swapping always happens leftward. An optimization would be to choose the
    //       direction with fewest swaps. (m items swapped leftward n times require only
    //       (m - n) swaps rightward.) Also, by adding conditions it is possible to 
    //       reduce the number of shifts of empty spaces (by using Write - Items and 
    //       adding checks for conditions).
    //
    swapsNeeded = (ULONG)((ringBuffer->ReadPointer - ringBuffer->Items) / ringBuffer->ItemSize);
    while (swapsNeeded > 0)
    {
        // Copy entry to be overwritten to temporary swap location.
        //
        RtlCopyMemory(addressForSwap,
                      addressToOverwrite,
                      ringBuffer->ItemSize);

        // There is now space to shift, so shift.
        //
        UCHAR* addressOfDataToShift = addressToOverwrite + ringBuffer->ItemSize;
        size_t bytesToMove = endOfRingBuffer - addressOfDataToShift;
        RtlMoveMemory(addressToOverwrite,
                      addressOfDataToShift,
                      bytesToMove);

        // Copy from temporary swap location to complete swap.
        //
        UCHAR* destinationAddress = endOfRingBuffer - ringBuffer->ItemSize;
        RtlCopyMemory(destinationAddress,
                      addressForSwap,
                      ringBuffer->ItemSize);

        swapsNeeded--;
    }

    // Update the Read and Write pointers.
    //
    ringBuffer->ReadPointer = ringBuffer->Items;
    ringBuffer->WritePointer = ringBuffer->Items + 
                               (ringBuffer->ItemsPresentCount * ringBuffer->ItemSize);
    if (ringBuffer->WritePointer == endOfRingBuffer)
    {
        // This case occurs when the Ring Buffer is full.
        //
        ringBuffer->WritePointer = ringBuffer->Items;
    }

Exit:

    // Erase all items that are not present. (Erase stale data.)
    //
    ULONG numberOfItemsToClear =  ringBuffer->ItemsCount - ringBuffer->ItemsPresentCount;
    UCHAR* eraseStartAddress = endOfRingBuffer - (numberOfItemsToClear * ringBuffer->ItemSize);
    RtlZeroMemory(eraseStartAddress,
                  (numberOfItemsToClear * ringBuffer->ItemSize));

    if (Lock)
    {
        DMF_ModuleUnlock(DmfModule);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_SegmentsRead(
    _In_ DMFMODULE DmfModule,
    _In_reads_(NumberOfSegments) UCHAR** Segments,
    _In_reads_(NumberOfSegments) ULONG* SegmentSizes,
    _In_reads_(NumberOfSegments) ULONG* SegmentOffsets,
    _In_ ULONG NumberOfSegments
    )
/*++

Routine Description:

    Read data from the Ring Buffer. (Reads via a map of segments.)

Arguments:

    DmfModule - This Module's handle.
    Segments - Table of addresses to read (each segment) (target buffer).
    SegmentSizes - The size of each segment (size of each target buffer).
    SegmentOffsets - The offset in the Ring Buffer entry of each segment.
    NumberOfSegments - Number of segments in the above tables.

Return Value:

    NTSTATUS indicates if the Ring Buffer is empty.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_RingBuffer* moduleContext;
    RingBuffer_CustomItemProcessContext customItemProcessContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_RingBuffer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    customItemProcessContext.Segments = Segments;
    customItemProcessContext.SegmentSizes = SegmentSizes;
    customItemProcessContext.SegmentOffsets = SegmentOffsets;
    customItemProcessContext.NumberOfSegments = NumberOfSegments;
    customItemProcessContext.DataCopy = RingBuffer_ItemProcessCallbackRead;

    DMF_ModuleLock(DmfModule);

    ntStatus = RingBuffer_Read(&moduleContext->RingBuffer,
                               (UCHAR*)&customItemProcessContext,
                               moduleContext->RingBuffer.ItemSize,
                               RingBuffer_ItemProcessCallbackSegments);

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_SegmentsWrite(
    _In_ DMFMODULE DmfModule,
    _In_reads_(NumberOfSegments) UCHAR** Segments,
    _In_reads_(NumberOfSegments) ULONG* SegmentSizes,
    _In_reads_(NumberOfSegments) ULONG* SegmentOffsets,
    _In_ ULONG NumberOfSegments
    )
/*++

Routine Description:

    Write data to the Ring Buffer. (Writes via a map of segments.)

Arguments:

    DmfModule - This Module's handle.
    Segments - Table of addresses to write (each segment) (source buffer).
    SegmentSizes - The size of each segment (size of each source buffer).
    SegmentOffsets - The offset in the Ring Buffer entry of each segment.
    NumberOfSegments - Number of segments in the above tables.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_RingBuffer* moduleContext;
    RingBuffer_CustomItemProcessContext customItemProcessContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_RingBuffer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    customItemProcessContext.Segments = Segments;
    customItemProcessContext.SegmentSizes = SegmentSizes;
    customItemProcessContext.SegmentOffsets = SegmentOffsets;
    customItemProcessContext.NumberOfSegments = NumberOfSegments;
    customItemProcessContext.DataCopy = RingBuffer_ItemProcessCallbackWrite;

    DMF_ModuleLock(DmfModule);

    ntStatus = RingBuffer_Write(&moduleContext->RingBuffer,
                                (UCHAR*)&customItemProcessContext,
                                moduleContext->RingBuffer.ItemSize,
                                RingBuffer_ItemProcessCallbackSegments);

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_TotalSizeGet(
    _In_ DMFMODULE DmfModule,
    _Out_ ULONG* TotalSize
    )
/*++

Routine Description:

    Return the total size of the Ring Buffer in bytes.

Arguments:

    DmfModule - This Module's handle.
    TotalSize - Receives the size of the Ring Buffer.

Return Value:

    None

--*/
{
    DMF_CONTEXT_RingBuffer* moduleContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_RingBuffer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // No need to lock because this value never changes after initialization.
    //

    *TotalSize = moduleContext->RingBuffer.TotalSize;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_Write(
    _In_ DMFMODULE DmfModule,
    _In_reads_(SourceBufferSize) UCHAR* SourceBuffer,
    _In_ ULONG SourceBufferSize
    )
/*++

Routine Description:

    Write data to the Ring Buffer. (Writes the whole entry.)

Arguments:

    DmfModule - This Module's handle.
    SourceBuffer - Address of data to write to the Write Pointer.
    SourceBufferSize - Amount of data in bytes to write to the Write Pointer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_RingBuffer* moduleContext;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_RingBuffer);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    ASSERT(SourceBufferSize <= moduleContext->RingBuffer.ItemSize);
    ntStatus = RingBuffer_Write(&moduleContext->RingBuffer,
                                SourceBuffer,
                                SourceBufferSize,
                                RingBuffer_ItemProcessCallbackWrite);

    DMF_ModuleUnlock(DmfModule);

    return ntStatus;
}

// eof: Dmf_RingBuffer.c
//
