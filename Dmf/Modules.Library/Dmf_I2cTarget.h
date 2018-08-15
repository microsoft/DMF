/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_I2cTarget.h

Abstract:

    Companion file to Dmf_I2cTarget.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Module will not load if I2c Connection not found.
    //
    BOOLEAN I2cConnectionMandatory;
    // Indicates the index of I2C resource that the Client wants to access.
    //
    ULONG I2cResourceIndex;
    // Microseconds to delay on SPB Read operations.
    //
    ULONG ReadDelayUs;
    // Time units(ms) to wait for SPB Read operation to complete.
    //
    ULONGLONG ReadTimeoutMs;
    // Time units(ms) to wait for SPB Write operation to complete.
    //
    ULONGLONG WriteTimeoutMs;
} DMF_CONFIG_I2cTarget;

// This macro declares the following functions:
// DMF_I2cTarget_ATTRIBUTES_INIT()
// DMF_CONFIG_I2cTarget_AND_ATTRIBUTES_INIT()
// DMF_I2cTarget_Create()
//
DECLARE_DMF_MODULE(I2cTarget)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_AddressRead(
    _In_ DMFMODULE DmfModule,
    _In_reads_(AddressLength) UCHAR* Address,
    _In_ ULONG AddressLength,
    _Out_writes_(BufferLength) VOID* Buffer,
    _In_ ULONG BufferLength
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_AddressWrite(
    _In_ DMFMODULE DmfModule,
    _In_reads_(AddressLength) UCHAR* Address,
    _In_ ULONG AddressLength,
    _In_reads_(BufferLength) VOID* Buffer,
    _In_ ULONG BufferLength
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_BufferRead(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferLength) VOID* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMs
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_BufferWrite(
    _In_ DMFMODULE DmfModule,
    _Inout_updates_(BufferLength) VOID* Buffer,
    _In_ ULONG BufferLength,
    _In_ ULONG TimeoutMs
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_I2cTarget_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_opt_ BOOLEAN* I2cConnectionAssigned
    );

// eof: Dmf_I2cTarget.h
//
