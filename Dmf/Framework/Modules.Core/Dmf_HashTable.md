## DMF_HashTable

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a classic Hash Table data structure.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_HashTable
````
typedef struct
{
  // Maximum Key length in bytes.
  //
  ULONG MaximumKeyLength;

  // Maximum Value length in bytes.
  //
  ULONG MaximumValueLength;

  // Maximum number of Key-Value pairs to store in the Hash Table.
  //
  ULONG MaximumTableSize;

  // A callback to replace the default hashing algorithm.
  //
  EVT_DMF_HashTable_HashCalculate* EvtHashTableHashCalculate;
} DMF_CONFIG_HashTable;
````
Member | Description
----|----
MaximumKeyLength | Maximum supported Key length in bytes.
MaximumValueLength | Maximum supported Value length in bytes.
MaximumTableSize | Maximum number of Key-Value pairs to store in the Hash Table. This number may be not be zero.
EvtHashTableHashCalculate | A callback to replace the default hashing algorithm. By default, FNV-1a hashing algorithm is used.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_HashTable_HashCalculate
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
ULONG_PTR
EVT_DMF_HashTable_HashCalculate(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength
    );
````

A Client callback to replace the default hashing algorithm.

##### Returns

The calculated hash value.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HashTable Module handle.
Key | A Key whose hash needs to be calculated.
KeyLength | The Length of the Key in bytes.

##### Remarks

* By default, FNV-1a hashing algorithm is used.
* Provide this callback only if the default hashing algorithm needs to be replaced.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_HashTable_Find
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_HashTable_Find(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _Inout_updates_to_(*ValueLength, *ValueLength) UCHAR* Value,
    _Inout_ ULONG* ValueLength
    );
````

A Client callback called by DMF_HashTable_Find.
Allows the Client to perform Client specific tasks on a given Key-Value pair.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HashTable Module handle.
Key | The given Key.
KeyLength | The Length of the Key in bytes.
Value | The given Value.
ValueLength | The Length of the Value in bytes.

##### Remarks

* The Key should not be modified.
* The Value and the ValueLength can be modified in this callback.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_HashTable_Enumerate
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_HashTable_Enumerate(
    _In_ DMFMODULE DmfModule,
    _In_reads_(KeyLength) UCHAR* Key,
    _In_ ULONG KeyLength,
    _In_reads_(ValueLength) UCHAR* Value,
    _In_ ULONG ValueLength,
    _In_ VOID* CallbackContext
    );
````

A Client callback called by DMF_HashTable_Enumerate for every Key-Value pair in a Hash Table.

##### Returns

TRUE to continue enumeration.
FALSE to stop enumeration.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HashTable Module handle.
Key | The given Key.
KeyLength | The Length of the Key in bytes.
Value | The given Value.
ValueLength | The Length of the Value in bytes.
CallbackContext | A Context pointer passed by the Client into DMF_HashTable_Enumerate.

##### Remarks

* Key and Value buffers in this callback are read only.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HashTable_Enumerate

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_HashTable_Enumerate(
  _In_ DMFMODULE DmfModule,
  _In_ EVT_DMF_HashTable_Enumerate* CallbackEnumerate,
  _In_ VOID* CallbackContext
  );
````

This Method enumerates all the Key-Value pairs in a Hash Table instance and calls a given callback for each pair.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HashTable Module handle.
CallbackEnumerate | The given callback.
CallbackContext | A Client specific context that is passed to the given callback.

##### Remarks

* Clients use this Method when they need to search or perform actions on all the Key-Value pairs in a Hash Table.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HashTable_Find

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HashTable_Find(
  _In_ DMFMODULE DmfModule,
  _In_reads_(KeyLength) UCHAR* Key,
  _In_ ULONG KeyLength,
  _In_ EVT_DMF_HashTable_Find* CallbackFind
  );
````

This Method finds a given Key in a Hash Table and calls a given callback.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HashTable Module handle.
Key | The given Key.
KeyLength | The Length of the Key in bytes.
CallbackFind | The callback to perform Client specific tasks on the Value associated with the Key.

##### Remarks

* In case the Key is absent in the Hash Table, it will be added with the Value set to zero before calling the callback.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HashTable_Read

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HashTable_Read(
  _In_ DMFMODULE DmfModule,
  _In_reads_(KeyLength) UCHAR* Key,
  _In_ ULONG KeyLength,
  _Out_writes_(ValueBufferLength) UCHAR* ValueBuffer,
  _In_ ULONG ValueBufferLength,
  _Out_opt_ ULONG* ValueLength
  );
````

Given a Key, this method reads a Value associated with it.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HashTable Module handle.
Key | The given Key.
KeyLength | The Length of the Key in bytes.
ValueBuffer | The buffer where the Value data will be written.
ValueBufferLength | The length of the ValueBuffer in bytes.
ValueLength | The actual length of the Value data written to ValueBuffer.

##### Remarks

* STATUS_BUFFER_TOO_SMALL is returned if ValueBufferLength is less than the Value data length.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_HashTable_Write

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_HashTable_Write(
  _In_ DMFMODULE DmfModule,
  _In_reads_(KeyLength) UCHAR* Key,
  _In_ ULONG KeyLength,
  _In_reads_(ValueLength) UCHAR* Value,
  _In_ ULONG ValueLength
  );
````

Write a Key-Value pair into a Hash Table.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_HashTable Module handle.
Key | The given Key.
KeyLength | The Length of the Key in bytes.
Value | The given Value.
ValueLength | The Length of the Value in bytes.

##### Remarks

* If an element with the given Key already exists - its Value will be updated.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* Always test the driver using DEBUG builds because many important checks for integrity are performed in DEBUG build that
   are not performed in RELEASE build.
* The memory to store Hash Table entries is pre-allocated when the Module is created.
   Make sure MaximumKeyLength, MaximumValueLength and MaximumTableSize are configured properly.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DMF_BranchTrack

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Data Structures

-----------------------------------------------------------------------------------------------------------------------------------

