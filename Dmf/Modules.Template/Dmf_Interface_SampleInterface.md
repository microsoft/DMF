## DMF_Interface_SampleInterface

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Summary

-----------------------------------------------------------------------------------------------------------------------------------

Sample Interface to demonstrate how DMF Interfaces are created and used.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Bind Time Data

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_INTERFACE_PROTOCOL_SampleInterface_BIND_DATA
````
typedef struct _DMF_INTERFACE_PROTOCOL_SampleInterface_BIND_DATA
{
    // Stores this Protocol's Id.
    //
    ULONG ProtocolId;
} DMF_INTERFACE_PROTOCOL_SampleInterface_BIND_DATA;

````

##### Remarks

Data provided by the Protocol to the Transport during Bind Operation.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_INTERFACE_TRANSPORT_SampleInterface_BIND_DATA
````
typedef struct _DMF_INTERFACE_TRANSPORT_SampleInterface_BIND_DATA
{
    // Stores this Transport's Id.
    //
    ULONG TransportId;
} DMF_INTERFACE_TRANSPORT_SampleInterface_BIND_DATA;

````

##### Remarks

Data provided by the Transport to the Protocol during Bind Operation.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_INTERFACE_SampleInterface_ProtocolCallback1
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_SampleInterface_ProtocolCallback1(
    _In_ DMFINTERFACE DmfInterface
    );
    
````

A Sample Callback that should be implemented by the Protocol Module.

##### Returns

VOID.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Dmf_Interface_SampleInterface Interface handle.

##### Remarks

* This callback is called by the Transport Module.

-----------------------------------------------------------------------------------------------------------------------------------

#### Protocol Module's Declaration Data

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA
````
typedef struct _DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA
{
    // The Protocol Interface Descriptor. 
    // Every Interface must have this as the first member of its Protocol Declaration Data.
    //
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR DmfProtocolDescriptor;
    // Stores callbacks implemented by this Interface Protocol.
    //
    EVT_DMF_INTERFACE_SampleInterface_ProtocolCallback1* EVT_SampleInterface_ProtocolCallback1;
} DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA;

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_PROTOCOL_SampleInterface_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA* ProtocolDeclarationData,
    _In_ EVT_DMF_INTERFACE_ProtocolBind* EvtProtocolBind,
    _In_ EVT_DMF_INTERFACE_ProtocolUnbind* EvtProtocolUnbind,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ EVT_DMF_INTERFACE_SampleInterface_ProtocolCallback1* EvtSampleInterfaceProtocolCallback1
    )

````

##### Remarks

Data provided by the Protocol Module to indicate that it implements the Dmf_Interface_SampleInterface.
The DMF_INTERFACE_PROTOCOL_SampleInterface_DESCRIPTOR_INIT is used by the Protocol Module to initialize the Declaration Data.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Methods

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_INTERFACE_SampleInterface_TransportBind
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SampleInterface_TransportBind(
    _In_ DMFINTERFACE DmfInterface,
    _In_ DMF_INTERFACE_PROTOCOL_SampleInterface_BIND_DATA* ProtocolBindData,
    _Out_ DMF_INTERFACE_TRANSPORT_SampleInterface_BIND_DATA* TransportBindData
    )
    
````
The Bind method that should be implemented by the Transport Module.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Dmf_Interface_SampleInterface Interface handle.
ProtocolBindData | Bind Data provided by the Protocol Module for the Transport Module.
TransportBindData | Bind Data provided by the Transport Module for the Protocol Module.

##### Remarks

* This method is called by the Protocol Module during the Bind operation.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_INTERFACE_SampleInterface_TransportUnbind
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
DMF_INTERFACE_SampleInterface_TransportUnbind(
    _In_ DMFINTERFACE DmfInterface
    )
    
````
The Unbind method that should be implemented by the Transport Module.

##### Returns

VOID.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Dmf_Interface_SampleInterface Interface handle.

##### Remarks

* This method is called by the Protocol Module during the Unbind operation.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_INTERFACE_SampleInterface_TransportMethod1
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
DMF_INTERFACE_SampleInterface_TransportMethod1(
    _In_ DMFINTERFACE DmfInterface
    )
    
````
A sample TransportMethod1 implemented by the Transport Module.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfInterface | The Dmf_Interface_SampleInterface Interface handle.

##### Remarks

* This is a sample method called by the Protocol Module.
-----------------------------------------------------------------------------------------------------------------------------------

#### Transport Module's Declaration Data

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA
````
typedef struct _DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA
{
    // The Transport Interface Descriptor.
    // Every Interface must have this as the first member of its Transport Declaration Data.
    //
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR DmfTransportDescriptor;
    // Stores methods implemented by this Interface Transport.
    //
    DMF_INTERFACE_SampleInterface_TransportBind* DMF_SampleInterface_TransportBind;
    DMF_INTERFACE_SampleInterface_TransportUnbind* DMF_SampleInterface_TransportUnbind;
    DMF_INTERFACE_SampleInterface_TransportMethod1* DMF_SampleInterface_TransportMethod1;
} DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA;

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_INTERFACE_TRANSPORT_SampleInterface_DESCRIPTOR_INIT(
    _Out_ DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA* TransportDeclarationData,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind,
    _In_ DMF_INTERFACE_SampleInterface_TransportBind* SampleInterfaceTransportBind,
    _In_ DMF_INTERFACE_SampleInterface_TransportUnbind* SampleInterfaceTransportUnbind,
    _In_ DMF_INTERFACE_SampleInterface_TransportMethod1* SampleInterfaceTransportMethod1
    )

````

##### Remarks

Data provided by the Transport Module to indicate that it implements the Dmf_Interface_SampleInterface. 
The DMF_INTERFACE_TRANSPORT_SampleInterface_DESCRIPTOR_INIT is used by the Transport Module to initialize the Declaration Data.

-----------------------------------------------------------------------------------------------------------------------------------

##### Methods exposed to Protocol.
````
DMF_INTERFACE_SampleInterface_TransportBind DMF_SampleInterface_TransportBind;
DMF_INTERFACE_SampleInterface_TransportUnbind DMF_SampleInterface_TransportUnbind;
DMF_INTERFACE_SampleInterface_TransportMethod1 DMF_SampleInterface_TransportMethod1;

````

##### Callbacks exposed to Transport.
````
EVT_DMF_INTERFACE_SampleInterface_ProtocolCallback1 EVT_SampleInterface_ProtocolCallback1;

````

##### Required Macro to be included at the end of the Interface.

````
// This macro defines the SampleInterfaceProtocolDeclarationDataGet and SampleInterfaceTransportDeclarationDataGet.
// Call this macro after the DMF_INTERFACE_PROTOCOL_SampleInterface_DECLARATION_DATA and
// DMF_INTERFACE_TRANSPORT_SampleInterface_DECLARATION_DATA are defined.
//
DECLARE_DMF_INTERFACE(SampleInterface);

````

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Remarks

* Detailed explanation about using the Interface that Protocol and Transport Modules need to consider.

-----------------------------------------------------------------------------------------------------------------------------------

#### Interface Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* Dmf_SampleInterfaceTransport1
* Dmf_SampleInterfaceTransport2
* Dmf_SampleInterfaceProtocol1

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

* List possible future work for this Interface.

-----------------------------------------------------------------------------------------------------------------------------------
#### Interface Category

-----------------------------------------------------------------------------------------------------------------------------------

Template

-----------------------------------------------------------------------------------------------------------------------------------

