/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_BufferPool.c

Abstract:

    Allows a Client to create a list of buffers, store them in a list,
    and retrieve and add to the list. The Module performs bounds-checking
    on the buffer when its Methods access the buffer. Buffers may have
    optional timers so that buffers can be automatically processed
    after a specified period of time in the list.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Core.h"
#include "DmfModules.Core.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_BufferPool.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Contains a list of buffers and a lookaside buffer from which to allocate more
// buffers if necessary.
//
typedef struct
{
    // Allocations are done from a look aside list if the call requests a buffer but there are
    // no buffers in the list.
    //
    ULONG EnableLookAside;
    // BufferPool mode. Placed here to avoid needing to get Config on every put.
    //
    BufferPool_ModeType BufferPoolMode;
    // List of buffers.
    //
    LIST_ENTRY BufferList;
    // Number of buffers currently in list.
    //
    ULONG NumberOfBuffersInList;
    // Lookaside List for the source of buffers.
    //
    DMF_PORTABLE_LOOKASIDELIST LookasideList;
    // Number of additional buffers allocated besides the initial buffers.
    // When buffers are returned to the list and EnableLookAside is true, if this value is
    // more than zero, the buffer is not added to the list. It is just deleted.
    // This allows us to make sure number of buffers in the list is never more than
    // the initial number of buffers.
    //
    ULONG NumberOfAdditionalBuffersAllocated;
    // For debug purposes.
    //
    ULONG NumberOfBuffersSpecifiedByClient;
    // For debug purposes.
    //
    BOOLEAN BufferPoolEnumerating;
} DMF_CONTEXT_BufferPool;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(BufferPool)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(BufferPool)

// Memory Pool Tag.
//
#define MemoryTag 'oMPB'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//
typedef ULONG BufferPool_SentinelType;
#define BufferPool_Signature        0x87654321
#define BufferPool_SentinelContext  0x11112222
#define BufferPool_SentinelData     0x33334444
#define BufferPool_SentinelSize     sizeof(BufferPool_SentinelType)

typedef struct
{
    // Stores the location of this buffer in the list.
    //
    LIST_ENTRY ListEntry;
    // WDF Memory object for this structure and the client buffer that is
    // located immediately after this structure.
    //
    WDFMEMORY BufferPoolEntryMemory;
    // The associated memory descriptor.
    //
    WDF_MEMORY_DESCRIPTOR MemoryDescriptor;
    // Client buffer memory.
    //
    WDFMEMORY ClientBufferMemory;
    // Timer for buffer in cases where client wants to automatically do processing on
    // entries in list.
    // NOTE: This timer is optionally created so it must be checked prior to use.
    //
    WDFTIMER Timer;
    // For resetting timer again.
    //
    ULONGLONG TimerExpirationMilliseconds;
    // Absolute time for TimerExpirationMilliseconds
    //
    ULONGLONG TimerExpirationAbsoluteTime100ns;
    // Timer Callback function.
    //
    EVT_DMF_BufferPool_TimerCallback* TimerExpirationCallback;
    // Context for this buffer's Timer Expiration Callback.
    //
    VOID* TimerExpirationCallbackContext;
    // NOTE: This pointer points to the end of this structure.
    //
    VOID* ClientBuffer;
    // Client buffer context. Client can store per buffer information here.
    //
    VOID* ClientBufferContext;
    // For validation purposes.
    //
    ULONG SizeOfClientBuffer;
    ULONG BufferContextSize;
    ULONG SizeOfBufferPoolEntry;
    LIST_ENTRY* CurrentlyInsertedList;
    DMFMODULE CurrentlyInsertedDmfModule;
    DMFMODULE CreatedByDmfModule;
    BufferPool_SentinelType* SentinelData;
    BufferPool_SentinelType* SentinelContext;
    ULONG Signature;
} BUFFERPOOL_ENTRY;

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
BufferPool_TimerFieldsClear(
    _In_ DMFMODULE DmfModule,
    _Out_ BUFFERPOOL_ENTRY* BufferPoolEntry
    )
/*++

Routine Description:

    Clears fields associated with timer handling for the given buffer. These fields are 
    used to determine if the timer is enabled so that the timer can be stopped when the 
    buffer is removed from the list. It is essential that the timer be enabled only when 
    the buffer is in the list.

Arguments:

    DmfModule - This Module's handle (for validation purposes).
    BufferPoolEntry - The given buffer.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    BufferPoolEntry->TimerExpirationMilliseconds = 0;
    BufferPoolEntry->TimerExpirationAbsoluteTime100ns = 0;
    BufferPoolEntry->TimerExpirationCallback = NULL;
    BufferPoolEntry->TimerExpirationCallbackContext = NULL;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
BufferPool_RemoveEntryList(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_CONTEXT_BufferPool* ModuleContext,
    _In_ BUFFERPOOL_ENTRY* BufferPoolEntry
    )
/*++

Routine Description:

    Remove a given buffer from the buffer list.

Arguments:

    DmfModule - This Module's handle (for validation purposes).
    ModuleContext - This Module's context.
    BufferPoolEntry - The associated data of the given buffer to remove.

Return Value:

    Void.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    DmfAssert(BufferPoolEntry->CurrentlyInsertedList == &ModuleContext->BufferList);
    DmfAssert(BufferPoolEntry->CurrentlyInsertedDmfModule == DmfModule);
    DmfAssert(ModuleContext->NumberOfBuffersInList > 0);

    RemoveEntryList(&BufferPoolEntry->ListEntry);
    ModuleContext->NumberOfBuffersInList--;

    BufferPoolEntry->CurrentlyInsertedList = NULL;
    BufferPoolEntry->CurrentlyInsertedDmfModule = NULL;
    BufferPoolEntry->ListEntry.Blink = NULL;
    BufferPoolEntry->ListEntry.Flink = NULL;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BUFFERPOOL_ENTRY*
BufferPool_RemoveHeadList(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_CONTEXT_BufferPool* ModuleContext
    )
/*++

Routine Description:

    Remove the first buffer from the list (at the head of the list) in FIFO order.
    If a timer is active for the buffer, this call cancels the timer. If it is unsuccessful
    to cancel the timer, it skips that buffer.

Arguments:

    DmfModule - This Module's handle (for validation purposes).
    ModuleContext - This Module's context.

Return Value:

    The BUFFERPOOL_ENTRY pointer associated with the buffer that is removed or
    NULL if the list if empty.

--*/
{
    BUFFERPOOL_ENTRY* bufferPoolEntry;
    LIST_ENTRY* listEntry;

    UNREFERENCED_PARAMETER(DmfModule);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    bufferPoolEntry = NULL;

    listEntry = ModuleContext->BufferList.Flink;
    while (listEntry != &ModuleContext->BufferList)
    {
        bufferPoolEntry = CONTAINING_RECORD(listEntry,
                                            BUFFERPOOL_ENTRY,
                                            ListEntry);

        // If a timer is set, then try to stop the timer. If the timer cannot be stopped,
        // then do not remove this buffer because its timer callback will be called
        // very soon. This avoids a race condition between removal, enumeration and timer callbacks.
        //
        if (bufferPoolEntry->TimerExpirationCallback != NULL)
        {
            // The timer is running. Try to stop it.
            //
            if (! WdfTimerStop(bufferPoolEntry->Timer,
                               FALSE))
            {
                // Timer callback will be called soon, so skip this buffer.
                // (Try to remove the next buffer.)
                //
                listEntry = listEntry->Flink;
                bufferPoolEntry = NULL;
                continue;
            }
            else
            {
                // The timer has been stopped. The timer callback will not be called.
                // Clears fields associated with timer handling.
                // This buffer will be removed now.
                //
                BufferPool_TimerFieldsClear(DmfModule,
                                            bufferPoolEntry);
            }
            
        }

        DmfAssert(ModuleContext->NumberOfBuffersInList > 0);
        RemoveEntryList(listEntry);

        ModuleContext->NumberOfBuffersInList--;
        DmfAssert(bufferPoolEntry->CurrentlyInsertedList == &ModuleContext->BufferList);
        DmfAssert(bufferPoolEntry->CurrentlyInsertedDmfModule == DmfModule);
        bufferPoolEntry->CurrentlyInsertedList = NULL;
        bufferPoolEntry->CurrentlyInsertedDmfModule = NULL;

        bufferPoolEntry->ListEntry.Blink = NULL;
        bufferPoolEntry->ListEntry.Flink = NULL;

        break;
    }

    return bufferPoolEntry;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BUFFERPOOL_ENTRY*
BufferPool_FirstBufferPeek(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_CONTEXT_BufferPool* ModuleContext
    )
/*++

Routine Description:

    Returns (but does not remove) the first buffer from the list (at the head of the list)
    in FIFO order.

Arguments:

    DmfModule - This Module's handle (for validation purposes).
    ModuleContext - This Module's context.

Return Value:

    The BUFFERPOOL_ENTRY associated with the buffer that is first in the list.

--*/
{
    BUFFERPOOL_ENTRY* bufferPoolEntry;
    LIST_ENTRY* listEntry;

    UNREFERENCED_PARAMETER(DmfModule);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    if (IsListEmpty(&ModuleContext->BufferList))
    {
        bufferPoolEntry = NULL;
        goto Exit;
    }

    DmfAssert(ModuleContext->NumberOfBuffersInList > 0);
    listEntry = ModuleContext->BufferList.Flink;

    bufferPoolEntry = CONTAINING_RECORD(listEntry,
                                        BUFFERPOOL_ENTRY,
                                        ListEntry);

    DmfAssert(bufferPoolEntry->CurrentlyInsertedList == &ModuleContext->BufferList);
    DmfAssert(bufferPoolEntry->CurrentlyInsertedDmfModule == DmfModule);
    DmfAssert(bufferPoolEntry->ListEntry.Blink != NULL);
    DmfAssert(bufferPoolEntry->ListEntry.Flink != NULL);

Exit:

    return bufferPoolEntry;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
BufferPool_InsertTailList(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_CONTEXT_BufferPool* ModuleContext,
    _Inout_ BUFFERPOOL_ENTRY* BufferPoolEntry
    )
/*++

Routine Description:

    Adds a given buffer to the end of the list.

Arguments:

    DmfModule - This Module's handle (for validation purposes).
    ModuleContext - This Module's context.
    BufferPoolEntry - The given buffer.

Return Value:

    None

--*/
{
    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    // Verify that this buffer is not in any other list. Inserting the buffer into
    // more than one list is a fatal error.
    //
    DmfAssert(BufferPoolEntry->ListEntry.Blink == NULL);
    DmfAssert(BufferPoolEntry->ListEntry.Flink == NULL);
    DmfAssert(BufferPoolEntry->CurrentlyInsertedList == NULL);
    DmfAssert(BufferPoolEntry->CurrentlyInsertedDmfModule == NULL);

    // Add to end of list and increment the number of buffers in the list.
    //
    InsertTailList(&ModuleContext->BufferList,
                   &BufferPoolEntry->ListEntry);
    ModuleContext->NumberOfBuffersInList++;

    DmfAssert(((ModuleContext->NumberOfBuffersSpecifiedByClient > 0) && 
              (ModuleContext->NumberOfBuffersInList <= ModuleContext->NumberOfBuffersSpecifiedByClient)) ||
              (0 == ModuleContext->NumberOfBuffersSpecifiedByClient));

    // Remember this for validation purposes.
    //
    BufferPoolEntry->CurrentlyInsertedList = &ModuleContext->BufferList;
    BufferPoolEntry->CurrentlyInsertedDmfModule = DmfModule;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BUFFERPOOL_ENTRY*
BufferPool_BufferPoolEntryGetFromClientBuffer(
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Given a properly formed Client Buffer, retrieve its corresponding BUFFERPOOL_ENTRY.

Arguments:

    ClientBuffer - The given Client Buffer.

Return Value:

    The BUFFERPOOL_ENTRY that corresponds to ClientBuffer.

--*/
{
    BUFFERPOOL_ENTRY* bufferPoolEntry;

    FuncEntry(DMF_TRACE);

    DmfAssert(ClientBuffer != NULL);

    // Given the Client Buffer, get the associated meta data.
    // NOTE: The meta data is located sizeof(BUFFERPOOL_ENTRY) bytes before the Client buffer.
    //
    bufferPoolEntry = ((BUFFERPOOL_ENTRY*)ClientBuffer) - 1;

    DmfVerifierAssert("DMF_BufferPool signature mismatch", 
                      bufferPoolEntry->Signature == BufferPool_Signature);
    DmfVerifierAssert("DMF_BufferPool data sentinel mismatch",
                      *(bufferPoolEntry->SentinelData) == BufferPool_SentinelData);
    DmfVerifierAssert("DMF_BufferPool context sentinel mismatch",
                      *(bufferPoolEntry->SentinelContext) == BufferPool_SentinelContext);

    return bufferPoolEntry;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
BufferPool_BufferPoolEntryPut(
    _In_ DMFMODULE DmfModule,
    _In_ BUFFERPOOL_ENTRY* BufferPoolEntry
    )
/*++

Routine Description:

    Given a BufferPoolEntry corresponding to a Client Buffer, add it to the
    list of buffers.
    NOTE: This function is only used when the Client Driver wants to return an entry
          to the list. This function filters the add and does not add in the case when
          an additional entry has been allocated from the look aside list when the list was empty.

Arguments:

    DmfModule - This Module's handle.
    BufferPoolEntry - The given BufferPool entry corresponding to Client buffer.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferPool* moduleContext;
    WDFMEMORY bufferPoolEntryMemory;

    FuncEntry(DMF_TRACE);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    bufferPoolEntryMemory = BufferPoolEntry->BufferPoolEntryMemory;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->EnableLookAside)
    {
        if (moduleContext->NumberOfAdditionalBuffersAllocated > 0)
        {
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Delete Additional Buffer BufferPoolEntryMemory=0x%p", bufferPoolEntryMemory);
            // Just delete the buffer. It returns to the lookaside list.
            //
            WdfObjectDelete(bufferPoolEntryMemory);
            bufferPoolEntryMemory = NULL;
            // There is one less additional buffer now.
            //
            moduleContext->NumberOfAdditionalBuffersAllocated--;
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "NumberOfAdditionalBuffersAllocated=%d", moduleContext->NumberOfAdditionalBuffersAllocated);
            // Do not add the entry back into the list.
            //
            goto Exit;
        }
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Add Buffer BufferPoolEntryMemory=0x%p NumberOfAdditionalBuffersAllocated=%d", bufferPoolEntryMemory, moduleContext->NumberOfAdditionalBuffersAllocated);

    // Add the buffer to the list. (This function validates that the buffer has
    // not already been added to another list in DEBUG mode.)
    //
    BufferPool_InsertTailList(DmfModule,
                              moduleContext,
                              BufferPoolEntry);

Exit:

    FuncExitVoid(DMF_TRACE);
}

typedef struct
{
    BUFFERPOOL_ENTRY* BufferPoolEntry;
    DMFMODULE DmfModuleInsertedList;
} BUFFERPOOL_TIMER_CONTEXT;

EVT_WDF_TIMER BufferPool_BufferPoolEntryTimerHandler;
WDF_DECLARE_CONTEXT_TYPE(BUFFERPOOL_TIMER_CONTEXT);

VOID
BufferPool_BufferPoolTimerContextSet(
    _In_ BUFFERPOOL_ENTRY* BufferPoolEntry,
    _In_opt_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Store the given DMF Module handle in BufferPoolEntry Timer's context.

Arguments:

    BufferPoolEntry - BufferPool entry which contains the Timer object.
    BufferPoolEntry - The given DMF Module handle.

Return Value:

    None

--*/
{
    BUFFERPOOL_TIMER_CONTEXT* bufferPoolTimerContext;

    DmfAssert(BufferPoolEntry != NULL);
    DmfAssert(BufferPoolEntry->Timer != NULL);

    bufferPoolTimerContext = WdfObjectGet_BUFFERPOOL_TIMER_CONTEXT(BufferPoolEntry->Timer);
    DmfAssert(bufferPoolTimerContext->BufferPoolEntry == BufferPoolEntry);

    bufferPoolTimerContext->DmfModuleInsertedList = DmfModule;
}

VOID
BufferPool_BufferPoolEntryTimerHandler(
    _In_ WDFTIMER WdfTimer
    )
/*++

Routine Description:

    Timer callback. The BufferPool entry corresponding to the timer will be removed 
    from the list, and will be passed to the Client's timer expiration callback.
    Upon timer expiration callback, Client owns the buffer. 

Parameters:

    WdfTimer - The timer object that contains the PBUFFER_LIST_ENTRY.

Return:

    None

--*/
{
    BUFFERPOOL_TIMER_CONTEXT* bufferPoolTimerContext;
    BUFFERPOOL_ENTRY* bufferPoolEntryTimer;
    BUFFERPOOL_ENTRY* bufferPoolEntryInList;
    DMFMODULE dmfModule;
    DMF_CONTEXT_BufferPool* moduleContext;
    EVT_DMF_BufferPool_TimerCallback* timerExpirationCallback;
    LIST_ENTRY* listEntry;

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "BufferPool Entry timer expires");

    // Get the BUFFERPOOL_TIMER_CONTEXT from the WDF Object.
    //
    bufferPoolTimerContext = WdfObjectGet_BUFFERPOOL_TIMER_CONTEXT(WdfTimer);
    DmfAssert(bufferPoolTimerContext != NULL);
    bufferPoolEntryTimer = bufferPoolTimerContext->BufferPoolEntry;
    dmfModule = bufferPoolTimerContext->DmfModuleInsertedList;
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    DMF_ModuleLock(dmfModule);

    timerExpirationCallback = NULL;
    // If timer callback executed, buffer associated with the timer should be present in the list.
    // But it might not be the first one. Search for it.
    //
    listEntry = moduleContext->BufferList.Flink;
    while (listEntry != &moduleContext->BufferList)
    {
        // Prepare to call the enumeration function.
        //
        bufferPoolEntryInList = CONTAINING_RECORD(listEntry,
                                                  BUFFERPOOL_ENTRY,
                                                  ListEntry);

        // Prepare to read the next entry in list at top of loop.
        //
        listEntry = listEntry->Flink;

        // Only remove the buffer if it's in the list AND the Client Driver's
        // callback is not NULL. It is a legitimate case that the callback is NULL if the
        // buffer had been removed from the list and added back without a timer.
        //
        if ((bufferPoolEntryInList == bufferPoolEntryTimer) &&
            (bufferPoolEntryTimer->TimerExpirationCallback) != NULL)
        {
            // Found it. Remove it from list 
            //
            DmfAssert(bufferPoolEntryTimer->Timer != NULL);

            // Remove item from list.
            // (If the Client wants to use this buffer, Client has saved off the buffer in Client's Context).
            // NOTE: Client Driver now owns buffer!
            //
            BufferPool_RemoveEntryList(dmfModule,
                                       moduleContext,
                                       bufferPoolEntryTimer);

            timerExpirationCallback = bufferPoolEntryTimer->TimerExpirationCallback;
            BufferPool_TimerFieldsClear(dmfModule,
                                        bufferPoolEntryTimer);
            
            // The only matching buffer has been found.
            //
            break;
        }

        // Keep searching.
        //
    }

    DMF_ModuleUnlock(dmfModule);

    // Due to race conditions with cancel routines, it is possible the buffer was removed from the list
    // during timer expiration.
    //
    if (timerExpirationCallback != NULL)
    {
        // Call the client driver's timer callback function.
        //
        timerExpirationCallback(dmfModule,
                                bufferPoolEntryTimer->ClientBuffer,
                                bufferPoolEntryTimer->ClientBufferContext,
                                bufferPoolEntryTimer->TimerExpirationCallbackContext);
    }
    else
    {
        // Buffer was removed from the list while timer was expiring.
        //
    }

    FuncExitVoid(DMF_TRACE);
}

#if defined(DMF_USER_MODE)
_IRQL_requires_max_(PASSIVE_LEVEL)
#else
_IRQL_requires_max_(DISPATCH_LEVEL)
#endif // defined(DMF_USER_MODE)
_Must_inspect_result_
NTSTATUS
BufferPool_BufferPoolEntryCreateAndAddToList(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Creates a new Client Buffer BUFFERPOOL_ENTRY and adds the Client Buffer to the
    list of buffers.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_BufferPool* moduleContext;
    DMF_CONFIG_BufferPool* moduleConfig;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFMEMORY memory;
    BUFFERPOOL_ENTRY* bufferPoolEntry;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;
    BUFFERPOOL_TIMER_CONTEXT* bufferPoolTimerContext;

    FuncEntry(DMF_TRACE);

    DmfAssert(DMF_ModuleIsLocked(DmfModule));

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Allocate space for the list entry that holds the meta data for the buffer.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = DMF_Portable_LookasideListCreateMemory(&moduleContext->LookasideList,
                                                      &memory);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreateFromLookaside ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Populate the buffer meta-data.
    //
    bufferPoolEntry = (BUFFERPOOL_ENTRY*)WdfMemoryGetBuffer(memory,
                                                            NULL);

    bufferPoolEntry->Signature = BufferPool_Signature;
    bufferPoolEntry->CreatedByDmfModule = DmfModule;
    bufferPoolEntry->CurrentlyInsertedList = NULL;
    bufferPoolEntry->CurrentlyInsertedDmfModule = NULL;
    bufferPoolEntry->BufferPoolEntryMemory = memory;
    bufferPoolEntry->SizeOfBufferPoolEntry = sizeof(BUFFERPOOL_ENTRY);
    bufferPoolEntry->SizeOfClientBuffer = moduleConfig->Mode.SourceSettings.BufferSize;
    bufferPoolEntry->BufferContextSize = moduleConfig->Mode.SourceSettings.BufferContextSize;
    // The client buffer is located immediately after the buffer list entry.
    //
    bufferPoolEntry->ClientBuffer = (VOID*)(bufferPoolEntry + 1);
    // For validation purposes to check for buffer overrun.
    //
    bufferPoolEntry->SentinelData = (BufferPool_SentinelType*)(((UCHAR*)bufferPoolEntry->ClientBuffer) + bufferPoolEntry->SizeOfClientBuffer);
    *(bufferPoolEntry->SentinelData) = BufferPool_SentinelData;
    // The client buffer context is located immediately after the buffer sentinel data.
    //
    bufferPoolEntry->ClientBufferContext = (UCHAR*)(bufferPoolEntry->SentinelData) + BufferPool_SentinelSize;
    // For validation purposes to check for buffer context overrun.
    //
    bufferPoolEntry->SentinelContext = (BufferPool_SentinelType*)(((UCHAR*)bufferPoolEntry->ClientBufferContext) + bufferPoolEntry->BufferContextSize);
    *(bufferPoolEntry->SentinelContext) = BufferPool_SentinelContext;
    // Timer related.
    //
    if (moduleConfig->Mode.SourceSettings.CreateWithTimer)
    {
        WDF_TIMER_CONFIG_INIT(&timerConfig,
                              BufferPool_BufferPoolEntryTimerHandler);

        WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&timerAttributes,
                                               BUFFERPOOL_TIMER_CONTEXT);
        // NOTE: Make the memory associated with buffer list entry, the parent of the timer.
        // The parent will remain relevant even if the list entry is moved from one collection to another.
        //
        timerAttributes.ExecutionLevel = WdfExecutionLevelPassive;
        timerAttributes.ParentObject = bufferPoolEntry->BufferPoolEntryMemory;

        // Create the timer the first time this API is used. This prevents many unnecessary timers
        // from being created when timers are not used.
        //
        ntStatus = WdfTimerCreate(&timerConfig,
                                  &timerAttributes,
                                  &bufferPoolEntry->Timer);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Save this BUFFERPOOL_ENTRY pointer in the Timer's context.
        //
        bufferPoolTimerContext = WdfObjectGet_BUFFERPOOL_TIMER_CONTEXT(bufferPoolEntry->Timer);
        bufferPoolTimerContext->BufferPoolEntry = bufferPoolEntry;
        bufferPoolTimerContext->DmfModuleInsertedList = DmfModule;
    }
    else
    {
        bufferPoolEntry->Timer = NULL;
    }
    BufferPool_TimerFieldsClear(DmfModule,
                                bufferPoolEntry);
    // List related.
    //
    bufferPoolEntry->ListEntry.Blink = NULL;
    bufferPoolEntry->ListEntry.Flink = NULL;

    // Initialize the client buffer context to all zeros.
    //
    RtlZeroMemory(bufferPoolEntry->ClientBufferContext,
                  bufferPoolEntry->BufferContextSize);

    // Create the Client Memory Handle.
    // Some functions use Memory Descriptors and Offsets. Others use Memory Handles.
    // -----------------------------------------------------------------------------
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = bufferPoolEntry->BufferPoolEntryMemory;

    // Prevent SAL "parameter must not be zero" error.
    //
    #pragma warning(suppress:28160)
    ntStatus = WdfMemoryCreatePreallocated(&objectAttributes,
                                           bufferPoolEntry->ClientBuffer,
                                           bufferPoolEntry->SizeOfClientBuffer,
                                           &bufferPoolEntry->ClientBufferMemory);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreatePreallocated ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&bufferPoolEntry->MemoryDescriptor,
                                      bufferPoolEntry->ClientBufferMemory,
                                      NULL);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Create Buffer: BufferPoolMemory=0x%p SizeOfClientBuffer=%d ClientBufferMemory=0x%p", bufferPoolEntry->BufferPoolEntryMemory, bufferPoolEntry->SizeOfClientBuffer, bufferPoolEntry->ClientBufferMemory);

    // Add the buffer to the list. (This function validates that the buffer has
    // not already been added to another list in DEBUG mode.)
    // NOTE: This entry goes directly into the list. Do not call BufferPool_BufferPoolEntryPut because
    //       that function will filter buffers put into the list and delete the entries when EnableLookAside is TRUE.
    //
    BufferPool_InsertTailList(DmfModule,
                              moduleContext,
                              bufferPoolEntry);

Exit:

    DmfAssert(((moduleContext->NumberOfBuffersSpecifiedByClient > 0) &&
              (moduleContext->NumberOfBuffersInList <= moduleContext->NumberOfBuffersSpecifiedByClient)) ||
              (0 == moduleContext->NumberOfBuffersSpecifiedByClient));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
WDFMEMORY
BufferPool_BufferPoolEntryGet(
    _In_ DMFMODULE DmfModule,
    _Out_ BUFFERPOOL_ENTRY** BufferPoolEntry
    )
/*++

Routine Description:

    Remove the next entry (head of list) if it is present. If it is not present,
    and if the client instantiated the Module with EnableLookAside = TRUE, then a
    new entry is created from the associated lookaside list add added to the list.
    It is removed and returned to the client.

Arguments:

    DmfModule - This Module's handle.
    BufferPoolEntry - The associated BUFFERPOOL_ENTRY.

Return Value:

    NULL means there is no buffer to remove from the list; otherwise, it is the
    WDF Memory of the entry removed from the list.

--*/
{
    DMF_CONTEXT_BufferPool* moduleContext;
    WDFMEMORY returnValue;
    BUFFERPOOL_ENTRY* bufferPoolEntryLocal;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(BufferPoolEntry != NULL);

    DMF_ModuleLock(DmfModule);

    DmfAssert(((moduleContext->NumberOfBuffersSpecifiedByClient > 0) && 
              (moduleContext->NumberOfBuffersInList <= moduleContext->NumberOfBuffersSpecifiedByClient)) ||
              (0 == moduleContext->NumberOfBuffersSpecifiedByClient));

    bufferPoolEntryLocal = BufferPool_FirstBufferPeek(DmfModule,
                                                      moduleContext);
    if (NULL == bufferPoolEntryLocal)
    {
        DmfAssert(moduleContext->NumberOfBuffersInList == 0);
        // If the Client instantiated the Module with EnableLookAside = TRUE,
        // then create a new buffer and add it to the list.
        //
        if (moduleContext->EnableLookAside)
        {
            NTSTATUS ntStatus;

            ntStatus = BufferPool_BufferPoolEntryCreateAndAddToList(DmfModule);
            if (! NT_SUCCESS(ntStatus))
            {
                goto Exit;
            }

            // Track the number of additional buffers beside those initially allocated.
            //
            moduleContext->NumberOfAdditionalBuffersAllocated++;

            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Add Additional Buffer NumberOfAdditionalBuffersAllocated=%d", moduleContext->NumberOfAdditionalBuffersAllocated);

            DmfAssert(((moduleContext->NumberOfBuffersSpecifiedByClient > 0) && 
                      (moduleContext->NumberOfBuffersInList <= moduleContext->NumberOfBuffersSpecifiedByClient)) ||
                      (0 == moduleContext->NumberOfBuffersSpecifiedByClient));

            // We just created and added a new buffer. Now get it from the list.
            //
            bufferPoolEntryLocal = BufferPool_RemoveHeadList(DmfModule,
                                                             moduleContext);
        }
        else
        {
            // Return no buffer because there is no buffer in the list.
            //
            DmfAssert(NULL == bufferPoolEntryLocal);
        }
    }
    else
    {
        bufferPoolEntryLocal = BufferPool_RemoveHeadList(DmfModule,
                                                         moduleContext);
    }

Exit:

    *BufferPoolEntry = bufferPoolEntryLocal;
    if (bufferPoolEntryLocal != NULL)
    {
        returnValue = bufferPoolEntryLocal->BufferPoolEntryMemory;
    }
    else
    {
        returnValue = NULL;
    }

    DmfAssert(((moduleContext->NumberOfBuffersSpecifiedByClient > 0) && 
              (moduleContext->NumberOfBuffersInList <= moduleContext->NumberOfBuffersSpecifiedByClient)) ||
              (0 == moduleContext->NumberOfBuffersSpecifiedByClient));

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Remove Entry: MemoryHandle=0x%p", returnValue);

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "returnValue=0x%p", returnValue);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
VOID*
BufferPool_BufferGet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Remove the next entry (head of list) if it is present. If it is not present,
    and if the Client instantiated the Module with EnableLookAside = TRUE, then a new entry
    is created from the associated lookaside list add added to the list. It is
    removed and returned to the client.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    The address of the Client Buffer retrieved from the list; otherwise NULL to indicate
    that the list is empty.

--*/
{
    WDFMEMORY bufferPoolEntryMemory;
    BUFFERPOOL_ENTRY* bufferPoolEntry;
    VOID* returnValue;

    FuncEntry(DMF_TRACE);

    returnValue = NULL;

    bufferPoolEntryMemory = BufferPool_BufferPoolEntryGet(DmfModule,
                                                          &bufferPoolEntry);
    if (NULL == bufferPoolEntryMemory)
    {
        goto Exit;
    }

    DmfAssert(bufferPoolEntry != NULL);
    DmfVerifierAssert("DMF_BufferPool signature mismatch", 
                      bufferPoolEntry->Signature == BufferPool_Signature);
    DmfVerifierAssert("DMF_BufferPool data sentinel mismatch", 
                      *(bufferPoolEntry->SentinelData) == BufferPool_SentinelData);
    DmfVerifierAssert("DMF_BufferPool context sentinel mismatch", 
                      *(bufferPoolEntry->SentinelContext) == BufferPool_SentinelContext);
    DmfAssert(bufferPoolEntry->BufferPoolEntryMemory == bufferPoolEntryMemory);
    DmfAssert(bufferPoolEntry->ClientBuffer != NULL);
    DmfAssert(sizeof(BUFFERPOOL_ENTRY) == bufferPoolEntry->SizeOfBufferPoolEntry);

    returnValue = bufferPoolEntry->ClientBuffer;

Exit:

    FuncExit(DMF_TRACE, "returnValue=0x%p", returnValue);

    return returnValue;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
BufferPool_BufferPoolCreate(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Creates the lookaside list and allocate the default number of buffers.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_BufferPool* moduleContext;
    DMF_CONFIG_BufferPool* moduleConfig;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    ULONG bufferIndex;
    ULONG sizeOfEachAllocation;

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Populate Module Context.
    //
    moduleContext->EnableLookAside = moduleConfig->Mode.SourceSettings.EnableLookAside;
    DmfAssert((moduleConfig->BufferPoolMode == BufferPool_Mode_Source && 
              ((! moduleConfig->Mode.SourceSettings.EnableLookAside && moduleConfig->Mode.SourceSettings.BufferCount > 0) ||
              moduleConfig->Mode.SourceSettings.EnableLookAside)) ||
              (moduleConfig->BufferPoolMode == BufferPool_Mode_Sink && 
              (! moduleConfig->Mode.SourceSettings.EnableLookAside && moduleConfig->Mode.SourceSettings.BufferCount == 0))
              );
    DmfAssert((moduleConfig->Mode.SourceSettings.CreateWithTimer && moduleConfig->Mode.SourceSettings.BufferCount > 0) ||
              (! moduleConfig->Mode.SourceSettings.CreateWithTimer));
    moduleContext->BufferPoolMode = moduleConfig->BufferPoolMode;
    // NOTE: Allow Source Mode to have zero buffers for cases where no buffers are needed. (For example, an 
    //       input/output stream where input is not used sometimes.)
    //
    DmfAssert((moduleConfig->BufferPoolMode == BufferPool_Mode_Source) ||
              (moduleConfig->BufferPoolMode == BufferPool_Mode_Sink && moduleConfig->Mode.SourceSettings.BufferCount == 0));
    moduleContext->NumberOfBuffersSpecifiedByClient = moduleConfig->Mode.SourceSettings.BufferCount;

#if defined(DMF_USER_MODE)
    // It is not possible to use "PutWithTimer" Method when lookaside list is enabled in User-mode
    // because buffers are deleted in the timer callback which causes the child WDFTIMER to also
    // be deleted. That, in turn, can cause a deadlock and verifier issue.
    //
    if ((moduleConfig->Mode.SourceSettings.CreateWithTimer) &&
        (moduleConfig->Mode.SourceSettings.EnableLookAside))
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_NOT_SUPPORTED;
        goto Exit;
    }
#endif

    // Create the list that holds all the buffers.
    //
    InitializeListHead(&moduleContext->BufferList);
    moduleContext->NumberOfBuffersInList = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Create Buffer List: BufferCount=%d BufferSize=%d",
                moduleConfig->Mode.SourceSettings.BufferCount,
                moduleConfig->Mode.SourceSettings.BufferSize);

    if (moduleConfig->BufferPoolMode == BufferPool_Mode_Source)
    {
        DmfAssert(moduleConfig->Mode.SourceSettings.BufferSize > 0);
        sizeOfEachAllocation = sizeof(BUFFERPOOL_ENTRY) +
                               moduleConfig->Mode.SourceSettings.BufferSize +
                               moduleConfig->Mode.SourceSettings.BufferContextSize +
                               BufferPool_SentinelSize +
                               BufferPool_SentinelSize;

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        objectAttributes.ParentObject = DMF_ParentDeviceGet(DmfModule);

        // 'Error annotation: __formal(3,BufferSize) cannot be zero.'
        // '_Param_(1)' could be '0':  this does not adhere to the specification for the function 'DMF_Portable_LookasideListCreate'. 
        //
        #pragma warning(suppress:28160)
        #pragma warning(suppress:6387)
        ntStatus = DMF_Portable_LookasideListCreate(&objectAttributes,
                                                    sizeOfEachAllocation,
                                                    moduleConfig->Mode.SourceSettings.PoolType,
                                                    &objectAttributes,
                                                    MemoryTag,
                                                    &moduleContext->LookasideList);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Portable_LookasideListCreate ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        DMF_ModuleLock(DmfModule);
        for (bufferIndex = 0; bufferIndex < moduleConfig->Mode.SourceSettings.BufferCount; bufferIndex++)
        {
            // This function cannot be in paged code because this call increases IRQL.
            //
            ntStatus = BufferPool_BufferPoolEntryCreateAndAddToList(DmfModule);
            if (! NT_SUCCESS(ntStatus))
            {
                break;
            }
        }
        DMF_ModuleUnlock(DmfModule);

        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "BufferPool_BufferPoolEntryCreateAndAddToList ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }
    else
    {
        // The list does not allocate any initial buffers.
        //
        ntStatus = STATUS_SUCCESS;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BufferPool_ListFlushAndDestroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Remove all entries from the list making sure that any associated timers
    are stopped.

Parameters:

    DmfModule - This Module's handle.

Return:

    None

--*/
{
    BUFFERPOOL_ENTRY* bufferPoolEntryInList;
    DMF_CONTEXT_BufferPool* moduleContext;
    WDFMEMORY bufferPoolEntryMemory;
    WDFTIMER timer;
    LIST_ENTRY* listEntry;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    // NOTE: It is possible a list may have more entries than when it was initially created.
    //       (For example, a Consumer List is created with zero entries but may have had entries
    //       added to it.)
    //
    DmfAssert((moduleContext->NumberOfBuffersInList <= moduleContext->NumberOfBuffersSpecifiedByClient) ||
              (moduleContext->NumberOfBuffersSpecifiedByClient == 0));

    listEntry = moduleContext->BufferList.Flink;
    while (listEntry != &moduleContext->BufferList)
    {
        // Prepare to call the enumeration function.
        //
        bufferPoolEntryInList = CONTAINING_RECORD(listEntry,
                                                  BUFFERPOOL_ENTRY,
                                                  ListEntry);
        bufferPoolEntryMemory = bufferPoolEntryInList->BufferPoolEntryMemory;

        // Store timer in local variable so that we can wait for it after buffer
        // is deleted outside of lock.
        //
        timer = bufferPoolEntryInList->Timer;

        // Remove from list but do not delete.
        //
        BufferPool_RemoveEntryList(DmfModule,
                                   moduleContext,
                                   bufferPoolEntryInList);

        DMF_ModuleUnlock(DmfModule);

        // List entry is now accessible only by this thread
        // Other threads accessing the collection will not find this list entry and hence will not access it.

        if (timer != NULL)
        {
            // Stop and wait for timer callback to execute.
            // NOTE: Callback will first look in list and see that corresponding
            //       buffer is removed so it will do nothing.
            //
            WdfTimerStop(timer,
                         TRUE);

            // Delete memory for entry.
            //
            WdfObjectDelete(timer);
            timer = NULL;
        }
        WdfObjectDelete(bufferPoolEntryMemory);
        bufferPoolEntryMemory = NULL;

        DMF_ModuleLock(DmfModule);

        // Keep searching.
        //
        listEntry = moduleContext->BufferList.Flink;
    }

    DMF_ModuleUnlock(DmfModule);

    // For debug purposes, make sure the list is empty.
    //
    DmfAssert(moduleContext->BufferList.Blink == &moduleContext->BufferList);
    DmfAssert(moduleContext->BufferList.Flink == &moduleContext->BufferList);

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
BufferPool_BufferPoolDestroy(
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
    DMF_CONTEXT_BufferPool* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    BufferPool_ListFlushAndDestroy(DmfModule);

    // Delete the look aside list.
    //
#if !defined(DMF_USER_MODE)
    if (moduleContext->LookasideList.WdflookasideList != NULL)
    {
        WdfObjectDelete(moduleContext->LookasideList.WdflookasideList);
        moduleContext->LookasideList.WdflookasideList = NULL;
    }
#endif // !defined(DMF_USER_MODE)

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
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_BufferPool_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type BufferPool.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    ntStatus = BufferPool_BufferPoolCreate(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_BufferPool_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type BufferPool.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    BufferPool_BufferPoolDestroy(DmfModule);

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
DMF_BufferPool_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type BufferPool.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_BufferPool;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_BufferPool;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_BufferPool);
    dmfCallbacksDmf_BufferPool.DeviceOpen = DMF_BufferPool_Open;
    dmfCallbacksDmf_BufferPool.DeviceClose = DMF_BufferPool_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_BufferPool,
                                            BufferPool,
                                            DMF_CONTEXT_BufferPool,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_BufferPool.CallbacksDmf = &dmfCallbacksDmf_BufferPool;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_BufferPool,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

#if defined(DEBUG)
    // If Client wants to use paged pool buffers, then Client must instantiate this Module at Passive Level (which this Module allows).
    // To do so, the DmfModuleAttributes->PassiveLevel must equal TRUE.
    //
    // NOTE: Only check this for Source Mode since Sink Mode initializes Pool Type to zero since it is not used.
    //       Zero can mean different pool types on different platforms, e.g., ARM.
    //
    DMF_CONFIG_BufferPool* moduleConfig;
    moduleConfig = DMF_CONFIG_GET(*DmfModule);

    if ((moduleConfig->BufferPoolMode == BufferPool_Mode_Source) &&
        (DMF_IsPoolTypePassiveLevel(moduleConfig->Mode.SourceSettings.PoolType)))
    {
        DmfAssert(DMF_ModuleLockIsPassive(*DmfModule));
    }
#endif

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferPool_ContextGet(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _Out_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Get the context associated with the ClientBuffer.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The Client Buffer.
    ClientBufferContext - Client context associated with the buffer.

Return Value:

    None

--*/
{
    BUFFERPOOL_ENTRY* bufferPoolEntry;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferPool);

    bufferPoolEntry = BufferPool_BufferPoolEntryGetFromClientBuffer(ClientBuffer);

    // For consistency, the Module from which the pool was created must be passed in.
    //
    DmfAssert(bufferPoolEntry->CreatedByDmfModule == DmfModule);

    DmfAssert(bufferPoolEntry->ClientBufferContext == (UCHAR*)(bufferPoolEntry->SentinelData) + BufferPool_SentinelSize);

    DmfAssert(ClientBufferContext != NULL);
    *ClientBufferContext = bufferPoolEntry->ClientBufferContext;

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
DMF_BufferPool_Count(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Return the number of entries currently in the list.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    Number of entries currently in the list

--*/
{
    DMF_CONTEXT_BufferPool* moduleContext;
    ULONG numberOfBuffersInList;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferPool);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);

    numberOfBuffersInList = moduleContext->NumberOfBuffersInList;

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "numberOfBuffersInList=%d", numberOfBuffersInList);

    return numberOfBuffersInList;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferPool_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_BufferPool_Enumeration EntryEnumerationCallback,
    _In_opt_ VOID* ClientDriverCallbackContext,
    _Out_opt_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Enumerate all the buffers in the list, calling a Client Driver's callback function
    for each buffer. If the Client wishes, the buffer can be removed from the list.
    NOTE: Module lock is held during this call.

Arguments:

    DmfModule - This Module's handle.
    EntryEnumerationCallback - Caller's enumeration function called for each buffer in the list.
    ClientDriverCallbackContext - Context passed for this call.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferPool* moduleContext;
    BUFFERPOOL_ENTRY* bufferPoolEntry;
    BOOLEAN doneEnumerating;
    BufferPool_EnumerationDispositionType enumerationDisposition;
    LIST_ENTRY* listEntry;
    BOOLEAN timerWasInQueue;
    ULARGE_INTEGER currentSystemTime;
    LONG64 differenceInTime100ns;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferPool);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->BufferPoolMode == BufferPool_Mode_Sink);

    DMF_ModuleLock(DmfModule);

    differenceInTime100ns = 0;
    doneEnumerating = FALSE;
    if (ClientBuffer != NULL)
    {
        *ClientBuffer = NULL;
    }
    if (ClientBufferContext != NULL)
    {
        *ClientBufferContext = NULL;
    }
    DmfAssert(! moduleContext->BufferPoolEnumerating);
    moduleContext->BufferPoolEnumerating = TRUE;
    listEntry = moduleContext->BufferList.Flink;
    while (! doneEnumerating)
    {
        if (listEntry == &moduleContext->BufferList)
        {
            // No more entries in the list.
            //
            doneEnumerating = TRUE;
            break;
        }

        bufferPoolEntry = CONTAINING_RECORD(listEntry,
                                            BUFFERPOOL_ENTRY,
                                            ListEntry);

        // Prepare to read the next entry in list at top of loop.
        //
        listEntry = listEntry->Flink;

        if (bufferPoolEntry->TimerExpirationCallback != NULL)
        {
            // Temporarily try to stop the timer to prevent future race conditions. 
            //
            if (! WdfTimerStop(bufferPoolEntry->Timer,
                               FALSE))
            {
                // Timer callback will be called soon, so skip this buffer.
                //
                continue;
            }
#if defined(DMF_USER_MODE)
            FILETIME fileTime;
            GetSystemTimeAsFileTime(&fileTime);
            currentSystemTime.QuadPart = (((ULONGLONG)fileTime.dwHighDateTime) << 32) + fileTime.dwLowDateTime;
#else
            KeQuerySystemTime((PLARGE_INTEGER)&currentSystemTime);
#endif
            if (currentSystemTime.QuadPart >= bufferPoolEntry->TimerExpirationAbsoluteTime100ns)
            {
                differenceInTime100ns = 0;
            }
            else
            {
                differenceInTime100ns = currentSystemTime.QuadPart - bufferPoolEntry->TimerExpirationAbsoluteTime100ns;
            }
        }

        DmfAssert(bufferPoolEntry->CurrentlyInsertedList != NULL);
        DmfAssert(bufferPoolEntry->CurrentlyInsertedDmfModule == DmfModule);

        // Call the Caller's Enumeration function.
        //
        enumerationDisposition = EntryEnumerationCallback(DmfModule,
                                                          bufferPoolEntry->ClientBuffer,
                                                          bufferPoolEntry->ClientBufferContext,
                                                          ClientDriverCallbackContext);
        // Determine what Client wants to do now.
        //
        switch (enumerationDisposition)
        {
            case BufferPool_EnumerationDisposition_StopEnumeration:
            {
                // Stop enumerating.
                //
                doneEnumerating = TRUE;
                // Fall through
                //
            }
            case BufferPool_EnumerationDisposition_ContinueEnumeration:
            {
                // Continue enumeration with next item.
                //
                if (bufferPoolEntry->TimerExpirationCallback != NULL)
                {
                    timerWasInQueue = WdfTimerStart(bufferPoolEntry->Timer,
                                                    differenceInTime100ns);
                    DmfAssert(! timerWasInQueue);
                }
                break;
            }
            case BufferPool_EnumerationDisposition_RemoveAndStopEnumeration:
            {
                // Remove the buffer if possible and stop enumerating.
                //
                doneEnumerating = TRUE;
                DmfAssert(ClientBuffer != NULL);

                // The timer has been stopped. Clear the associated fields.
                //
                BufferPool_TimerFieldsClear(DmfModule,
                                            bufferPoolEntry);

                BufferPool_RemoveEntryList(DmfModule,
                                           moduleContext,
                                           bufferPoolEntry);

                DmfAssert(bufferPoolEntry->ClientBuffer != NULL);
                // 'Dereferencing NULL pointer'
                // If Client specifies BufferPool_EnumerationDisposition_RemoveAndStop, Client owns the buffer
                // so Client has to pass a valid ClientBuffer pointer.
                //
                #pragma warning(suppress: 6011)
                *ClientBuffer = bufferPoolEntry->ClientBuffer;

                if (ClientBufferContext != NULL)
                {
                    DmfAssert(bufferPoolEntry->ClientBufferContext == (UCHAR*)(bufferPoolEntry->SentinelData) + BufferPool_SentinelSize);
                    DmfAssert(ClientBufferContext != NULL);
                    *ClientBufferContext = bufferPoolEntry->ClientBufferContext;
                }
                break;
            }
            case BufferPool_EnumerationDisposition_StopTimerAndStopEnumeration:
            {
                doneEnumerating = TRUE;
                // Fall through
                //
            }
            case BufferPool_EnumerationDisposition_StopTimerAndContinueEnumeration:
            {
                // The timer was stopped. Clear the associated fields.
                //
                BufferPool_TimerFieldsClear(DmfModule,
                                            bufferPoolEntry);
                break;
            }
            case BufferPool_EnumerationDisposition_ResetTimerAndStopEnumeration:
            {
                doneEnumerating = TRUE;
                // Fall through
                //
            }
            case BufferPool_EnumerationDisposition_ResetTimerAndContinueEnumeration:
            {
                // Continue enumeration with next item.
                //
                if (bufferPoolEntry->TimerExpirationCallback)
                {
                    timerWasInQueue = WdfTimerStart(bufferPoolEntry->Timer,
                                                    WDF_REL_TIMEOUT_IN_MS(bufferPoolEntry->TimerExpirationMilliseconds));
                    DmfAssert(! timerWasInQueue);
                }
                break;
            }
            default:
            {
                DmfAssert(FALSE);
                break;
            }
        }
    }

    DmfAssert(moduleContext->BufferPoolEnumerating);
    moduleContext->BufferPoolEnumerating = FALSE;

    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferPool_Get(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_opt_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Removes the next buffer in the list (head of the list) if there is a buffer.
    Then, returns the Client Buffer and its associated Client Buffer Context.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The Client Buffer.
    ClientBufferContext - Client context associated with the buffer.

Return Value:

    STATUS_SUCCESS if a buffer is removed from the list.
    STATUS_UNSUCCESSFUL if the list is empty.

--*/
{
    NTSTATUS ntStatus;
    VOID* clientBuffer;
    BUFFERPOOL_ENTRY* bufferPoolEntry;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferPool);

    clientBuffer = BufferPool_BufferGet(DmfModule);
    if (NULL == clientBuffer)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    bufferPoolEntry = BufferPool_BufferPoolEntryGetFromClientBuffer(clientBuffer);

    DmfAssert(bufferPoolEntry->ClientBuffer != NULL);
    *ClientBuffer = bufferPoolEntry->ClientBuffer;

    DmfAssert(bufferPoolEntry->ClientBufferContext == (UCHAR*)(bufferPoolEntry->SentinelData) + BufferPool_SentinelSize);
    if (ClientBufferContext != NULL)
    {
        if (bufferPoolEntry->BufferContextSize > 0)
        {
            *ClientBufferContext = bufferPoolEntry->ClientBufferContext;
        }
        else
        {
            // No ASSERT to maintain compatibility with older Clients.
            //
            *ClientBufferContext = NULL;
        }
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferPool_GetWithMemory(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_ VOID** ClientBufferContext,
    _Out_ WDFMEMORY* ClientBufferMemory
    )
/*++

Routine Description:

    Removes the next buffer in the list (head of the list) if there is a buffer.
    Then, returns the Client Buffer and its associated Client Buffer Context.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The Client Buffer.
    ClientBufferContext - Client context associated with the buffer.
    ClientBufferMemory - WDFMEMORY associated with Client Buffer.

Return Value:

    STATUS_SUCCESS if a buffer is removed from the list.
    STATUS_UNSUCCESSFUL if the list is empty.

--*/
{
    NTSTATUS ntStatus;
    VOID* clientBuffer;
    BUFFERPOOL_ENTRY* bufferPoolEntry;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferPool);

    clientBuffer = BufferPool_BufferGet(DmfModule);
    if (NULL == clientBuffer)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    bufferPoolEntry = BufferPool_BufferPoolEntryGetFromClientBuffer(clientBuffer);

    DmfAssert(bufferPoolEntry->ClientBuffer != NULL);
    *ClientBuffer = bufferPoolEntry->ClientBuffer;

    DmfAssert(bufferPoolEntry->ClientBufferContext == (UCHAR*)(bufferPoolEntry->SentinelData) + BufferPool_SentinelSize);
    DmfAssert(ClientBufferContext != NULL);
    *ClientBufferContext = bufferPoolEntry->ClientBufferContext;

    DmfAssert(ClientBufferMemory != NULL);
    *ClientBufferMemory = bufferPoolEntry->ClientBufferMemory;

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_BufferPool_GetWithMemoryDescriptor(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** ClientBuffer,
    _Out_ PWDF_MEMORY_DESCRIPTOR MemoryDescriptor,
    _Out_ VOID** ClientBufferContext
    )
/*++

Routine Description:

    Removes the next buffer in the list (head of the list) if there is a buffer.
    Then, returns the Client Buffer and its associated Memory Descriptor and ClientBufferContext.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The Client Buffer.
    MemoryDescriptor - WDF Memory Descriptor associated with removed buffer.
    ClientBufferContext - Client context associated with the buffer.

Return Value:

    STATUS_SUCCESS if a buffer is removed from the list.
    STATUS_INSUFFICIENT_RESOURCES if the list is empty.

--*/
{
    NTSTATUS ntStatus;
    VOID* clientBuffer;
    BUFFERPOOL_ENTRY* bufferPoolEntry;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferPool);

    clientBuffer = BufferPool_BufferGet(DmfModule);
    if (NULL == clientBuffer)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    bufferPoolEntry = BufferPool_BufferPoolEntryGetFromClientBuffer(clientBuffer);

    DmfAssert(bufferPoolEntry->ClientBuffer != NULL);
    *ClientBuffer = bufferPoolEntry->ClientBuffer;

    DmfAssert(MemoryDescriptor != NULL);
    *MemoryDescriptor = bufferPoolEntry->MemoryDescriptor;

    DmfAssert(bufferPoolEntry->ClientBufferContext == (UCHAR*)(bufferPoolEntry->SentinelData) + BufferPool_SentinelSize);
    DmfAssert(ClientBufferContext != NULL);
    *ClientBufferContext = bufferPoolEntry->ClientBufferContext;

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

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
    )
/*++

Routine Description:

    Populates parameters with information about the given client buffer.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The given buffer.
    MemoryDescriptor - WDF Memory Descriptor associated with the buffer.
    ClientBufferMemory - WDF Memory Handle associated with the buffer.
    ClientBufferSize - The size of the buffer.
    ClientBufferContext - Client context associated with the buffer.

Return Value:

    None

--*/
{
    BUFFERPOOL_ENTRY* bufferPoolEntry;

    FuncEntry(DMF_TRACE);

    // This function is called while the Module is closing as it is flushing its buffers.
    //
    DMFMODULE_VALIDATE_IN_METHOD_CLOSING_OK(DmfModule,
                                            BufferPool);

    bufferPoolEntry = BufferPool_BufferPoolEntryGetFromClientBuffer(ClientBuffer);

    // For consistency, the Module from which the pool was created must be passed in.
    //
    DmfAssert(bufferPoolEntry->CreatedByDmfModule == DmfModule);

    if (MemoryDescriptor != NULL)
    {
        *MemoryDescriptor = bufferPoolEntry->MemoryDescriptor;
    }

    if (ClientBufferMemory != NULL)
    {
        DmfAssert(bufferPoolEntry->ClientBufferMemory != NULL);
        *ClientBufferMemory = bufferPoolEntry->ClientBufferMemory;
    }

    if (ClientBufferSize != NULL)
    {
        *ClientBufferSize = bufferPoolEntry->SizeOfClientBuffer;
    }

    if (ClientBufferContext != NULL)
    {
        DmfAssert(bufferPoolEntry->ClientBufferContext == (UCHAR*)(bufferPoolEntry->ClientBuffer) + bufferPoolEntry->SizeOfClientBuffer + sizeof(BufferPool_SentinelType));
        *ClientBufferContext = bufferPoolEntry->ClientBufferContext;
    }

    if (ClientBufferContextSize != NULL)
    {
        *ClientBufferContextSize = bufferPoolEntry->BufferContextSize;
    }

    FuncExitVoid(DMF_TRACE);

}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferPool_Put(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer
    )
/*++

Routine Description:

    Adds a Client Buffer to the list.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: This must be a properly formed buffer that was created by this Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_BufferPool* moduleContext;
    BUFFERPOOL_ENTRY* bufferPoolEntry;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD_CLOSING_OK(DmfModule,
                                            BufferPool);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Given the Client Buffer, get the associated meta data.
    //
    bufferPoolEntry = BufferPool_BufferPoolEntryGetFromClientBuffer(ClientBuffer);

    DmfAssert(((moduleContext->BufferPoolMode == BufferPool_Mode_Source) && 
              (bufferPoolEntry->CreatedByDmfModule == DmfModule)) ||
              (moduleContext->BufferPoolMode == BufferPool_Mode_Sink));

    // In Source mode, clear out the buffer before inserting into buffer list.
    // This ensures stale data is removed from the buffer and does not appear when the buffer is re-used.
    //
    if (moduleContext->BufferPoolMode == BufferPool_Mode_Source)
    {
        // Clear the Client Buffer.
        //
        RtlZeroMemory(ClientBuffer,
                      bufferPoolEntry->SizeOfClientBuffer);

        // Clear the Client Buffer Context.
        //
        if (bufferPoolEntry->BufferContextSize > 0)
        {
            DmfAssert(bufferPoolEntry->ClientBufferContext != NULL);
            RtlZeroMemory(bufferPoolEntry->ClientBufferContext,
                          bufferPoolEntry->BufferContextSize);
        }
        DmfAssert(NULL == bufferPoolEntry->TimerExpirationCallback);
        DmfAssert(0 == bufferPoolEntry->TimerExpirationAbsoluteTime100ns);
        DmfAssert(0 == bufferPoolEntry->TimerExpirationMilliseconds);
        DmfAssert(NULL == bufferPoolEntry->TimerExpirationCallbackContext);
    }

    DMF_ModuleLock(DmfModule);

    BufferPool_BufferPoolEntryPut(DmfModule,
                                  bufferPoolEntry);

    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_BufferPool_PutInSinkWithTimer(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientBuffer,
    _In_ ULONGLONG TimerExpirationMilliseconds,
    _In_ EVT_DMF_BufferPool_TimerCallback* TimerExpirationCallback,
    _In_opt_ VOID* TimerExpirationCallbackContext
    )
/*++

Routine Description:

    Adds a Client Buffer to the list and starts a timer. If the buffer is still in the list 
    when the timer expires, buffer will be removed from the list, and the TimerExpirationCallback
    will be called. Client owns the buffer in TimerExpirationCallback.

Arguments:

    DmfModule - This Module's handle.
    ClientBuffer - The buffer to add to the list.
                   NOTE: This must be a properly formed buffer that was created by any instance of Dmf_BufferPool.
    TimerExpirationMilliseconds - Set the optional timer to expire after this many milliseconds.
    TimerExpirationCallback - Callback function to call when timer expires.
    TimerExpirationCallbackContext - This Context will be passed to TimerExpirationCallback.

Return Value:

    TRUE if timer is in system timer queue.
    FALSE otherwise.

--*/
{
    BOOLEAN timerWasInQueue;
    DMF_CONTEXT_BufferPool* moduleContext;
    BUFFERPOOL_ENTRY* bufferPoolEntry;
#if defined(DMF_USER_MODE)
    FILETIME fileTime;
#endif
    ULARGE_INTEGER currentSystemTime;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 BufferPool);

    DmfAssert(TimerExpirationCallback != NULL);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DmfAssert(moduleContext->BufferPoolMode == BufferPool_Mode_Sink);

#if defined(DMF_USER_MODE)
    // It is not possible to use this Method when buffers come from the "lookaside
    // list" in User-mode because the buffers are just allocated and deallocated
    // as needed. The problem is that in the timer callback the buffers are actually
    // deleted and a child of the buffer is the corresponding WDFTIMER which is also
    // deleted because it is a child object. The problem is that deleting the WDFTIMER
    // from inside the timer callback can cause a deadlock and does cause a WDF
    // verifier violation.
    //
    DMF_CONFIG_BufferPool* moduleConfig;
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    DmfAssert(!moduleConfig->Mode.SourceSettings.EnableLookAside);
#endif

    // Given the Client Buffer, get the associated meta data.
    // NOTE: Client Driver (caller) owns the buffer at this time.
    //
    bufferPoolEntry = BufferPool_BufferPoolEntryGetFromClientBuffer(ClientBuffer);
    DmfAssert(bufferPoolEntry->Timer != NULL);

    // NOTE: The timer is guaranteed to be not running,
    //       since it was stop or expired when Client got the buffer.
    //

    DMF_ModuleLock(DmfModule);

    // Set the timer parameters in buffer context.
    //
    DmfAssert(bufferPoolEntry->TimerExpirationCallback == NULL);
    DmfAssert(! moduleContext->BufferPoolEnumerating);
    bufferPoolEntry->TimerExpirationCallback = TimerExpirationCallback;
    bufferPoolEntry->TimerExpirationMilliseconds = TimerExpirationMilliseconds;
#if defined(DMF_USER_MODE)
    GetSystemTimeAsFileTime(&fileTime);
    currentSystemTime.QuadPart = (((ULONGLONG)fileTime.dwHighDateTime) << 32) + fileTime.dwLowDateTime;
#else
    KeQuerySystemTime((PLARGE_INTEGER)&currentSystemTime);
#endif
    bufferPoolEntry->TimerExpirationAbsoluteTime100ns = currentSystemTime.QuadPart + WDF_ABS_TIMEOUT_IN_MS(TimerExpirationMilliseconds);
    bufferPoolEntry->TimerExpirationCallbackContext = TimerExpirationCallbackContext;

    // Save the DmfModule in the Timer's context so that the timer handler knows
    // where to remove the buffer from.
    //
    BufferPool_BufferPoolTimerContextSet(bufferPoolEntry,
                                         DmfModule);

    BufferPool_BufferPoolEntryPut(DmfModule,
                                  bufferPoolEntry);

    // Start the timer. Timer is guaranteed to not have been in the timer queue nor running its callback function.
    // This is because Client has no direct access to the timer. The timer was stopped when the buffer was 
    // previously retrieved.
    //
    timerWasInQueue = WdfTimerStart(bufferPoolEntry->Timer,
                                    WDF_REL_TIMEOUT_IN_MS(TimerExpirationMilliseconds));
    DmfAssert(! timerWasInQueue);

    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_BufferPool.c
//
