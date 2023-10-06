/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Stack.h

Abstract:

    Companion file to Dmf_Stack.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Maximum number of entries to store.
    //
    ULONG StackDepth;
    // The size of each entry.
    //
    ULONG StackElementSize;
} DMF_CONFIG_Stack;

// This macro declares the following functions:
// DMF_Stack_ATTRIBUTES_INIT()
// DMF_CONFIG_Stack_AND_ATTRIBUTES_INIT()
// DMF_Stack_Create()
//
DECLARE_DMF_MODULE(Stack)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
ULONG
DMF_Stack_Depth(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Stack_Flush(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Stack_Pop(
    _In_ DMFMODULE DmfModule,
    _Out_writes_bytes_(ClientBufferSize) VOID* ClientBuffer,
    _In_ size_t ClientBufferSize
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Stack_Push(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );

// eof: Dmf_Stack.h
//
