## DMF_UefiOperation

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module provides UEFI basic operations. It allows clients to do data reading and changing to UEFI.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_UefiOperation_FirmwareEnvironmentVariableAllocateGet

````
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
    );
````

Allows the Client to get the UEFI variable data from certain UEFI Guid and name in both User-mode and Kernel-mode.
This API allocates required memory automatically and the client is responsible to free the memory.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.
Name | Name of UEFI variable to read data from.
Guid | GUID of UEFI variable to read data from.
VariableBuffer | Buffer that will store the data that is read from the UEFI variable.
VariableBufferSize | As input, it pass the desired size that needs to be read. As output, it send back the actual size that was read from UEFI.
VariableBufferHandle | WDF Memory Handle for the client
Attributes | Location to which the routine writes the attributes of the specified environment variable.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_UefiOperation_FirmwareEnvironmentVariableGet

````
_Success_(return)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UefiOperation_FirmwareEnvironmentVariableGet(
    _In_ DMFMODULE DmfModule,
    _In_ LPCTSTR Name,
    _In_ LPCTSTR Guid,
    _Out_writes_(*VariableBufferSize) VOID* VariableBuffer,
    _Inout_ DWORD* VariableBufferSize
    );
````

Allows the Client to get the UEFI variable data from certain UEFI Guid and name.
This Method is deprecated. Use DMF_UefiOperation_FirmwareEnvironmentVariableGetEx() instead.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.
Name | Name of UEFI variable to read data from.
Guid | GUID of UEFI variable to read data from.
VariableBuffer | Buffer that will store the data that is read from the UEFI variable.
VariableBufferSize | As input, it pass the desired size that needs to be read. As output, it send back the actual size that was read from UEFI.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_UefiOperation_FirmwareEnvironmentVariableGetEx

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UefiOperation_FirmwareEnvironmentVariableGetEx(
    _In_ DMFMODULE DmfModule,
    _In_ UNICODE_STRING* Name,
    _In_ LPGUID Guid,
    _Out_writes_bytes_opt_(*VariableBufferSize) VOID* VariableBuffer,
    _Inout_ ULONG* VariableBufferSize,
    _Out_opt_ ULONG* Attributes
    );
````

Allows the Client to get the UEFI variable data to certain UEFI Guid and name in both User-mode and Kernel-mode.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.
Name | Name of UEFI variable to read data from.
Guid | GUID of UEFI variable to read data from.
VariableBuffer | Buffer that will store the data that is read from the UEFI variable.
VariableBufferSize | As input, it passes the desired size that needs to be read. As output, it sends back the actual size that was read from UEFI.
Attributes | Location to which the routine writes the attributes of the specified environment variable.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_UefiOperation_FirmwareEnvironmentVariableSet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_UefiOperation_FirmwareEnvironmentVariableSet(
    _In_ DMFMODULE DmfModule,
    _In_ LPCTSTR Name,
    _In_ LPCTSTR Guid,
    _In_reads_(VariableBufferSize) VOID* VariableBuffer,
    _In_ DWORD VariableBufferSize
    );
````

Allows the Client to set the UEFI variable data to certain UEFI Guid and name.
This Method is deprecated. Use DMF_UefiOperation_FirmwareEnvironmentVariableSetEx() instead.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.
Name | Name of UEFI variable to write data to. 
Guid | GUID of UEFI variable to write data to.
VariableBuffer | Buffer that stores the data that is to be written to the UEFI variable.
VariableBufferSize | Size of VariableBuffer in bytes.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_UefiOperation_FirmwareEnvironmentVariableSetEx

````
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
    );
````

Allows the Client to set the UEFI variable data to certain UEFI Guid and name in both User-mode and Kernel-mode.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | This Module's handle.
Name | Name of UEFI variable to write data to. 
Guid | GUID of UEFI variable to write data to.
VariableBuffer | Buffer that stores the data that is to be written to the UEFI variable.
VariableBufferSize | Size of VariableBuffer in bytes.
Attributes | The attributes to assign to the specified environment variable.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

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

Hardware

-----------------------------------------------------------------------------------------------------------------------------------

