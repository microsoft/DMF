/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_File.c

Abstract:

    Support for working with files in drivers.

Environment:

    Kernel-mode Driver Framework
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

#if defined(DMF_USER_MODE)
typedef NTSTATUS
(*RtlDecompressBuffer)(USHORT CompressionFormat,
                       PUCHAR UncompressedBuffer,
                       ULONG UncompressedBufferSize,
                       PUCHAR CompressedBuffer,
                       ULONG CompressedBufferSize,
                       PULONG FinalUncompressedSize
    );
#endif

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

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_BufferDecompress(
    _In_ DMFMODULE DmfModule,
    _In_ USHORT CompressionFormat,
    _Out_writes_bytes_(UncompressedBufferSize) UCHAR* UncompressedBuffer,
    _In_ ULONG UncompressedBufferSize,
    _In_reads_bytes_(CompressedBufferSize) UCHAR* CompressedBuffer,
    _In_ ULONG CompressedBufferSize,
    _Out_ ULONG* FinalUncompressedSize
    )
/*++

Routine Description:

    Decompresses the input buffer and writes the uncompressed buffer back.

Arguments:

    DmfModule - This Module's handle.
    CompressionFormat - CompressionFormat.
    UncompressedBuffer - Uncompressed output buffer handle of buffer holding data to write.
    UncompressedBufferSize -  Uncompressed output buffer size
    CompressedBuffer - Compressed input buffer handle
    CompressedBufferSize - Compressed input buffer size
    FinalUncompressedSize - Final uncompressed size of the buffer

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 File);

#if defined(DMF_USER_MODE)
    HMODULE dllModule = GetModuleHandle(TEXT("ntdll.dll"));
    if (dllModule != 0)
    {
        RtlDecompressBuffer decompressBuffer = (RtlDecompressBuffer)GetProcAddress(dllModule, "RtlDecompressBuffer");
        ntStatus = decompressBuffer(CompressionFormat,
                                    UncompressedBuffer,
                                    UncompressedBufferSize,
                                    CompressedBuffer,
                                    CompressedBufferSize,
                                    FinalUncompressedSize);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "GetModuleHandle fails");
        ntStatus = STATUS_UNSUCCESSFUL;
    }
#elif defined(DMF_KERNEL_MODE)
    ntStatus = RtlDecompressBuffer(CompressionFormat,
                                   UncompressedBuffer,
                                   UncompressedBufferSize,
                                   CompressedBuffer,
                                   CompressedBufferSize,
                                   FinalUncompressedSize);
#endif

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_DriverFileRead(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* FileName, 
    _Out_ WDFMEMORY* FileContentMemory,
    _Out_opt_ UCHAR** Buffer,
    _Out_opt_ size_t* BufferLength
    )
/*++

Routine Description:

    Reads the contents of a file that has been installed into the directory where the driver was installed into.
    This Method is useful for drivers installed in "state-separated" version of Windows.

Arguments:

    DmfModule - This Module's handle.
    FileName - Name of the file (located in driver installation directory).
    FileContentMemory - Buffer handle where read file contents are copied. The caller owns this memory.
    Buffer - The buffer containing the read data.
    BufferLength - The length of the returned data.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    #if !defined(MAX_PATH)
        #define MAX_PATH    256
    #endif
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFMEMORY driverPathAndFileNameMemory;
    size_t driverPathAndFileNameSize;
    wchar_t* driverPathAndFileName;
    errno_t errorCode;
    WDFDRIVER driver;
    size_t maximumNumberOfCharacters;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 File);

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "Reading driver file [%S]",
                FileName);

    driverPathAndFileNameMemory = NULL;

    // Allocate temporary path name dynamically to save stack space.
    //
    driverPathAndFileNameSize = sizeof(WCHAR) * MAX_PATH;
    maximumNumberOfCharacters = driverPathAndFileNameSize / sizeof(WCHAR);
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               PagedPool,
                               MemoryTag,
                               driverPathAndFileNameSize,
                               &driverPathAndFileNameMemory,
                               (VOID**)&driverPathAndFileName);
    if (!NT_SUCCESS(ntStatus)) 
    {
        goto Exit;
    }

    // Zero out full target buffer onto which strings are copied so that the strings
    // will be zero terminated.
    //
    RtlZeroMemory(driverPathAndFileName,
                  driverPathAndFileNameSize);

    driver = WdfGetDriver();

    // NOTE: Methods for getting driver directory differ between Kernel and User-modes.
    //

#if defined(DMF_USER_MODE)

    // NOTE: Unable to find this function in any EWDK including 2.27:
    //       WdfDriverRetrieveDriverDataDirectoryString()
    //
    ntStatus = STATUS_NOT_SUPPORTED;
    // This check avoids unreachable code compilation error.
    //
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

#elif defined(DMF_KERNEL_MODE)

    DRIVER_OBJECT* driverObject = WdfDriverWdmGetDriverObject(driver);

    UNICODE_STRING fullPath;
    ntStatus = RtlUnicodeStringInit(&fullPath,
                                    NULL);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    ntStatus = IoQueryFullDriverPath(driverObject,
                                     &fullPath);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Get WCHAR from UNICODE so that the string functions can be used.
    //
    RtlStringCchCopyUnicodeString(driverPathAndFileName,
                                  maximumNumberOfCharacters,
                                  &fullPath);

#endif

    // Look for the final backlash in the driver path.
    //
    wchar_t *endOfPath = wcsrchr(driverPathAndFileName,
                                 L'\\');
    if (NULL == endOfPath)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid driver path: [%S]", driverPathAndFileName);
        ntStatus = STATUS_DIRECTORY_NOT_SUPPORTED;
        goto Exit;
    }

    // Remove driver file name so that only path remains.
    //
    *(endOfPath + 1) = L'\0';

    // Append the name of the file to read.
    //
    errorCode = wcscat_s(driverPathAndFileName,
                         maximumNumberOfCharacters,
                         FileName);
    if (errorCode != 0)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "driverPathAndFileName = [%S]", driverPathAndFileName);
    ntStatus = DMF_File_ReadEx(DmfModule,
                               driverPathAndFileName,
                               FileContentMemory,
                               Buffer,
                               BufferLength);

Exit:

#if defined(DMF_USER_MODE)
    #if (KMDF_VERSION_MAJOR >= 2) || (KMDF_VERSION_MINOR >= 27)
        if (fullPathString != NULL)
        {
            WdfObjectDelete(fullPathString);
        }
    #endif
#endif

    if (driverPathAndFileNameMemory != NULL)
    {
        WdfObjectDelete(driverPathAndFileNameMemory);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
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
    fileContentsMemory = WDF_NO_HANDLE;
    *FileContentMemory = WDF_NO_HANDLE;
    fileSize.QuadPart = 0;

#if defined(DMF_USER_MODE)
    BOOL returnValue = FALSE;
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
#elif defined(DMF_KERNEL_MODE)
    OBJECT_ATTRIBUTES fileAttributes;
    IO_STATUS_BLOCK fileIoStatusBlockOpen;
    IO_STATUS_BLOCK fileIoStatusBlockStatus;
    IO_STATUS_BLOCK fileIoStatusBlockRead;
    LARGE_INTEGER byteOffset;
    FILE_STANDARD_INFORMATION fileInformation;
    HANDLE fileHandle;

    InitializeObjectAttributes(&fileAttributes,
                               &fileNameString,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    fileHandle = NULL;
    ntStatus = ZwOpenFile(&fileHandle,
                          GENERIC_READ | SYNCHRONIZE,
                          &fileAttributes,
                          &fileIoStatusBlockOpen,
                          0,
                          FILE_SYNCHRONOUS_IO_NONALERT);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    fileIoStatusBlockStatus = fileIoStatusBlockOpen;
    fileIoStatusBlockRead = fileIoStatusBlockOpen;

    ntStatus = ZwQueryInformationFile(fileHandle,
                                      &fileIoStatusBlockStatus,
                                      &fileInformation,
                                      sizeof(fileInformation),
                                      FileStandardInformation);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    fileSize.QuadPart = sizeof(CHAR) * ((size_t)fileInformation.EndOfFile.QuadPart);
    byteOffset.QuadPart = 0;

#endif

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

#if !defined(MAXDWORD)
#define MAXDWORD 0xFFFFFFFF
#endif

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

#if defined(DMF_USER_MODE)
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
#elif defined(DMF_KERNEL_MODE)
        ntStatus = ZwReadFile(fileHandle,
                              NULL,
                              NULL,
                              NULL,
                              &fileIoStatusBlockRead,
                              readBuffer,
                              sizeOfOneRead,
                              &byteOffset,
                              NULL);
        if ((!NT_SUCCESS(ntStatus)) ||
            (fileIoStatusBlockRead.Information == 0))
        {
            ntStatus = STATUS_FILE_NOT_AVAILABLE;
            goto Exit;
        }
        numberOfBytesRead = (ULONG)fileIoStatusBlockRead.Information;
#endif

        readBuffer += numberOfBytesRead;
        bytesRemaining -= numberOfBytesRead;

        if (numberOfBytesRead == 0)
        {
            DmfAssert(bytesRemaining == 0);
        }
    }

    *FileContentMemory = fileContentsMemory;

Exit:

#if defined(DMF_USER_MODE)
    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }
#elif defined(DMF_KERNEL_MODE)
    if (fileHandle != NULL)
    {
        ZwClose(fileHandle);
        fileHandle = NULL;
    }
#endif

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_ReadEx(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* FileName, 
    _Out_ WDFMEMORY* FileContentMemory,
    _Out_opt_ UCHAR** Buffer,
    _Out_opt_ size_t* BufferLength
    )
/*++

Routine Description:

    Reads the contents of a file into a buffer that is allocated for the Client. Client is responsible
    for freeing the allocated memory.

Arguments:

    DmfModule - This Module's handle.
    FileName - Name of the file.
    FileContentMemory - Buffer handle where read file contents are copied. The caller owns this memory.
    Buffer - The buffer containing the read data.
    BufferLength - The length of the returned data.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    UNICODE_STRING unicodeFileName;
    WDFSTRING wdfFileNameString;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 File);

    wdfFileNameString = NULL;

    // Convert from WCHAR* to WDFSTRING.
    //
    RtlInitUnicodeString(&unicodeFileName,
                         FileName);
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    ntStatus = WdfStringCreate(&unicodeFileName,
                               &objectAttributes,
                               &wdfFileNameString);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfStringCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = DMF_File_Read(DmfModule,
                             wdfFileNameString,
                             FileContentMemory);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (Buffer != NULL ||
        BufferLength != NULL)
    {
        size_t bufferLength;
        UCHAR* buffer = (UCHAR*)WdfMemoryGetBuffer(*FileContentMemory,
                                                   &bufferLength);
        if (Buffer != NULL)
        {
            *Buffer = buffer;
        }
        if (BufferLength != NULL)
        {
            *BufferLength = bufferLength;
        }
    }

Exit:

    if (wdfFileNameString != NULL)
    {
        WdfObjectDelete(wdfFileNameString);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_File_Write(
    _In_ DMFMODULE DmfModule,
    _In_ WDFSTRING FileName,
    _In_ WDFMEMORY FileContentMemory
    )
/*++

Routine Description:

    Writes the contents of a wdf memory to a file.
    This function will try to create the file if it doesn't exists and overwrite current file.

Arguments:

    DmfModule - This Module's handle.
    FileName - Name of the file.
    FileContentMemory - Buffer handle of buffer holding data to write.

Return Value:

    NTSTATUS

--*/
{
    BYTE* writeBuffer;
    size_t bytesRemaining;
    DWORD sizeOfOneWrite;
    DWORD numberOfBytesWritten = 0;
    LARGE_INTEGER byteOffset;
    UNICODE_STRING fileNameString;
    HANDLE fileHandle;
    NTSTATUS ntStatus;
    IO_STATUS_BLOCK ioStatus = { 0 };

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 File);
#if defined(DMF_USER_MODE)
    fileHandle = INVALID_HANDLE_VALUE;
#elif defined(DMF_KERNEL_MODE)
    fileHandle = NULL;
#endif

    byteOffset.QuadPart = 0;
    ntStatus = STATUS_SUCCESS;
    
    WdfStringGetUnicodeString(FileName,
                              &fileNameString);

    TraceEvents(TRACE_LEVEL_VERBOSE, 
                DMF_TRACE, 
                "Writing to file %S ",
                fileNameString.Buffer);

    writeBuffer = (BYTE*) WdfMemoryGetBuffer(FileContentMemory,
                                             &bytesRemaining);

#if !defined(MAXDWORD)
#define MAXDWORD 0xFFFFFFFF
#endif
#if defined(DMF_USER_MODE)
    BOOL returnValue = FALSE;
    fileHandle = CreateFile(fileNameString.Buffer,
                            GENERIC_WRITE,
                            0,
                            NULL,
                            CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        TraceError(DMF_TRACE,
                   "CreateFile fails: to Open %S! ntStatus=%!STATUS!",
                   fileNameString.Buffer,
                   ntStatus);
        goto Exit;
    }

#elif defined(DMF_KERNEL_MODE)
    OBJECT_ATTRIBUTES fileAttributes;

    InitializeObjectAttributes(&fileAttributes,
                               &fileNameString, 
                               OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    ntStatus = ZwCreateFile(&fileHandle, 
                            GENERIC_ALL | SYNCHRONIZE, 
                            &fileAttributes,
                            &ioStatus,
                            0, 
                            FILE_ATTRIBUTE_NORMAL, 
                            0, 
                            FILE_OVERWRITE_IF, 
                            0, 
                            NULL,
                            FILE_SYNCHRONOUS_IO_NONALERT);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "ZwCreateFile fails ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }
#endif

    while (bytesRemaining > 0)
    {
        numberOfBytesWritten = 0;
        sizeOfOneWrite = MAXDWORD;
        if (bytesRemaining < sizeOfOneWrite)
        {
            sizeOfOneWrite = (DWORD)bytesRemaining;
        }

#if defined(DMF_USER_MODE)
        returnValue = WriteFile(fileHandle,
                                writeBuffer,
                                sizeOfOneWrite,
                                &numberOfBytesWritten,
                                NULL);
        if (!returnValue)
        {
            ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
            TraceError(DMF_TRACE,
                       "WriteFile fails: to Write %S !ntStatus=%!STATUS!", 
                       fileNameString.Buffer,
                       ntStatus);
            break;
        }

#elif defined(DMF_KERNEL_MODE)
        
        // Write to destination.
        //
        ntStatus = ZwWriteFile(fileHandle,
                               NULL,
                               NULL,
                               NULL,
                               &ioStatus,
                               writeBuffer,
                               sizeOfOneWrite,
                               &byteOffset,
                               NULL);

        if (!NT_SUCCESS(ntStatus) ||
            ioStatus.Information == 0)
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "ZwWriteFile failed ntStatus=%!STATUS!",
                        ntStatus);

            goto Exit;
        }

        numberOfBytesWritten = (ULONG)ioStatus.Information;
#endif

        writeBuffer += numberOfBytesWritten;
        bytesRemaining -= numberOfBytesWritten;

        if (numberOfBytesWritten == 0)
        {
            DmfAssert(bytesRemaining == 0);
        }
    }

Exit:
#if defined(DMF_USER_MODE)
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(fileHandle);
        fileHandle = INVALID_HANDLE_VALUE;
    }
#elif defined(DMF_KERNEL_MODE)
    if (fileHandle != NULL)
    {
        ZwClose(fileHandle);
        fileHandle = NULL;
    }
#endif

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_File.c
//
