## DMF_SerialTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module gives the Client access to a Serial device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_SerialTarget
````
typedef struct
{
  // Serial Io device configuration parameters.
  //
  EVT_DMF_SerialTarget_CustomConfiguration* EvtSerialTargetCustomConfiguration;
  // Open in Read or Write mode.
  //
  ULONG OpenMode;
  // Share Access.
  //
  ULONG ShareAccess;
  // Child Request Stream Module.
  //
  DMF_CONFIG_ContinuousRequestTarget ContinuousRequestTargetModuleConfig;
} DMF_CONFIG_SerialTarget;
````
Member | Description
----|----
EvtSerialTargetCustomConfiguration | Callback that allows the Client to perform specialized configuration of the serial device.
OpenMode | See WDK documentation.
ShareAccess | See WDK documentation.
ContinuousRequestTargetModuleConfig | Allows the Client to configure the underlying Child DMF_ContinuousRequestTarget Module to send and receive data from the Serial device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### SerialStream_ConfigurationParameters_Flags
````
typedef enum
{
  SerialBaudRateFlag        = 0x0001,
  SerialLineControlFlag       = 0x0002,
  SerialCharsFlag          = 0x0004,
  SerialTimeoutsFlag        = 0x0008,
  SerialQueueSizeFlag        = 0x0010,
  SerialHandflowFlag        = 0x0020,
  SerialWaitMaskFlag        = 0x0040,
  SerialClearRtsFlag        = 0x0080,
  SerialClearDtrFlag        = 0x0100,
  ConfigurationParametersValidFlags = SerialBaudRateFlag |
                    SerialLineControlFlag |
                    SerialCharsFlag |
                    SerialTimeoutsFlag |
                    SerialQueueSizeFlag |
                    SerialHandflowFlag |
                    SerialWaitMaskFlag |
                    SerialClearRtsFlag |
                    SerialClearDtrFlag
} SerialStream_ConfigurationParameters_Flags;
````

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------
##### SerialTarget_Configuration
````
typedef struct
{
  ULONG Flags;
  SERIAL_BAUD_RATE BaudRate;
  SERIAL_LINE_CONTROL SerialLineControl;
  SERIAL_CHARS SerialChars;
  SERIAL_TIMEOUTS SerialTimeouts;
  SERIAL_QUEUE_SIZE QueueSize;
  SERIAL_HANDFLOW SerialHandflow;
  ULONG SerialWaitMask;
} SerialTarget_Configuration;
````

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_SerialTarget_CustomConfiguration
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_SerialTarget_CustomConfiguration(
    _In_ DMFMODULE DmfModule,
    _Out_ SerialTarget_Configuration* ConfigurationParameters
    );
````

Generally, this Module applies a
default configuration retrieved from the Resource Hub. The Client then uses this callback to update the any of the Serial
configuration parameters to suit the Client's specific needs.

##### Returns

TRUE if Client wants to update any of the Serial configuration parameters. FALSE otherwise.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SerialTarget Module handle.
ConfigurationParameters | The Serial configuration parameters specified by the Client on return.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SerialTarget_BufferPut

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SerialTarget_BufferPut(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* ClientBuffer
  );
````

This Method adds a given DMF_BufferPool buffer back into this Module's Output DMF_BufferPool.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SerialTarget Module handle.
ClientBuffer | The given DMF_BufferPool buffer.

##### Remarks

* NOTE: ClientBuffer must be a properly formed DMF_BufferPool buffer.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SerialTarget_IoTargetGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SerialTarget_IoTargetGet(
  _In_ DMFMODULE DmfModule,
  _In_ WDFIOTARGET* IoTarget
  );
````

Gets the internal WDFIOTARGET that receives requests from an instance of this Module.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SerialTarget Module handle.
IoTarget | The address where the underlying WDFIOTARGET returned to the Client.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SerialTarget_Send

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SerialTarget_Send(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
  _In_ size_t ResponseLength,
  _In_ SerialTarget_RequestType RequestType,
  _In_ ULONG RequestIoctl,
  _In_ ULONG RequestTimeoutMilliseconds,
  _In_opt_ EVT_DMF_SerialTarget_SingleAsynchronousBufferOutput* EvtSerialTargetSingleAsynchronousRequest,
  _In_opt_ VOID* SingleAsynchronousRequestClientContext
  );
````

This Method uses the given parameters to create a Request and send it asynchronously to the Module's underlying WDFIOTARGET.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SerialTarget Module handle.
RequestBuffer | The Client buffer that is sent to this Module's underlying WDFIOTARGET.
RequestLength | The size in bytes of RequestBuffer.
ResponseBuffer | The Client buffer that receives data from this Module's underlying WDFIOTARGET.
ResponseLength | The size in bytes of ResponseBuffer.
RequestType | The type of Request to send to this Module's underlying WDFIOTARGET.
RequestIoctl | The IOCTL that tells the Module's underlying WDFIOTARGET the purpose of the associated Request that is sent.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
EvtSerialTargetSingleAsynchronousRequest | The Client callback that is called when this Module's underlying WDFIOTARGET completes the request.
SingleAsynchronousRequestClientContext | The Client specific context that is sent to EvtSerialTargetSingleAsynchronousRequest.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SerialTarget_SendSynchronously

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SerialTarget_SendSynchronously(
  _In_ DMFMODULE DmfModule,
  _In_reads_bytes_(RequestLength) VOID* RequestBuffer,
  _In_ size_t RequestLength,
  _Out_writes_bytes_(ResponseLength) VOID* ResponseBuffer,
  _In_ size_t ResponseLength,
  _In_ SerialTarget_RequestType RequestType,
  _In_ ULONG RequestIoctl,
  _In_ ULONG RequestTimeoutMilliseconds,
  _Out_opt_ size_t* BytesWritten
  );
````

This Method uses the given parameters to create a Request and send it synchronously to the Module's underlying WDFIOTARGET.

##### Returns

NTSTATUS. Fails if the Request cannot be sent to the Modules internal WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SerialTarget Module handle.
RequestBuffer | The Client buffer that is sent to this Module's underlying WDFIOTARGET.
RequestLength | The size in bytes of RequestBuffer.
ResponseBuffer | The Client buffer that receives data from this Module's underlying WDFIOTARGET.
ResponseLength | The size in bytes of ResponseBuffer.
RequestType | The type of Request to send to this Module's underlying WDFIOTARGET.
RequestIoctl | The IOCTL that tells the Module's underlying WDFIOTARGET the purpose of the associated Request that is sent.
RequestTimeoutMilliseconds | A time in milliseconds that causes the call to timeout if it is not completed in that time period. Use zero for no timeout.
BytesWritten | The number of bytes transferred to/from the underlying WDFIOTARGET.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SerialTarget_Start

````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DMF_SerialTarget_Start(
  _In_ DMFMODULE DmfModule
  );
````

Starts streaming (causes the created Requests and associated buffers to be sent to the underlying WDFIOTARGET).

##### Returns

NTSTATUS. Fails if Requests cannot be sent to the underlying WDFIOTARGET.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SerialTarget Module handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_SerialTarget_Stop

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_SerialTarget_Stop(
  _In_ DMFMODULE DmfModule
  );
````

Stops streaming. The pending requests are canceled and no new Requests will be sent to the underlying WDFIOTARGET.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SerialTarget Module handle.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* [DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM] Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE. Clients that select any type of paged pool as PoolType must set DMF_MODULE_ATTRIBUTES.PassiveLevel = TRUE.

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

