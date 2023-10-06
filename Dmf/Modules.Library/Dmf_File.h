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

// This macro declares the following functions:
// DMF_File_ATTRIBUTES_INIT()
// DMF_File_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(File)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_BufferDecompress(
    _In_opt_ DMFMODULE DmfModule,
    _In_ USHORT CompressionFormat,
    _Out_writes_bytes_(UncompressedBufferSize) UCHAR* UncompressedBuffer,
    _In_ ULONG UncompressedBufferSize,
    _In_reads_bytes_(CompressedBufferSize) UCHAR* CompressedBuffer,
    _In_ ULONG CompressedBufferSize,
    _Out_ ULONG* FinalUncompressedSize
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_DriverFileRead(
    _In_opt_ DMFMODULE DmfModule,
    _In_z_ WCHAR* FileName, 
    _Out_ WDFMEMORY* FileContentMemory,
    _Out_opt_ UCHAR** Buffer,
    _Out_opt_ size_t* BufferLength
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_FileExists(
    _In_opt_ DMFMODULE DmfModule,
    _In_z_ WCHAR* FileName,
    _Out_ BOOLEAN* FileExists
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_Read(
    _In_opt_ DMFMODULE DmfModule,
    _In_ WDFSTRING FileName, 
    _Out_ WDFMEMORY* FileContentMemory
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_ReadEx(
    _In_opt_ DMFMODULE DmfModule,
    _In_z_ WCHAR* FileName, 
    _Out_ WDFMEMORY* FileContentMemory,
    _Out_opt_ UCHAR** Buffer,
    _Out_opt_ size_t* BufferLength
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_Write(
    _In_opt_ DMFMODULE DmfModule,
    _In_ WDFSTRING FileName,
    _In_ WDFMEMORY FileContentMemory
    );

// eof: Dmf_File.h
//
