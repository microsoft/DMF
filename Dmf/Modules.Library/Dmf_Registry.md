## DMF_Registry

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Contains support that allows Clients to perform simple as well as complex registry manipulation.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### Registry_ActionType
````
typedef enum
{
  Registry_ActionTypeInvalid,
  // Write the value.
  //
  Registry_ActionTypeWrite,
  // Delete the value.
  //
  Registry_ActionTypeDelete,
  // Return read value to the caller.
  //
  Registry_ActionTypeRead,
  // Do nothing. The comparison function does the action.
  //
  Registry_ActionTypeNone
} Registry_ActionType;
````
Indicates what actions the Registry Action function should do.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------
##### Registry_Entry
Holds information for a single registry entry.

````
typedef struct
{
  // It is always prepended by the Branch Prefix string.
  //
  PWCHAR ValueName;
  // Supported types are:
  // REG_DWORD, REG_SZ, REG_MULTISZ, REG_BINARY.
  //
  ULONG ValueType;
  // This value has different meanings depending on the ValueType.
  // REG_SZ, REG_MULTISZ, REG_BINARY: Pointer to the data.
  // REG_DWORD: Lower 4 bytes are the DWORD.
  //
  VOID* ValueData;
  // Use Registry_TABLE_AUTO_SIZE for REG_SZ, REG_MULTISZ, REG_DWORD.
  // Otherwise store the size of the data pointed to by ValueData.
  //
  ULONG ValueSize;
} Registry_Entry;
````

Holds information for a branch of registry entries which consist of one or more registry entries under a single key.

-----------------------------------------------------------------------------------------------------------------------------------
##### Registry_Branch
````
typedef struct
{
  // The Key under which all the entries in the array are stored. This name is
  // prepended onto the name of value in addition to a "\\".
  //
  PWCHAR BranchValueNamePrefix;
  // The entries in the branch.
  //
  Registry_Entry* RegistryTableEntries;
  // The number of entries in the branch.
  //
  ULONG ItemCount;
} Registry_Branch;
````

-----------------------------------------------------------------------------------------------------------------------------------

##### Registry_Tree

Holds information for a set of registry branches.

````
typedef struct
{
  // Path under the node to all the branches.
  //
  PWCHAR RegistryPath;
  // List of the branches in the tree.
  //
  Registry_Branch* Branches;
  // Number of branches in the tree.
  //
  ULONG NumberOfBranches;
} Registry_Tree;
````

-----------------------------------------------------------------------------------------------------------------------------------

##### Registry_ContextScheduledTaskCallback

````
typedef struct
{
  EVT_DMF_Registry_CallbackWork** Callbacks;
  ULONG NumberOfCallbacks;
} Registry_ContextScheduledTaskCallback;
````

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_Registry_CallbackWork
````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_Registry_CallbackWork(
    _In_ DMFMODULE DmfModule
    );
````

DMF_Registry Module callback. This callback is passed to DMF_Registry_CallbackWork.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_Registry_KeyEnumerationCallback
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_Registry_KeyEnumerationCallback(
    _In_ VOID* ClientContext,
    _In_ HANDLE RootHandle,
    _In_ PWCHAR KeyName
    );
````

A Client callback called for every KeyName under RootHandle.

##### Returns

TRUE indicates enumeration should continue. FALSE, otherwise.

##### Parameters
Parameter | Description
----|----
ClientContext | A Client specific context.
RootHandle | A registry handle.
KeyName | The name of a key under RootHandle.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_Registry_ValueComparisonCallback
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_Registry_ValueComparisonCallback(
    _In_ DMFMODULE DmfModule,
    _In_opt_ VOID* ClientContext,
    _In_ VOID* ValueDataInRegistry,
    _In_ ULONG ValueDataInRegistrySize,
    _In_opt_ VOID* ClientDataInRegistry,
    _In_ ULONG ClientDataInRegistrySize
    );
````

Given data read from a value in the registry, this callback compares it with a Client specific value using a Client specific
comparison encoded in this callback. The result of the comparison is returned to the caller.

##### Returns

TRUE indicates the ValueDataInRegistry is equal to ClientDataInRegistry (per a Client specific comparison).

##### Parameters
Parameter | Description
----|----
ClientContext | A Client specific context.
ValueDataInRegistry | The registry data read from the value.
ValueDataInRegistrySize | The size in bytes of ValueDataInRegistry.
ClientDataInRegistry | The value to compare ValueDataInRegistry to.
ClientDataInRegistrySize | The size in bytes of CilentDataInRegistry.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Registry_AllSubKeysFromHandleEnumerate

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_AllSubKeysFromHandleEnumerate(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
  _In_ VOID* ClientCallbackContext
  );
````

Given a registry handle, enumerate all the subkeys under the associated key recursively. Call a given Client callback for each
enumerated subkey.

##### Returns

TRUE indicates the enumeration was successfully completed.
FALSE indicates that an error occurred.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ClientCallback | The given Client callback to execute for each enumerated subkey of the given handle.
ClientCallbackContext | A Client specific context passed to ClientCallback.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Registry_CallbackWork

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_CallbackWork(
  _In_ WDFDEVICE WdfDevice,
  _In_ EVT_DMF_Registry_CallbackWork CallbackWork
  );
````

Creates a dynamic instance of the DMF_Registry Module handle. Then calls a Client callback that performs work using that handle.
The Client is able to perform work using that handle. Finally, when the Client callback ends, the handle is destroyed.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
WdfDevice | The Client driver's WDFDEVICE.
CallbackWork | The Client callback that is passed the a DMF_Registry Module handle.

##### Remarks

* The Client is able to perform operations using the registry without creating the DMF_Registry Module instance directly.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_CustomAction

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_CustomAction(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _In_ ULONG ValueType,
  _In_opt_ VOID* ValueDataToCompare,
  _In_ ULONG ValueDataToCompareSize,
  _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
  _In_opt_ VOID* ComparisonCallbackContext
  );
````

Given a registry handle and a value name/type under that handle, read the data stored in that value and call a Client callback
function which determines if an action should occur based on a comparison, and if so, performs that action.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
ValueType | The type of the given value.
ValueSize | The size in bytes of the given value.
ValueDataToCompare | This is the given data to write if the Client callback determines that the data should be written.
ValueDataToCompareSize | The size in bytes of ValueDataToWrite.
ComparisonCallback | The given Client callback to execute for each enumerated subkey of the given handle.

##### Remarks

* The functions that write and delete based on a Client comparison function's result are written using this function.
* The Client is able to write similar functions using this function.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_EnumerateKeysFromName

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_EnumerateKeysFromName(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RootKeyName,
  _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
  _In_ VOID* ClientCallbackContext
  );
````

Given a registry path name, enumerate all the subkeys under that path. Call a given Client callback for each enumerated key.

##### Returns

TRUE indicates the enumeration was successfully completed.
FALSE indicates that an error occurred.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RootKeyName | The given registry path name.
ClientCallback | The given Client callback to execute for each enumerated key.
ClientCallbackContext | Client specific context to pass to ClientCallback.

##### Remarks

* An error is returned if the RootKeyName does not exist or cannot be opened.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Registry_HandleClose

````
_IRQL_requires_max_(PASSIVE_LEVEL)
void
DMF_Registry_HandleClose(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle
  );
````

Given a registry handle, close it.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Registry_HandleDelete

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleDelete(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle
  );
````

Given a registry handle, delete that handle.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Registry_HandleOpenByHandle

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
HANDLE
DMF_Registry_HandleOpenByHandle(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR Name,
  _In_ BOOLEAN TryToCreate
  );
````

Given a registry handle and a path under that handle, open a new handle to the given path.

##### Returns

The handle to the given registry path name or NULL if the path does not exist.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
Name | The given registry path name.
TryToCreate | Creates the path specified by Name if the path does not exist.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Registry_HandleOpenById

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleOpenById(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG PredefinedKeyId,
    _In_ ULONG AccessMask,
    _Out_ HANDLE* RegistryHandle
    );
````

Open a predefined registry key.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
PredefinedKeyId | The Id of the predefined key to open. See IoOpenDeviceRegistryKey documentation for a list of Ids.
AccessMask | The desired access mask. See MSDN.
RegistryHandle | The address of the handle that is returned to the Client.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_HandleOpenByName

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
HANDLE
DMF_Registry_HandleOpenByName(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR Name
  );
````

Given a registry path name, open a handle to it.

##### Returns

The handle to the given registry path name or NULL if the path does not exist.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Name | The given registry path name.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_HandleOpenByNameEx

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleOpenByNameEx(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR Name,
  _In_ ULONG AccessMask,
  _In_ BOOLEAN Create,
  _Out_ HANDLE* RegistryHandle
  );
````

Given a registry path name, open a handle to it. This Method allows the Client to specify additional parameters indicating how
the handle should be opened.

##### Returns

NTSTATUS. Fails if the handle cannot be opened as the Client specifies.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Name | The given registry path name.
AccessMask | The desired access mask. See MSDN.
Create | Creates the path and opens it if the given path does not exist.
RegistryHandle | The address of the handle that is returned to the Client.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueDelete

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueDelete(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName
  );
````

Given a registry path name and value name under that handle, delete that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueRead

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueRead(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _In_ ULONG RegistryType,
  _Out_writes_opt_(BufferSize) UCHAR* Buffer,
  _In_ ULONG BufferSize,
  _Out_opt_ ULONG* BytesRead
  );
````

Given a registry path name and a value name/type under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
ValueType | The type of the given value.
Buffer | The data read is written to this buffer.
BufferSize | The size in bytes of Buffer.
BytesRead | The size in bytes of the data read is written to this address.

##### Remarks

* This function is used by many functions that read specific types of values.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueReadBinary

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadBinary(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _Out_writes_opt_(BufferSize) UCHAR* Buffer,
  _In_ ULONG BufferSize,
  _Out_opt_ ULONG* BytesRead
  );
````

Given a registry path name and a BINARY value name under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
ValueType | The type of the given value.
Buffer | The data read is written to this buffer.
BufferSize | The size in bytes of Buffer.
BytesRead | The number of bytes read is written to this address.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueReadDword

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadDword(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _Out_ ULONG* Buffer
  );
````

Given a registry path name and a DWORD value name under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
ValueType | The type of the given value.
Buffer | The data read is written to this buffer.
BufferSize | The size in bytes of Buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueReadDwordAndValidate

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadDwordAndValidate(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _Out_ ULONG* Buffer,
  _In_ ULONG Minimum,
  _In_ ULONG Maximum
  );
````

Given a registry path name and a DWORD value name under that handle as well as its required minimum/maximum values, read data from
that value. Then, validate it per the required minimum and maximum values. The return value is an error if the value read is
not within the required minimum and maximum values.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
Buffer | The data read is written to this buffer.
Minimum | The required minimum value of the data read.
Maximum | The required maximum value of the data read.

##### Remarks

* Generally speaking, the Client will set the value read to a default value if an error is returned.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueReadMultiString

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadMultiString(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
  _In_ ULONG NumberOfCharacters,
  _Out_opt_ ULONG* BytesRead
  );
````

Given a registry path name and a MULTISTRING value name under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
ValueType | The type of the given value.
Buffer | The data read is written to this buffer.
NumberOfCharacters | The size in number of characters (WCHAR) of buffer.
BytesRead | The number of bytes read is written to this address.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueReadQword

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadQword(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _Out_ ULONGLONG* Buffer
  );
````

Given a registry path name and a QWORD value name under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
ValueType | The type of the given value.
Buffer | The data read is written to this buffer.
BufferSize | The size in bytes of Buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueReadQwordAndValidate

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadQwordAndValidate(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _Out_ ULONGLONG* Buffer,
  _In_ ULONGLONG Minimum,
  _In_ ULONGLONG Maximum
  );
````

Given a registry path name and a QWORD value name under that handle as well as its required minimum/maximum values, read data from
that value. Then, validate it per the required minimum and maximum values. The return value is an error if the value read is
not within the required minimum and maximum values.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
Buffer | The data read is written to this buffer.
Minimum | The required minimum value of the data read.
Maximum | The required maximum value of the data read.

##### Remarks

* Generally speaking, the Client will set the value read to a default value if an error is returned.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueReadString

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadString(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
  _In_ ULONG NumberOfCharacters,
  _Out_opt_ ULONG* BytesRead
  );
````

Given a registry path name and a STRING value name under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
ValueType | The type of the given value.
Buffer | The data read is written to this buffer.
NumberOfCharacters | The size in number of characters (WCHAR) of buffer.
BytesRead | The number of bytes read is written to this address.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueWrite

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWrite(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _In_ ULONG RegistryType,
  _In_reads_(BufferSize) UCHAR* Buffer,
  _In_ ULONG BufferSize
  );
````

Given a registry path name and a value name/type under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
ValueType | The type of the given value.
Buffer | The data written is read from this buffer.
BufferSize | The size in bytes of Buffer.

##### Remarks

* This function is used by many functions that read specific types of values.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueWriteBinary

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteBinary(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _In_reads_(BufferSize) UCHAR* Buffer,
  _In_ ULONG BufferSize
  );
````

Given a registry path name and a BINARY value name under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
Buffer | The data written is read from this buffer.
BufferSize | The size in bytes of Buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueWriteDword

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteDword(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _In_ ULONG ValueData
  );
````

Given a registry path name and a DWORD value name under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
ValueData | The data to write.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueWriteMultiString

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteMultiString(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _In_reads_(NumberOfCharacters) PWCHAR Buffer,
  _In_ ULONG NumberOfCharacters
  );
````

Given a registry path name and a MULTISTRING value name under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
Buffer | The data written is read from this buffer.
NumberOfCharacters | The size in number of characters (WCHAR) of buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueWriteQword

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteQword(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _In_ ULONGLONG ValueData
  );
````

Given a registry path name and a QWORD value name under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
ValueData | The data to write.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_PathAndValueWriteString

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteString(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR RegistryPathName,
  _In_ PWCHAR ValueName,
  _In_reads_(NumberOfCharacters) PWCHAR Buffer,
  _In_ ULONG NumberOfCharacters
  );
````

Given a registry path name and a STRING value name under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryPathName | The given registry path name.
ValueName | The name of the given value.
Buffer | The data written is read from this buffer.
NumberOfCharacters | The size in number of characters (WCHAR) of buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Registry_RegistryPathDelete

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_RegistryPathDelete(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR Name
  );
````

Given a registry path name and delete that path.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Name | The given registry path name.
ValueName | The name of the given value.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ScheduledTaskCallbackContainer

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
ScheduledTask_Result_Type
DMF_Registry_ScheduledTaskCallbackContainer(
  _In_ DMFMODULE DmfScheduledTask,
  _In_ VOID* ClientCallbackContext,
  _In_ WDF_POWER_DEVICE_STATE PreviousState
  );
````

This Method is used by DMF internally. Clients should not use this Method.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_SubKeysFromHandleEnumerate

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_SubKeysFromHandleEnumerate(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
  _In_ VOID* ClientCallbackContext
  );
````

Given a registry handle, enumerate all its subkeys and call a given Client callback for each one.

##### Returns

TRUE indicates the enumeration was successfully completed.
FALSE indicates that an error occurred.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ClientCallback | The given Client callback to execute for each enumerated subkey of the given handle.
ClientCallbackContext | A Client specific context passed to ClientCallback.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_SubKeysFromPathNameContainingStringEnumerate

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_SubKeysFromPathNameContainingStringEnumerate(
  _In_ DMFMODULE DmfModule,
  _In_ PWCHAR PathName,
  _In_ PWCHAR LookFor,
  _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
  _In_ VOID* ClientCallbackContext
  );
````

Given a registry path name, enumerate all the subkeys under that path which contain a given string. Call a given Client
callback for each enumerated key.

##### Returns

TRUE indicates the enumeration was successfully completed.
FALSE indicates that an error occurred.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
PathName | The given registry path name.
LookFor | The given string to search for in key.
ClientCallback | The given Client callback to execute for each enumerated key.
ClientCallbackContext | Client specific context to pass to ClientCallback.

##### Remarks

* An error is returned if the RootKeyName does not exist or cannot be opened.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_Registry_TreeWriteDeferred

````
NTSTATUS
DMF_Registry_TreeWriteDeferred(
  _In_ DMFMODULE DmfModule,
  _In_ Registry_Tree* RegistryTree,
  _In_ ULONG ItemCount
  );
````

Given a registry tree data structure, this Method writes all the items in the tree to the registry. The Method keeps trying in
case the Method encounters a STATUS_OBJECT_NOT_FOUND error while writing the entries.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryTree | A list of Registry_Tree items.
ItemCount | The number of items in RegistryTree.

##### Remarks

* This Method is useful when writing multiple registry entries.
* The Registry_Tree structure organizes the data to write into branches.
* Soon after booting it is possible that STATUS_OBJECT_NOT_FOUND error happens while accessing the registry.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_Registry_TreeWriteEx
````
NTSTATUS
DMF_Registry_TreeWriteEx(
  _In_ DMFMODULE DmfModule,
  _In_ Registry_Tree* RegistryTree,
  _In_ ULONG ItemCount
  );
````

Given a registry tree data structure, this Method writes all the items in the tree to the registry.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
RegistryTree | A list of Registry_Tree items.
ItemCount | The number of items in RegistryTree.

##### Remarks

* This Method is useful when writing multiple registry entries.
* The Registry_Tree structure organizes the data to write into branches.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueDelete

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueDelete(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName
  );
````

Given a registry handle and a value name/type under that handle, delete that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Registry_ValueDeleteIfNeeded

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueDeleteIfNeeded(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _In_opt_ VOID* ValueDataToCompare,
  _In_ ULONG ValueDataToCompareSize,
  _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
  _In_opt_ VOID* ComparisonCallbackContext
  );
````

Given a registry handle and a value name/type and size under that handle, read that value. Then, call a given Client callback
function which determines if the given value should be deleted, and if so, deletes it.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
ValueDataToCompare | This is the given data to write if the Client callback determines that the value should be deleted.
ValueDataToCompareSize | The size in bytes of ValueDataToWrite.
ComparisonCallback | The given Client callback to execute for each enumerated subkey of the given handle.
ComparisonCallbackContext | A Client specific context passed to ClientCallback.

##### Remarks

* This function saves the programmer from writing a lot of code for a common operation.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueRead

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueRead(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _In_ ULONG ValueType,
  _Out_writes_opt_(BufferSize) UCHAR* Buffer,
  _In_ ULONG BufferSize,
  _Out_opt_ ULONG* BytesRead
  );
````

Given a registry handle and a value name/type under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
ValueType | The type of the given value.
Buffer | The data read is written to this buffer.
BufferSize | The size in bytes of Buffer.
BytesRead | The size in bytes of the data read is written to this address.

##### Remarks

* This function is used by many functions that read specific types of values.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueReadBinary

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadBinary(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _Out_writes_opt_(BufferSize) UCHAR* Buffer,
  _In_ ULONG BufferSize,
  _Out_opt_ ULONG* BytesRead
  );
````

Given a registry handle and a BINARY value name under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data read is written to this buffer.
BufferSize | The number of characters (WCHAR) in buffer.
BytesRead | The size in bytes of the data read is written to this address.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueReadDword

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadDword(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _Out_ ULONG* Buffer
  );
````

Given a registry handle and a DWORD value name under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data read is written to this buffer.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueReadDwordAndValidate

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadDwordAndValidate(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _Out_ ULONG* Buffer,
  _In_ ULONG Minimum,
  _In_ ULONG Maximum
  );
````

Given a registry handle and a DWORD value name under that handle as well as its required minimum/maximum values, read data from
that value. Then, validate it per the required minimum and maximum values. The return value is an error if the value read is
not within the required minimum and maximum values.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data read is written to this buffer.
Minimum | The required minimum value of the data read.
Maximum | The required maximum value of the data read.

##### Remarks

* Generally speaking, the Client will set the value read to a default value if an error is returned.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueReadMultiString

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadMultiString(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
  _In_ ULONG NumberOfCharacters,
  _Out_opt_ ULONG* BytesRead
  );
````

Given a registry handle and a MULTTSTRING value name under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data read is written to this buffer.
NumberOfCharacters | The size in number of characters (WCHAR) of buffer.
BytesRead | The size in bytes of the data read is written to this address.

##### Remarks

* MULTISTRING values are composed of several string separated by CR/LF with a double CR/LF at the end of the buffer.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueReadQword

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadQword(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _Out_ ULONGLONG* Buffer
  );
````

Given a registry handle and a QWORD value name under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data read is written to this buffer.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueReadQwordAndValidate

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadQwordAndValidate(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _Out_ PULONGLONG Buffer,
  _In_ ULONGLONG Minimum,
  _In_ ULONGLONG Maximum
  );
````

Given a registry handle and a QWORD value name under that handle as well as its required minimum/maximum values, read data
from that value. Then, validate it per the required minimum and maximum values. The return value is an error if the value read is
not within the required minimum and maximum values.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data read is written to this buffer.
Minimum | The required minimum value of the data read.
Maximum | The required maximum value of the data read.

##### Remarks

* Generally speaking, the Client will set the value read to a default value if an error is returned.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueReadString

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadString(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
  _In_ ULONG NumberOfCharacters,
  _Out_opt_ ULONG* BytesRead
  );
````

Given a registry handle and a STRING value name under that handle, read data from that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data read is written to this buffer.
NumberOfCharacters | The size in number of characters (WCHAR) of buffer.
BytesRead | The size in bytes of the data read is written to this address.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueWrite

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWrite(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _In_ ULONG ValueType,
  _In_reads_(BufferSize) UCHAR* Buffer,
  _In_ ULONG BufferSize
  );
````

Given a registry handle and a value name/type under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
ValueType | The type of the given value.
Buffer | The data to write is read from this buffer.
BufferSize | The size in bytes of Buffer.
BytesRead | The size in bytes of the data written is written to this address.

##### Remarks

* This function is used by many functions that write specific types of values.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Registry_ValueWriteBinary

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteBinary(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _In_reads_(BufferSize) UCHAR* Buffer,
  _In_ ULONG BufferSize
  );
````

Given a registry handle and a BINARY value name under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data written is read from this buffer.
BufferSize | The size in bytes of Buffer.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueWriteDword

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteDword(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _In_ ULONG ValueData
  );
````

Given a registry handle and a DWORD value name under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data written is read from this buffer.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_Registry_ValueWriteIfNeeded

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteIfNeeded(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _In_ ULONG ValueType,
  _In_ VOID* ValueDataToWrite,
  _In_ ULONG ValueDataToWriteSize,
  _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
  _In_opt_ VOID* ComparisonCallbackContext,
  _In_ BOOLEAN WriteIfNotFound
  );
````

Given a registry handle and a value name/type/size under that handle, read that value. Then, call a given Client callback
which determines if the given data to write should be written and if so, write the given data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
ValueType | The type of the given value.
ValueSize | The size in bytes of the given value.
ValueDataToWrite | This is the given data to write if the Client callback determines that the data should be written.
ValueDataToWriteSize | The size in bytes of ValueDataToWrite.
ComparisonCallback | The given Client callback to execute for each enumerated subkey of the given handle.
ComparisonCallbackContext | A Client specific context passed to ClientCallback.
WriteIfNotFound | If the given value is not found, this flag indicates that the given data to write should be written.

##### Remarks

* This function saves the programmer from writing a lot of code for a common operation.

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueWriteMultiString

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteMultiString(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _In_reads_(NumberOfCharacters) PWCHAR Buffer,
  _In_ ULONG NumberOfCharacters
  );
````

Given a registry handle and a MULTISTRING value name under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data written is read from this buffer.
NumberOfCharacters | The size in number of characters (WCHAR) of buffer.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueWriteQword

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteQword(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _In_ ULONGLONG ValueData
  );
````

Given a registry handle and a QWORD value name under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data written is read from this buffer.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------


##### DMF_Registry_ValueWriteString

````
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteString(
  _In_ DMFMODULE DmfModule,
  _In_ HANDLE Handle,
  _In_ PWCHAR ValueName,
  _In_reads_(NumberOfCharacters) PWCHAR Buffer,
  _In_ ULONG NumberOfCharacters
  );
````

Given a registry handle and a STRING value name under that handle, write data to that value.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_Registry Module handle.
Handle | The given registry handle.
ValueName | The name of the given value.
Buffer | The data written is read from this buffer.
NumberOfCharacters | The size in number of characters (WCHAR) of buffer.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module saves the Client from write a lot of non-trivial code to find and operate on registry keys.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

