 /*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Thread.c

Abstract:

    Implements a System Thread and provides support to manipulate the thread.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_Thread.tmh"

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
    // Thread Handle.
    //
    HANDLE ThreadHandle;
    // Thread object.
    //
    VOID* ThreadObject;
    // Thread work callback function.
    //
    EVT_DMF_Thread_Function* EvtThreadFunction;
    // Work Ready Event.
    //
    DMF_PORTABLE_EVENT EventWorkReady;
    // Stop Event.
    //
    DMF_PORTABLE_EVENT EventStopInternal;
    // Pointer to either the event above, or to the Client Driver's Stop Event.
    //
    DMF_PORTABLE_EVENT* EventStop;
    // Indicates whether a stop request is pending for thread work completion.
    // NOTE: There is no way to check if an event is set in User-mode as there
    //       is in Kernel-mode. So, this flag is necessary. Both User and Kernel 
    //       mode will execute the same algorithm.
    //
    BOOLEAN IsThreadStopPending;
} DMF_CONTEXT_Thread;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Thread)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(Thread)

// Memory Pool Tag.
//
#define MemoryTag 'drhT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Thread_WorkerThread(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    The worker thread loop. It loops indefinitely until the thread is stopped. When
    there is an indication that there is work ready to be done, the Client's callback
    function is called to do that work.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Thread* moduleContext;
    DMF_CONFIG_Thread* moduleConfig;
    #define Thread_NumberOfWaitObjects (2)
    DMF_PORTABLE_EVENT* waitObjects[Thread_NumberOfWaitObjects];
    NTSTATUS waitStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Thread START");

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    waitObjects[0] = &moduleContext->EventWorkReady;
    waitObjects[1] = moduleContext->EventStop;

    waitStatus = STATUS_WAIT_0;
    while (STATUS_WAIT_1 != waitStatus)
    {
        waitStatus = DMF_Portable_EventWaitForMultiple(ARRAYSIZE(waitObjects),
                                                       waitObjects,
                                                       FALSE,
                                                       FALSE,
#if defined(DMF_USER_MODE)
                                                       INFINITE);
#else
                                                       NULL);
#endif // defined(DMF_USER_MODE)
        switch (waitStatus)
        {
            case STATUS_WAIT_0:
            {
                // Do the work the Client needs to do.
                //
                ASSERT(moduleConfig->ThreadControl.DmfControl.EvtThreadWork != NULL);
                moduleConfig->ThreadControl.DmfControl.EvtThreadWork(DmfModule);
                break;
            }
            case STATUS_WAIT_1:
            {
                // Exit event raised...Loop will exit.
                //
                break;
            }
            default:
            {
                ASSERT(FALSE);
                break;
            }
        }
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Thread END");
}

#if !defined(DMF_USER_MODE)

KSTART_ROUTINE Thread_ThreadCallback;

#pragma code_seg("PAGE")
_IRQL_requires_same_
_Function_class_(KSTART_ROUTINE)
VOID
Thread_ThreadCallback(
    _In_ VOID* Context
    )

#else

#pragma code_seg("PAGE")
_Function_class_(LPTHREAD_START_ROUTINE)
DWORD
WINAPI
Thread_ThreadCallback(
    _In_ VOID* Context
    )

#endif // !defined(DMF_USER_MODE)

/*++

Routine Description:

    The thread callback function.

Arguments:

    Context - This Module's handle.

Return Value:

    Kernel-mode: None
    User-mode: 0 always

--*/
{
    DMF_CONTEXT_Thread* moduleContext;
    DMF_CONFIG_Thread* moduleConfig;
    DMFMODULE dmfModule;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModule = DMFMODULEVOID_TO_MODULE(Context);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    switch (moduleConfig->ThreadControlType)
    {
        case ThreadControlType_ClientControl:
        {
            // Call the Client Driver's Thread Callback.
            //
            ASSERT(moduleConfig->ThreadControl.ClientControl.EvtThreadFunction != NULL);
            moduleConfig->ThreadControl.ClientControl.EvtThreadFunction(dmfModule);
            break;
        }
        case ThreadControlType_DmfControl:
        {
            // If the Client Driver wants to do preprocessing, do it now.
            //
            if (moduleConfig->ThreadControl.DmfControl.EvtThreadPre != NULL)
            {
                TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Thread PRE");
                moduleConfig->ThreadControl.DmfControl.EvtThreadPre(dmfModule);
            }

            // Execute the main loop function.
            //
            Thread_WorkerThread(dmfModule);

            // If the Client Driver wants to do some post processing, do it now.
            //
            if (moduleConfig->ThreadControl.DmfControl.EvtThreadPost != NULL)
            {
                TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Thread POST");
                moduleConfig->ThreadControl.DmfControl.EvtThreadPost(dmfModule);
            }
            break;
        }
        default:
        {
            ASSERT(FALSE);
            break;
        }
    }

#if defined(DMF_USER_MODE)
    FuncExit(DMF_TRACE, "return=0");
    return 0;
#else
    FuncExitVoid(DMF_TRACE);
#endif // defined(DMF_USER_MODE)
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
Thread_ThreadCreate(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Create the thread in a suspended state.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Thread* moduleContext;
    DMF_CONFIG_Thread* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (ThreadControlType_DmfControl == moduleConfig->ThreadControlType)
    {
        // Create the Work Ready Event.
        //
        DMF_Portable_EventCreate(&moduleContext->EventWorkReady,
                                 SynchronizationEvent,
                                 FALSE);
        // If the Client Driver does not supply an address of its own Stop Event, create
        // the Stop Event here; otherwise, use the Client Driver's Stop Event.
        // The Client Driver will be able to stop the thread using this event.
        //
        ASSERT(NULL == moduleContext->EventStop);

        // Clear in case this thread was previously stopped.
        //
        moduleContext->IsThreadStopPending = FALSE;

        // Create the Stop Event on behalf of Client Driver.
        // (Usually for drivers with a single thread.)
        //
        DMF_Portable_EventCreate(&moduleContext->EventStopInternal,
                                 NotificationEvent,
                                 FALSE);
        moduleContext->EventStop = &moduleContext->EventStopInternal;
    }

    // Create the thread.
    //
#if !defined(DMF_USER_MODE)
    OBJECT_ATTRIBUTES objectAttributes;

    RtlZeroMemory(&objectAttributes,
                  sizeof(objectAttributes));
    InitializeObjectAttributes(&objectAttributes,
                               NULL,
                               OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);
    ntStatus = PsCreateSystemThread(&moduleContext->ThreadHandle,
                                    THREAD_ALL_ACCESS,
                                    &objectAttributes,
                                    NULL,
                                    NULL,
                                    Thread_ThreadCallback,
                                    DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "PsCreateSystemThread ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Add a reference to the thread object and obtain thread object pointer.
    //
    ntStatus = ObReferenceObjectByHandle(moduleContext->ThreadHandle,
                                         THREAD_ALL_ACCESS,
                                         *PsThreadType,
                                         KernelMode,
                                         &moduleContext->ThreadObject,
                                         NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ObReferenceObjectByHandle ntStatus=%!STATUS!", ntStatus);

        // Unable to object a thread object.
        //
        moduleContext->ThreadObject = NULL;
        // Close thread handle now because close will not be called.
        //
        ZwClose(moduleContext->ThreadHandle);
        moduleContext->ThreadHandle = NULL;

        goto Exit;
    }
#else
    ntStatus = STATUS_SUCCESS;
    HANDLE threadHandle = CreateThread(NULL,
                                       0,
                                       Thread_ThreadCallback,
                                       DmfModule,
                                       0,
                                       NULL);
    if (NULL == threadHandle)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CreateThread ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext->ThreadHandle = threadHandle;
#endif // !defined(DMF_USER_MODE)

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

VOID
Thread_StopEventSet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Allows the Client Driver to tell this Module's thread to stop running.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Thread* moduleContext;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ASSERT(moduleContext->EventStop != NULL);
    DMF_Portable_EventSet(moduleContext->EventStop);

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
Thread_ThreadDestroy(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    If the thread is running, wait for it to end then destroy it.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Thread* moduleContext;
    DMF_CONFIG_Thread* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (moduleContext->ThreadHandle != NULL)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Wait for ThreadHandle=0x%p to End...", moduleContext->ThreadHandle);
        // In order to prevent other Client Driver threads from stopping,
        // Set the StopEvent only if it was created by the object.
        //
        if (ThreadControlType_DmfControl == moduleConfig->ThreadControlType)
        {
            Thread_StopEventSet(DmfModule);

            // Set the flag indicating Stop Event is set and thread work hasn't been completed yet.
            //
            moduleContext->IsThreadStopPending = TRUE;
        }
        // Wait indefinitely for thread to end.
        //
#if !defined(DMF_USER_MODE)
        ASSERT(moduleContext->ThreadObject != NULL);
        KeWaitForSingleObject(moduleContext->ThreadObject,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Wait Satisfied: ThreadHandle=0x%p to End...", moduleContext->ThreadHandle);
        ZwClose(moduleContext->ThreadHandle);
        ObDereferenceObject(moduleContext->ThreadObject);
#else
        ASSERT(moduleContext->ThreadHandle != NULL);
        WaitForSingleObjectEx(moduleContext->ThreadHandle,
                              INFINITE,
                              FALSE);
        CloseHandle(moduleContext->ThreadHandle);
#endif // !defined(DMF_USER_MODE)
        moduleContext->ThreadHandle = NULL;
        moduleContext->ThreadObject = NULL;
        // If the thread is under DMF Control, then the internal event must have been set.
        // Otherwise, it is not set because the Client had complete control.
        //
        ASSERT(((ThreadControlType_DmfControl == moduleConfig->ThreadControlType) && (NULL != moduleContext->EventStop)) ||
               (ThreadControlType_ClientControl == moduleConfig->ThreadControlType) && (NULL == moduleContext->EventStop));
    }

    moduleContext->EventStop = NULL;

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
Thread_IsThreadStopPending(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Returns whether a thread stop is issued or not.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    BOOLEAN - Returns TRUE if the Client has set the stop event; FALSE, otherwise.

--*/
{
    DMF_CONTEXT_Thread* moduleContext;
    DMF_CONFIG_Thread* moduleConfig;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (ThreadControlType_DmfControl != moduleConfig->ThreadControlType)
    {
        // This is relevant only if the callback is ThreadControlType_ClientControl.
        //
        ASSERT(FALSE);
    }

    return moduleContext->IsThreadStopPending;
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
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Thread_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Thread.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // In case, Client has not explicitly stopped the thread, do that now.
    //
    Thread_ThreadDestroy(DmfModule);

    FuncExitNoReturn(DMF_TRACE);
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
DMF_Thread_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Thread.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Thread;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Thread;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Thread);
    dmfCallbacksDmf_Thread.DeviceClose = DMF_Thread_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Thread,
                                            Thread,
                                            DMF_CONTEXT_Thread,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Thread.CallbacksDmf = &dmfCallbacksDmf_Thread;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Thread,
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
BOOLEAN
DMF_Thread_IsStopPending(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Return whether a request to stop a thread has been issued or not.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    BOOLEAN

--*/
{
    BOOLEAN threadStopPending;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Thread);

    threadStopPending = Thread_IsThreadStopPending(DmfModule);

    FuncExit(DMF_TRACE, "threadStopPending=%d", threadStopPending);

    return threadStopPending;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Thread_Start(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Starts the thread.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Thread);

    ntStatus = Thread_ThreadCreate(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Thread_Stop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stop

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Thread);

    Thread_ThreadDestroy(DmfModule);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

VOID
DMF_Thread_WorkReady(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Allows the Client Driver to tell this Module's thread that its Work Callback
    should be called.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Thread* moduleContext;

    FuncEntry(DMF_TRACE);

    // By design this Method can be called by Close callback.
    //
    DMFMODULE_VALIDATE_IN_METHOD_CLOSING_OK(DmfModule,
                                            Thread);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    DMF_Portable_EventSet(&moduleContext->EventWorkReady);

    FuncExitVoid(DMF_TRACE);
}

// eof: Dmf_Thread.c
//
