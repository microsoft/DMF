/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_UefiOperation.c

Abstract:

    This Module provides UEFI basic operations.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_UefiOperation.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This Module has no Context.
//
DMF_MODULE_DECLARE_NO_CONTEXT(UefiOperation)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(UefiOperation)

#define MemoryTag 'MUEF'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if defined(DMF_USER_MODE)

#define GUID_STRING_SIZE                                        (sizeof(L"{00000000-0000-0000-0000-000000000000}") / sizeof(WCHAR))
#define MAX_VARIABLE_NAME_LENGTH                                128

_IRQL_requires_max_(PASSIVE_LEVEL)
HRESULT 
UefiOperation_ProcessPrivilegeSet(
    _In_ BOOL Enable, 
    _Inout_ LPCTSTR Privilege
    )
/*++

Routine Description:

    Attempt to enable (or disable) the specified privilege (e.g. SE_SYSTEM_ENVIRONMENT_NAME or SE_DEBUG_NAME) using the given process token.
    Otherwise, Get/SetFirmwareEnvironmentVariable API's will not succeed (in the case of SE_SYSTEM_ENVIRONMENT_NAME.)
    In order to succeed, the process will have to have been launched as Administrator.

Arguments:

    Enable - Enable privilege or not
    Privilege - String of Privilege type

Return Value:

    S_OK if successful.
    Other if there is an error.

--*/
{
    HRESULT result;
    HANDLE token;
    TOKEN_PRIVILEGES tokenPrivileges;
    BOOL status;
    LUID luid;

    FuncEntry(DMF_TRACE);

    result = S_OK;
    token = NULL;
    tokenPrivileges = { 0 };

    // Get the access token for this process. 
    //
    status = (OpenThreadToken(GetCurrentThread(), 
                              TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                              FALSE, 
                              &token)
                              || 
                              OpenProcessToken(GetCurrentProcess(), 
                              TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, 
                              &token));
    if (!status)
    {
        result = HRESULT_FROM_WIN32(::GetLastError());
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "OpenThreadToken and OpenProcessToken fails: result=%!HRESULT!", result);
        goto Exit;
    }

    // Get the LUID for the specified privilege. 
    //
    status = LookupPrivilegeValue(NULL,          
                                  Privilege, 
                                  &luid);        
    if (!status)
    {
        result = HRESULT_FROM_WIN32(::GetLastError());
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "LookupPrivilegeValue fails: result=%!HRESULT!", result);
        goto Exit;
    }
    tokenPrivileges.PrivilegeCount = 1;
    tokenPrivileges.Privileges[0].Luid = luid;
    tokenPrivileges.Privileges[0].Attributes = (Enable) ? SE_PRIVILEGE_ENABLED : 0;

    // Enable the privilege or disable all privileges.
    //
    status = AdjustTokenPrivileges(token,
                                   FALSE,
                                   &tokenPrivileges,
                                   sizeof(tokenPrivileges),
                                   (PTOKEN_PRIVILEGES)NULL,
                                   (PDWORD)NULL);
    if (!status)
    {
        result = HRESULT_FROM_WIN32(::GetLastError());
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "AdjustTokenPrivileges fails: result=%!HRESULT!", result);
        goto Exit;
    }

Exit:

    if (token != NULL)
    {
        CloseHandle(token);
    }

    FuncExit(DMF_TRACE, "result=%!HRESULT!", result);

    return result;
}

#endif //defined(DMF_USER_MODE)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UefiOperation_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type UefiOperation.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_UefiOperation;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);
    
    // Initialize module context
    //
    DMF_MODULE_DESCRIPTOR_INIT(dmfModuleDescriptor_UefiOperation,
                               UefiOperation,
                               DMF_MODULE_OPTIONS_DISPATCH,
                               DMF_MODULE_OPEN_OPTION_OPEN_Create);

    // ObjectAttributes must be initialized and
    // ParentObject attribute must be set to either WDFDEVICE or DMFMODULE.
    //
    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_UefiOperation,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// Module Methods
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UefiOperation_FirmwareEnvironmentVariableAllocateGet(
    _In_ DMFMODULE DmfModule,
    _In_ UNICODE_STRING* Name,
    _In_ LPGUID Guid,
    _Out_ VOID** VariableBuffer,
    _Inout_ ULONG* VariableBufferSize,
    _Out_ WDFMEMORY* VariableBufferHandle,
    _Out_opt_ ULONG* Attributes
    )
/*++

Routine Description:

    Get the UEFI variable data to certain UEFI Guid and name in both User-mode and Kernel-mode.
    Allocates the required memory size and the client is responsible to free the memory.

Arguments:

    DmfModule - This Module's handle.
    Name - Name of UEFI variable to read data from.
    Guid - GUID of UEFI variable to read data from.
    VariableBuffer - Buffer that will store the data that is read from the UEFI variable.
    VariableBufferSize - As input, it passes the desired size that needs to be read.
                         As output, it sends back the actual size that was read from UEFI.
    VariableBufferHandle - WDF memory handle for the allocated memory
    Attributes - Location to which the routine writes the attributes of the specified environment variable.
                 (Optional argument, only used in Kernel-mode)

Return Value:

    STATUS_SUCCESS if successful.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    VOID* localBuffer;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 UefiOperation);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);

#if !defined(DMF_USER_MODE)
    // Get the variable length
    //
    ntStatus = ExGetFirmwareEnvironmentVariable(Name,
                                                Guid,
                                                NULL,
                                                VariableBufferSize,
                                                Attributes);
    if ((!NT_SUCCESS(ntStatus)) &&
        (ntStatus != STATUS_BUFFER_TOO_SMALL))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "ExGetFirmwareEnvironmentVariable fails to read %S %!STATUS!",
                    Name->Buffer,
                    ntStatus);
        goto Exit;
    }

    // Allocate memory for exact variable size.
    //
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               NonPagedPoolNx,
                               MemoryTag,
                               *VariableBufferSize,
                               VariableBufferHandle,
                               (VOID**)&localBuffer);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfMemoryCreate fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    RtlZeroMemory(localBuffer,
                  *VariableBufferSize);

    // Read the variable.
    //
    ntStatus = ExGetFirmwareEnvironmentVariable(Name,
                                                Guid,
                                                localBuffer,
                                                VariableBufferSize,
                                                Attributes);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "ExGetFirmwareEnvironmentVariable fails: ntStatus=%!STATUS!",
                    ntStatus);
        ntStatus = STATUS_UNSUCCESSFUL;
        WdfObjectDelete(VariableBufferHandle);
        goto Exit;
    }

    *VariableBuffer = localBuffer;

#else // !defined(DMF_USER_MODE).

// Initial Memory Allocation Attempt Size (in Bytes).
//
#define INITIAL_MEMORY_ALLOCATION_SIZE_BYTES 16

// Maximum Memory Allocation Retries upto 1 MB.
// 
#define MAXIMUM_MEMORY_ALLOCATION_ATTEMPTS (1024 * 1024)

// Macro to check if ntStatus is equal to WIN32's Error Code (ERROR_INSUFFICIENT_BUFFER).
//
#define IS_ERROR_INSUFFICIENT_BUFFER(ntStatus) ((ntStatus & 0xFF) == ERROR_INSUFFICIENT_BUFFER)

    *VariableBufferSize = INITIAL_MEMORY_ALLOCATION_SIZE_BYTES;
    BOOLEAN allocateMoreMemory;

    do {

        allocateMoreMemory = FALSE;

        // Allocate memory for exact variable size.
        //
        ntStatus = WdfMemoryCreate(&objectAttributes,
                                   NonPagedPoolNx,
                                   MemoryTag,
                                   *VariableBufferSize,
                                   VariableBufferHandle,
                                   (VOID**)&localBuffer);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "WdfMemoryCreate fails: ntStatus=%!STATUS!",
                        ntStatus);
            goto Exit;
        }

        RtlZeroMemory(localBuffer,
                      *VariableBufferSize);

        ntStatus = DMF_UefiOperation_FirmwareEnvironmentVariableGetEx(DmfModule,
                                                                      Name,
                                                                      Guid,
                                                                      localBuffer,
                                                                      VariableBufferSize,
                                                                      Attributes);

        if (IS_ERROR_INSUFFICIENT_BUFFER(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "DMF_UefiOperation_FirmwareEnvironmentVariableGetEx fails: ntStatus=%!STATUS!",
                        ntStatus);

            // Increase memory allocation size by double and try.
            //
            *VariableBufferSize = *VariableBufferSize << 1;
            if (*VariableBufferSize <= MAXIMUM_MEMORY_ALLOCATION_ATTEMPTS)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "Retrying with more buffer size...");
                allocateMoreMemory = TRUE;
            }
            WdfObjectDelete(*VariableBufferHandle);
        }
    } while (allocateMoreMemory);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "DMF_UefiOperation_FirmwareEnvironmentVariableGetEx fails: ntStatus=%!STATUS!",
                    ntStatus);
        WdfObjectDelete(*VariableBufferHandle);
        goto Exit;
    }

    *VariableBuffer = localBuffer;

#endif // !defined(DMF_USER_MODE).

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
    return ntStatus;
}
#pragma code_seg()

#if defined(DMF_USER_MODE)

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS 
DMF_UefiOperation_FirmwareEnvironmentVariableGet(
    _In_ DMFMODULE DmfModule,
    _In_ LPCTSTR Name,
    _In_ LPCTSTR Guid,
    _Out_writes_bytes_opt_(*VariableBufferSize) VOID* VariableBuffer,
    _Inout_ DWORD* VariableBufferSize
    )
/*++

Routine Description:

    Allows the Client to get the UEFI variable data from certain UEFI Guid and name.
    This Method is deprecated. Use DMF_UefiOperation_FirmwareEnvironmentVariableGetEx() instead.

Arguments:

    DmfModule - This Module's handle.
    Name - Name of UEFI variable to read data from.
    Guid - GUID of UEFI variable to read data from.
    VariableBuffer - Buffer that will store the data that is read from the UEFI variable.
    VariableBufferSize - As input, it passes the desired size that needs to be read. 
                      As output, it sends back the actual size that was read from UEFI.

Return Value:

    STATUS_SUCCESS if successful.
    Other NTSTATUS if there is an error.

--*/
{
    HRESULT result;
    NTSTATUS ntStatus;
    DWORD size;

    FuncEntry(DMF_TRACE);

    // NOTE: In this Method, DmfModule can be NULL to support this call before WdfDeviceCreate() has been called.
    //
    UNREFERENCED_PARAMETER(DmfModule);

    result = UefiOperation_ProcessPrivilegeSet(TRUE,
                                               SE_SYSTEM_ENVIRONMENT_NAME);
    if (!SUCCEEDED(result))
    {
        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UefiOperation_ProcessPrivilegeSet fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    size = GetFirmwareEnvironmentVariable(Name,
                                          Guid,
                                          VariableBuffer,
                                          *VariableBufferSize);
    if (size == 0)
    {
        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "GetFirmwareEnvironmentVariable fails: [%ls] ntStatus=%!STATUS!", Name, ntStatus);
        goto Exit;
    }

    *VariableBufferSize = size;

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

#endif //defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UefiOperation_FirmwareEnvironmentVariableGetEx(
    _In_opt_ DMFMODULE DmfModule,
    _In_ UNICODE_STRING* Name,
    _In_ LPGUID Guid,
    _Out_writes_bytes_opt_(*VariableBufferSize) VOID* VariableBuffer,
    _Inout_ ULONG* VariableBufferSize,
    _Out_opt_ ULONG* Attributes
    )
/*++

Routine Description:

    Get the UEFI variable data to certain UEFI Guid and name in both User-mode and Kernel-mode.

Arguments:

    DmfModule - This Module's handle.
    Name - Name of UEFI variable to read data from.
    Guid - GUID of UEFI variable to read data from.
    VariableBuffer - Buffer that will store the data that is read from the UEFI variable.
    VariableBufferSize - As input, it passes the desired size that needs to be read. 
                      As output, it sends back the actual size that was read from UEFI.
    Attributes - Location to which the routine writes the attributes of the specified environment variable.
                 (Optional argument, only used in Kernel-mode)

Return Value:

    STATUS_SUCCESS if successful.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // NOTE: In this Method, DmfModule can be NULL to support this call before WdfDeviceCreate() has been called.
    //
    UNREFERENCED_PARAMETER(DmfModule);

#if !defined(DMF_USER_MODE)
    ntStatus = ExGetFirmwareEnvironmentVariable(Name,
                                                Guid,
                                                VariableBuffer,
                                                VariableBufferSize,
                                                Attributes);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ExGetFirmwareEnvironmentVariable fails to read %S %!STATUS!",
                                                   Name->Buffer,
                                                   ntStatus);
        goto Exit;
    }

#else // !defined(DMF_USER_MODE).
    UINT returnValue;
    WCHAR guidString[GUID_STRING_SIZE];
    WCHAR variableName[MAX_VARIABLE_NAME_LENGTH];

    if (Attributes != NULL)
    {
        // For SAL.
        //
        *Attributes = 0;
    }

    returnValue = StringFromGUID2(*Guid,
                                  guidString,
                                  ARRAYSIZE(guidString));
    if (!returnValue)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "StringFromGUID2 failed to convert GUID to String. ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Ensure that the returned string from StringFromGUID2 and the variable Name string are null-terminated before passing as parameters.
    //
    guidString[GUID_STRING_SIZE - 1] = L'\0';

    // Create the zero terminated variable name string.
    //
    size_t numberOfElementsToCopy = (Name->Length) / sizeof(WCHAR);

    wcsncpy_s(variableName, 
              ARRAYSIZE(variableName), 
              Name->Buffer, 
              numberOfElementsToCopy);

    variableName[numberOfElementsToCopy] = L'\0';

    ntStatus = DMF_UefiOperation_FirmwareEnvironmentVariableGet(DmfModule,
                                                                (LPCTSTR)variableName,
                                                                (LPCTSTR)guidString,
                                                                VariableBuffer,
                                                                VariableBufferSize);

#endif // !defined(DMF_USER_MODE).

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
    return ntStatus;
}
#pragma code_seg()

#if defined(DMF_USER_MODE)

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS 
DMF_UefiOperation_FirmwareEnvironmentVariableSet(
    _In_ DMFMODULE DmfModule,
    _In_ LPCTSTR Name,
    _In_ LPCTSTR Guid,
    _In_reads_(VariableBufferSize) VOID* VariableBuffer,
    _In_ DWORD VariableBufferSize
    )
/*++

Routine Description:

    Set the UEFI variable data to certain UEFI Guid and name.
    This Method is deprecated. Use DMF_UefiOperation_FirmwareEnvironmentVariableSetEx() instead.

Arguments:

    DmfModule - This Module's handle.
    Name - Name of UEFI variable to write data to.
    Guid - GUID of UEFI variable to write data to.
    VariableBuffer - Buffer that stores the data that is to be written to the UEFI variable.
    VariableBufferSize - Size of VariableBuffer in bytes.

Return Value:

    STATUS_SUCCESS if successful.
    Other NTSTATUS if there is an error.

--*/
{
    HRESULT hresult;
    NTSTATUS ntStatus;
    BOOL result;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 UefiOperation);

    hresult = UefiOperation_ProcessPrivilegeSet(TRUE,
                                                SE_SYSTEM_ENVIRONMENT_NAME);
    if (!SUCCEEDED(hresult))
    {
        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "UefiOperation_ProcessPrivilegeSet fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    result = SetFirmwareEnvironmentVariable(Name,
                                            Guid,
                                            VariableBuffer,
                                            VariableBufferSize);
    if (!result)
    {
        ntStatus = NTSTATUS_FROM_WIN32(GetLastError());
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "SetFirmwareEnvironmentVariable fails: [%ls] ntStatus=%!STATUS!", Name, ntStatus);
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "SetFirmwareEnvironmentVariable success: [%ls]", Name);

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
    return ntStatus;
}

#endif //defined(DMF_USER_MODE)

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UefiOperation_FirmwareEnvironmentVariableSetEx(
    _In_ DMFMODULE DmfModule,
    _In_ UNICODE_STRING* Name,
    _In_ LPGUID Guid,
    _In_reads_bytes_opt_(VariableBufferSize) VOID* VariableBuffer,
    _In_ ULONG VariableBufferSize,
    _In_ ULONG Attributes
    )
/*++

Routine Description:
    
    Set the UEFI variable data to certain UEFI Guid and name in both User-mode and Kernel-mode.

Arguments:

    DmfModule - This Module's handle.
    Name - Name of UEFI variable to write data to. 
    Guid - GUID of UEFI variable to write data to.
    VariableBuffer - Buffer that stores the data that is to be written to the UEFI variable.
    VariableBufferSize - Size of VariableBuffer in bytes.
    Attributes - The attributes to assign to the specified environment variable.
                 (Optional argument, only used in Kernel-mode)

Return Value:

    STATUS_SUCCESS if successful.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 UefiOperation);

#if !defined(DMF_USER_MODE)
    ntStatus = ExSetFirmwareEnvironmentVariable(Name,
                                                Guid,
                                                VariableBuffer,
                                                VariableBufferSize,
                                                Attributes);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ExSetFirmwareEnvironmentVariable fails to write %S %!STATUS!",
                                                  Name->Buffer,
                                                  ntStatus);
        goto Exit;
    }

#else // !defined(DMF_USER_MODE).
    UNREFERENCED_PARAMETER(Attributes);

    UINT returnValue;
    WCHAR guidString[GUID_STRING_SIZE];
    WCHAR variableName[MAX_VARIABLE_NAME_LENGTH];

    returnValue = StringFromGUID2(*Guid,
                                  guidString,
                                  ARRAYSIZE(guidString));
    if (!returnValue)
    {
        ntStatus = STATUS_BUFFER_TOO_SMALL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "StringFromGUID2 failed to convert GUID to String. ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Ensure that the returned string from StringFromGUID2 and the variable Name string are null-terminated before passing as parameters.
    //
    guidString[GUID_STRING_SIZE - 1] = L'\0';

    // Create the zero terminated variable name string.
    //
    size_t numberOfElementsToCopy = (Name->Length) / sizeof(WCHAR);

    wcsncpy_s(variableName, 
              ARRAYSIZE(variableName), 
              Name->Buffer, 
              numberOfElementsToCopy);

    variableName[numberOfElementsToCopy] = L'\0';

    ntStatus = DMF_UefiOperation_FirmwareEnvironmentVariableSet(DmfModule,
                                                                (LPCTSTR)variableName,
                                                                (LPCTSTR)guidString,
                                                                VariableBuffer,
                                                                VariableBufferSize);

#endif // !defined(DMF_USER_MODE).

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);
    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_UefiOperation.c
//
