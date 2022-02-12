#pragma once
/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_UefiOperation.h

Abstract:

    Companion file to Dmf_UefiOperation.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// This macro declares the following functions:
// DMF_UefiOperation_ATTRIBUTES_INIT()
// DMF_UefiOperation_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(UefiOperation)

// Module Methods
//

#if defined(DMF_USER_MODE)

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS 
DMF_UefiOperation_FirmwareEnvironmentVariableGet(
    _In_ LPCTSTR Name,
    _In_ LPCTSTR Guid,
    _Out_writes_bytes_opt_(*VariableBufferSize) VOID* VariableBuffer,
    _Inout_ DWORD* VariableBufferSize
    );

#endif // defined(DMF_USER_MODE)

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UefiOperation_FirmwareEnvironmentVariableGetEx(
    _In_ UNICODE_STRING* Name,
    _In_ LPGUID Guid,
    _Out_writes_bytes_opt_(*VariableBufferSize) VOID* VariableBuffer,
    _Inout_ ULONG* VariableBufferSize,
    _Out_opt_ ULONG* Attributes
    );

#if defined(DMF_USER_MODE)

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UefiOperation_FirmwareEnvironmentVariableSet(
    _In_ LPCTSTR Name,
    _In_ LPCTSTR Guid,
    _In_reads_(VariableBufferSize) VOID* VariableBuffer,
    _In_ DWORD VariableBufferSize
    );

#endif // defined(DMF_USER_MODE)

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UefiOperation_FirmwareEnvironmentVariableSetEx(
    _In_ UNICODE_STRING* Name,
    _In_ LPGUID Guid,
    _In_reads_bytes_opt_(VariableBufferSize) VOID* VariableBuffer,
    _In_ ULONG VariableBufferSize,
    _In_ ULONG Attributes
    );

// eof: Dmf_UefiOperation.h
//
