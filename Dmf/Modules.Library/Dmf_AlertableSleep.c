/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AlertableSleep.c

Abstract:

    Support for "sleep" operations that are interruptible.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_AlertableSleep.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // The event that interrupts all the delays.
    //
    DMF_PORTABLE_EVENT Event[ALERTABLE_SLEEP_MAXIMUM_TIMERS];
    // Flag that indicates that event has been set. Do not wait anymore.
    //
    BOOLEAN DoNotWait[ALERTABLE_SLEEP_MAXIMUM_TIMERS];
    // Number of Events so that Module Config is not queried often.
    //
    ULONG EventCount;
    // For debug purposes to make sure two threads don't wait on the same event.
    //
    BOOLEAN CurrentlyWaiting[ALERTABLE_SLEEP_MAXIMUM_TIMERS];
    // Indicates that the Module is closing so all attempts to start waiting on an event
    // must fail.
    //
    BOOLEAN Closing;
} DMF_CONTEXT_AlertableSleep;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(AlertableSleep)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(AlertableSleep)

// Memory Pool Tag.
//
#define MemoryTag 'oMSA'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

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
DMF_AlertableSleep_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type AlertableSleep.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AlertableSleep* moduleContext;
    DMF_CONFIG_AlertableSleep* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    // Copy this so that Module Config is not queried often.
    //
    DmfAssert(moduleConfig->EventCount > 0);
    DmfAssert(moduleConfig->EventCount <= ALERTABLE_SLEEP_MAXIMUM_TIMERS);
    moduleContext->EventCount = moduleConfig->EventCount;

    for (ULONG eventIndex = 0; eventIndex < moduleConfig->EventCount; eventIndex++)
    {
        // This object enforces that only a single thread waits on a particular event index.
        // Therefore, SynchronizationEvent is used.
        //
        DMF_Portable_EventCreate(&moduleContext->Event[eventIndex],
                                 SynchronizationEvent,
                                 FALSE);
        // Initialized here in the unlikely case of Open/Close/Open without 
        // a Create before second open. This is something that needs to be
        // addressed in all DMF Modules.
        //
        moduleContext->CurrentlyWaiting[eventIndex] = FALSE;
        moduleContext->DoNotWait[eventIndex] = FALSE;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_AlertableSleep_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type AlertableSleep.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_AlertableSleep* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_ModuleLock(DmfModule);
    DmfAssert(!moduleContext->Closing);
    moduleContext->Closing = TRUE;
    DMF_ModuleUnlock(DmfModule);

    ULONG eventIndex;

    for (eventIndex = 0; eventIndex < moduleContext->EventCount; eventIndex++)
    {
        // Set the event to cause all waiting threads to resume execution.
        //
        DMF_AlertableSleep_Abort(DmfModule,
                                 eventIndex);
    }

    // Now close all the events.
    //
    for (eventIndex = 0; eventIndex < moduleContext->EventCount; eventIndex++)
    {
        // NOTE: In Kernel-mode this is a NOP so it is OK for caller in another
        //       thread to be waiting for event while this happens.
        //
        DMF_Portable_EventClose(&moduleContext->Event[eventIndex]);
    }

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
DMF_AlertableSleep_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type AlertableSleep.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_AlertableSleep;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_AlertableSleep;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_AlertableSleep);
    dmfCallbacksDmf_AlertableSleep.DeviceOpen = DMF_AlertableSleep_Open;
    dmfCallbacksDmf_AlertableSleep.DeviceClose = DMF_AlertableSleep_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_AlertableSleep,
                                            AlertableSleep,
                                            DMF_CONTEXT_AlertableSleep,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_AlertableSleep.CallbacksDmf = &dmfCallbacksDmf_AlertableSleep;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_AlertableSleep,
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

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_AlertableSleep_Abort(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EventIndex
    )
/*++

Routine Description:

    Given an event index, set the corresponding event to allow any waiting thread
    to continue execution.

Arguments:

    DmfModule - This Module's handle.
    EventIndex - Which event the caller wants to interrupt.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AlertableSleep* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // By design this Method can be called by Close callback.
    //
    DMFMODULE_VALIDATE_IN_METHOD_CLOSING_OK(DmfModule,
                                            AlertableSleep);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (EventIndex >= moduleContext->EventCount)
    {
        DmfAssert(FALSE);
        goto Exit;
    }

    DMF_ModuleLock(DmfModule);

    // Don't let the caller wait on this event again because it has been
    // interrupted. Cause the caller to receive STATUS_UNSUCCESSFUL if 
    // a thread tries to wait using this event.
    //
    moduleContext->DoNotWait[EventIndex] = TRUE;

    // Interrupt the wait.
    // NOTE: Another thread may or may not be waiting on this event.
    //
    DMF_Portable_EventSet(&moduleContext->Event[EventIndex]);

    DMF_ModuleUnlock(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_AlertableSleep_ResetForReuse(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EventIndex
    )
/*++

Routine Description:

    Given an event index, restore it to its original uninterrupted state. (Use this function
    to before waiting on an event after it has been interrupted. For example, interrupt 
    in D0Exit and wait after the machine has resumed.)
    NOTE: This function should be used from the WAITING thread, not the INTERRUPTING
          thread to avoid race conditions. Calling from the waiting thread always
          works.

Arguments:

    DmfModule - This Module's handle.
    EventIndex - Which event the caller wants to reset.

Return Value:

    None

--*/
{
    DMF_CONTEXT_AlertableSleep* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 AlertableSleep);

    if (EventIndex >= moduleContext->EventCount)
    {
        DmfAssert(FALSE);
        goto Exit;
    }

    DMF_ModuleLock(DmfModule);

    // Allow the caller to wait on this event.
    //
    moduleContext->DoNotWait[EventIndex] = FALSE;

    // NOTE: Only call this function from the WAITING thread.
    //
    DmfAssert(! moduleContext->CurrentlyWaiting[EventIndex]);

    // Clear the event so that threads will wait.
    //
    DMF_Portable_EventReset(&moduleContext->Event[EventIndex]);

    DMF_ModuleUnlock(DmfModule);

Exit:

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AlertableSleep_Sleep(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EventIndex,
    _In_ ULONG Milliseconds
    )
/*++

Routine Description:

    Given an event index, make the current thread wait:
      1. A given number of milliseconds.
      2. The event is set.
      3. The wait is aborted by another thread.

    TODO: Add version of this API that allows an infinite wait.

Arguments:

    DmfModule - This Module's handle.
    EventIndex - Which event the caller wants to wait on.
    Milliseconds - How long the caller wants to wait.

Return Value:

    STATUS_UNSUCCESSFUL - The timer was interrupted.
    STATUS_SUCCESS - The full delay happened.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_AlertableSleep* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 AlertableSleep);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (EventIndex >= moduleContext->EventCount)
    {
        DmfAssert(FALSE);
        goto Exit;
    }

    DMF_ModuleLock(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Wait EventIndex=%u: Milliseconds%d-ms DoNotWait=%d Closing=%!bool!",
                EventIndex,
                Milliseconds,
                moduleContext->DoNotWait[EventIndex],
                moduleContext->Closing);

    if (moduleContext->Closing)
    {
        // Don't wait on the event if the Module is closing.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Event[%u] closing. Do not wait.", EventIndex);
    }
    else if (moduleContext->DoNotWait[EventIndex])
    {
        // The event has already been set. Do not wait and return unsuccessful.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Event[%u] already interrupted. Do not wait.", EventIndex);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Wait[%u] for %d-ms...", EventIndex, Milliseconds);

        DmfAssert(! moduleContext->CurrentlyWaiting[EventIndex]);
        moduleContext->CurrentlyWaiting[EventIndex] = TRUE;

        // Unlock before waiting.
        //
        DMF_ModuleUnlock(DmfModule);

        // Wait for the time specified by the caller or until the event is set.
        //
        ntStatus = DMF_Portable_EventWaitForSingleObject(&moduleContext->Event[EventIndex],
                                                         &Milliseconds,
                                                         TRUE);

        // Lock again.
        //
        DMF_ModuleLock(DmfModule);

        if (STATUS_TIMEOUT == ntStatus)
        {
            // The thread delayed for the time specified by the caller. It is considered success.
            //
            ntStatus = STATUS_SUCCESS;
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Wait[%u] Satisfied", EventIndex);
        }
        else
        {
            // The thread started running again because the event was set. It means the full delay
            // did not happen. It is considered unsuccessful.
            //
            ntStatus = STATUS_UNSUCCESSFUL;
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Wait[%u] Interrupted", EventIndex);
        }

        moduleContext->CurrentlyWaiting[EventIndex] = FALSE;
    }

    DMF_ModuleUnlock(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_AlertableSleep.c
//
