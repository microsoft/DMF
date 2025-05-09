/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_NotifyUserWithEvent.c

Abstract:

    Provides support for opening and setting User-mode events in Kernel-mode.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_NotifyUserWithEvent.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_NotifyUserWithEvent
{
    // This event is shared between Kernel-mode and User-mode.
    // NOTE: This array can be allocated dynamically but it is not very big.
    // If more data per event is added later, that should be considered.
    //
    HANDLE NotifyUserWithEvent[NotifyUserWithEvent_MAXIMUM_EVENTS];
} DMF_CONTEXT_NotifyUserWithEvent;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(NotifyUserWithEvent)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(NotifyUserWithEvent)

// Memory Pool Tag.
//
#define MemoryTag 'EWUN'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
NotifyUserWithEvent_EventCreate(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EventIndex
    )
/*++

Routine Description:

    Create a shared event between Kernel-mode and User-mode.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCCESS - The event was created.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_NotifyUserWithEvent* moduleContext;
    DMF_CONFIG_NotifyUserWithEvent* moduleConfig;
    OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(EventIndex <= moduleConfig->MaximumEventIndex);
    DmfAssert(EventIndex < NotifyUserWithEvent_MAXIMUM_EVENTS);
    DmfAssert(moduleConfig->EventNames[EventIndex].Length > 0);

    InitializeObjectAttributes(&objectAttributes,
                               (PUNICODE_STRING)&moduleConfig->EventNames[EventIndex],
                               OBJ_KERNEL_HANDLE,
                               (HANDLE)NULL,
                               (PSECURITY_DESCRIPTOR)NULL);

    ntStatus = ZwOpenEvent(&moduleContext->NotifyUserWithEvent[EventIndex],
                           EVENT_MODIFY_STATE,
                           &objectAttributes);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ZwOpenEvent ntStatus=%!STATUS! EventIndex=%u", ntStatus, EventIndex);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
NotifyUserWithEvent_EventsDestroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy all the shared events that were opened.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_NotifyUserWithEvent* moduleContext;
    ULONG eventIndex;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    for (eventIndex = 0; eventIndex < NotifyUserWithEvent_MAXIMUM_EVENTS; eventIndex++)
    {
        if (moduleContext->NotifyUserWithEvent[eventIndex] != NULL)
        {
            NTSTATUS ntStatus;

            ntStatus = ZwClose(moduleContext->NotifyUserWithEvent[eventIndex]);
            DmfAssert(NT_SUCCESS(ntStatus));
            moduleContext->NotifyUserWithEvent[eventIndex] = NULL;
        }
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
NotifyUserWithEvent_EventSet(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EventIndex
    )
/*++

Routine Description:

    Set a specific shared event between Kernel-mode and User-mode.

Arguments:

    DmfModule - This Module's handle.
    EventIndex - The index of the event to set.

Return Value:

    STATUS_SUCCESS - The event exists and was set.
    STATUS_UNSUCCESSFUL - The event does not exist and was not set.
    Any other status means the event exists but could not be set. (User-mode may have closed the event.)

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_NotifyUserWithEvent* moduleContext;
    DMF_CONFIG_NotifyUserWithEvent* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(EventIndex <= moduleConfig->MaximumEventIndex);
    DmfAssert(EventIndex < NotifyUserWithEvent_MAXIMUM_EVENTS);

    if (moduleContext->NotifyUserWithEvent[EventIndex] != NULL)
    {
        PKEVENT eventObject;

        ntStatus = ObReferenceObjectByHandle(moduleContext->NotifyUserWithEvent[EventIndex],
                                             0,
                                             *ExEventObjectType,
                                             KernelMode,
                                             (VOID**)&eventObject,
                                             NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ObReferenceObjectByHandle EventIndex=%u", EventIndex);
            goto Exit;
        }

        // Set the event. User-mode waiters will be notified.
        //
        KeSetEvent(eventObject,
                   0,
                   FALSE);

        ObDereferenceObject(eventObject);

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Event Set EventIndex=%u", EventIndex);
    }
    else
    {
        // Tell caller that the event was not set.
        //
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Event NOT set because it does not exist EventIndex=%u", EventIndex);
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

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
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_NotifyUserWithEvent_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type NotifyUserWithEvent.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_NotifyUserWithEvent* moduleContext;
    DMF_CONFIG_NotifyUserWithEvent* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Validate this value even during runtime because the Client Driver may
    // set this value dynamically and incorrectly, although that is unlikely.
    //
    if (moduleConfig->MaximumEventIndex >= NotifyUserWithEvent_MAXIMUM_EVENTS)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    // Events are created dynamically as they are used.
    //
    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

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
DMF_NotifyUserWithEvent_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type NotifyUserWithEvent.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_NotifyUserWithEvent;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_NotifyUserWithEvent;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_NotifyUserWithEvent);
    dmfCallbacksDmf_NotifyUserWithEvent.DeviceOpen = DMF_NotifyUserWithEvent_Open;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_NotifyUserWithEvent,
                                            NotifyUserWithEvent,
                                            DMF_CONTEXT_NotifyUserWithEvent,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_NotifyUserWithEvent.CallbacksDmf = &dmfCallbacksDmf_NotifyUserWithEvent;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_NotifyUserWithEvent,
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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
Dmf_NotifyUserWithEvent_Notify(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Set the default (index = 0) shared event if possible.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS - The event exists and was set.
    STATUS_UNSUCCESSFUL - The event does not exist and was not set.
    Any other status means the event exists but could not be set. (User-mode may have closed the event.)

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithEvent);

    ntStatus = Dmf_NotifyUserWithEvent_NotifyByIndex(DmfModule,
                                                     NotifyUserWithEvent_DefaultIndex);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
Dmf_NotifyUserWithEvent_NotifyByIndex(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG EventIndex
    )
/*++

Routine Description:

    Set the shared event if possible.

Arguments:

    DmfModule - This Module's handle.
    EventIndex - The index of the event to set.

Return Value:

    STATUS_SUCCESS - The event exists and was set.
    STATUS_UNSUCCESSFUL - The event does not exist and was not set.
    Any other status means the event exists but could not be set. (User-mode may have closed the event.)

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_NotifyUserWithEvent* moduleContext;
    DMF_CONFIG_NotifyUserWithEvent* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 NotifyUserWithEvent);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    DmfAssert(EventIndex <= moduleConfig->MaximumEventIndex);
    DmfAssert(EventIndex < NotifyUserWithEvent_MAXIMUM_EVENTS);

    DMF_ModuleLock(DmfModule);

    // Every time that driver sets event, try to create it.
    // After setting event, destroy it.
    // This allows driver to know if application has received the
    // event and, crucially, works properly when the application
    // stops and restarts (destroying and creating its event).
    //
    ntStatus = NotifyUserWithEvent_EventCreate(DmfModule,
                                               EventIndex);
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = NotifyUserWithEvent_EventSet(DmfModule,
                                                EventIndex);
        NotifyUserWithEvent_EventsDestroy(DmfModule);
    }

    DMF_ModuleUnlock(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_NotifyUserWithEvent.c
//
