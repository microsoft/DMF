/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_PingPongBuffer.h

Abstract:

    Companion file to Dmf_PingPongBuffer.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // The size of the ping pong buffers.
    //
    ULONG BufferSize;
    // Pool Type.
    // Note: Pool type can be passive if PassiveLevel in Module Attributes is set to TRUE.
    //
    POOL_TYPE PoolType;
} DMF_CONFIG_PingPongBuffer;

// This macro declares the following functions:
// DMF_PingPongBuffer_ATTRIBUTES_INIT()
// DMF_CONFIG_PingPongBuffer_AND_ATTRIBUTES_INIT()
// DMF_PingPongBuffer_Create()
//
DECLARE_DMF_MODULE(PingPongBuffer)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
UCHAR*
DMF_PingPongBuffer_Consume(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG StartOffset,
    _In_ ULONG PacketLength
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
UCHAR*
DMF_PingPongBuffer_Get(
    _In_ DMFMODULE DmfModule,
    _Out_ ULONG* Size
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_PingPongBuffer_Reset(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_PingPongBuffer_Shift(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG StartOffset
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_PingPongBuffer_Write(
    _In_ DMFMODULE DmfModule,
    _In_reads_(NumberOfBytesToWrite) UCHAR* SourceBuffer,
    _In_ ULONG NumberOfBytesToWrite,
    _Out_ ULONG* ResultSize
    );

// eof: Dmf_PingPongBuffer.h
//
