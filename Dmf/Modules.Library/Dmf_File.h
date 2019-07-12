/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_File.h

Abstract:

    Companion file to Dmf_File.c.

Environment:

    Kernel-mode Driver Framework (pending)
    User-mode Driver Framework

--*/

#pragma once

// TODO: Kernel-mode support is pending.
//
#if defined(DMF_USER_MODE)

// This macro declares the following functions:
// DMF_File_ATTRIBUTES_INIT()
// DMF_File_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(File)

// Module Methods
//

_Must_inspect_result_
NTSTATUS
DMF_File_Read(
    _In_ DMFMODULE DmfModule,
    _In_ WDFSTRING FileName, 
    _Out_ WDFMEMORY* FileContentMemory
    );

#endif // defined(DMF_USER_MODE)

// eof: Dmf_File.h
//
