/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_File.c

Abstract:

    Support for working with files in drivers.

Environment:

    Kernel-mode Driver Framework (pending)
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_File.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_NO_CONTEXT(File)

// This Module has no CONFIG.
//
DMF_MODULE_DECLARE_NO_CONFIG(File)

#define MemoryTag 'FfmD'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type File.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_File;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_MODULE_DESCRIPTOR_INIT(dmfModuleDescriptor_File,
                               File,
                               DMF_MODULE_OPTIONS_DISPATCH,
                               DMF_MODULE_OPEN_OPTION_OPEN_Create);

    // ObjectAttributes must be initialized and
    // ParentObject attribute must be set to either WDFDEVICE or DMFMODULE.
    //
    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_File,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_Must_inspect_result_
NTSTATUS
DMF_File_Read(
    _In_ DMFMODULE DmfModule,
    _In_ WDFSTRING FileName, 
    _Out_ WDFMEMORY* FileContentMemory
    )
/*++

Routine Description:

    Reads the contents of a file into a buffer that is allocated for the Client. Client is responsible
    for freeing the allocated memory.

Arguments:

    DmfModule - This Module's handle.
    FileName - Name of the file.
    FileContentMemory - Buffer handle where read file contents are copied. The caller owns this memory.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    BOOL returnValue;
    LONGLONG bytesRemaining;
    BYTE* readBuffer;
    DWORD sizeOfOneRead;
    DWORD numberOfBytesRead;
    LARGE_INTEGER fileSize;
    UNICODE_STRING fileNameString;
    WDFMEMORY fileContentsMemory;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 File);

    WdfStringGetUnicodeString(FileName,
                              &fileNameString);

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "Reading file %S ",
                fileNameString.Buffer);

    device = DMF_ParentDeviceGet(DmfModule);
    returnValue = FALSE;
    fileContentsMemory = WDF_NO_HANDLE;
    *FileContentMemory = WDF_NO_HANDLE;

    HANDLE hFile = CreateFile(fileNameString.Buffer,
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        TraceError(DMF_TRACE,
                   "CreateFile fails: to Open %S! ntStatus=%!STATUS!",
                   fileNameString.Buffer,
                   ntStatus);
        goto Exit;
    }

    fileSize.QuadPart = 0;
    returnValue = GetFileSizeEx(hFile,
                                &fileSize);
    if (!returnValue)
    {
        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        TraceError(DMF_TRACE,
                   "GetFileSizeEx fails: to Read %S !ntStatus=%!STATUS!", 
                   fileNameString.Buffer,
                   ntStatus);
        goto Exit;
    }

    // Allocate the required buffer to read the file content.
    //
    BYTE* fileContentBuffer;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               (size_t)fileSize.QuadPart,
                               &fileContentsMemory,
                               (VOID**)&fileContentBuffer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "WdfMemoryCreate fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    // Now read the contents.
    //
    bytesRemaining = fileSize.QuadPart;
    readBuffer = fileContentBuffer;
    while (bytesRemaining > 0)
    {
        numberOfBytesRead = 0;
        sizeOfOneRead = MAXDWORD;
        if (bytesRemaining < sizeOfOneRead)
        {
            sizeOfOneRead = (DWORD) bytesRemaining;
        }

        returnValue = ReadFile(hFile,
                               readBuffer,
                               sizeOfOneRead,
                               &numberOfBytesRead,
                               NULL);
        if (!returnValue)
        {
            ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
            TraceError(DMF_TRACE,
                       "ReadFile fails: to Read %S !ntStatus=%!STATUS!", 
                       fileNameString.Buffer,
                       ntStatus);
            break;
        }

        readBuffer += numberOfBytesRead;
        bytesRemaining -= numberOfBytesRead;

        if (numberOfBytesRead == 0)
        {
            DmfAssert(bytesRemaining == 0);
        }
    }

    *FileContentMemory = fileContentsMemory;

Exit:

    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }

    return ntStatus;
}

// eof: Dmf_File.c
//
