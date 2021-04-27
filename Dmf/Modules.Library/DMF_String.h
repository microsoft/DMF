/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_String.h

Abstract:

    Companion file to Dmf_String.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

#if !defined(DMF_WDF_DRIVER)

__forceinline
_IRQL_requires_max_(DISPATCH_LEVEL)
_At_(DestinationString->Buffer, _Post_equal_to_(SourceString))
_At_(DestinationString->Length, _Post_equal_to_(_String_length_(SourceString) * sizeof(WCHAR)))
_At_(DestinationString->MaximumLength, _Post_equal_to_((_String_length_(SourceString)+1) * sizeof(WCHAR)))
VOID
DMF_String_RtlInitUnicodeString(
    _Out_ PUNICODE_STRING DestinationString,
    _In_opt_z_ __drv_aliasesMem WCHAR* SourceString
    )
{
    DestinationString->Buffer = SourceString;
    DestinationString->Length = (USHORT)(wcslen(SourceString) * sizeof(WCHAR));
    // Add WCHAR for final \0\0.
    //
    DestinationString->MaximumLength = DestinationString->Length + sizeof(WCHAR);
}

// This function is not available in non-Windows environments.
//
#define RtlInitUnicodeString DMF_String_RtlInitUnicodeString

#endif

#if defined(DMF_USER_MODE)

__forceinline
NTSTATUS
DMF_String_AnsiStringInitialize(
    _In_ ANSI_STRING* AnsiString,
    _In_ CHAR* String
    )
{
    size_t size;

    size = strlen(String);

    RtlZeroMemory(AnsiString,
                  sizeof(ANSI_STRING));

    AnsiString->Buffer = String;

    // Size in bytes of the string not including NULL terminator.
    //
    AnsiString->Length = ((USHORT)size * sizeof(CHAR));
    // It is the maximum number of characters that can be stored in Buffer including the final zero.
    //
    AnsiString->MaximumLength = (USHORT)(((size)*sizeof(CHAR)) + sizeof(CHAR));
    
    return STATUS_SUCCESS;
}

// This function is not available in User-mode drivers.
//
#define RtlInitAnsiString DMF_String_AnsiStringInitialize

#endif

typedef
_Function_class_(EVT_DMF_String_CompareCharCallback)
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
LONG
(EVT_DMF_String_CompareCharCallback)(_In_ DMFMODULE DmfModule,
                                     _In_ CHAR* StringInList, 
                                     _In_ CHAR* LookFor);

typedef
_Function_class_(EVT_DMF_String_MutilSzCallback)
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
(EVT_DMF_String_MultiSzCallback)(_In_ DMFMODULE DmfModule,
                                 _In_ WCHAR* String,
                                 _In_ VOID* CallbackContext);

// This macro declares the following functions:
// DMF_String_ATTRIBUTES_INIT()
// DMF_String_Create()
//
DECLARE_DMF_MODULE_NO_CONFIG(String)

// Module Methods
//

LONG
DMF_String_FindInListChar(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR** StringList,
    _In_ ULONG NumberOfStringsInStringList,
    _In_ CHAR* LookFor,
    _In_ EVT_DMF_String_CompareCharCallback ComparisonCallback
    );

LONG
DMF_String_FindInListExactChar(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR** StringList,
    _In_ ULONG NumberOfStringsInStringList,
    _In_ CHAR* LookFor
    );

LONG
DMF_String_FindInListExactGuid(
    _In_ DMFMODULE DmfModule,
    _In_ GUID* GuidList,
    _In_ ULONG NumberOfGuidsInGuidList,
    _In_ GUID* LookFor
    );

LONG
DMF_String_FindInListLookForLeftMatchChar(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR** StringList,
    _In_ ULONG NumberOfStringsInStringList,
    _In_ CHAR* LookFor
    );

NTSTATUS
DMF_String_MultiSzEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_z_ WCHAR* MultiSzWideString,
    _In_ EVT_DMF_String_MultiSzCallback Callback,
    _In_ VOID* CallbackContext
    );

WCHAR*
DMF_String_MultiSzFindLast(
    _In_ DMFMODULE DmfModule,
    _In_z_ WCHAR* MultiSzWideString
    );

NTSTATUS
DMF_String_RtlAnsiStringToUnicodeString(
    _In_ DMFMODULE DmfModule,
    _Out_ PUNICODE_STRING DestinationString,
    _In_ PCANSI_STRING SourceString
    );

NTSTATUS
DMF_String_RtlUnicodeStringToAnsiString(
    _In_ DMFMODULE DmfModule,
    _Out_ PANSI_STRING DestinationString,
    _In_ PCUNICODE_STRING SourceString
    );

NTSTATUS
DMF_String_WideStringCopyAsNarrow(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(BufferSize) CHAR* NarrowString,
    _In_ ULONG BufferSize,
    _In_z_ WCHAR* WideString
    );

// eof: Dmf_String.h
//
