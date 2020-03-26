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

// Structures' information is available here:
// http://www.dmtf.org/standards/smbios
//

// These are the structures that Clients pass to Methods.
//

// SMBIOS_TABLE_TYPE_01 (SystemInformation)
//
typedef struct
{
    CHAR* Manufacturer;
    CHAR* ProductName;
    CHAR* Version;
    CHAR* SerialNumber;
    UCHAR Uuid[16];
    UCHAR WakeUpType;
    CHAR* SKUNumber;
    CHAR* Family;
} SmbiosWmi_TableType01;

// This macro declares the following functions:
// DMF_SmbiosWmi_ATTRIBUTES_INIT()
// DMF_SmbiosWmi_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(SmbiosWmi)

// Module Methods
//

NTSTATUS
DMF_SmbiosWmi_TableType01Get(
    _In_ DMFMODULE DmfModule,
    _Out_ SmbiosWmi_TableType01* SmbiosTable01Buffer,
    _Inout_ size_t* SmbiosTable01BufferSize
    );

#if ! defined(DMF_USER_MODE)

_Check_return_
NTSTATUS
DMF_SmbiosWmi_TableCopy(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* TargetBuffer,
    _In_ ULONG TargetBufferSize
    );

#endif

_Check_return_
NTSTATUS
DMF_SmbiosWmi_TableCopyEx(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* TargetBuffer,
    _In_ size_t* TargetBufferSize
    );

#if !defined(DMF_USER_MODE)

VOID
DMF_SmbiosWmi_TableInformationGet(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR** TargetBuffer,
    _Out_ ULONG* TargetBufferSize
    );

#endif

VOID
DMF_SmbiosWmi_TableInformationGetEx(
    _In_ DMFMODULE DmfModule,
    _Out_ UCHAR** TargetBuffer,
    _Out_ size_t* TargetBufferSize
    );

// eof: Dmf_SmbiosWmi.h
//
