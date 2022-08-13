/*++

   Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT license.

Module Name:

    Dmf_Time.c

Abstract:

    Implements a Time data structure.

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
#include "Dmf_Time.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_Time
{
    // Get Performance Frequency during module Open. This value will not change. 
    //
    LARGE_INTEGER PerformanceFrequency;
} DMF_CONTEXT_Time;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Time)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_NO_CONFIG(Time)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define NANOSECONDS_TO_MILLISECONDS         (1000000U)
#define SECONDS_TO_NANOSECONDS              (1000000000U)

__forceinline
NTSTATUS
ElapsedTimeInNanosecondsGet(
    _In_ LONGLONG CurrentTick,
    _In_ LONGLONG LastQueryTick,
    _In_ LONGLONG PerformanceFrequency,
    _Out_ LONGLONG* ElapsedTimeInNanoseconds
    )
/*++

Routine Description:

    Calculate elapsed time in nanoseconds given the current and last tick count.

Arguments:

    CurrentTick - Current tick count.
    LastQueryTick - Last tick count.
    PerformanceFrequency - performance-counter frequency.
    ElapsedTimeInNanoseconds - Return the elapsed time in nanoseconds.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    LONGLONG elaspedTicks;
    LONGLONG result;

    DmfAssert(PerformanceFrequency);

    ntStatus = STATUS_UNSUCCESSFUL;

    elaspedTicks = CurrentTick - LastQueryTick;

#ifdef DMF_KERNEL_MODE
    ntStatus = RtlLong64Mult(elaspedTicks,
                             SECONDS_TO_NANOSECONDS, 
                             &result);
    if (!NT_SUCCESS(ntStatus))
    {
        DmfAssert(false);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Integer overflow when performing multiplication of %lld and %lld", elaspedTicks, SECONDS_TO_NANOSECONDS);
        goto Exit;
    }
#else
    HRESULT hResult;

    hResult = Long64Mult(elaspedTicks,
                         SECONDS_TO_NANOSECONDS,
                         &result);
    if (hResult != S_OK)
    {
        DmfAssert(false);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Integer overflow when performing multiplication of %lld and %lld", elaspedTicks, SECONDS_TO_NANOSECONDS);
        ntStatus = STATUS_INTEGER_OVERFLOW;
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;
#endif

    *ElapsedTimeInNanoseconds = (result / PerformanceFrequency);

Exit:

    return ntStatus;
}

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
DMF_Time_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

   Initialize an instance of a DMF Module of type Time.

Arguments:

   DmfModule - This Module's handle.

Return Value:

   NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Time* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = STATUS_SUCCESS;

    // moduleContext->PerformanceFrequency saves the performance counter frequency in ticks per second.
    //
#if defined(DMF_KERNEL_MODE)
    KeQueryPerformanceCounter(&moduleContext->PerformanceFrequency);
#else
    QueryPerformanceFrequency(&moduleContext->PerformanceFrequency);
#endif

    DmfAssert(moduleContext->PerformanceFrequency.QuadPart != 0);

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
DMF_Time_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

   Create an instance of a DMF Module of type Time.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Time;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Time;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Time);
    dmfCallbacksDmf_Time.DeviceOpen = DMF_Time_Open;
    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Time,
                                            Time,
                                            DMF_CONTEXT_Time,
                                            DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Time.CallbacksDmf = &dmfCallbacksDmf_Time;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Time,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Time_ElapsedTimeMillisecondsGet(
    _In_ DMFMODULE DmfModule,
    _In_ LONGLONG StartTime,
    _Out_ LONGLONG* ElapsedTimeInMilliSeconds
    )
/*++

Routine Description:

    Calculate the elasped time in milliseconds.

Arguments:

    DmfModule - This Module's handle.
    StartTick - Tick start count.
    ElapsedTimeInMilliSeconds - Return the elapsed time in milliseconds.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Time* moduleContext;
    LARGE_INTEGER elapsedTimeInNanoseconds;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Time);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Calculate the elapsed time.
    //
    ntStatus = DMF_Time_ElapsedTimeNanosecondsGet(DmfModule,
                                                  StartTime,
                                                  &elapsedTimeInNanoseconds.QuadPart);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    *ElapsedTimeInMilliSeconds = (elapsedTimeInNanoseconds.QuadPart / NANOSECONDS_TO_MILLISECONDS);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Time_ElapsedTimeNanosecondsGet(
    _In_ DMFMODULE DmfModule,
    _In_ LONGLONG StartTick,
    _Out_ LONGLONG* ElapsedTimeInNanoSeconds
    )
/*++

Routine Description:

    Calculate the elasped time in nanoseconds.

Arguments:

    DmfModule - This Module's handle.
    StartTick - Tick start count.
    ElapsedTimeInNanoSeconds - Return the elapsed time in nanoseconds.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Time* moduleContext;
    LARGE_INTEGER endTick;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Time);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Get current tick count.
    //
#if defined(DMF_KERNEL_MODE)
    endTick = KeQueryPerformanceCounter(NULL);
#else
    QueryPerformanceCounter(&endTick);
#endif

    DmfAssert(StartTick < endTick.QuadPart);

    // Calculate the elapsed time.
    //
    ntStatus = ElapsedTimeInNanosecondsGet(endTick.QuadPart,
                                           StartTick,
                                           moduleContext->PerformanceFrequency.QuadPart,
                                           ElapsedTimeInNanoSeconds);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
LONGLONG
DMF_Time_TickCountGet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Return the current tick count.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    Current tick count.

--*/
{
    LARGE_INTEGER timeTick;

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Time);

#if defined(DMF_KERNEL_MODE)
    timeTick = KeQueryPerformanceCounter(NULL);
#else
    QueryPerformanceCounter(&timeTick);
#endif

    FuncExitVoid(DMF_TRACE);

    return timeTick.QuadPart;
}

// eof: Dmf_Time.c
//
