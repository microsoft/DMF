/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_Tests_Registry.c

Abstract:

    Functional tests for DMF_Registry Module.

Environment:

    Kernel-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"

#include "Dmf_Tests_Registry.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//
#if !defined(DMF_USER_MODE)
#define REGISTRY_PATH_NAME          L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\DmfKTest"
#else
#define REGISTRY_PATH_NAME          L"SOFTWARE\\Microsoft\\DmfUTest"
#endif
#define VALUENAME_STRING            L"string"
#define VALUENAME_MULTISTRING       L"multistring"
#define VALUENAME_BINARY            L"binary"
#define VALUENAME_DWORD             L"ulong"
#define VALUENAME_QWORD             L"ulonglong"

#define VALUEDATA_DWORD             0x87654321
#define VALUEDATA_QWORD             0x1234567887654321

#define SUBKEYNAME_1                L"subkey1"
#define SUBKEYNAME_2                L"subkey2"

#define ZERO_BUFFER(buffer)         RtlZeroMemory(&buffer, sizeof(buffer));

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Registry Module to test
    //
    DMFMODULE DmfModuleRegistry;

    // Helper Module - Alertable Sleep
    //
    DMFMODULE DmfModuleAlertableSleep;
    // Helper Module - Work Thread
    //
    DMFMODULE DmfModuleThread;
    // Flag signaling that tests should be aborted.
    //
    BOOLEAN AbortTests;
} DMF_CONTEXT_Tests_Registry;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Tests_Registry)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Tests_Registry)

// Memory Pool Tag.
//
#define MemoryTag 'geRT'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static UCHAR binaryOriginal[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
static WCHAR stringOriginal[] = { L"DmfTest" };
static WCHAR multiStringOriginal[] = { L"DmfTest 1\0DmfTest 2\0DmfTest 3\0\0" };
static ULONG ulongOriginal = VALUEDATA_DWORD;
static ULONGLONG ulonglongOriginal = VALUEDATA_QWORD;

static PWCHAR subkeys[] = {SUBKEYNAME_1, 
                           SUBKEYNAME_2};

#if !defined(DMF_USER_MODE)
// A set of entries in the branch.
//
static
Registry_Entry
RegistryEntries[] =
{
    Registry_TABLE_ENTRY_REG_SZ(VALUENAME_STRING, stringOriginal),
    Registry_TABLE_ENTRY_REG_MULTI_SZ(VALUENAME_MULTISTRING, multiStringOriginal),
    Registry_TABLE_ENTRY_REG_BINARY(VALUENAME_BINARY, binaryOriginal, sizeof(binaryOriginal)),
    Registry_TABLE_ENTRY_REG_DWORD(VALUENAME_DWORD, VALUEDATA_DWORD),
    Registry_TABLE_ENTRY_REG_QWORD(VALUENAME_QWORD, VALUEDATA_QWORD),
};

// A branch to be written to the Registry.
//
static
Registry_Branch
RegistryBranches[] =
{
    {
        Registry_BRANCH_PREFIX_NONE,
        RegistryEntries,
        ARRAYSIZE(RegistryEntries)
    },
};

// The sets of branches to be written to the Registry.
//
static
Registry_Tree
RegistryTree[] =
{
    {
        REGISTRY_PATH_NAME,
        RegistryBranches,
        ARRAYSIZE(RegistryBranches)
    },
};
#endif

struct EnumCallbackContext
{
    LONG NumberOfKeys;
};

struct CompareCallbackContext
{
    VOID* ClientData;
    ULONG ClientDataSize;
};

_Function_class_(EVT_DMF_Registry_KeyEnumerationCallback)
static
BOOLEAN
RegistryKeyEnumerationFunction(
    _In_ VOID* ClientContext,
    _In_ HANDLE RootHandle,
    _In_ PWCHAR KeyName
    )
{
    EnumCallbackContext* callbackContext;
    LONG index;

    UNREFERENCED_PARAMETER(RootHandle);

    callbackContext = (EnumCallbackContext*)ClientContext;
    DmfAssert(callbackContext != NULL);

    for (index = 0; index < ARRAYSIZE(subkeys); ++index)
    {
        if (0 == wcscmp(KeyName,
                        subkeys[index]))
        {
            callbackContext->NumberOfKeys++;
        }
    }

    return TRUE;
}

_Function_class_(EVT_DMF_Registry_ValueComparisonCallback)
static
BOOLEAN
RegistryValueComparisonFunction_IfEqual(
    _In_ DMFMODULE DmfModule,
    _In_opt_ VOID* ClientContext,
    _In_ VOID* ValueDataInRegistry,
    _In_ ULONG ValueDataInRegistrySize,
    _In_opt_ VOID* ClientDataInRegistry,
    _In_ ULONG ClientDataInRegistrySize
    )
{
    ULONG sizeToCompare;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientContext);

    sizeToCompare = min(ValueDataInRegistrySize, ClientDataInRegistrySize);

    DmfAssert(ValueDataInRegistrySize == ClientDataInRegistrySize);

    //''ClientDataInRegistry' could be '0':  this does not adhere to the specification for the function 'RtlCompareMemor'
    //
    #pragma warning(suppress:6387)
    return (RtlCompareMemory(ValueDataInRegistry,
                             ClientDataInRegistry,
                             sizeToCompare) == sizeToCompare);
}

_Function_class_(EVT_DMF_Registry_ValueComparisonCallback)
static
BOOLEAN
RegistryValueComparisonFunction_IfEqualToContext(
    _In_ DMFMODULE DmfModule,
    _In_opt_ VOID* ClientContext,
    _In_ VOID* ValueDataInRegistry,
    _In_ ULONG ValueDataInRegistrySize,
    _In_opt_ VOID* ClientDataInRegistry,
    _In_ ULONG ClientDataInRegistrySize
    )
{
    CompareCallbackContext* callbackContext;
    ULONG sizeToCompare;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientDataInRegistry);
    UNREFERENCED_PARAMETER(ClientDataInRegistrySize);

    callbackContext = (CompareCallbackContext*)ClientContext;
    DmfAssert(NULL != callbackContext);

    #pragma warning(suppress:28182)
    sizeToCompare = min(ValueDataInRegistrySize, callbackContext->ClientDataSize);

    DmfAssert(ValueDataInRegistrySize == callbackContext->ClientDataSize);

    return (RtlCompareMemory(ValueDataInRegistry,
                             callbackContext->ClientData,
                             sizeToCompare) == sizeToCompare);
}

_Function_class_(EVT_DMF_Registry_ValueComparisonCallback)
static
BOOLEAN
RegistryValueComparisonFunction_IfDefault(
    _In_ DMFMODULE DmfModule,
    _In_opt_ VOID* ClientContext,
    _In_ VOID* ValueDataInRegistry,
    _In_ ULONG ValueDataInRegistrySize,
    _In_opt_ VOID* ClientDataInRegistry,
    _In_ ULONG ClientDataInRegistrySize
)
{
    ULONG sizeToCompare;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientDataInRegistry);
    UNREFERENCED_PARAMETER(ClientContext);
    UNREFERENCED_PARAMETER(ClientDataInRegistrySize);

    sizeToCompare = min(ValueDataInRegistrySize, sizeof(ulongOriginal));

    DmfAssert(ValueDataInRegistrySize == ClientDataInRegistrySize);
    DmfAssert(ValueDataInRegistrySize == sizeof(ulongOriginal));

    return (RtlCompareMemory(ValueDataInRegistry,
                             &ulongOriginal,
                             sizeToCompare) == sizeToCompare);
}

#if !defined(DMF_USER_MODE)
// User-mode driver cannot create arbitrary keys at runtime, 
// so test should not delete them at runtime.  
// For User-mode, The arbitrary path is created through INF and not deleted during test.
//
#pragma code_seg("PAGE")
static
VOID
Tests_Registry_ValidatePathDeleted(
    _In_ DMFMODULE DmfModuleRegistry
)
{
    NTSTATUS ntStatus;
    HANDLE keyHandle;

    PAGED_CODE();

    keyHandle = NULL;
    ntStatus = DMF_Registry_HandleOpenByNameEx(DmfModuleRegistry,
                                               REGISTRY_PATH_NAME,
                                               0,
                                               FALSE,
                                               &keyHandle);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
    DmfAssert(NULL == keyHandle);

    if (NULL != keyHandle)
    {
        DMF_Registry_HandleClose(DmfModuleRegistry,
                                 keyHandle);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_DeleteValues(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = DMF_Registry_PathAndValueDelete(DmfModuleRegistry,
                                               REGISTRY_PATH_NAME,
                                               VALUENAME_STRING);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
    ntStatus = DMF_Registry_PathAndValueDelete(DmfModuleRegistry,
                                               REGISTRY_PATH_NAME,
                                               VALUENAME_MULTISTRING);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
    ntStatus = DMF_Registry_PathAndValueDelete(DmfModuleRegistry,
                                               REGISTRY_PATH_NAME,
                                               VALUENAME_BINARY);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
    ntStatus = DMF_Registry_PathAndValueDelete(DmfModuleRegistry,
                                               REGISTRY_PATH_NAME,
                                               VALUENAME_DWORD);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
    ntStatus = DMF_Registry_PathAndValueDelete(DmfModuleRegistry,
                                               REGISTRY_PATH_NAME,
                                               VALUENAME_QWORD);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_DeletePath(
    _In_ DMFMODULE DmfModuleRegistry
)
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = DMF_Registry_RegistryPathDelete(DmfModuleRegistry,
                                               REGISTRY_PATH_NAME);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_ReadNonExistent(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    NTSTATUS ntStatus;
    WCHAR binary[64];
    WCHAR string[64];
    WCHAR multiString[64];
    ULONGLONG ulonglong;
    ULONG ulong;

    PAGED_CODE();

    ZERO_BUFFER(string);
    ntStatus = DMF_Registry_PathAndValueReadString(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_STRING,
                                                   string,
                                                   ARRAYSIZE(string),
                                                   NULL);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
    ZERO_BUFFER(multiString);
    ntStatus = DMF_Registry_PathAndValueReadMultiString(DmfModuleRegistry,
                                                        REGISTRY_PATH_NAME,
                                                        VALUENAME_MULTISTRING,
                                                        multiString,
                                                        ARRAYSIZE(multiString),
                                                        NULL);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
    ZERO_BUFFER(binary);
    ntStatus = DMF_Registry_PathAndValueReadBinary(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_BINARY,
                                                   (UCHAR*)binary,
                                                   sizeof(binary),
                                                   NULL);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
    ZERO_BUFFER(ulong);
    ntStatus = DMF_Registry_PathAndValueReadDword(DmfModuleRegistry,
                                                  REGISTRY_PATH_NAME,
                                                  VALUENAME_DWORD,
                                                  &ulong);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
    ZERO_BUFFER(ulonglong);
    ntStatus = DMF_Registry_PathAndValueReadQword(DmfModuleRegistry,
                                                  REGISTRY_PATH_NAME,
                                                  VALUENAME_QWORD,
                                                  &ulonglong);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_WriteValues(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = DMF_Registry_PathAndValueWriteString(DmfModuleRegistry,
                                                    REGISTRY_PATH_NAME,
                                                    VALUENAME_STRING,
                                                    stringOriginal,
                                                    ARRAYSIZE(stringOriginal));
    DmfAssert(NT_SUCCESS(ntStatus));
    ntStatus = DMF_Registry_PathAndValueWriteMultiString(DmfModuleRegistry,
                                                         REGISTRY_PATH_NAME,
                                                         VALUENAME_MULTISTRING,
                                                         multiStringOriginal,
                                                         ARRAYSIZE(multiStringOriginal));
    DmfAssert(NT_SUCCESS(ntStatus));
    ntStatus = DMF_Registry_PathAndValueWriteBinary(DmfModuleRegistry,
                                                    REGISTRY_PATH_NAME,
                                                    VALUENAME_BINARY,
                                                    (UCHAR*)binaryOriginal,
                                                    sizeof(binaryOriginal));
    DmfAssert(NT_SUCCESS(ntStatus));
    ntStatus = DMF_Registry_PathAndValueWriteDword(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_DWORD,
                                                   ulongOriginal);
    DmfAssert(NT_SUCCESS(ntStatus));
    ntStatus = DMF_Registry_PathAndValueWriteQword(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_QWORD,
                                                   ulonglongOriginal);
    DmfAssert(NT_SUCCESS(ntStatus));
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_ReadAndValidateBytesRead(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    NTSTATUS ntStatus;
    ULONG bytesRead;

    PAGED_CODE();

    // Disable "Using 'bytesRead' from failed function call for this section. By design
    // bytesRead contains the number of bytes needed when STATUS_BUFFER_TOO_SMALL is returned.
    //
    #pragma warning(push)
    #pragma warning(disable: 6102)
    ntStatus = DMF_Registry_PathAndValueReadString(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_STRING,
                                                   NULL,
                                                   0,
                                                   &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(stringOriginal));
    ntStatus = DMF_Registry_PathAndValueReadMultiString(DmfModuleRegistry,
                                                        REGISTRY_PATH_NAME,
                                                        VALUENAME_MULTISTRING,
                                                        NULL,
                                                        0,
                                                        &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(multiStringOriginal));
    ntStatus = DMF_Registry_PathAndValueReadBinary(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_BINARY,
                                                   NULL,
                                                   0,
                                                   &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(binaryOriginal));
    #pragma warning(pop)
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_ReadAndValidateData(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    NTSTATUS ntStatus;
    WCHAR binary[64];
    WCHAR string[64];
    WCHAR multiString[64];
    ULONGLONG ulonglong;
    ULONG ulong;

    PAGED_CODE();

    ZERO_BUFFER(string);
    ntStatus = DMF_Registry_PathAndValueReadString(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_STRING,
                                                   string,
                                                   ARRAYSIZE(string),
                                                   NULL);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(string,
                               stringOriginal,
                               sizeof(stringOriginal)) == sizeof(stringOriginal));
    ZERO_BUFFER(multiString);
    ntStatus = DMF_Registry_PathAndValueReadMultiString(DmfModuleRegistry,
                                                        REGISTRY_PATH_NAME,
                                                        VALUENAME_MULTISTRING,
                                                        multiString,
                                                        ARRAYSIZE(multiString),
                                                        NULL);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(multiString,
                               multiStringOriginal,
                               sizeof(multiStringOriginal)) == sizeof(multiStringOriginal));
    ZERO_BUFFER(binary);
    ntStatus = DMF_Registry_PathAndValueReadBinary(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_BINARY,
                                                   (UCHAR*)binary,
                                                   sizeof(binary),
                                                   NULL);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(binary,
                               binaryOriginal,
                               sizeof(binaryOriginal)) == sizeof(binaryOriginal));
    ZERO_BUFFER(ulong);
    ntStatus = DMF_Registry_PathAndValueReadDword(DmfModuleRegistry,
                                                  REGISTRY_PATH_NAME,
                                                  VALUENAME_DWORD,
                                                  &ulong);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(ulong == ulongOriginal);
    ZERO_BUFFER(ulong);

    ntStatus = DMF_Registry_PathAndValueReadDwordAndValidate(DmfModuleRegistry,
                                                             REGISTRY_PATH_NAME,
                                                             VALUENAME_DWORD,
                                                             &ulong,
                                                             0,
                                                             1);
    DmfAssert(STATUS_INVALID_DEVICE_REQUEST == ntStatus);
    ZERO_BUFFER(ulong);
    ntStatus = DMF_Registry_PathAndValueReadDwordAndValidate(DmfModuleRegistry,
                                                             REGISTRY_PATH_NAME,
                                                             VALUENAME_DWORD,
                                                             &ulong,
                                                             0x00000000,
                                                             0xFFFFFFFF);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(ulong == ulongOriginal);
    ZERO_BUFFER(ulonglong);
    ntStatus = DMF_Registry_PathAndValueReadQword(DmfModuleRegistry,
                                                  REGISTRY_PATH_NAME,
                                                  VALUENAME_QWORD,
                                                  &ulonglong);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(ulonglong == ulonglongOriginal);
    ZERO_BUFFER(ulonglong);
    ntStatus = DMF_Registry_PathAndValueReadQwordAndValidate(DmfModuleRegistry,
                                                             REGISTRY_PATH_NAME,
                                                             VALUENAME_QWORD,
                                                             &ulonglong,
                                                             0,
                                                             1);
    DmfAssert(STATUS_INVALID_DEVICE_REQUEST == ntStatus);
    ZERO_BUFFER(ulonglong);
    ntStatus = DMF_Registry_PathAndValueReadQwordAndValidate(DmfModuleRegistry,
                                                             REGISTRY_PATH_NAME,
                                                             VALUENAME_QWORD,
                                                             &ulonglong,
                                                             0x0000000000000000,
                                                             0xFFFFFFFFFFFFFFFF);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(ulonglong == ulonglongOriginal);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_ReadAndValidateDataAndBytesRead(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    NTSTATUS ntStatus;
    WCHAR binary[64];
    WCHAR string[64];
    WCHAR multiString[64];
    ULONG bytesRead;

    PAGED_CODE();

    ZERO_BUFFER(string);
    ntStatus = DMF_Registry_PathAndValueReadString(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_STRING,
                                                   string,
                                                   ARRAYSIZE(string),
                                                   &bytesRead);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(string,
                               stringOriginal,
                               sizeof(stringOriginal)) == bytesRead);
    ZERO_BUFFER(multiString);
    ntStatus = DMF_Registry_PathAndValueReadMultiString(DmfModuleRegistry,
                                                        REGISTRY_PATH_NAME,
                                                        VALUENAME_MULTISTRING,
                                                        multiString,
                                                        ARRAYSIZE(multiString),
                                                        &bytesRead);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(multiString,
                               multiStringOriginal,
                               sizeof(multiStringOriginal)) == bytesRead);
    ZERO_BUFFER(binary);
    ntStatus = DMF_Registry_PathAndValueReadBinary(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_BINARY,
                                                   (UCHAR*)binary,
                                                   sizeof(binary),
                                                   &bytesRead);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(binary,
                               binaryOriginal,
                               sizeof(binaryOriginal)) == bytesRead);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_ReadSmallBufferWithoutBytesRead(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    NTSTATUS ntStatus;
    UCHAR smallBuffer[1];

    PAGED_CODE();

    // Disable "Using 'bytesRead' from failed function call for this section. By design
    // bytesRead contains the number of bytes needed when STATUS_BUFFER_TOO_SMALL is returned.
    //
    #pragma warning(push)
    #pragma warning(disable: 6102)

    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_PathAndValueReadString(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_STRING,
                                                   (PWCHAR)smallBuffer,
                                                   ARRAYSIZE(smallBuffer),
                                                   NULL);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_PathAndValueReadMultiString(DmfModuleRegistry,
                                                        REGISTRY_PATH_NAME,
                                                        VALUENAME_MULTISTRING,
                                                        (PWCHAR)smallBuffer,
                                                        ARRAYSIZE(smallBuffer),
                                                        NULL);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_PathAndValueReadBinary(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_BINARY,
                                                   (UCHAR*)smallBuffer,
                                                   sizeof(smallBuffer),
                                                   NULL);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);

    #pragma warning(pop)

}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_ReadSmallBufferWithBytesRead(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    NTSTATUS ntStatus;
    UCHAR smallBuffer[1];
    ULONG bytesRead;

    PAGED_CODE();

    // Disable "Using 'bytesRead' from failed function call for this section. By design
    // bytesRead contains the number of bytes needed when STATUS_BUFFER_TOO_SMALL is returned.
    //
    #pragma warning(push)
    #pragma warning(disable: 6102)

    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_PathAndValueReadString(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_STRING,
                                                   (PWCHAR)smallBuffer,
                                                   ARRAYSIZE(smallBuffer),
                                                   &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(stringOriginal));
    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_PathAndValueReadMultiString(DmfModuleRegistry,
                                                        REGISTRY_PATH_NAME,
                                                        VALUENAME_MULTISTRING,
                                                        (PWCHAR)smallBuffer,
                                                        ARRAYSIZE(smallBuffer),
                                                        &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(multiStringOriginal));
    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_PathAndValueReadBinary(DmfModuleRegistry,
                                                   REGISTRY_PATH_NAME,
                                                   VALUENAME_BINARY,
                                                   (UCHAR*)smallBuffer,
                                                   sizeof(smallBuffer),
                                                   &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(binaryOriginal));

    #pragma warning(pop)
}
#pragma code_seg()
#endif

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_Enumerate(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    BOOLEAN result;
    EnumCallbackContext callbackContext;

    PAGED_CODE();

    callbackContext.NumberOfKeys = 0;

    result = DMF_Registry_EnumerateKeysFromName(DmfModuleRegistry,
                                                REGISTRY_PATH_NAME,
                                                RegistryKeyEnumerationFunction,
                                                &callbackContext);
    DmfAssert(TRUE == result);
    DmfAssert(ARRAYSIZE(subkeys) == callbackContext.NumberOfKeys);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Path_NameContainingStringEnumerate(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    BOOLEAN result;
    EnumCallbackContext callbackContext;

    PAGED_CODE();

    // Make sure SUBKEYNAME_1 can be found.
    //
    callbackContext.NumberOfKeys = 0;
    result = DMF_Registry_SubKeysFromPathNameContainingStringEnumerate(DmfModuleRegistry,
                                                                       REGISTRY_PATH_NAME,
                                                                       SUBKEYNAME_1,
                                                                       RegistryKeyEnumerationFunction, 
                                                                       &callbackContext);
    DmfAssert(TRUE == result);
    DmfAssert(1 == callbackContext.NumberOfKeys);

    // Make sure SUBKEYNAME_2 can be found.
    //
    callbackContext.NumberOfKeys = 0;
    result = DMF_Registry_SubKeysFromPathNameContainingStringEnumerate(DmfModuleRegistry,
                                                                       REGISTRY_PATH_NAME,
                                                                       SUBKEYNAME_2,
                                                                       RegistryKeyEnumerationFunction, 
                                                                       &callbackContext);
    DmfAssert(TRUE == result);
    DmfAssert(1 == callbackContext.NumberOfKeys);

    // Make sure not existing keys reported as not found.
    //
    callbackContext.NumberOfKeys = 0;
    result = DMF_Registry_SubKeysFromPathNameContainingStringEnumerate(DmfModuleRegistry,
                                                                       REGISTRY_PATH_NAME,
                                                                       L"DoesNotExist",
                                                                       RegistryKeyEnumerationFunction, 
                                                                       &callbackContext);
    DmfAssert(TRUE == result);
    DmfAssert(0 == callbackContext.NumberOfKeys);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_DeleteValues(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = DMF_Registry_ValueDelete(DmfModuleRegistry,
                                        Handle,
                                        VALUENAME_STRING);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
    ntStatus = DMF_Registry_ValueDelete(DmfModuleRegistry,
                                        Handle,
                                        VALUENAME_MULTISTRING);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
    ntStatus = DMF_Registry_ValueDelete(DmfModuleRegistry,
                                        Handle,
                                        VALUENAME_BINARY);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
    ntStatus = DMF_Registry_ValueDelete(DmfModuleRegistry,
                                        Handle,
                                        VALUENAME_DWORD);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
    ntStatus = DMF_Registry_ValueDelete(DmfModuleRegistry,
                                        Handle,
                                        VALUENAME_QWORD);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
}
#pragma code_seg()

#if !defined(DMF_USER_MODE)
// User-mode driver cannot create subkeys at runtime, 
// so test should not delete them at runtime.  
// For User-mode, The subkeys are created through INF and not deleted during test.
//
#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_DeleteSubkeys(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    NTSTATUS ntStatus;
    HANDLE subkeyHandle;
    LONG index;

    PAGED_CODE();

    for (index = 0; index < ARRAYSIZE(subkeys); ++index)
    {
        subkeyHandle = DMF_Registry_HandleOpenByHandle(DmfModuleRegistry,
                                                       Handle,
                                                       subkeys[index],
                                                       FALSE);
        DmfAssert(NULL != subkeyHandle);

        ntStatus = DMF_Registry_HandleDelete(DmfModuleRegistry,
                                             subkeyHandle);
        DmfAssert(NT_SUCCESS(ntStatus));

#if defined(DMF_USER_MODE)
        // Regardless of the above call, close the handle.
        // NOTE: Per MSDN, in Kernel-mode do not call this function after deleting the key.
        //
        DMF_Registry_HandleClose(DmfModuleRegistry,
                                 subkeyHandle);
#endif
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_DeletePath(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
)
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = DMF_Registry_HandleDelete(DmfModuleRegistry,
                                         Handle);
    DmfAssert(NT_SUCCESS(ntStatus) || (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus));
}
#pragma code_seg()
#endif

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_ReadNonExistent(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    NTSTATUS ntStatus;
    WCHAR binary[64];
    WCHAR string[64];
    WCHAR multiString[64];
    ULONGLONG ulonglong;
    ULONG ulong;

    PAGED_CODE();

    ZERO_BUFFER(string);
    ntStatus = DMF_Registry_ValueReadString(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_STRING,
                                            string,
                                            ARRAYSIZE(string),
                                            NULL);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
    ZERO_BUFFER(multiString);
    ntStatus = DMF_Registry_ValueReadMultiString(DmfModuleRegistry,
                                                 Handle,
                                                 VALUENAME_MULTISTRING,
                                                 multiString,
                                                 ARRAYSIZE(multiString),
                                                 NULL);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
    ZERO_BUFFER(binary);
    ntStatus = DMF_Registry_ValueReadBinary(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_BINARY,
                                            (UCHAR*)binary,
                                            sizeof(binary),
                                            NULL);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
    ZERO_BUFFER(ulong);
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
    ZERO_BUFFER(ulonglong);
    ntStatus = DMF_Registry_ValueReadQword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_QWORD,
                                           &ulonglong);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_WriteValues(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = DMF_Registry_ValueWriteString(DmfModuleRegistry,
                                             Handle,
                                             VALUENAME_STRING,
                                             stringOriginal,
                                             ARRAYSIZE(stringOriginal));
    DmfAssert(NT_SUCCESS(ntStatus));
    ntStatus = DMF_Registry_ValueWriteMultiString(DmfModuleRegistry,
                                                  Handle,
                                                  VALUENAME_MULTISTRING,
                                                  multiStringOriginal,
                                                  ARRAYSIZE(multiStringOriginal));
    DmfAssert(NT_SUCCESS(ntStatus));
    ntStatus = DMF_Registry_ValueWriteBinary(DmfModuleRegistry,
                                             Handle,
                                             VALUENAME_BINARY,
                                             binaryOriginal,
                                             sizeof(binaryOriginal));
    DmfAssert(NT_SUCCESS(ntStatus));
    ntStatus = DMF_Registry_ValueWriteDword(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_DWORD,
                                            ulongOriginal);
    DmfAssert(NT_SUCCESS(ntStatus));
    ntStatus = DMF_Registry_ValueWriteQword(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_QWORD,
                                            ulonglongOriginal);
    DmfAssert(NT_SUCCESS(ntStatus));
}
#pragma code_seg()

#if !defined(DMF_USER_MODE)
// User-mode driver cannot create subkeys at runtime.
//
#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_WriteSubkeys(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    HANDLE subkeyHandle;
    LONG index;

    PAGED_CODE();

    for (index = 0; index < ARRAYSIZE(subkeys); ++index)
    {
        subkeyHandle = DMF_Registry_HandleOpenByHandle(DmfModuleRegistry,
                                                       Handle,
                                                       subkeys[index],
                                                       TRUE);
        DmfAssert(NULL != subkeyHandle);
        DMF_Registry_HandleClose(DmfModuleRegistry,
                                 subkeyHandle);
    }
}
#pragma code_seg()
#endif

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_ReadAndValidateBytesRead(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    NTSTATUS ntStatus;
    ULONG bytesRead;

    PAGED_CODE();

    // Disable "Using 'bytesRead' from failed function call for this section. By design
    // bytesRead contains the number of bytes needed when STATUS_BUFFER_TOO_SMALL is returned.
    //
    #pragma warning(push)
    #pragma warning(disable: 6102)

    ntStatus = DMF_Registry_ValueReadString(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_STRING,
                                            NULL,
                                            0,
                                            &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(stringOriginal));
    ntStatus = DMF_Registry_ValueReadMultiString(DmfModuleRegistry,
                                                 Handle,
                                                 VALUENAME_MULTISTRING,
                                                 NULL,
                                                 0,
                                                 &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(multiStringOriginal));
    ntStatus = DMF_Registry_ValueReadBinary(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_BINARY,
                                            NULL,
                                            0,
                                            &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(binaryOriginal));
    #pragma warning(pop)
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_ReadAndValidateData(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    NTSTATUS ntStatus;
    WCHAR binary[64];
    WCHAR string[64];
    WCHAR multiString[64];
    ULONGLONG ulonglong;
    ULONG ulong;

    PAGED_CODE();

    ZERO_BUFFER(string);
    ntStatus = DMF_Registry_ValueReadString(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_STRING,
                                            string,
                                            ARRAYSIZE(string),
                                            NULL);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(string,
                               stringOriginal,
                               sizeof(stringOriginal)) == sizeof(stringOriginal));
    ZERO_BUFFER(multiString);
    ntStatus = DMF_Registry_ValueReadMultiString(DmfModuleRegistry,
                                                 Handle,
                                                 VALUENAME_MULTISTRING,
                                                 multiString,
                                                 ARRAYSIZE(multiString),
                                                 NULL);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(multiString,
                               multiStringOriginal,
                               sizeof(multiStringOriginal)) == sizeof(multiStringOriginal));
    ZERO_BUFFER(binary);
    ntStatus = DMF_Registry_ValueReadBinary(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_BINARY,
                                            (UCHAR*)binary,
                                            sizeof(binary),
                                            NULL);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(binary,
                               binaryOriginal,
                               sizeof(binaryOriginal)) == sizeof(binaryOriginal));
    ZERO_BUFFER(ulong);
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(ulong == ulongOriginal);

    ntStatus = DMF_Registry_ValueReadDwordAndValidate(DmfModuleRegistry,
                                                      Handle,
                                                      VALUENAME_DWORD,
                                                      &ulong,
                                                      0,
                                                      1);
    DmfAssert(STATUS_INVALID_DEVICE_REQUEST == ntStatus);
    ntStatus = DMF_Registry_ValueReadDwordAndValidate(DmfModuleRegistry,
                                                      Handle,
                                                      VALUENAME_DWORD,
                                                      &ulong,
                                                      0x00000000,
                                                      0xFFFFFFFF);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(ulong == ulongOriginal);
    ZERO_BUFFER(ulonglong);
    ntStatus = DMF_Registry_ValueReadQword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_QWORD,
                                           &ulonglong);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(ulonglong == ulonglongOriginal);
    ZERO_BUFFER(ulonglong);
    ntStatus = DMF_Registry_ValueReadQwordAndValidate(DmfModuleRegistry,
                                                      Handle,
                                                      VALUENAME_QWORD,
                                                      &ulonglong,
                                                      0,
                                                      1);
    DmfAssert(STATUS_INVALID_DEVICE_REQUEST == ntStatus);
    ZERO_BUFFER(ulonglong);
    ntStatus = DMF_Registry_ValueReadQwordAndValidate(DmfModuleRegistry,
                                                      Handle,
                                                      VALUENAME_QWORD,
                                                      &ulonglong,
                                                      0x0000000000000000,
                                                      0xFFFFFFFFFFFFFFFF);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(ulong == ulongOriginal);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_ReadAndValidateDataAndBytesRead(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    NTSTATUS ntStatus;
    WCHAR binary[64];
    WCHAR string[64];
    WCHAR multiString[64];
    ULONG bytesRead;

    PAGED_CODE();

    ZERO_BUFFER(string);
    ntStatus = DMF_Registry_ValueReadString(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_STRING,
                                            string,
                                            ARRAYSIZE(string),
                                            &bytesRead);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(string,
                               stringOriginal,
                               sizeof(stringOriginal)) == bytesRead);
    ZERO_BUFFER(multiString);
    ntStatus = DMF_Registry_ValueReadMultiString(DmfModuleRegistry,
                                                 Handle,
                                                 VALUENAME_MULTISTRING,
                                                 multiString,
                                                 ARRAYSIZE(multiString),
                                                 &bytesRead);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(multiString,
                               multiStringOriginal,
                               sizeof(multiStringOriginal)) == bytesRead);
    ZERO_BUFFER(binary);
    ntStatus = DMF_Registry_ValueReadBinary(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_BINARY,
                                            (UCHAR*)binary,
                                            sizeof(binary),
                                            &bytesRead);
    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(RtlCompareMemory(binary,
                               binaryOriginal,
                               sizeof(binaryOriginal)) == bytesRead);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_ReadSmallBufferWithoutBytesRead(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    NTSTATUS ntStatus;
    UCHAR smallBuffer[1];

    PAGED_CODE();

    // Disable "Using 'bytesRead' from failed function call for this section. By design
    // bytesRead contains the number of bytes needed when STATUS_BUFFER_TOO_SMALL is returned.
    //
    #pragma warning(push)
    #pragma warning(disable: 6102)

    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_ValueReadString(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_STRING,
                                            (PWCHAR)smallBuffer,
                                            ARRAYSIZE(smallBuffer),
                                            NULL);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_ValueReadMultiString(DmfModuleRegistry,
                                                 Handle,
                                                 VALUENAME_MULTISTRING,
                                                 (PWCHAR)smallBuffer,
                                                 ARRAYSIZE(smallBuffer),
                                                 NULL);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_ValueReadBinary(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_BINARY,
                                            (UCHAR*)smallBuffer,
                                            sizeof(smallBuffer),
                                            NULL);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);

    #pragma warning(pop)

}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_ReadSmallBufferWithBytesRead(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    NTSTATUS ntStatus;
    UCHAR smallBuffer[1];
    ULONG bytesRead;

    PAGED_CODE();

    // Disable "Using 'bytesRead' from failed function call for this section. By design
    // bytesRead contains the number of bytes needed when STATUS_BUFFER_TOO_SMALL is returned.
    //
    #pragma warning(push)
    #pragma warning(disable: 6102)

    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_ValueReadString(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_STRING,
                                            (PWCHAR)smallBuffer,
                                            ARRAYSIZE(smallBuffer),
                                            &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(stringOriginal));
    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_ValueReadMultiString(DmfModuleRegistry,
                                                 Handle,
                                                 VALUENAME_MULTISTRING,
                                                 (PWCHAR)smallBuffer,
                                                 ARRAYSIZE(smallBuffer),
                                                 &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(multiStringOriginal));
    ZERO_BUFFER(smallBuffer);
    ntStatus = DMF_Registry_ValueReadBinary(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_BINARY,
                                            (UCHAR*)smallBuffer,
                                            sizeof(smallBuffer),
                                            &bytesRead);
    DmfAssert(STATUS_BUFFER_TOO_SMALL == ntStatus);
    DmfAssert(bytesRead == sizeof(binaryOriginal));

    #pragma warning(pop)

}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_Enumerate(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    BOOLEAN result;
    EnumCallbackContext callbackContext;

    PAGED_CODE();


    callbackContext.NumberOfKeys = 0;
    result = DMF_Registry_SubKeysFromHandleEnumerate(DmfModuleRegistry,
                                                     Handle,
                                                     RegistryKeyEnumerationFunction,
                                                     &callbackContext);
    DmfAssert(TRUE == result);
    DmfAssert(ARRAYSIZE(subkeys) == callbackContext.NumberOfKeys);

    callbackContext.NumberOfKeys = 0;
    result = DMF_Registry_AllSubKeysFromHandleEnumerate(DmfModuleRegistry,
                                                        Handle,
                                                        RegistryKeyEnumerationFunction,
                                                        &callbackContext);
    DmfAssert(TRUE == result);
    DmfAssert(ARRAYSIZE(subkeys) == callbackContext.NumberOfKeys);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_ConditionalDelete(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    CompareCallbackContext callbackContext;
    NTSTATUS ntStatus;
    ULONG ulong;

    PAGED_CODE();

    ZERO_BUFFER(ulong);

    // Make sure the value exists
    //
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_SUCCESS == ntStatus);
    DmfAssert(VALUEDATA_DWORD == ulong);

    // Delete with failing condition, the value should remain.
    //
    ulong = VALUEDATA_DWORD + 1;
    ntStatus = DMF_Registry_ValueDeleteIfNeeded(DmfModuleRegistry,
                                                Handle,
                                                VALUENAME_DWORD,
                                                &ulong,
                                                sizeof(ulong),
                                                RegistryValueComparisonFunction_IfEqual,
                                                NULL);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    // Delete with failing condition, using callback context to pass the values. The value should remain.
    //
    callbackContext.ClientData = &ulong;
    callbackContext.ClientDataSize = sizeof(ulong);
    ntStatus = DMF_Registry_ValueDeleteIfNeeded(DmfModuleRegistry,
                                                Handle,
                                                VALUENAME_DWORD,
                                                NULL,
                                                0,
                                                RegistryValueComparisonFunction_IfEqualToContext,
                                                &callbackContext);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    // Make sure the value still exists
    //
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_SUCCESS == ntStatus);
    DmfAssert(VALUEDATA_DWORD == ulong);

    // Delete with succeeding condition, the value should be removed.
    //
    ulong = VALUEDATA_DWORD;
    ntStatus = DMF_Registry_ValueDeleteIfNeeded(DmfModuleRegistry,
                                                Handle,
                                                VALUENAME_DWORD,
                                                &ulong,
                                                sizeof(ulong),
                                                RegistryValueComparisonFunction_IfEqual,
                                                NULL);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    // Make sure the value was removed
    //
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);

    // Delete a non-existing value.
    //
    ulong = VALUEDATA_DWORD;
    ntStatus = DMF_Registry_ValueDeleteIfNeeded(DmfModuleRegistry,
                                                Handle,
                                                VALUENAME_DWORD,
                                                &ulong,
                                                sizeof(ulong),
                                                RegistryValueComparisonFunction_IfEqual,
                                                NULL);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_Handle_ConditionalWrite(
    _In_ DMFMODULE DmfModuleRegistry,
    _In_ HANDLE Handle
    )
{
    CompareCallbackContext callbackContext;
    NTSTATUS ntStatus;
    ULONG ulong;
    ULONG contextUlong;

    PAGED_CODE();

    ZERO_BUFFER(ulong);

    callbackContext.ClientData = &contextUlong;
    callbackContext.ClientDataSize = sizeof(contextUlong);

    // Make sure the value does not exist
    //
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);

    // Non-existing value, don't write if does not exist. The value should not be written.
    //
    ulong = VALUEDATA_DWORD;
    ntStatus = DMF_Registry_ValueWriteIfNeeded(DmfModuleRegistry,
                                               Handle,
                                               VALUENAME_DWORD,
                                               REG_DWORD,
                                               &ulong,
                                               sizeof(ulong),
                                               RegistryValueComparisonFunction_IfDefault,
                                               NULL,
                                               FALSE);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);

    // Make sure the value still does not exist
    //
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_OBJECT_NAME_NOT_FOUND == ntStatus);

    // Non-existing value, write if does not exist. The value should be written.
    //
    ulong = VALUEDATA_DWORD;
    ntStatus = DMF_Registry_ValueWriteIfNeeded(DmfModuleRegistry,
                                               Handle,
                                               VALUENAME_DWORD,
                                               REG_DWORD,
                                               &ulong,
                                               sizeof(ulong),
                                               RegistryValueComparisonFunction_IfDefault,
                                               NULL,
                                               TRUE);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    // Make sure the value was written
    //
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_SUCCESS == ntStatus);
    DmfAssert(VALUEDATA_DWORD == ulong);

    // Overwrite default value, new value should be written.
    //
    ulong = VALUEDATA_DWORD + 1;
    ntStatus = DMF_Registry_ValueWriteIfNeeded(DmfModuleRegistry,
                                               Handle,
                                               VALUENAME_DWORD,
                                               REG_DWORD,
                                               &ulong,
                                               sizeof(ulong),
                                               RegistryValueComparisonFunction_IfDefault,
                                               NULL,
                                               FALSE);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    // Make sure the new value was written
    //
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_SUCCESS == ntStatus);
    DmfAssert(VALUEDATA_DWORD + 1 == ulong);

    // Overwrite non-default value, new value should not be written.
    //
    ulong = VALUEDATA_DWORD + 2;
    ntStatus = DMF_Registry_ValueWriteIfNeeded(DmfModuleRegistry,
                                               Handle,
                                               VALUENAME_DWORD,
                                               REG_DWORD,
                                               &ulong,
                                               sizeof(ulong),
                                               RegistryValueComparisonFunction_IfDefault,
                                               NULL,
                                               FALSE);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    // Make sure the new value was not written
    //
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_SUCCESS == ntStatus);
    DmfAssert(VALUEDATA_DWORD + 1 == ulong);

    // Reset back to the default value.
    //
    ntStatus = DMF_Registry_ValueWriteDword(DmfModuleRegistry,
                                            Handle,
                                            VALUENAME_DWORD,
                                            VALUEDATA_DWORD);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    // Overwrite the value passing non-matching data in callback context, new value should not be written.
    //
    ulong = VALUEDATA_DWORD + 1;
    contextUlong = VALUEDATA_DWORD + 1;
    ntStatus = DMF_Registry_ValueWriteIfNeeded(DmfModuleRegistry,
                                               Handle,
                                               VALUENAME_DWORD,
                                               REG_DWORD,
                                               &ulong,
                                               sizeof(ulong),
                                               RegistryValueComparisonFunction_IfEqualToContext,
                                               &callbackContext,
                                               FALSE);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    // Make sure the new value was not written
    //
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_SUCCESS == ntStatus);
    DmfAssert(VALUEDATA_DWORD == ulong);

    // Overwrite the value passing non-matching data in callback context, new value should not be written.
    //
    ulong = VALUEDATA_DWORD + 1;
    contextUlong = VALUEDATA_DWORD;
    ntStatus = DMF_Registry_ValueWriteIfNeeded(DmfModuleRegistry,
                                               Handle,
                                               VALUENAME_DWORD,
                                               REG_DWORD,
                                               &ulong,
                                               sizeof(ulong),
                                               RegistryValueComparisonFunction_IfEqualToContext,
                                               &callbackContext,
                                               FALSE);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    // Make sure the new value was written
    //
    ntStatus = DMF_Registry_ValueReadDword(DmfModuleRegistry,
                                           Handle,
                                           VALUENAME_DWORD,
                                           &ulong);
    DmfAssert(STATUS_SUCCESS == ntStatus);
    DmfAssert(VALUEDATA_DWORD + 1 == ulong);
}
#pragma code_seg()

#if !defined(DMF_USER_MODE)
#pragma code_seg("PAGE")
static
VOID
Tests_Registry_TreeWrite(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = DMF_Registry_TreeWriteEx(DmfModuleRegistry,
                                        RegistryTree,
                                        ARRAYSIZE(RegistryTree));
    DmfAssert(STATUS_SUCCESS == ntStatus);
}
#pragma code_seg()

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_TreeWriteDeferred(
    _In_ DMFMODULE DmfModuleRegistry
    )
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = DMF_Registry_TreeWriteDeferred(DmfModuleRegistry,
                                              RegistryTree,
                                              ARRAYSIZE(RegistryTree));
    DmfAssert(STATUS_SUCCESS == ntStatus);
}
#pragma code_seg()
#endif

#pragma code_seg("PAGE")
static
VOID
Tests_Registry_RunTest(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Tests APIs Registry Module.

Arguments:

    DmfModule - this Module's handle.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_Tests_Registry* moduleContext;
    HANDLE registryHandle;
    NTSTATUS ntStatus;

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Delay for 10 seconds, to make sure Software hive is loaded.
    //
    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleep,
                                        0,
                                        30000);
    if (!NT_SUCCESS(ntStatus))
    {
        // Driver stopped immediately after starting.
        //
        goto Exit;
    }

    DMF_AlertableSleep_ResetForReuse(moduleContext->DmfModuleAlertableSleep,
                                     0);

    if (moduleContext->AbortTests)
    {
        goto Exit;
    }

    // Path and Value Tests
    // --------------------
    //
#if !defined(DMF_USER_MODE)
    // From User-mode, ONLY Read access is allowed for arbitrary registry path.
    // Hence the below tests are valid for kernel mode only.
    //

    // Delete everything.
    //
    Tests_Registry_Path_DeleteValues(moduleContext->DmfModuleRegistry);
    Tests_Registry_Path_DeletePath(moduleContext->DmfModuleRegistry);

    // Now, try to read some non-existent values.
    //
    Tests_Registry_Path_ReadNonExistent(moduleContext->DmfModuleRegistry);

    // Make sure the key was deleted
    //
    Tests_Registry_ValidatePathDeleted(moduleContext->DmfModuleRegistry);

    // Write the values using typed functions.
    //
    Tests_Registry_Path_WriteValues(moduleContext->DmfModuleRegistry);

    // Get sizes of values to read.
    //
    Tests_Registry_Path_ReadAndValidateBytesRead(moduleContext->DmfModuleRegistry);

    // Read values and compare to original with NULL bytesRead.
    //
    Tests_Registry_Path_ReadAndValidateData(moduleContext->DmfModuleRegistry);

    // Read values and compare to original with bytesRead.
    //
    Tests_Registry_Path_ReadAndValidateDataAndBytesRead(moduleContext->DmfModuleRegistry);

    // Try to read to small buffers with NULL bytesRead.
    //
    Tests_Registry_Path_ReadSmallBufferWithoutBytesRead(moduleContext->DmfModuleRegistry);

    // Try to read to small buffers with bytesRead.
    //
    Tests_Registry_Path_ReadSmallBufferWithBytesRead(moduleContext->DmfModuleRegistry);

    // Delete everything we wrote
    //
    Tests_Registry_Path_DeleteValues(moduleContext->DmfModuleRegistry);
    Tests_Registry_Path_DeletePath(moduleContext->DmfModuleRegistry);

    // Make sure everything was deleted.
    //
    Tests_Registry_Path_ReadNonExistent(moduleContext->DmfModuleRegistry);
    Tests_Registry_ValidatePathDeleted(moduleContext->DmfModuleRegistry);
#endif

    // Path/Predefined Id key open and Value Tests
    // Do same as above, but this time open the predefined key by id and operate only on the values, reusing the path handle.
    // --------------------
    //
    ULONG predefinedIds[] =
    {
        // This is just dummy entry to cause path API to be used.
        //
        0,
        // These are the predefined Ids.
        //
        PLUGPLAY_REGKEY_DEVICE,
        PLUGPLAY_REGKEY_DRIVER,
#if !defined(DMF_USER_MODE)
        // Note: PLUGPLAY_REGKEY_CURRENT_HWPROFILE may not be used alone.
        //
        PLUGPLAY_REGKEY_DEVICE | PLUGPLAY_REGKEY_CURRENT_HWPROFILE,
        PLUGPLAY_REGKEY_DRIVER | PLUGPLAY_REGKEY_CURRENT_HWPROFILE,
#else
        PLUGPLAY_REGKEY_DEVICE | WDF_REGKEY_DEVICE_SUBKEY,
        PLUGPLAY_REGKEY_DRIVER | WDF_REGKEY_DRIVER_SUBKEY
#endif
    };


    for (ULONG predefinedIdIndex = 0; predefinedIdIndex < ARRAYSIZE(predefinedIds); predefinedIdIndex++)
    {
        ACCESS_MASK accessMask = GENERIC_ALL;
        if (0 == predefinedIdIndex)
        {
#if defined(DMF_USER_MODE)
            // ONLY Read Only access works for arbitrary registry path in User-mode.
            //
            accessMask = KEY_READ;
#endif
            // Zero means open from the hard coded path.
            //
            ntStatus = DMF_Registry_HandleOpenByNameEx(moduleContext->DmfModuleRegistry,
                                                       REGISTRY_PATH_NAME,
                                                       accessMask,
                                                       TRUE,
                                                       &registryHandle);
        }
        else
        {
#if defined(DMF_USER_MODE)
            // For UMDF, ensure to set the right accessMask.
            // Reference: https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfdevice/nf-wdfdevice-wdfdeviceopenregistrykey
            //
            if ((predefinedIds[predefinedIdIndex] & PLUGPLAY_REGKEY_DEVICE) ||
                (predefinedIds[predefinedIdIndex] & PLUGPLAY_REGKEY_DRIVER))
            {
                accessMask = KEY_READ;
            }

            if ((predefinedIds[predefinedIdIndex] & WDF_REGKEY_DEVICE_SUBKEY) ||
                (predefinedIds[predefinedIdIndex] & WDF_REGKEY_DRIVER_SUBKEY))
            {
                accessMask |= KEY_SET_VALUE;
            }   
#endif

            // Open the predefined key.
            //
            ntStatus = DMF_Registry_HandleOpenById(moduleContext->DmfModuleRegistry,
                                                   predefinedIds[predefinedIdIndex],
                                                   accessMask,
                                                   &registryHandle);
        }
        DmfAssert(NT_SUCCESS(ntStatus));
        DmfAssert(registryHandle != NULL);
        if (registryHandle != NULL)
        {
            // Can not write values for read only access keys.
            //
            if ((accessMask & GENERIC_ALL) || (accessMask & KEY_SET_VALUE))
            {
                // Delete values.
                //
                Tests_Registry_Handle_DeleteValues(moduleContext->DmfModuleRegistry,
                                                   registryHandle);

                // Now, try to read some non-existent values.
                //
                Tests_Registry_Handle_ReadNonExistent(moduleContext->DmfModuleRegistry,
                                                      registryHandle);            

                // Write the values.
                //
                Tests_Registry_Handle_WriteValues(moduleContext->DmfModuleRegistry,
                                                  registryHandle);

                // Get sizes of values to read.
                //
                Tests_Registry_Handle_ReadAndValidateBytesRead(moduleContext->DmfModuleRegistry,
                                                               registryHandle);

                // Read values and compare to original with NULL bytesRead.
                //
                Tests_Registry_Handle_ReadAndValidateData(moduleContext->DmfModuleRegistry,
                                                          registryHandle);

                // Read values and compare to original with bytesRead.
                //
                Tests_Registry_Handle_ReadAndValidateDataAndBytesRead(moduleContext->DmfModuleRegistry,
                                                                      registryHandle);

                // Try to read to small buffers with NULL bytesRead.
                //
                Tests_Registry_Handle_ReadSmallBufferWithoutBytesRead(moduleContext->DmfModuleRegistry,
                                                                      registryHandle);

                // Try to read to small buffers with bytesRead.
                //
                Tests_Registry_Handle_ReadSmallBufferWithBytesRead(moduleContext->DmfModuleRegistry,
                                                                   registryHandle);

                // Delete everything we wrote and make sure it was deleted.
                //
                Tests_Registry_Handle_DeleteValues(moduleContext->DmfModuleRegistry,
                                                   registryHandle);
                Tests_Registry_Handle_ReadNonExistent(moduleContext->DmfModuleRegistry,
                                                      registryHandle);

            }

            BOOLEAN closeRegistryKey = TRUE;

            // Driver is not allowed to delete predefined keys.
            //
            if (0 == predefinedIdIndex)
            {
#if !defined(DMF_USER_MODE)
                // Don't delete the Path in User-mode, as the driver can not create it during runtime.
                //
                Tests_Registry_Handle_DeletePath(moduleContext->DmfModuleRegistry,
                                                 registryHandle);
                Tests_Registry_ValidatePathDeleted(moduleContext->DmfModuleRegistry);
                // NOTE: Per MSDN, in Kernel-mode do not call this function after deleting the key.
                //
                closeRegistryKey = FALSE;
#endif
                
            }

            if (closeRegistryKey)
            {
                DMF_Registry_HandleClose(moduleContext->DmfModuleRegistry,
                                         registryHandle);
            }
            registryHandle = NULL;
        }
    }

#if !defined(DMF_USER_MODE) 
    // Tree Write feature are not supported in User-mode.
    //

    // Tree Tests
    // ----------
    //

    // Make sure the path does not exist
    //
    Tests_Registry_ValidatePathDeleted(moduleContext->DmfModuleRegistry);

    // Write keys and values tree into the registry
    //
    Tests_Registry_TreeWrite(moduleContext->DmfModuleRegistry);

    // Make sure the data was written properly
    Tests_Registry_Path_ReadAndValidateData(moduleContext->DmfModuleRegistry);

    // Delete everything we wrote.
    //
    Tests_Registry_Path_DeleteValues(moduleContext->DmfModuleRegistry);
    Tests_Registry_Path_DeletePath(moduleContext->DmfModuleRegistry);
    // Tree Tests deferred
    // -------------------
    //

    // Make sure the path does not exist
    //
    Tests_Registry_ValidatePathDeleted(moduleContext->DmfModuleRegistry);

    // Write keys and values tree into the registry
    //
    Tests_Registry_TreeWriteDeferred(moduleContext->DmfModuleRegistry);

    ntStatus = DMF_AlertableSleep_Sleep(moduleContext->DmfModuleAlertableSleep,
                                        0,
                                        5000);
    DmfAssert(STATUS_SUCCESS == ntStatus);

    DMF_AlertableSleep_ResetForReuse(moduleContext->DmfModuleAlertableSleep,
                                     0);

    if (moduleContext->AbortTests)
    {
        goto Exit;
    }

    // Make sure the data was written properly
    //
    Tests_Registry_Path_ReadAndValidateData(moduleContext->DmfModuleRegistry);

    // Delete everything we wrote.
    //
    Tests_Registry_Path_DeleteValues(moduleContext->DmfModuleRegistry);
    Tests_Registry_Path_DeletePath(moduleContext->DmfModuleRegistry);
#endif


    // Enum and Conditional Tests
    // --------------------------
    //
#if !defined(DMF_USER_MODE)
    // Only Kernel-mode driver can write to arbitrary registry paths.
    // Make sure the path does not exist
    //
    Tests_Registry_ValidatePathDeleted(moduleContext->DmfModuleRegistry);

    ntStatus = DMF_Registry_HandleOpenByNameEx(moduleContext->DmfModuleRegistry,
                                               REGISTRY_PATH_NAME,
                                               GENERIC_ALL,
                                               TRUE,
                                               &registryHandle);
#else
    // For User-mode Open the predefined key where there is write access.
    //
    ntStatus = DMF_Registry_HandleOpenById(moduleContext->DmfModuleRegistry,
                                           PLUGPLAY_REGKEY_DEVICE | WDF_REGKEY_DEVICE_SUBKEY,
                                           KEY_READ | KEY_SET_VALUE,
                                           &registryHandle);
#endif

    DmfAssert(NT_SUCCESS(ntStatus));
    DmfAssert(registryHandle != NULL);

    if (registryHandle != NULL)
    {
        // Write the values.
        //
        Tests_Registry_Handle_WriteValues(moduleContext->DmfModuleRegistry, registryHandle);

#if !defined(DMF_USER_MODE)
        // These are only valid for Kernel mode driver.
        // Write the subkeys.
        //
        Tests_Registry_Handle_WriteSubkeys(moduleContext->DmfModuleRegistry, registryHandle);
#endif

        // Enum Tests
        // ----------
        //

        // Try to enumerate keys in the path
        //
        Tests_Registry_Path_Enumerate(moduleContext->DmfModuleRegistry);

        // Try to find keys by name in the path
        //
        Tests_Registry_Path_NameContainingStringEnumerate(moduleContext->DmfModuleRegistry);

        // Try to enumerate keys via a root key handle
        //
        Tests_Registry_Handle_Enumerate(moduleContext->DmfModuleRegistry, registryHandle);

        // Conditional Tests
        // -------------------
        //

        Tests_Registry_Handle_ConditionalDelete(moduleContext->DmfModuleRegistry, registryHandle);
        Tests_Registry_Handle_ConditionalWrite(moduleContext->DmfModuleRegistry, registryHandle);

        // Delete everything we wrote.
        //
        Tests_Registry_Handle_DeleteValues(moduleContext->DmfModuleRegistry, registryHandle);

#if !defined(DMF_USER_MODE)
        // From User-mode, we cannot create subkeys.
        //
        Tests_Registry_Handle_DeleteSubkeys(moduleContext->DmfModuleRegistry, registryHandle);

        // Driver is not allowed to delete predefined keys.
        //
        Tests_Registry_Handle_DeletePath(moduleContext->DmfModuleRegistry, registryHandle);
#endif
#if defined(DMF_USER_MODE)
        // NOTE: Per MSDN, in Kernel-mode do not call this function after deleting the key.
        //
        DMF_Registry_HandleClose(moduleContext->DmfModuleRegistry,
                                 registryHandle);
#endif
        registryHandle = NULL;
    }

    // Finalizing
    // -------------------
    //
#if !defined(DMF_USER_MODE)
    // Make sure the path does not exist
    //
    Tests_Registry_ValidatePathDeleted(moduleContext->DmfModuleRegistry);
#endif
Exit:

    return;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(EVT_DMF_Thread_Function)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_Registry_WorkThread(
    _In_ DMFMODULE DmfModuleThread
    )
{
    DMFMODULE dmfModule;

    PAGED_CODE();

    dmfModule = DMF_ParentModuleGet(DmfModuleThread);
    
    Tests_Registry_RunTest(dmfModule);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Tests_Registry_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Tests_Registry.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    DMF_CONTEXT_Tests_Registry* moduleContext;
    NTSTATUS ntStatus;
    
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleContext->AbortTests = FALSE;

    ntStatus = DMF_Thread_Start(moduleContext->DmfModuleThread);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Thread_Start fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    DMF_Thread_WorkReady(moduleContext->DmfModuleThread);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Tests_Registry_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Tests_Registry.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Tests_Registry* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleContext->AbortTests = TRUE;
    DMF_AlertableSleep_Abort(moduleContext->DmfModuleAlertableSleep, 0);
    DMF_Thread_Stop(moduleContext->DmfModuleThread);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Tests_Registry_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONTEXT_Tests_Registry* moduleContext;
    DMF_CONFIG_AlertableSleep moduleConfigAlertableSleep;
    DMF_CONFIG_Thread moduleConfigThread;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // AlertableSleep
    // ---------------
    //
    DMF_CONFIG_AlertableSleep_AND_ATTRIBUTES_INIT(&moduleConfigAlertableSleep, 
                                                  &moduleAttributes);
    moduleConfigAlertableSleep.EventCount = 1;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleAlertableSleep);

    // Thread
    // ------
    //
    DMF_CONFIG_Thread_AND_ATTRIBUTES_INIT(&moduleConfigThread,
                                          &moduleAttributes);
    moduleConfigThread.ThreadControlType = ThreadControlType_DmfControl;
    moduleConfigThread.ThreadControl.DmfControl.EvtThreadWork = Tests_Registry_WorkThread;
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleThread);

    // Registry
    // --------
    //
    DMF_Registry_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                        &moduleAttributes,
                        WDF_NO_OBJECT_ATTRIBUTES,
                        &moduleContext->DmfModuleRegistry);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Tests_Registry_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Tests_Registry.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Tests_Registry;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Tests_Registry;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Tests_Registry);
    dmfCallbacksDmf_Tests_Registry.ChildModulesAdd = DMF_Tests_Registry_ChildModulesAdd;
    dmfCallbacksDmf_Tests_Registry.DeviceOpen = Tests_Registry_Open;
    dmfCallbacksDmf_Tests_Registry.DeviceClose = Tests_Registry_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Tests_Registry,
                                            Tests_Registry,
                                            DMF_CONTEXT_Tests_Registry,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Tests_Registry.CallbacksDmf = &dmfCallbacksDmf_Tests_Registry;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Tests_Registry,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_Tests_Registry.c
//
