## DMF_ComponentFirmwareUpdate

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module abstracts Component Firmware Update (CFU) Protocol. It allows the Client to run the protocol without knowing the 
internal details. Clients can customize the behavior of the module through the configuration options.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_ComponentFirmwareUpdate
````
// Configuration of the module
//
typedef struct
{
    // Transport Config.
    //
    ComponentFirmwareUpdate_TRANSPORT_CONFIG TransportConfig;

    //-----START: Firmware binary related ---------
    //
    // Number of Firmware binary pairs that this component needs to work with.
    //
    DWORD NumberOfFirmwareComponents;

    // ComponentFirmwareUpdate callback function to be implemented by client to provide the firmware offer bits.
    //
    EVT_DMF_ComponentFirmwareUpdate_FirmwareGet* EvtComponentFirmwareUpdateFirmwareOfferGet;

    // ComponentFirmwareUpdate callback function to be implemented by client to provide the firmware payload bits.
    //
    EVT_DMF_ComponentFirmwareUpdate_FirmwareGet* EvtComponentFirmwareUpdateFirmwarePayloadGet;

    // Firmware Buffer Attribute to control whether this Module maintains local copy of the firmware buffers internally or not.
    //
    BOOLEAN FirmwareBuffersNotInPresistantMemory;
    //-----END: Firmware binary related ---------
    //

    //---- START: CFU protocol related -------
    //
    // Does this component support resuming from a previously interrupted update?.
    //
    BOOLEAN SupportResumeOnConnect;

    // Does this configuration support skipping the entire protocol transaction for an already known all up-to-date Firmware?
    //
    BOOLEAN SupportProtocolTransactionSkipOptimization;

    // Request "a force immediate reset" during offer stage?
    //
    BOOLEAN ForceImmediateReset;

    // Request "a force ignoring version" during offer stage?
    //
    BOOLEAN ForceIgnoreVersion;

    //----- END:  CFU protocol related -------
    //

    //---- START: Book keeping related  ------- (Optional)
    // Module updates registry with status information about the Firmware Update protocol stages.
    // If the below Identifier string is provided, Registry NameValue will be prefixed with this string.
    // This helps in external tools to distinguish status information for different instances under a device hardware key.
    //
    WCHAR InstanceIdentifier[MAX_INSTANCE_IDENTIFIER_LENGTH];

    // Should be 0 is client is not providing the string.
    //
    USHORT InstanceIdentifierLength;        // number of characters, excluding the terminal NULL.
    //
    //---- END: Book keeping related  -------
    //

} DMF_CONFIG_ComponentFirmwareUpdate;
````
Member | Description
----|----
TransportConfig | Transport Configuration for this module that the Client wants to associate with this module.
NumberOfFirmwareComponents | Number of Firmware binary (Offer,Payload) pairs that this module needs to work with.
EvtComponentFirmwareUpdateFirmwareOfferGet | Clients should specify this callback function to provide each firmware offer bits to this module. 
EvtComponentFirmwareUpdateFirmwarePayloadGet | Clients should specify this callback function to provide each firmware payload bits to this module.
FirmwareBuffersNotInPresistantMemory | Clients use this to indicate whether the firmware bits are persisting or not.
SupportResumeOnConnect | Client can use this to indicate whether this module should support 'Resume from an interrupted firmware download' or not.
SupportProtocolTransactionSkipOptimization | Client can use this to indicate whether this module should support 'Skipping the CFU transaction entirely for a previous known up-to-date firmware state' or not.
ForceImmediateReset | Client can use this to indicate whether to request "a force immediate reset" during offer stage or not.
ForceIgnoreVersion | Client can use this to indicate whether to request "a force ignoring version" during offer stage or not.
InstanceIdentifier | Client can provide an optional Instance Identifier string that this module can make use while storing book keeping entries.
InstanceIdentifierLength | Number of characters in the InstanceIdentifier above.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_ComponentFirmwareUpdate_FirmwareGet
````
typedef
_Function_class_(EVT_DMF_ComponentFirmwareUpdate_FirmwareGet)
_Must_inspect_result_
NTSTATUS
EVT_DMF_ComponentFirmwareUpdate_FirmwareGet(_In_ DMFMODULE DmfModule,
                                            _In_ DWORD FirmwareComponentIndex,
                                            _Out_ BYTE** FirmwareBuffer,
                                            _Out_ size_t* BufferLength);
````

Client specific callback that allows client to provide the firmware bits.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An opened DMF_ComponentFirmwareUpdate Module handle.
FirmwareComponentIndex | Index of the firmware component for this the firmware bits are requested by this module.
FirmwareBuffer | The buffer the Client populates.
BufferLength | The size of Buffer in bytes.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_ComponentFirmwareUpdate_Start

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_Start(
    _In_ DMFMODULE DmfModule
    );
````

Allows the Client to start the Component Firmware Update Protocol Transaction.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An opened DMF_ComponentFirmwareUpdate Module handle.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_ComponentFirmwareUpdate_Stop

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ComponentFirmwareUpdate_Stop(
    _In_ DMFMODULE DmfModule
    );
````

Allows the Client to stop the Component Firmware Update Protocol Transaction.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An opened DMF_ComponentFirmwareUpdate Module handle.


-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

DMF_ComponentFirmwareUpdateHidTransport

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Protocols

-----------------------------------------------------------------------------------------------------------------------------------

