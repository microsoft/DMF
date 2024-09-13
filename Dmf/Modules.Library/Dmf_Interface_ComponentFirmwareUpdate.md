## DMF_Interface_ComponentFirmwareUpdate

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Summary

-----------------------------------------------------------------------------------------------------------------------------------

Defines Interfaces for all Component Firmware Update Transports.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Bind Time Data

##### DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_BIND_DATA
````
typedef struct _DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_BIND_DATA
{
} DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_BIND_DATA;

````

##### Remarks

Data provided by the Protocol to the Transport during Bind Operation.

##### DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_BIND_DATA
````
typedef struct _DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_BIND_DATA
{
    // Wait Time out in Ms for response from transport.
    //
    ULONG TransportWaitTimeout;
    // Size of TransportHeader in bytes.
    // The protocol Module will allocate header block at the beginning of the buffer for to transport to use.
    //
    ULONG TransportHeaderSize;
    // Required size of Firmware Payload Buffer this transport needs (excluding the TransportHeaderSize above).
    //
    ULONG TransportFirmwarePayloadBufferRequiredSize;
    // Required size of Offer Buffer this transport needs (excluding the TransportHeaderSize above).
    //
    ULONG TransportOfferBufferRequiredSize;
    // Required size of FirmwareVersion Buffer this transport needs (excluding the TransportHeaderSize above).
    //
    ULONG TransportFirmwareVersionBufferRequiredSize;
    // Payload buffer fill alignment this transport needs.
    //
    UINT TransportPayloadFillAlignment;
} DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_BIND_DATA;

````

##### Remarks

Data provided by the Transport to the Protocol during Bind Operation.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Callbacks

##### EVT_ComponentFirmwareUpdate_FirmwareVersionResponse
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_ComponentFirmwareUpdate_FirmwareVersionResponse(
    _In_ DMFINTERFACE DmfInterface,
    _In_reads_bytes_(FirmwareVersionBufferSize) UCHAR* FirmwareVersionBuffer,
    _In_ size_t FirmwareVersionBufferSize,
    _In_ NTSTATUS ntStatus
    )
    
````

Retrieves the firmware versions from the device.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
FirmwareVersionsBuffer | Buffer with firmware information.
FirmwareVersionsBufferSize | size of the above in bytes.
ntStatus | NTSTATUS for the FirmwareVersionGet.

##### Remarks

* This callback is called by the Transport Module.

##### EVT_ComponentFirmwareUpdate_OfferResponse
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_ComponentFirmwareUpdate_OfferResponse(
    _In_ DMFINTERFACE DmfInterface,
    _In_reads_bytes_(ResponseBufferSize) UCHAR* ResponseBuffer,
    _In_ size_t ResponseBufferSize,
    _In_ NTSTATUS ntStatus
    )
    
````

    Callback to indicate the response to an offer that was sent to device.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
ResponseBuffer | Buffer with response information.
ResponseBufferSize | size of the above in bytes.
ntStatusCallback | NTSTATUS for the command that was sent down.

##### Remarks

* This callback is called by the Transport Module.

##### EVT_ComponentFirmwareUpdate_PayloadResponse
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_ComponentFirmwareUpdate_PayloadResponse(
    _In_ DMFINTERFACE DmfInterface,
    _In_reads_bytes_(ResponseBufferSize) UCHAR* ResponseBuffer,
    _In_ size_t ResponseBufferSize,
    _In_ NTSTATUS ntStatus
    )
    
````

    Callback to indicate the response to a payload that was sent to device.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
ResponseBuffer | Buffer with response information.
ResponseBufferSize | size of the above in bytes.
ntStatus | NTSTATUS for the command that was sent down.

##### Remarks

* This callback is called by the Transport Module.

-----------------------------------------------------------------------------------------------------------------------------------

#### Protocol Module's Declaration Data

##### DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA
````
typedef struct _DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA
{
    // The Protocol Interface Descriptor. 
    // Every Interface must have this as the first member of its Protocol Declaration Data.
    //
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR DmfProtocolDescriptor;
    // Stores callbacks implemented by this Interface Protocol.
    //
    EVT_DMF_INTERFACE_ComponentFirmwareUpdate_FirmwareVersionResponse* EVT_ComponentFirmwareUpdate_FirmwareVersionResponse;
    EVT_DMF_INTERFACE_ComponentFirmwareUpdate_OfferResponse* EVT_ComponentFirmwareUpdate_OfferResponse;
    EVT_DMF_INTERFACE_ComponentFirmwareUpdate_PayloadResponse* EVT_ComponentFirmwareUpdate_PayloadResponse;
} DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA;

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA* ProtocolDeclarationData,
    _In_ EVT_DMF_INTERFACE_ProtocolBind* EvtProtocolBind,
    _In_ EVT_DMF_INTERFACE_ProtocolUnbind* EvtProtocolUnbind,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ EVT_DMF_INTERFACE_ComponentFirmwareUpdate_FirmwareVersionResponse* EvtComponentFirmwareUpdate_FirmwareVersionResponse,
    _In_ EVT_DMF_INTERFACE_ComponentFirmwareUpdate_OfferResponse* EvtComponentFirmwareUpdate_OfferResponse,
    _In_ EVT_DMF_INTERFACE_ComponentFirmwareUpdate_PayloadResponse* EvtComponentFirmwareUpdate_PayloadResponse
    )
````

##### Remarks

Data provided by the Protocol Module to indicate that it implements the Dmf_Interface_ComponentFirmwareUpdate.
The DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DESCRIPTOR_INIT is used by the Protocol Module to initialize the Declaration Data.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Methods

##### DMF_INTERFACE_ComponentFirmwareUpdate_TransportBind
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_INTERFACE_ComponentFirmwareUpdate_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_BIND_DATA* TransportBindData
    )
    
````
The Bind method that should be implemented by the Transport Module.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfInterface | The Dmf_Interface_ComponentFirmwareUpdate Interface handle.
ProtocolBindData | Bind Data provided by the Protocol Module for the Transport Module.
TransportBindData | Bind Data provided by the Transport Module for the Protocol Module.

##### Remarks

* This method is called by the Protocol Module during the Bind operation.

##### DMF_INTERFACE_ComponentFirmwareUpdate_TransportUnbind
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_INTERFACE_ComponentFirmwareUpdate_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
    
````
The Unbind method that should be implemented by the Transport Module.

##### Returns

VOID.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Dmf_Interface_ComponentFirmwareUpdate Interface handle.

##### Remarks

* This method is called by the Protocol Module during the Unbind operation.

##### DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet(
    _In_ DMFINTERFACE DmfInterface
    )
    
````
    Retrieves the firmware versions from the device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.

##### Remarks

* This is a sample method called by the Protocol Module.

##### DMF_ComponentFirmwareUpdate_TransportOfferInformationSend
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportOfferInformationSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
    
````
    Sends Offer Information to the device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
Buffer | Header, followed by Offer Information to Send.
BufferSize | Size of the above in bytes.
HeaderSize | Size of the header. Header is at the begining of 'Buffer'.

##### Remarks

* This is a sample method called by the Protocol Module.

##### DMF_ComponentFirmwareUpdate_TransportOfferCommandSend
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportOfferCommandSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
    
````
    Sends Offer Command to the device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
Buffer | Header, followed by Offer Command to Send.
BufferSize | Size of the above in bytes.
HeaderSize | Size of the header. Header is at the begining of 'Buffer'.

##### Remarks

* This is a sample method called by the Protocol Module.

##### DMF_ComponentFirmwareUpdate_TransportOfferSend
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportOfferSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
    
````
    Sends an Offer to the device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
Buffer | Header, followed by Offer to Send.
BufferSize | Size of the above in bytes.
HeaderSize | Size of the header. Header is at the begining of 'Buffer'.

##### Remarks

* This is a sample method called by the Protocol Module.

##### DMF_ComponentFirmwareUpdate_TransportPayloadSend
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportPayloadSend(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
    
````
    Sends a chunk of Payload to the device.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.
Buffer | Header, followed by Payload to Send.
BufferSize | Size of the above in bytes.
HeaderSize | Size of the header. Header is at the begining of 'Buffer'.

##### Remarks

* This is a sample method called by the Protocol Module.

##### DMF_ComponentFirmwareUpdate_TransportProtocolStart
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportProtocolStart(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
    
````
    Method to Setup the transport for protocol transaction.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.

##### Remarks

* This is a sample method called by the Protocol Module.

##### DMF_ComponentFirmwareUpdate_TransportProtocolStop
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_TransportProtocolStop(
    _In_ DMFINTERFACE DmfInterface,
    _Inout_updates_bytes_(BufferSize) UCHAR* Buffer,
    _In_ size_t BufferSize,
    _In_ size_t HeaderSize
    )
    
````
    Method to clean up the transport after the protocol transaction.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfInterface | The Interface handle.

##### Remarks

* This is a sample method called by the Protocol Module.

-----------------------------------------------------------------------------------------------------------------------------------

#### Transport Module's Declaration Data

##### DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA
````
typedef struct _DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA
{
    // The Transport Interface Descriptor.
    // Every Interface must have this as the first member of its Transport Declaration Data.
    //
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR DmfTransportDescriptor;
    // Stores methods implemented by this Interface Transport.
    //
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportBind* DMF_ComponentFirmwareUpdate_TransportBind;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportUnbind* DMF_ComponentFirmwareUpdate_TransportUnbind;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportBind* DMF_ComponentFirmwareUpdate_TransportBind;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportUnbind* DMF_ComponentFirmwareUpdate_TransportUnbind;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportFirmwareVersionGet* DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferInformationSend* DMF_ComponentFirmwareUpdate_TransportOfferInformationSend;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferCommandSend* DMF_ComponentFirmwareUpdate_TransportOfferCommandSend;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferSend* DMF_ComponentFirmwareUpdate_TransportOfferSend;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportPayloadSend* DMF_ComponentFirmwareUpdate_TransportPayloadSend;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStart* DMF_ComponentFirmwareUpdate_TransportProtocolStart;
    DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStop* DMF_ComponentFirmwareUpdate_TransportProtocolStop;
} DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA;

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA* TransportDeclarationData,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportBind* ComponentFirmwareUpdateTransportBind,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportUnbind* ComponentFirmwareUpdateTransportUnbind,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportFirmwareVersionGet* ComponentFirmwareUpdate_TransportFirmwareVersionGet,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferInformationSend* ComponentFirmwareUpdate_TransportOfferInformationSend,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferCommandSend* ComponentFirmwareUpdate_TransportOfferCommandSend,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferSend* ComponentFirmwareUpdate_TransportOfferSend,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportPayloadSend* ComponentFirmwareUpdate_TransportPayloadSend,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStart* ComponentFirmwareUpdate_TransportProtocolStart,
    _In_ DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStop* ComponentFirmwareUpdate_TransportProtocolStop
    )

````

##### Remarks

Data provided by the Transport Module to indicate that it implements the Dmf_Interface_ComponentFirmwareUpdate. 
The DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DESCRIPTOR_INIT is used by the Transport Module to initialize the Declaration Data.

##### Methods exposed to Protocol.
````
DMF_INTERFACE_ComponentFirmwareUpdate_TransportBind DMF_ComponentFirmwareUpdate_TransportBind;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportUnbind DMF_ComponentFirmwareUpdate_TransportUnbind;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportFirmwareVersionGet DMF_ComponentFirmwareUpdate_TransportFirmwareVersionGet;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferInformationSend DMF_ComponentFirmwareUpdate_TransportOfferInformationSend;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferCommandSend DMF_ComponentFirmwareUpdate_TransportOfferCommandSend;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportOfferSend DMF_ComponentFirmwareUpdate_TransportOfferSend;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportPayloadSend DMF_ComponentFirmwareUpdate_TransportPayloadSend;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStart DMF_ComponentFirmwareUpdate_TransportProtocolStart;
DMF_INTERFACE_ComponentFirmwareUpdate_TransportProtocolStop DMF_ComponentFirmwareUpdate_TransportProtocolStop;

````

##### Callbacks exposed to Transport.
````
EVT_DMF_INTERFACE_ComponentFirmwareUpdate_FirmwareVersionResponse EVT_ComponentFirmwareUpdate_FirmwareVersionResponse;
EVT_DMF_INTERFACE_ComponentFirmwareUpdate_OfferResponse EVT_ComponentFirmwareUpdate_OfferResponse;
EVT_DMF_INTERFACE_ComponentFirmwareUpdate_PayloadResponse EVT_ComponentFirmwareUpdate_PayloadResponse;

````

##### Required Macro to be included at the end of the Interface.

````
// This macro defines the ComponentFirmwareUpdateProtocolDeclarationDataGet and ComponentFirmwareUpdateTransportDeclarationDataGet.
// Call this macro after the DMF_INTERFACE_PROTOCOL_ComponentFirmwareUpdate_DECLARATION_DATA and
// DMF_INTERFACE_TRANSPORT_ComponentFirmwareUpdate_DECLARATION_DATA are defined.
//
DECLARE_DMF_INTERFACE(ComponentFirmwareUpdate);

````

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Remarks



-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* Dmf_ComponentFirmwareUpdateHidTransportSurface
* Dmf_ComponentFirmwareUpdateBleLcGattTransport
* Dmf_ComponentFirmwareUpdateSurface
* Dmf_ComponentFirmwareUpdateHidTransport
* Dmf_ComponentFirmwareUpdate
* SurfaceCFUOverHid driver

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

* List possible future work for this Interface.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Category

-----------------------------------------------------------------------------------------------------------------------------------

Protocols

-----------------------------------------------------------------------------------------------------------------------------------

