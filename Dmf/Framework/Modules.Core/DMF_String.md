## DMF_String

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

    Support for general string related operations.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

#### Module Structures

* None

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

````
_Function_class_(EVT_DMF_String_CompareCharCallback)
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
LONG
(EVT_DMF_String_CompareCharCallback)(_In_ DMFMODULE DmfModule,
                                     _In_ CHAR* StringInList, 
                                     _In_ CHAR* LookFor);
````

This is the comparison callback that a Client uses to perform a Client specific comparison when searching a
list of strings for a given string.

##### Returns

    0 - indicates that the comparison matched.
    -1 - Indicates that LookFor is "less than" (before) StringInList.
    1 - Indicates that LookFor is "greater than" (after) StringInList.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
StringInList | The current string in the list.
LookFor | The string that the Client searches for.

-----------------------------------------------------------------------------------------------------------------------------------

````
_Function_class_(EVT_DMF_String_MutilSzCallback)
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
(EVT_DMF_String_MultiSzCallback)(_In_ DMFMODULE DmfModule,
                                 _In_ WCHAR* String,
                                 _In_ VOID* CallbackContext);
````

The multi sz string enumerator Method calls this callback for every string found within a given multi sz string.

##### Returns

    TRUE - Indicates that enumeration shall continue.
    FALSE - Indicates that enumeration shall stop.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
String | The given multi sz string.
CallbackContext | The call specific context passed by the caller.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_String_FindInListChar
````
LONG
DMF_String_FindInListChar(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR** StringList,
    _In_ ULONG NumberOfStringsInStringList,
    _In_ CHAR* LookFor,
    _In_ EVT_DMF_String_CompareCharCallback ComparisonCallback
    );
````
Given a list of strings and a string to find, find the string in the list.
A comparison callback function is passed so that a caller specific comparison use used.

##### Returns

-1 indicates the string to look for was not found.
Otherwise the index of the matching string in the list of strings is returned.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
StringList | The given list of strings to search. Do not pass any NULL strings.
NumberOfStringsInStringList | The number of strings in StringList.
LookFor | The given string to search for in the list.
ComparisonCallback | The Client callback function which performs the comparison between each string in the list and the given string to find.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_String_FindInListExactChar
````
LONG
DMF_String_FindInListExactChar(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR** StringList,
    _In_ ULONG NumberOfStringsInStringList,
    _In_ CHAR* LookFor
    );
````

Given a list of strings and a string to find, find the string in the list.
The comparison made is: Full string, exact match, case sensitive.

##### Returns

-1 indicates the string to look for was not found.
Otherwise the index of the matching string in the list of strings is returned.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
StringList | The given list of strings to search. Do not pass any NULL strings.
NumberOfStringsInStringList | The number of strings in StringList.
LookFor | The given string to search for in the list.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_String_FindInListExactGuid

````
LONG
DMF_String_FindInListExactGuid(
    _In_ DMFMODULE DmfModule,
    _In_ GUID* GuidList,
    _In_ ULONG NumberOfGuidsInGuidList,
    _In_ GUID* LookFor
    );
````
Given a list of GUIDs and a GUID to find, find the GUID in the list.
The comparison made is: Full GUID, exact match.

##### Returns

-1 indicates the GUID to look for was not found.
Otherwise the index of the matching GUID in the list of strings is returned.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
GuidList | The given list of GUIDs to search.
NumberOfGuidsInGuidList | The number of GUIDs in GuidList.
LookFor | The given GUID to search for in the list.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_String_FindInListLookForLeftMatchChar
````
LONG
DMF_String_FindInListLookForLeftMatchChar(
    _In_ DMFMODULE DmfModule,
    _In_ CHAR** StringList,
    _In_ ULONG NumberOfStringsInStringList,
    _In_ CHAR* LookFor
    );
````
Given a list of strings and a string to find, find the string in the list.
The comparison made is: Full LookFor matches with left side of string in list, exact match, case sensitive.

##### Returns

-1 indicates the string to look for was not found.
Otherwise the index of the matching string in the list of strings is returned.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
StringList | The given list of strings to search. Do not pass any NULL strings.
NumberOfStringsInStringList | The number of strings in StringList.
LookFor | The given string to search for in the list.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_String_MultiSzEnumerate

````
NTSTATUS
DMF_String_MultiSzEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_z_ WCHAR* MultiSzWideString,
    _In_ EVT_DMF_String_MultiSzCallback Callback,
    _In_ VOID* CallbackContext
    );
````
Calls a given enumeration callback for every string found in the given multi sz string.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
MultiSzWideString | The given multi sz string.
Callback | The given enumeration callback.
CallbackContext | The context passed to the enumeration callback.

##### Remarks

* This method supports multi sz strings containing 0 length strings.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_String_MultiSzFindLast

````
WCHAR*
DMF_String_MultiSzFindLast(
    _In_ DMFMODULE DmfModule,
    _In_z_ WCHAR* MultiSzWideString
    );
````
Returns the last found string in the given multi sz string.

##### Returns

The last string found in the given multi sz string.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
MultiSzWideString | The given multi sz string.

##### Remarks

* This method supports multi sz strings containing 0 length strings.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_String_RtlAnsiStringToUnicodeString

````
NTSTATUS
DMF_String_RtlAnsiStringToUnicodeString(
    _In_ DMFMODULE DmfModule,
    _Out_ PUNICODE_STRING DestinationString,
    _In_ PCANSI_STRING SourceString
    );
````

Provides support similar to RtlAnsiSTringToUnicodeString API. This version is compatible with both Kernel and User-mode.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
DestinationString | Target UNICODE String.
SourceString | Source ANSI String.

##### Remarks

* This version has no option for allocating the target string on behalf of the caller.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_String_RtlAnsiStringToUnicodeString

````
NTSTATUS
DMF_String_RtlUnicodeStringToAnsiString(
    _In_ DMFMODULE DmfModule,
    _Out_ PANSI_STRING DestinationString,
    _In_ PCUNICODE_STRING SourceString
    );
````

Provides support similar to DMF_String_RtlUnicodeStringToAnsiString API. This version is compatible with both Kernel and User-mode.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
DestinationString | Target ANSI String.
SourceString | Source UNICODE String.

##### Remarks

* This version has no option for allocating the target string on behalf of the caller.
-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_String_WideStringCopyAsNarrow

````
NTSTATUS
DMF_String_WideStringCopyAsNarrow(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(BufferSize) CHAR* NarrowString,
    _In_z_ WCHAR* WideString,
    _In_ ULONG BufferSize
    )
````

Copy a Wide String as an Narrow String.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_String Module handle.
AnsiString | Target Ansi String.
WideString | Source Wide String.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* Add more similar functions.
* Add support for WCHAR and UNICODE.

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

