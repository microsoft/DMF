/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ScheduledTask.h

Abstract:

    Companion file to Dmf_ScheduledTask.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// The result of the work that the Client attempts to do.
//
typedef enum
{
    ScheduledTask_WorkResult_Invalid,
    ScheduledTask_WorkResult_Success,
    ScheduledTask_WorkResult_Fail,
    // Use in deferred case only.
    //
    ScheduledTask_WorkResult_FailButTryAgain,
    ScheduledTask_WorkResult_SuccessButTryAgain
} ScheduledTask_Result_Type;

typedef
_Function_class_(EVT_DMF_ScheduledTask_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
ScheduledTask_Result_Type
EVT_DMF_ScheduledTask_Callback(_In_ DMFMODULE DmfModule,
                               _In_ VOID* CallbackContext,
                               _In_ WDF_POWER_DEVICE_STATE PreviousState);

// Indicates if the work is done one time ever, once every time Client starts.
//
typedef enum
{
    ScheduledTask_Persistence_Invalid,
    // Only do the work one time ever.
    //
    ScheduledTask_Persistence_PersistentAcrossReboots,
    // Do the work every time the Client starts.
    //
    ScheduledTask_Persistence_NotPersistentAcrossReboots
} ScheduledTask_Persistence_Type;

// Indicates when the work should be done.
//
typedef enum
{
    ScheduledTask_ExecuteWhen_Invalid,
    // In PrepareHardware.
    //
    ScheduledTask_ExecuteWhen_PrepareHardware,
    // In D0Entry.
    //
    ScheduledTask_ExecuteWhen_D0Entry,
    // Not applicable for this object. It means Client uses other Module Methods
    // do other work.
    //
    ScheduledTask_ExecuteWhen_Other
} ScheduledTask_ExecuteWhen_Type;

// Determines if the work is done immediately or deferred.
// Work that takes a "long time" should be deferred generally.
//
typedef enum
{
    ScheduledTask_ExecutionMode_Invalid,
    ScheduledTask_ExecutionMode_Immediate,
    ScheduledTask_ExecutionMode_Deferred
} ScheduledTask_ExecutionMode_Type;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // The Client Driver function that will perform work one time in a work item.
    //
    EVT_DMF_ScheduledTask_Callback* EvtScheduledTaskCallback;
    // Client context for above callback.
    //
    VOID* CallbackContext;
    // Indicates if the operation should be done every time driver loads or only a
    // single time (persistent across reboots).
    //
    ScheduledTask_Persistence_Type PersistenceType;
    // Indicates if the callback is executed immediately or is deferred.
    //
    ScheduledTask_ExecutionMode_Type ExecutionMode;
    // Indicates when the callback is executed, D0Entry or PrepareHardware.
    //
    ScheduledTask_ExecuteWhen_Type ExecuteWhen;
    // Retry timeout for ScheduledTask_WorkResult_SuccessButTryAgain.
    //
    ULONG TimerPeriodMsOnSuccess;
    // Retry timeout for ScheduledTask_WorkResult_FailButTryAgain.
    //
    ULONG TimerPeriodMsOnFail;
    // Delay before initial deferred call begins.
    //
    ULONG TimeMsBeforeInitialCall;
} DMF_CONFIG_ScheduledTask;

// This macro declares the following functions:
// DMF_ScheduledTask_ATTRIBUTES_INIT()
// DMF_CONFIG_ScheduledTask_AND_ATTRIBUTES_INIT()
// DMF_ScheduledTask_Create()
//
DECLARE_DMF_MODULE(ScheduledTask)

// Module Methods
//

ScheduledTask_Result_Type
DMF_ScheduledTask_ExecuteNow(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* CallbackContext
    );

NTSTATUS
DMF_ScheduledTask_ExecuteNowDeferred(
    _In_ DMFMODULE DmfModule,
    _In_opt_ VOID* CallbackContext
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ScheduledTask_TimesRunGet(
    _In_ DMFMODULE DmfModule,
    _Out_ ULONG* TimesRun
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ScheduledTask_TimesRunIncrement(
    _In_ DMFMODULE DmfModule,
    _Out_ ULONG* TimesRun
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ScheduledTask_TimesRunSet(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG TimesRun
    );

// eof: Dmf_ScheduledTask.h
//
