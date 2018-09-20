/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_PingPongBuffer.c

Abstract:

    Implements a ping pong buffer similar to a ring buffer but allows a Client Driver to write/read
    to/from offsets in the buffer. In addition, the object has a function that will automatically
    copy from one buffer to another in the case where a full buffer is followed by a partial buffer.
    This code is useful for cases where incoming data must be validated and parsed to determine
    where valid packets start and end.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_PingPongBuffer.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// There is always a Ping and a Pong buffer.
// (The "Ping" buffer is always populated before becoming the "Pong" buffer. Thus the name, 
// "PingPongBuffer".)
// This value should always be 2.
//
#define NUMBER_OF_PING_PONG_BUFFERS               (2)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // The size of the ping pong buffers.
    //
    ULONG BufferSize;

    // Buffers and offsets.
    // --------------------
    //
    WDFMEMORY BufferMemory[NUMBER_OF_PING_PONG_BUFFERS];
    // Two buffers, one of which is Ping, the other of which is Pong.
    //
    UCHAR* Buffer[NUMBER_OF_PING_PONG_BUFFERS];
    // Indicates which buffer is the Ping Buffer.
    //
    ULONG PingBufferIndex;
    // Each buffer has a Read Offset that skips invalid data.
    //
    ULONG BufferOffsetRead[NUMBER_OF_PING_PONG_BUFFERS];
    // Each buffer has a Write Offset that indicates where the incoming
    // data should be written to.
    //
    ULONG BufferOffsetWrite[NUMBER_OF_PING_PONG_BUFFERS];
} DMF_CONTEXT_PingPongBuffer;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(PingPongBuffer)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(PingPongBuffer)

// Memory Pool Tag.
//
#define MemoryTag 'MBPP'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
PingPongBuffer_PingPongBufferDestroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy the list of buffers and the corresponding lookaside list
    if it is present.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_PingPongBuffer* moduleContext;
    ULONG bufferIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (bufferIndex = 0; bufferIndex < NUMBER_OF_PING_PONG_BUFFERS; bufferIndex++)
    {
        // In cases of fault injection or low resources, all buffers may
        // not be allocated.
        //
        if (moduleContext->BufferMemory[bufferIndex] != NULL)
        {
            WdfObjectDelete(moduleContext->BufferMemory[bufferIndex]);
            moduleContext->BufferMemory[bufferIndex] = NULL;
        }
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
PingPongBuffer_PingPongBufferCreate(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Creates the ping and pong buffers.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_PingPongBuffer* moduleContext;
    DMF_CONFIG_PingPongBuffer* moduleConfig;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    ULONG bufferIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Populate Module Config.
    //
    ASSERT(moduleConfig->BufferSize > 0);
    moduleContext->BufferSize = moduleConfig->BufferSize;

    // Create the collection that holds the buffer list.
    //
    for (bufferIndex = 0; bufferIndex < NUMBER_OF_PING_PONG_BUFFERS; bufferIndex++)
    {
        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        objectAttributes.ParentObject = DmfModule;

        ntStatus = WdfMemoryCreate(&objectAttributes,
                                   moduleConfig->PoolType,
                                   MemoryTag,
                                   moduleContext->BufferSize,
                                   &moduleContext->BufferMemory[bufferIndex],
                                   (VOID* *)&moduleContext->Buffer[bufferIndex]);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        RtlZeroMemory(moduleContext->Buffer[bufferIndex],
                      moduleContext->BufferSize);

        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Buffer[%d]=0x%p BufferMemory[%d]=0x%p", bufferIndex, moduleContext->Buffer[bufferIndex], bufferIndex, moduleContext->BufferMemory[bufferIndex]);
    }

Exit:

    if (! NT_SUCCESS(ntStatus))
    {
        // Clean up all buffers that have been allocated in the above loop in case
        // it terminated in middle. (Mostly for fault injection mode.)
        //
        PingPongBuffer_PingPongBufferDestroy(DmfModule);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
UCHAR*
PingPongBuffer_PongGet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Returns the Pong Buffer.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    The Pong Buffer.

--*/
{
    UCHAR* returnValue;
    ULONG inactivePacketBufferIndex;
    DMF_CONTEXT_PingPongBuffer* moduleContext;

    FuncEntry(DMF_TRACE);

    ASSERT(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    inactivePacketBufferIndex = (moduleContext->PingBufferIndex + 1) % NUMBER_OF_PING_PONG_BUFFERS;

    returnValue = moduleContext->Buffer[inactivePacketBufferIndex];

    FuncExit(DMF_TRACE, "returnValue=0x%p", returnValue);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
UCHAR*
PingPongBuffer_PingGet(
    _In_ DMFMODULE DmfModule,
    _Out_ ULONG* Size
    )
/*++

Routine Description:

    Returns the Ping Buffer.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    The Ping Buffer.

--*/
{
    UCHAR* returnValue;
    DMF_CONTEXT_PingPongBuffer* moduleContext;

    FuncEntry(DMF_TRACE);

    ASSERT(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->PingBufferIndex < NUMBER_OF_PING_PONG_BUFFERS);
    returnValue = moduleContext->Buffer[moduleContext->PingBufferIndex];

    // Return the current write offset address that corresponds with the current active buffer.
    //
    *Size = moduleContext->BufferOffsetWrite[moduleContext->PingBufferIndex];

    // Return the address in the Ping Buffer where the next write should happen.
    //
    ASSERT(*Size <= moduleContext->BufferSize);

    FuncExit(DMF_TRACE, "returnValue=0x%p", returnValue);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
static
UCHAR*
PingPongBuffer_PingWriteOffsetGet(
    _In_ DMFMODULE DmfModule,
    _Out_ ULONG* WriteOffset
    )
/*++

Routine Description:

    Returns the Ping Buffer and its corresponding Write Offset.

Arguments:

    DmfModule - This Module's handle.
    WriteOffset - The offset in the write buffer that is returned to the caller.

Return Value:

    The Ping Buffer where the Client Driver will write to.

--*/
{
    UCHAR* returnValue;
    DMF_CONTEXT_PingPongBuffer* moduleContext;

    FuncEntry(DMF_TRACE);

    ASSERT(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(WriteOffset != NULL);
    ASSERT(moduleContext->PingBufferIndex < NUMBER_OF_PING_PONG_BUFFERS);

    // Return the current write offset address that corresponds with the current active buffer.
    //
    *WriteOffset = moduleContext->BufferOffsetWrite[moduleContext->PingBufferIndex];

    // Return the address in the Ping Buffer where the next write should happen.
    //
    ASSERT(*WriteOffset <= moduleContext->BufferSize);

    // Note that if the buffer is full, the WriteOffset will be outside of the buffer range.
    //
    returnValue = &moduleContext->Buffer[moduleContext->PingBufferIndex][*WriteOffset];

    FuncExit(DMF_TRACE, "returnValue=0x%p", returnValue);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
static
UCHAR*
PingPongBuffer_PingReadOffsetGet(
    _In_ DMFMODULE DmfModule,
    _Out_ ULONG* ReadOffset
    )
/*++

Routine Description:

    Returns the Ping Buffer and its corresponding Read Offset.

Arguments:

    DmfModule - This Module's handle.
    ReadOffset - The offset in the Ping Buffer where the Client Driver will
                 read from (valid start of the Packet).

Return Value:

    The Ping Buffer where the Client Driver will read from.

--*/
{
    UCHAR* returnValue;
    DMF_CONTEXT_PingPongBuffer* moduleContext;

    FuncEntry(DMF_TRACE);

    ASSERT(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(ReadOffset != NULL);
    ASSERT(moduleContext->PingBufferIndex < NUMBER_OF_PING_PONG_BUFFERS);

    // Return the current read offset address that corresponds with the current active buffer.
    //
    *ReadOffset = moduleContext->BufferOffsetRead[moduleContext->PingBufferIndex];

    // Get the current read offset in the current active buffer.
    //
    ASSERT(*ReadOffset <= moduleContext->BufferSize);
    returnValue = &moduleContext->Buffer[moduleContext->PingBufferIndex][*ReadOffset];

    FuncExit(DMF_TRACE, "returnValue=0x%p", returnValue);

    return returnValue;
}

static
VOID
PingPongBuffer_Switch(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG StartOffset,
    _In_ ULONG PacketLength,
    _In_ ULONG WriteOffset,
    _In_ UCHAR* PacketBufferRead
    )
/*++

Routine Description:

    Determines if the Ping Buffer should be switched and switches if necessary. This happens
    when the Ping Buffer will be returned to the caller but there is additional data in the
    buffer (for the next buffer). In this case, the extra data in the Ping Buffer is copied
    to the Pong Buffer, the Pong Buffer is switched and the caller receives the Pong
    Buffer as the buffer with new data.

Arguments:

    DmfModule - This Module's handle.
    StartOffset - Start Offset where valid data starts in the Ping Buffer.
    PacketLength - Amount of valid data in the current buffer.
    WriteOffset - Where the next incoming data will be written. If it is past the end
                  of the valid data, then a copy and switch to Pong Buffer should occur.
    PacketBufferRead - Pointer to the active buffer. 

Return Value:

    None

--*/
{
    UCHAR* activePacket;
    UCHAR* inactivePacket;
    UCHAR* source;
    ULONG numberOfBytesToWrite;
    DMF_CONTEXT_PingPongBuffer* moduleContext;

    FuncEntry(DMF_TRACE);

    ASSERT(DMF_ModuleIsLocked(DmfModule));

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(StartOffset <= moduleContext->BufferSize);
    ASSERT(WriteOffset <= moduleContext->BufferSize);
    ASSERT(PacketLength <= moduleContext->BufferSize);
    ASSERT(PacketBufferRead != NULL);

    numberOfBytesToWrite = 0;

    // If there is additional data after what the caller will read, it is necessary to
    // copy it from the active buffer that the caller will consume into the inactive buffer
    // that will be used for new data.
    //
    if (StartOffset + PacketLength < WriteOffset)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Switch: StartOffset=%d WriteOffset=%d PacketLength=%d", StartOffset, WriteOffset, PacketLength);

        activePacket = moduleContext->Buffer[moduleContext->PingBufferIndex];
        inactivePacket = PingPongBuffer_PongGet(DmfModule);

        // PacketBufferRead will be returned to caller. It has PacketLength of
        // valid data that will be consumed by the caller. Any data after that needs
        // to be copied to the Pong Buffer. Determine the first byte of additional
        // data that needs to be copied.
        //
        source = (UCHAR*)(PacketBufferRead) + PacketLength;

        // Determine how many bytes remain in the Ping Buffer that must now be
        // copied to the Pong Buffer in preparation for the switch of active packets.
        //
        ASSERT((ULONG)(WriteOffset - (source - (UCHAR*)(activePacket))) == (WriteOffset - StartOffset - PacketLength));
        numberOfBytesToWrite = (WriteOffset - StartOffset - PacketLength);

        // Copy the data from Ping Buffer to Pong Buffer.
        //
        RtlCopyMemory(inactivePacket,
                      source,
                      numberOfBytesToWrite);
    }

    // Extra data from the Ping Buffer has been copied to the Pong Buffer. The Ping Buffer
    // will be returned to caller. Now change the Ping Buffer to make the current Pong
    // Buffer the Ping Buffer.
    //
    ASSERT(moduleContext->PingBufferIndex < NUMBER_OF_PING_PONG_BUFFERS);
    moduleContext->PingBufferIndex = (moduleContext->PingBufferIndex + 1) % NUMBER_OF_PING_PONG_BUFFERS;

    // Next write will happen after the data that is just written.
    //
    moduleContext->BufferOffsetWrite[moduleContext->PingBufferIndex] = numberOfBytesToWrite;

    // And reset the corresponding Read Offset.
    //
    moduleContext->BufferOffsetRead[moduleContext->PingBufferIndex] = 0;

    FuncExitVoid(DMF_TRACE);
}

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
DMF_PingPongBuffer_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type PingPongBuffer.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = PingPongBuffer_PingPongBufferCreate(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_PingPongBuffer_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type PingPongBuffer.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    PingPongBuffer_PingPongBufferDestroy(DmfModule);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_PingPongBuffer;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_PingPongBuffer;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_PingPongBuffer_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type PingPongBuffer.

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

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_PingPongBuffer);
    DmfCallbacksDmf_PingPongBuffer.DeviceOpen = DMF_PingPongBuffer_Open;
    DmfCallbacksDmf_PingPongBuffer.DeviceClose = DMF_PingPongBuffer_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_PingPongBuffer,
                                            PingPongBuffer,
                                            DMF_CONTEXT_PingPongBuffer,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_PingPongBuffer.CallbacksDmf = &DmfCallbacksDmf_PingPongBuffer;
    DmfModuleDescriptor_PingPongBuffer.ModuleConfigSize = sizeof(DMF_CONFIG_PingPongBuffer);

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_PingPongBuffer,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

#if defined(DEBUG)
    DMF_CONFIG_PingPongBuffer* moduleConfig;
    moduleConfig = DMF_CONFIG_GET(*DmfModule);

    if (DMF_IsPoolTypePassiveLevel(moduleConfig->PoolType))
    {
        ASSERT(DMF_ModuleLockIsPassive(*DmfModule));
    }
#endif

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
UCHAR*
DMF_PingPongBuffer_Consume(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG StartOffset,
    _In_ ULONG PacketLength
    )
/*++

Routine Description:

    Prepare the ping pong buffer object to return the current Ping Buffer to the Client Driver.
    If it is necessary to copy some data from the Ping Buffer to the Pong buffer, this
    work is done. When the function returns, the caller knows which buffer contains valid
    data for consumption and the Ping Buffer has been prepared for more data.

Arguments:

    DmfModule - This Module's handle.
    StartOffset - The offset in the Ping Buffer where the valid data begins.
    PacketLength - The number of bytes of data in the Ping Buffer.

Return Value:

    The buffer that is ready for use by the Client Driver.

--*/
{
    UCHAR* packetBufferRead;
    ULONG* readOffsetAddress;
    ULONG* writeOffsetAddress;
    ULONG readOffset;
    DMF_CONTEXT_PingPongBuffer* moduleContext;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_PingPongBuffer);

    DMF_ModuleLock(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // The caller will read the valid data from this offset. There may be invalid data
    // before this offset.
    //
    readOffsetAddress = &moduleContext->BufferOffsetRead[moduleContext->PingBufferIndex];
    *readOffsetAddress = StartOffset;

    // Save the buffer pointer that will be returned to the caller.
    //
    packetBufferRead = PingPongBuffer_PingReadOffsetGet(DmfModule,
                                                        &readOffset);

    writeOffsetAddress = &moduleContext->BufferOffsetWrite[moduleContext->PingBufferIndex];

    // Before returning, determine if it is necessary to switch to the next Ping Buffer.
    // If so, switch in preparation for the next incoming raw data.
    //
    PingPongBuffer_Switch(DmfModule,
                          StartOffset,
                          PacketLength,
                          *writeOffsetAddress,
                          packetBufferRead);

    // Read offset has already been used in the above call. Clear it for
    // the next iteration.
    //
    *readOffsetAddress = 0;

    // The new data will be written at the start of the buffer.
    //
    *writeOffsetAddress = 0;

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "packetBufferRead=0x%p", packetBufferRead);

    // This is the new valid buffer the caller will read from.
    //
    return packetBufferRead;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
UCHAR*
DMF_PingPongBuffer_Get(
    _In_ DMFMODULE DmfModule,
    _Out_ ULONG* Size
    )
/*++

Routine Description:

    Returns the Ping Buffer.

Arguments:

    DmfModule - This Module's handle.
    Size - Current size of Ping buffer.

Return Value:

    The Ping Buffer.

--*/
{
    UCHAR* returnValue;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_PingPongBuffer);

    DMF_ModuleLock(DmfModule);

    returnValue = PingPongBuffer_PingGet(DmfModule, 
                                         Size);

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "returnValue=0x%p", returnValue);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_PingPongBuffer_Reset(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Clear the Read/Write offsets of the Ping Buffer.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_PingPongBuffer* moduleContext;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_PingPongBuffer);

    DMF_ModuleLock(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->PingBufferIndex < NUMBER_OF_PING_PONG_BUFFERS);
    moduleContext->BufferOffsetRead[moduleContext->PingBufferIndex] = 0;
    moduleContext->BufferOffsetWrite[moduleContext->PingBufferIndex] = 0;

    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_PingPongBuffer_Shift(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG StartOffset
    )
/*++

Routine Description:

    Cleanup the active buffer by discarding data that has already been processed.
    Copy the remaining data to Pong Buffer and activate it.

Arguments:

    DmfModule - This Module's handle.
    StartOffset - The offset in the Ping Buffer where data that needs to be processed begins.

Return Value:

    None

--*/
{
    UCHAR* activePacket;
    UCHAR* inactivePacket;
    ULONG numberOfBytes;
    ULONG writeOffset;
    ULONG* writeOffsetAddress;
    ULONG readOffset;
    ULONG* readOffsetAddress;
    DMF_CONTEXT_PingPongBuffer* moduleContext;

    FuncEntry(DMF_TRACE);

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_PingPongBuffer);

    DMF_ModuleLock(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    readOffsetAddress = &moduleContext->BufferOffsetRead[moduleContext->PingBufferIndex];
    readOffset = *readOffsetAddress;
    ASSERT(StartOffset >= readOffset);
    ASSERT(StartOffset <= moduleContext->BufferSize);

    // Calculate number of bytes that need to be copied from Ping to Pong buffer.
    //
    writeOffsetAddress = &moduleContext->BufferOffsetWrite[moduleContext->PingBufferIndex];
    writeOffset = *writeOffsetAddress;
    ASSERT(StartOffset <= writeOffset);
    numberOfBytes = writeOffset - StartOffset;

    activePacket = moduleContext->Buffer[moduleContext->PingBufferIndex];
    inactivePacket = PingPongBuffer_PongGet(DmfModule);

    // Copy the data from Ping Buffer to Pong Buffer.
    //
    RtlCopyMemory(inactivePacket,
                  activePacket + StartOffset,
                  numberOfBytes);

    // Data from the Ping Buffer has been copied to the Pong Buffer. 
    // Now activate the current Pong Buffer.
    //
    ASSERT(moduleContext->PingBufferIndex < NUMBER_OF_PING_PONG_BUFFERS);
    moduleContext->PingBufferIndex = (moduleContext->PingBufferIndex + 1) % NUMBER_OF_PING_PONG_BUFFERS;

    // Next write will happen after the data that is just written.
    //
    moduleContext->BufferOffsetWrite[moduleContext->PingBufferIndex] = numberOfBytes;

    // And reset the corresponding Read Offset.
    //
    moduleContext->BufferOffsetRead[moduleContext->PingBufferIndex] = 0;

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "PingBufferIndex=%d, PingBuffer=0x%p, BytesToProcess:%d",
             moduleContext->PingBufferIndex,
             activePacket,
             numberOfBytes);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_PingPongBuffer_Write(
    _In_ DMFMODULE DmfModule,
    _In_reads_(NumberOfBytesToWrite) UCHAR* SourceBuffer,
    _In_ ULONG NumberOfBytesToWrite,
    _Out_ ULONG* ResultSize
    )
/*++

Routine Description:

    Writes data into the Ping Buffer and updates its corresponding Write Offset.

Arguments:

    DmfModule - This Module's handle.
    SourceBuffer - The buffer of bytes to write.
    NumberOfBytesToWrite - The number of bytes to read from the above buffer and write
                            to the target buffer.
    ResultSize - The next location where the next write of data should occur after this call.

Return Value:

    STATUS_SUCCESS is always expected.
    STATUS_UNSUCCESSFUL means the Client is trying to write an improper amount of data.

--*/
{
    DMF_CONTEXT_PingPongBuffer* moduleContext;
    UCHAR* activeBuffer;
    ULONG writeOffsetAddress;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    // It should always succeed if Client is not doing invalid operations.
    //
    ntStatus = STATUS_SUCCESS;

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_PingPongBuffer);

    DMF_ModuleLock(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // This is the current buffer and location where transferred data should be written for the caller.
    //
    activeBuffer = PingPongBuffer_PingWriteOffsetGet(DmfModule,
                                                     &writeOffsetAddress);
    ASSERT(moduleContext->BufferSize >= NumberOfBytesToWrite);

    // Check if the Client is trying to perform an invalid write into the ping pong buffer. This should never happen
    // because the Client should have allocated a properly size buffer.
    //
    if (writeOffsetAddress + NumberOfBytesToWrite > moduleContext->BufferSize)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                    "New data is too large for Ping Buffer BufferSize=%d NumberOfBytesToWrite=%d WriteOffset=%d",
                    moduleContext->BufferSize,
                    NumberOfBytesToWrite,
                    writeOffsetAddress);
        ASSERT(FALSE);
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    // Copy new the buffer data into the current write location.
    //
    ASSERT(moduleContext->PingBufferIndex < NUMBER_OF_PING_PONG_BUFFERS);
    ASSERT(((UCHAR*)activeBuffer) + NumberOfBytesToWrite <= (&moduleContext->Buffer[moduleContext->PingBufferIndex][moduleContext->BufferSize]));
    RtlCopyMemory(activeBuffer,
                  SourceBuffer,
                  NumberOfBytesToWrite);

    // Update the write offset for the amount just written.
    //
    moduleContext->BufferOffsetWrite[moduleContext->PingBufferIndex] += NumberOfBytesToWrite;
    activeBuffer = PingPongBuffer_PingWriteOffsetGet(DmfModule,
                                                     &writeOffsetAddress);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Write %d bytes to activeBuffer=0x%p from SourceBuffer=0x%p WriteOffset=%d", NumberOfBytesToWrite, activeBuffer, SourceBuffer, writeOffsetAddress);

Exit:

    // Tell the caller the updated write offset.
    //
    *ResultSize = writeOffsetAddress;

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_PingPongBuffer.c
//
