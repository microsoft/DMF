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

typedef
_Function_class_(EVT_DMF_String_CompareCharCallback)
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
LONG
(EVT_DMF_String_CompareCharCallback)(_In_ DMFMODULE DmfModule,
                                     _In_ CHAR* StringInList, 
                                     _In_ CHAR* LookFor);

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
DMF_String_WideStringCopyAsAnsi(
    _Out_writes_(BufferSize) CHAR* AnsiString,
    _In_z_ WCHAR* WideString,
    _In_ ULONG BufferSize
    );

// eof: Dmf_String.h
//
