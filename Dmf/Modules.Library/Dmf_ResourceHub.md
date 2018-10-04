## DMF_ResourceHub

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Provides support for extracting data from a Resource Hub and processing SPB IOCTL requests.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_ResourceHub
````
typedef struct
{
  // Target bus type.
  // TODO: Currently only I2C is supported.
  //
  DIRECTFW_SERIAL_BUS_TYPE TargetBusType;
  // Callback to get Transfer List from Spb.
  //
  EVT_DMF_ResourceHub_DispatchTransferList* EvtResourceHubDispatchTransferList;
} DMF_CONFIG_ResourceHub;
````

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### DIRECTFW_SERIAL_BUS_TYPE
````
typedef enum
{
  Reserved = 0,
  I2C,
  SPI,
  UART
} DIRECTFW_SERIAL_BUS_TYPE;
````
Member | Description
----|----
I2C | I2C bus.
Spi | SPI bus.
UART | UART bus.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_ResourceHub_DispatchTransferList
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_ResourceHub_DispatchTransferList(
    _In_ DMFMODULE DmfModule,
    _In_ SPB_TRANSFER_LIST* SpbTransferListBuffer,
    _In_ size_t SpbTransferListBufferSize,
    _In_ USHORT I2CSecondaryDeviceAddress,
    _Out_ size_t *TotalTransferLength
    );
````

Callback to the Client when data is transferred via the Resource Hub.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ResourceHub Module handle.
SpbTransferListBuffer | The SPB transfer list. The Client parses this list and performs actions based on the data in that list.
SpbTransferListBufferSize | The size in bytes of the buffer pointed to by SpbTransferListBuffer.
I2CSecondaryDeviceAddress | I2C secondaryDevice address of the OpRegion that processed the SPB request.
TotalTransferLength | Allows the Client to return the number of bytes processed by the Client to the issuer of the associated SPB request.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

##### IOCTL_SPB_EXECUTE_SEQUENCE

Minimum Input Buffer Size: sizeof(SPB_TRANSFER_LIST)

Minimum Output Buffer Size: 0

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_IoctlHandler

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

* Add support for non-I2c buses.

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

