/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_NotifyUserWithRequest.h

Abstract:

    Companion file to Dmf_NotifyUserWithRequest.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

typedef
_Function_class_(EVT_DMF_NotifyUserWithRequeset_Complete)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_NotifyUserWithRequeset_Complete(_In_ DMFMODULE DmfModule,
                                        _In_ WDFREQUEST Request,
                                        _In_opt_ ULONG_PTR Context,
                                        _In_ NTSTATUS NtStatus);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Maximum number of pending events allowed. This is to prevent a caller from
    // submitting millions of events.
    //
    LONG MaximumNumberOfPendingRequests;
    // Maximum number of User-mode data entries stored.
    //
    LONG MaximumNumberOfPendingDataBuffers;
    // Size of User-mode data entry.
    //
    LONG SizeOfDataBuffer;
    // This Handler is optionally called when a Request is canceled.
    //
    EVT_DMF_NotifyUserWithRequeset_Complete* EvtPendingRequestsCancel;
    // Client Driver's Event Log Provider name to enable
    // event logging from the Dmf_NotifyUserWithRequest Module.
    //
    PWSTR ClientDriverProviderName;
} DMF_CONFIG_NotifyUserWithRequest;

// This macro declares the following functions:
// DMF_NotifyUserWithRequest_ATTRIBUTES_INIT()
// DMF_CONFIG_NotifyUserWithRequest_AND_ATTRIBUTES_INIT()
// DMF_NotifyUserWithRequest_Create()
//
DECLARE_DMF_MODULE(NotifyUserWithRequest)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequest_DataProcess(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequeset_Complete* EventCallbackFunction,
    _In_opt_ VOID* EventCallbackContext,
    _In_ NTSTATUS NtStatus
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequest_EventRequestAdd(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_NotifyUserWithRequest_RequestProcess(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequest_RequestReturn(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequeset_Complete* EventCallbackFunction,
    _In_opt_ ULONG_PTR EventCallbackContext,
    _In_ NTSTATUS NtStatus
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotifyUserWithRequest_RequestReturnAll(
    _In_ DMFMODULE DmfModule,
    _In_opt_ EVT_DMF_NotifyUserWithRequeset_Complete* EventCallbackFunction,
    _In_opt_ ULONG_PTR EventCallbackContext,
    _In_ NTSTATUS NtStatus
    );

// eof: Dmf_NotifyUserWithRequest.h
//
