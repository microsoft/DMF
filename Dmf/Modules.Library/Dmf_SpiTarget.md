## DMF_SpiTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This Module gives the Client access to an underlying SPI device connected on SPB bus.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_SpiTarget
````
typedef struct
{
  // Indicates the resource index of the SPI line this instance connects to.
  //
  ULONG ResourceIndex;
  // This is for debugging and validation purposes only.
  // Do not enable in a production driver.
  //
  EVT_DMF_SpiTarget_LatencyCalculation* LatencyCalculationCallback;
} DMF_CONFIG_SpiTarget;
````
Member | Description
----|----
ResourceIndex | Indicates the index of SPI resource that the Client wants to access.
LatencyCalculationCallback | Callback that allows the Client to measure performance. Do not enable in a production driver as this is for debug purposes only.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

##### SpiTarget_LatencyCalculation_Message
````
typedef enum
{
  SpiTarget_LatencyCalculation_Message_Invalid,
  SpiTarget_LatencyCalculation_Message_Start,
  SpiTarget_LatencyCalculation_Message_End
} SpiTarget_LatencyCalculation_Message;
````
Member | Description
----|----
SpiTarget_LatencyCalculation_Message_Start | Start timing calculation.
SpiTarget_LatencyCalculation_Message_End | End timing calculation.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_DMF_SpiTarget_LatencyCalculation
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_SpiTarget_LatencyCalculation(
    _In_ DMFMODULE DmfModule,
    _In_ SpiTarget_LatencyCalculation_Message LatencyCalculationMessage,
    _In_ VOID* Buffer,
    _In_ ULONG BufferSize
    );
````

Enables a Client to perform performance calculations. This is for debug purposes only.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpiTarget Module handle.
LatencyCalculationMessage | Indicates whether the performance calculation should start or end.
Buffer | The data transferred (read) via SPI is written at this address.
BufferSize | The size in bytes of Buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_SpiTarget_IsResourceAssigned

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_SpiTarget_IsResourceAssigned(
    _In_ DMFMODULE DmfModule,
    _Out_ BOOLEAN* SpiConnectionAssigned
    );
````

Allows the Client to determine if the requested resource was assigned.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpiTarget Module handle.
SpiConnectionAssigned | TRUE if the requested resource was found and assigned.

##### Remarks

* Use this Method to allow a single driver that supports different resources to run on multiple platforms.

##### DMF_SpiTarget_Write

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpiTarget_Write(
  _In_ DMFMODULE DmfModule,
  _Inout_updates_(BufferLength) UCHAR* Buffer,
  _In_ ULONG BufferLength,
  _In_ ULONG TimeoutMilliseconds
  );
````

Writes a buffer via the SPI bus.

##### Returns

NTSTATUS indicating the success or failure of the write.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpiTarget Module handle.
Buffer | The data transferred (written) via SPI is located at this address.
BufferLength | The size in bytes of Buffer.
TimeoutMilliseconds | Indicates that the Write transaction should fail after TimeoutMs milliseconds. Set to zero to indicate there is no timeout.

##### Remarks

##### DMF_SpiTarget_WriteRead

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SpiTarget_WriteRead(
  _In_ DMFMODULE DmfModule,
  _Inout_reads_(OutDataLength) UCHAR* OutData,
  _In_ ULONG OutDataLength,
  _In_updates_(InDataLength) UCHAR* InData,
  _In_ ULONG InDataLength,
  _In_ ULONG TimeoutMilliseconds
  );
````

Performs a write-read transaction.

##### Returns

NTSTATUS indicating the success or failure of the transaction.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SpiTarget Module handle.
OutData | The data to be written.
AddressLength | The size in bytes of OutData.
InData | The data that is read.
BufferLength | The size in bytes of InData.
TimeoutMilliseconds | Indicates that the Read transaction should fail after TimeoutMilliseconds milliseconds. Set to zero to indicate there is no timeout.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* The code that writes to SPI bus is not necessarily standard. It is written for a specific controller.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Targets

-----------------------------------------------------------------------------------------------------------------------------------

