/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SmbiosWmi.h

Abstract:

    Companion file to Dmf_SmbiosWmi.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// This macro declares the following functions:
// DMF_SmbiosWmi_ATTRIBUTES_INIT()
// DMF_SmbiosWmi_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(SmbiosWmi)

// Module Methods
//

_Check_return_
NTSTATUS
DMF_SmbiosWmi_TableCopy(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* TargetBuffer,
    _In_ ULONG TargetBufferSize
    );

VOID
DMF_SmbiosWmi_TableInformationGet(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR** TargetBuffer,
    _Out_ ULONG* TargetBufferSize
    );

// eof: Dmf_SmbiosWmi.h
//
