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

