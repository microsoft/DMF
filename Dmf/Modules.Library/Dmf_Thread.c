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

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_Thread.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_Thread
{
    // Thread Handle.
    //
    HANDLE ThreadHandle;
    // Thread object.
    //
    VOID* ThreadObject;
    // Work Ready Event used to signal that the thread should do work.
    //
    DMF_PORTABLE_EVENT EventWorkReady;
    // Stop Event used to signal that the thread should stop accepting work.
    //
    DMF_PORTABLE_EVENT EventStop;
    // Close Event used to signal that the thread can exit.
    // 
    DMF_PORTABLE_EVENT EventClose;
    // Start Event used to signal that the thread can start accepting work.
    // 
    DMF_PORTABLE_EVENT EventStart;
    // Event used to signal that the thread has stopped running work
    // 
    DMF_PORTABLE_EVENT EventStopComplete;
    // Indicates whether a stop request is pending for thread work completion.
    // NOTE: There is no way to check if an event is set in User-mode as there
    //       is in Kernel-mode. So, this flag is necessary. Both User and Kernel 
    //       mode will execute the same algorithm.
    //
    BOOLEAN IsThreadStopPending;
    // Indicates whether the thread is suspended.
    //
    BOOLEAN IsStopped;
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

    // NOTE: Place EventStop first in the array in case both events are set
    //       so that if there is a lot of pending work, Thread_ThreadStop will
    //       wait for all the work to complete. This is necessary so that 
    //       PnP operations are not delayed.
    //
    waitObjects[0] = &moduleContext->EventStop;
    waitObjects[1] = &moduleContext->EventWorkReady;

    waitStatus = STATUS_WAIT_1;
    while (STATUS_WAIT_0 != waitStatus)
    {
        waitStatus = DMF_Portable_EventWaitForMultiple(ARRAYSIZE(waitObjects),
                                                       waitObjects,
                                                       FALSE,
                                                       NULL,
                                                       FALSE);
        switch (waitStatus)
        {
            case STATUS_WAIT_1:
            {
                // Do the work the Client needs to do.
                //
                DmfAssert(moduleConfig->ThreadControl.DmfControl.EvtThreadWork != NULL);
                moduleConfig->ThreadControl.DmfControl.EvtThreadWork(DmfModule);
                break;
            }
            case STATUS_WAIT_0:
            {
                // Exit event raised...Loop will exit.
                // NOTE: This event has higher priority.
                //
                break;
            }
            default:
            {
                DmfAssert(FALSE);
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
    #define Thread_NumberOfWaitObjects (2)
    DMF_PORTABLE_EVENT* waitObjects[Thread_NumberOfWaitObjects];
    NTSTATUS waitStatus;
    DMFMODULE dmfModule;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModule = DMFMODULEVOID_TO_MODULE(Context);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    waitObjects[0] = &moduleContext->EventStart;
    waitObjects[1] = &moduleContext->EventClose;

    waitStatus = STATUS_WAIT_0;
    while (TRUE)
    {
        // Wait for start.
        //
        waitStatus = DMF_Portable_EventWaitForMultiple(ARRAYSIZE(waitObjects),
                                                       waitObjects,
                                                       FALSE,
                                                       NULL,
                                                       FALSE);
        
        if (STATUS_WAIT_1 == waitStatus)
        {
            // Close event raised...Loop will exit.
            //
            TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Thread END");
            break;
        }

        if (STATUS_WAIT_0 != waitStatus)
        {
            // This should never happen.
            //
            DmfAssert(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Portable_EventWaitForMultiple fails: waitStatus=%!STATUS!", waitStatus);
            continue;
        }

        switch (moduleConfig->ThreadControlType)
        {
            case ThreadControlType_ClientControl:
            {
                // Call the Client Driver's Thread Callback.
                //
                DmfAssert(moduleConfig->ThreadControl.ClientControl.EvtThreadFunction != NULL);
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
                DmfAssert(FALSE);
                break;
            }
        }

        // Signal that thread has stopped.
        //
        DMF_Portable_EventSet(&moduleContext->EventStopComplete);
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
Thread_ThreadStop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Allows the Client Driver to tell this Module's thread to stop accepting any work. It waits for any ongoing work to complete.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Thread* moduleContext;
    DMF_CONFIG_Thread* moduleConfig;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
   
    // This function should not be called if the thread is already stopped.
    //
    if (moduleContext->IsStopped)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Thread is already stopped");
        goto Exit;
    }

    // In order to prevent other Client Driver threads from stopping,
    // Set the StopEvent only if it was created by the object.
    //
    if (ThreadControlType_DmfControl == moduleConfig->ThreadControlType)
    {
        DMF_Portable_EventSet(&moduleContext->EventStop);

        // Set the flag indicating Stop Event is set and thread work hasn't been completed yet.
        //
        moduleContext->IsThreadStopPending = TRUE;
    }

    // Wait indefinitely for ongoing work to complete.
    //
    ntStatus = DMF_Portable_EventWaitForSingleObject(&moduleContext->EventStopComplete,
                                                     NULL,
                                                     FALSE);
    if (!NT_SUCCESS(ntStatus))
    {
        DmfAssert(FALSE);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Portable_EventWaitForSingleObject fails: ntStatus=%!STATUS!", ntStatus);
    }

    moduleContext->IsStopped = TRUE;

Exit:

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
        
        // Set close event to end internal thread callback.
        //
        DMF_Portable_EventSet(&moduleContext->EventClose);

        // Wait indefinitely for thread to end.
        //
#if !defined(DMF_USER_MODE)
        DmfAssert(moduleContext->ThreadObject != NULL);
        KeWaitForSingleObject(moduleContext->ThreadObject,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Wait Satisfied: ThreadHandle=0x%p to End...", moduleContext->ThreadHandle);
        ZwClose(moduleContext->ThreadHandle);
        ObDereferenceObject(moduleContext->ThreadObject);
#else
        DmfAssert(moduleContext->ThreadHandle != NULL);
        WaitForSingleObjectEx(moduleContext->ThreadHandle,
                              INFINITE,
                              FALSE);
        CloseHandle(moduleContext->ThreadHandle);
#endif // !defined(DMF_USER_MODE)
        moduleContext->ThreadHandle = NULL;
        moduleContext->ThreadObject = NULL;
    }

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
        DmfAssert(FALSE);
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
_Function_class_(DMF_Open)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Thread_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Thread.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS always.

--*/
{
    DMF_CONTEXT_Thread* moduleContext;
    DMF_CONFIG_Thread* moduleConfig;
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    ntStatus = STATUS_SUCCESS;

    // Create the Start Event.
    //
    ntStatus = DMF_Portable_EventCreate(&moduleContext->EventStart,
                                        SynchronizationEvent,
                                        FALSE);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Portable_EventCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Create the Stop Complete Event.
    //
    ntStatus = DMF_Portable_EventCreate(&moduleContext->EventStopComplete,
                                        SynchronizationEvent,
                                        FALSE);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Portable_EventCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Create the Close Event.
    //
    ntStatus = DMF_Portable_EventCreate(&moduleContext->EventClose,
                                        NotificationEvent,
                                        FALSE);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Portable_EventCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (ThreadControlType_DmfControl == moduleConfig->ThreadControlType)
    {
        // Create the Work Ready Event.
        //
        ntStatus = DMF_Portable_EventCreate(&moduleContext->EventWorkReady,
                                            SynchronizationEvent,
                                            FALSE);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Portable_EventCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Create the Stop Event.
        //
        ntStatus = DMF_Portable_EventCreate(&moduleContext->EventStop,
                                            SynchronizationEvent,
                                            FALSE);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Portable_EventCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

     ntStatus = Thread_ThreadCreate(DmfModule);
     if (!NT_SUCCESS(ntStatus))
     {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Thread_ThreadCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
     }

     // Setting to TRUE intially as client needs to explicitly call DMF_Thread_Start for the thread to
     // accept client work in DMF control mode or to run the client callback in Client control mode.
     //
     moduleContext->IsStopped = TRUE;
Exit:

     FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
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
    DMF_CONTEXT_Thread* moduleContext;
    DMF_CONFIG_Thread* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Stop thread in case client did not call stop.
    //
    Thread_ThreadStop(DmfModule);

    Thread_ThreadDestroy(DmfModule);

    // This is necessary for User-mode. It is a NOP in Kernel-mode.
    //
    DMF_Portable_EventClose(&moduleContext->EventStart);
    DMF_Portable_EventClose(&moduleContext->EventStopComplete);
    DMF_Portable_EventClose(&moduleContext->EventClose);
    
    if (ThreadControlType_DmfControl == moduleConfig->ThreadControlType)
    {
        DMF_Portable_EventClose(&moduleContext->EventWorkReady);
        DMF_Portable_EventClose(&moduleContext->EventStop);
    }

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
    dmfCallbacksDmf_Thread.DeviceOpen = DMF_Thread_Open;
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

    DMFMODULE_VALIDATE_IN_METHOD_CLOSING_OK(DmfModule,
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
    DMF_CONFIG_Thread* moduleConfig;
    DMF_CONTEXT_Thread* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Thread);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ntStatus = STATUS_SUCCESS;

    if (moduleContext->IsStopped == FALSE)
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_DEVICE_STATE;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Thread is already running");
        goto Exit;
    }

    if (ThreadControlType_DmfControl == moduleConfig->ThreadControlType)
    {
        // Clear in case this thread was previously stopped.
        //
        moduleContext->IsThreadStopPending = FALSE;
    }

    moduleContext->IsStopped = FALSE;

    DMF_Portable_EventSet(&moduleContext->EventStart);

Exit:

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

    Thread_ThreadStop(DmfModule);

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
