/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_RingBuffer.h

Abstract:

    Companion file to Dmf_RingBuffer.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// These definitions indicate the mode of Ring Buffer.
//
typedef enum
{
    RingBuffer_Mode_Invalid = 0,
    // If the Ring Buffer is full, an error will occur when Client writes to it.
    //
    RingBuffer_Mode_FailIfFullOnWrite,
    // If the Ring Buffer is full, oldest item will be deleted to make space for new element.
    // Thus, writes never fail.
    //
    RingBuffer_Mode_DeleteOldestIfFullOnWrite,
    RingBuffer_Mode_Maximum,
} RingBuffer_ModeType;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Maximum number of entries to store.
    //
    ULONG ItemCount;
    // The size of each entry.
    //
    ULONG ItemSize;
    // Indicates the mode of the Ring Buffer. 
    //
    RingBuffer_ModeType Mode;
} DMF_CONFIG_RingBuffer;

// This macro declares the following functions:
// DMF_RingBuffer_ATTRIBUTES_INIT()
// DMF_CONFIG_RingBuffer_AND_ATTRIBUTES_INIT()
// DMF_RingBuffer_Create()
//
DECLARE_DMF_MODULE(RingBuffer)

// Callback that allows client to process the Ring Buffer entry in a way that
// the client wants.
//
typedef
_Function_class_(EVT_DMF_RingBuffer_Enumeration)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_RingBuffer_Enumeration(_In_ DMFMODULE DmfModule,
                               _Inout_updates_(BufferSize) UCHAR* Buffer,
                               _In_ ULONG BufferSize,
                               _In_opt_ VOID* CallbackContext);

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Lock,
    _In_ EVT_DMF_RingBuffer_Enumeration* RingBufferEntryCallback,
    _In_opt_ VOID* RingBufferEntryCallbackContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_EnumerateToFindItem(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_RingBuffer_Enumeration* RingBufferEntryCallback,
    _In_opt_ VOID* RingBufferEntryCallbackContext,
    _In_ UCHAR* Item,
    _In_ ULONG ItemSize
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_Read(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(TargetBufferSize) UCHAR* TargetBuffer,
    _In_ ULONG TargetBufferSize
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_ReadAll(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(TargetBufferSize) UCHAR* TargetBuffer,
    _In_ ULONG TargetBufferSize,
    _Out_ ULONG* BytesWritten
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_Reorder(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Lock
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_SegmentsRead(
    _In_ DMFMODULE DmfModule,
    _In_reads_(NumberOfSegments) UCHAR** Segments,
    _In_reads_(NumberOfSegments) ULONG* SegmentSizes,
    _In_reads_(NumberOfSegments) ULONG* SegmentOffsets,
    _In_ ULONG NumberOfSegments
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_SegmentsWrite(
    _In_ DMFMODULE DmfModule,
    _In_reads_(NumberOfSegments) UCHAR** Segments,
    _In_reads_(NumberOfSegments) ULONG* SegmentSizes,
    _In_reads_(NumberOfSegments) ULONG* SegmentOffsets,
    _In_ ULONG NumberOfSegments
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RingBuffer_TotalSizeGet(
    _In_ DMFMODULE DmfModule,
    _Out_ ULONG* TotalSize
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_RingBuffer_Write(
    _In_ DMFMODULE DmfModule,
    _In_reads_(SourceBufferSize) UCHAR* SourceBuffer,
    _In_ ULONG SourceBufferSize
    );

// eof: Dmf_RingBuffer.h
//
