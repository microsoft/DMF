/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_NotifyUserWithRequestMultiple.h

Abstract:

    Companion file to Dmf_NotifyUserWithRequestMultiple.c.

Environment:

    Kernel-mode
    User-mode

--*/

#pragma once

// These definitions specify additional functionality
// that can be enabled.
//
typedef union _NOTIFY_USER_MULTIPLE_MODE_TYPE
{
    UINT32 NotifyUserMultipleMode;
    struct {                                      // Bits:
        UINT32 ReplayLastMessageToNewClients:1;   //    0: Cache last buffer and send to new user.
        UINT32 Reserved:31;                       // 31-1: Reserved
    } Modes;
} NOTIFY_USER_MULTIPLE_MODE_TYPE;

// This is an optional callback that can be used in situations where
// the client needs to evaluate the WDFFILEOBJECT to 
// decide whether or the user (driver, application, service)
// corresponding to the WDFFILEOBJECT gets the notifications from this module.
//
typedef
_Function_class_(EVT_DMF_NotifyUserWithRequestMultiple_ArrivalCallback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_NotifyUserWithRequestMultiple_ArrivalCallback(_In_ DMFMODULE DmfModule,
                                                      _In_ WDFFILEOBJECT FileObject);

// Optional callback registered by Client for user departure. Note this is
// not called if the client returned a failure status in ArrivalCallback.
//
typedef
_Function_class_(EVT_DMF_NotifyUserWithRequestMultiple_DepartureCallback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_NotifyUserWithRequestMultiple_DepartureCallback(_In_ DMFMODULE DmfModule,
                                                        _In_ WDFFILEOBJECT FileObject);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Maximum number of pending events allowed. This is to prevent a caller from
    // submitting millions of events.
    //
    LONG MaximumNumberOfPendingRequests;
    // Maximum number of data buffers stored.
    //
    LONG MaximumNumberOfPendingDataBuffers;
    // Size of data buffer.
    //
    LONG SizeOfDataBuffer;
    // Client callback function invoked by passing a request and data buffer.
    // All NotifyUserWithRequest child Modules will share this callback.
    //
    EVT_DMF_NotifyUserWithRequest_Complete* CompletionCallback;
    // Callback registered by Client for Data/Request processing
    // upon new Client arrival. If Client returns a non-success status,
    // the User will not receive the notifications from this module.
    //
    EVT_DMF_NotifyUserWithRequestMultiple_ArrivalCallback* EvtClientArrivalCallback;
    // Callback registered by Client for Data/Request processing
    // upon Client departure.
    //
    EVT_DMF_NotifyUserWithRequestMultiple_DepartureCallback* EvtClientDepartureCallback;
    // Client can specify special functionality provided by this Module.
    //
    NOTIFY_USER_MULTIPLE_MODE_TYPE ModeType;
} DMF_CONFIG_NotifyUserWithRequestMultiple;

// This macro declares the following functions:
// DMF_NotifyUserWithRequestMultiple_ATTRIBUTES_INIT()
// DMF_CONFIG_NotifyUserWithRequestMultiple_AND_ATTRIBUTES_INIT()
// DMF_NotifyUserWithRequestMultiple_Create()
//
DECLARE_DMF_MODULE(NotifyUserWithRequestMultiple)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_NotifyUserWithRequestMultiple_DataBroadcast(
    _In_ DMFMODULE DmfModule,
    _In_reads_(DataBufferSize) VOID* DataBuffer,
    _In_ size_t DataBufferSize,
    _In_ NTSTATUS NtStatus
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequestMultiple_RequestProcess(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    );

// eof: Dmf_NotifyUserWithRequestMultiple.h
//
