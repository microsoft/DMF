/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Thread.h

Abstract:

    Companion file to Dmf_Thread.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client Driver callback function.
//
typedef
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_Thread_Function(_In_ DMFMODULE DmfModule);

// Indicates what callbacks the Client Driver will receive.
//
typedef enum
{
    ThreadControlType_Invalid,
    // The client will have complete control of thread callback function.
    //
    ThreadControlType_ClientControl,
    // The Client Driver will be called when work is available for the Client
    // Driver to perform, but the Client Driver will not control looping.
    //
    ThreadControlType_DmfControl
} ThreadControlType;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Indicates what callbacks the Client Driver will receive.
    //
    ThreadControlType ThreadControlType;
    union
    {
        // In this mode, the Client Driver is responsible for looping
        // and waiting using Client Driver's structures.
        //
        struct
        {
            // Thread work callback function.
            //
            EVT_DMF_Thread_Function* EvtThreadFunction;
        } ClientControl;
        // In this mode, the Client Driver must use the object's Module Methods
        // to set and stop the thread.
        //
        struct
        {
            // Optional callback that does work before looping.
            //
            EVT_DMF_Thread_Function* EvtThreadPre;
            // Mandatory callback that does work when work is ready.
            //
            EVT_DMF_Thread_Function* EvtThreadWork;
            // Optional callback that does work after looping but before thread ends.
            //
            EVT_DMF_Thread_Function* EvtThreadPost;
        } DmfControl;
    } ThreadControl;
} DMF_CONFIG_Thread;

// This macro declares the following functions:
// DMF_Thread_ATTRIBUTES_INIT()
// DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT()
// DMF_Thread_Create()
//
DECLARE_DMF_MODULE(Thread)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Thread_IsStopPending(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Thread_Start(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Thread_Stop(
    _In_ DMFMODULE DmfModule
    );

VOID
DMF_Thread_WorkReady(
    _In_ DMFMODULE DmfModule
    );

// eof: Dmf_Thread.h
//
