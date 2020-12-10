## DMF_I2cTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module gives the Client access to an underlying I2C device connected on SPB bus.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_I2cTarget
````
typedef struct
{
  // Module will not load if Gpio Connection not found.
  //
  BOOLEAN I2cConnectionMandatory;
  // Indicates the index of I2C resource that the Client wants to access.
  //
  ULONG I2cResourceIndex;
  // Microseconds to delay on SPB Read operations.
  //
  ULONG ReadDelayUs;
  // Time units(ms) to wait for SPB Read Operation to Complete.
  //
  ULONGLONG ReadTimeoutMs;
  // Time units(ms) to wait for SPB Write Operation to Complete.
  //
  ULONGLONG WriteTimeoutMs;
} DMF_CONFIG_I2cTarget;
````
Member | Description
----|----
I2cResourceIndex | Indicates the index of I2C resource that the Client wants to access.
ReadDelayUs | Microseconds to delay on SPB Read operations.
ReadTimeoutMs | Time units(ms) to wait for SPB Read Operation to Complete.
WriteTimeoutMs | Time units(ms) to wait for SPB Write Operation to Complete.

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

##### DMF_I2cTarget_AddressRead

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_AddressRead(
  _In_ DMFMODULE DmfModule,
  _In_reads_(AddressLength) UCHAR* Address,
  _In_ ULONG AddressLength,
  _Out_writes_(BufferLength) VOID* Buffer,
  _In_ ULONG BufferLength
  );
````

Reads a buffer via the I2C bus from the given address. The given address is written. Then, the data is read.

##### Returns

NTSTATUS indicating the success or failure of the read.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_I2cTarget Module handle.
Address | The given address (on the I2c device).
AddressLength | The size in bytes of the Address buffer.
Buffer | The data transferred (read) via I2C is located at this address.
BufferLength | The size in bytes of Buffer.

##### Remarks
-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_I2cTarget_AddressWrite

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_AddressWrite(
  _In_ DMFMODULE DmfModule,
  _In_reads_(AddressLength) UCHAR* Address,
  _In_ ULONG AddressLength,
  _In_reads_(BufferLength) VOID* Buffer,
  _In_ ULONG BufferLength
  );
````

Writes a buffer via the I2C bus from the given address. The given address is written. Then, the data is written.

##### Returns

NTSTATUS indicating the success or failure of the write.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_I2cTarget Module handle.
Address | The given address (on the I2c device).
AddressLength | The size in bytes of the Address buffer.
Buffer | The data transferred (written) via I2C is located at this address.
BufferLength | The size in bytes of Buffer.

##### Remarks
-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_I2cTarget_BufferRead

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_BufferRead(
  _In_ DMFMODULE DmfModule,
  _Inout_updates_(BufferLength) VOID* Buffer,
  _In_ ULONG BufferLength,
  _In_ ULONG TimeoutMs
  );
````

Reads a buffer via the I2C bus.

##### Returns

NTSTATUS indicating the success or failure of the read.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_I2cTarget Module handle.
Buffer | The data transferred (read) via I2C is written at this address.
BufferLength | The size in bytes of Buffer.
TimeoutMs | Indicates that the Read transaction should fail after TimeoutMs milliseconds. Set to zero to indicate there is no timeout.

##### Remarks
-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_I2cTarget_BufferWrite

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_I2cTarget_BufferWrite(
  _In_ DMFMODULE DmfModule,
  _Inout_updates_(BufferLength) VOID* Buffer,
  _In_ ULONG BufferLength,
  _In_ ULONG TimeoutMs
  );
````

Writes a buffer via the I2C bus.

##### Returns

NTSTATUS indicating the success or failure of the write.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_I2cTarget Module handle.
Buffer | The data transferred (written) via I2C is located at this address.
BufferLength | The size in bytes of Buffer.
TimeoutMs | Indicates that the Write transaction should fail after TimeoutMs milliseconds. Set to zero to indicate there is no timeout.

##### Remarks
-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_I2cTarget_IsResourceAssigned

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_I2cTarget_IsResourceAssigned(
  _In_ DMFMODULE DmfModule,
  _Out_opt_ BOOLEAN* I2cConnectionAssigned
  );
````

Indicates if the I2c resource set by the Client has been found.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_I2cTarget Module handle.
I2cConnectionAssigned | Flag indicating if the I2c resource is found or not.

##### Remarks

* Use this Method to determine if the resources needed are found. This is for the cases where the Client driver runs regardless of whether or not I2C resource is available.

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

Targets

-----------------------------------------------------------------------------------------------------------------------------------

