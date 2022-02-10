/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SymbolicLinkTarget.h

Abstract:

    Companion file to Dmf_SymbolicLinkTarget.c.

Environment:

    User-mode Driver Framework

--*/

#pragma once

#if defined(DMF_USER_MODE)

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Symbolic link to open.
    //
    PWCHAR SymbolicLinkName;
    // Open in Read or Write mode.
    //
    ULONG OpenMode;
    // Share Access.
    //
    ULONG ShareAccess;
} DMF_CONFIG_SymbolicLinkTarget;

// This macro declares the following functions:
// DMF_SymbolicLinkTarget_ATTRIBUTES_INIT()
// DMF_CONFIG_SymbolicLinkTarget_AND_ATTRIBUTES_INIT()
// DMF_SymbolicLinkTarget_Create()
//
DECLARE_DMF_MODULE(SymbolicLinkTarget)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SymbolicLinkTarget_DeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ DWORD dwIoControlCode,
    _In_reads_bytes_opt_(nInBufferSize) LPVOID lpInBuffer,
    _In_ DWORD nInBufferSize,
    _Out_writes_bytes_to_opt_(nOutBufferSize, *lpBytesReturned) LPVOID lpOutBuffer,
    _In_ DWORD nOutBufferSize,
    _Out_opt_ LPDWORD lpBytesReturned,
    _Inout_opt_ LPOVERLAPPED lpOverlapped
    );

#endif // defined(DMF_USER_MODE)

// eof: Dmf_SymbolicLinkTarget.h
//
