/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_BufferPool.h

Abstract:

    Companion file to Dmf_BufferPool.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// These definitions indicate the mode of BufferPool.
// Indicates how the Client will use the BufferPool.
//
typedef enum
{
    BufferPool_Mode_Invalid = 0,
    // Initialized with list of empty buffers.
    //
    BufferPool_Mode_Source,
    // Initialized with zero buffers.
    //
    BufferPool_Mode_Sink,
    BufferPool_Mode_Maximum,
} BufferPool_ModeType;

// These definitions indicate what the enumerator does after calling the 
// Client's enumeration callback.
//
typedef enum
{
    BufferPool_EnumerationDisposition_Invalid = 0,
    // Continue enumerating.
    //
    BufferPool_EnumerationDisposition_ContinueEnumeration,
    // Stop enumerating.
    //
    BufferPool_EnumerationDisposition_StopEnumeration,
    // Remove the enumerated buffer and stop enumerating.
    // (Client now owns the buffer).
    //
    BufferPool_EnumerationDisposition_RemoveAndStopEnumeration,
    // Stop the timer associated with the buffer and stop enumerating.
    //
    BufferPool_EnumerationDisposition_StopTimerAndStopEnumeration,
    // Stop the timer associated with the buffer and continue enumerating.
    //
    BufferPool_EnumerationDisposition_StopTimerAndContinueEnumeration,
    // Reset the timer associated with the buffer and continue enumerating.
    //
    BufferPool_EnumerationDisposition_ResetTimerAndStopEnumeration,
    // Reset the timer associated with the buffer and continue enumerating.
    //
    BufferPool_EnumerationDisposition_ResetTimerAndContinueEnumeration,
    BufferPool_EnumerationDisposition_Maximum,
} BufferPool_EnumerationDispositionType;

// Callback function called by DMF_BufferPool_Enumerate.
//
typedef
_Function_class_(EVT_DMF_BufferPool_Enumeration)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BufferPool_EnumerationDispositionType
EVT_DMF_BufferPool_Enumeration(_In_ DMFMODULE DmfModule,
                               _In_ VOID* ClientBuffer,
                               _In_ VOID* ClientBufferContext,
                               _In_opt_ VOID* ClientDriverCallbackContext);

// Callback function called when a buffer's timer expires.
//
typedef
_Function_class_(EVT_DMF_BufferPool_TimerCallback)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_BufferPool_TimerCallback(_In_ DMFMODULE DmfModule,
                                 _In_ VOID* ClientBuffer,
                                 _In_ VOID* ClientBufferContext,
                                 _In_opt_ VOID* ClientDriverCallbackContext);

// Settings for BufferPool_Mode_Source.
//
typedef struct
{
    // Maximum number of entries to store.
    //
    ULONG BufferCount;
    // The size of each entry.
    //
    ULONG BufferSize;
    // Size of client buffer context.
    //
    ULONG BufferContextSize;
    // Indicates if a look aside list should be used.
    //
    ULONG EnableLookAside;
    // Indicates if a timer is created with the buffer.
    // Use this flag if the buffers from this list will be added to 
    // another list using *WithTimer API.
    //
    ULONG CreateWithTimer;
    // Pool Type.
    // Note: Pool type can be passive if PassiveLevel in Module Attributes is set to TRUE.
    //
    POOL_TYPE PoolType;
} BufferPool_SourceSettings;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Indicates the mode of BufferPool.
    //
    BufferPool_ModeType BufferPoolMode;
    union
    {
        // Each mode has its own settings. 
        // NOTE: Currently no custom settings are required for Sink mode.
        //
        BufferPool_SourceSettings SourceSettings;
    } Mode;
} DMF_CONFIG_BufferPool;

// This macro declares the following functions:
// DMF_BufferPool_ATTRIBUTES_INIT()
// DMF_CONFIG_BufferPool_AND_ATTRIBUTES_INIT()
// DMF_BufferPool_Create()
//
DECLARE_DMF_MODULE(BufferPool)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferPool_ContextGet(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _Out_ VOID** ClientBufferContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
DMF_BufferPool_Count(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferPool_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_BufferPool_Enumeration EntryEnumerationCallback,
    _In_opt_ VOID* ClientDriverCallbackContext,
    _Out_opt_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferPool_Get(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferPool_GetWithMemory(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_ VOID** ClientBufferContext,
    _Out_ WDFMEMORY* ClientBufferMemory
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferPool_GetWithMemoryDescriptor(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_ PWDF_MEMORY_DESCRIPTOR MemoryDescriptor,
    _Out_ VOID** ClientBufferContext
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferPool_ParametersGet(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _Out_opt_ PWDF_MEMORY_DESCRIPTOR MemoryDescriptor,
    _Out_opt_ WDFMEMORY* ClientBufferMemory,
    _Out_opt_ ULONG* ClientBufferSize,
    _Out_opt_ VOID** ClientBufferContext,
    _Out_opt_ ULONG* ClientBufferContextSize
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferPool_Put(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferPool_PutInSinkWithTimer(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ ULONGLONG TimerExpirationMilliseconds,
    _In_ EVT_DMF_BufferPool_TimerCallback* TimerExpirationCallback,
    _In_opt_ VOID* TimerExpirationCallbackContext
    );

// eof: Dmf_BufferPool.h
//
