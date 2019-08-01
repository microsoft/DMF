## DMF_CmApi

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Gives access to the CM_* API. This API gives access to device and device interface configuration information. Client registers for 
arrival/removal of instances of a given device interface GUID. The Client can then call Methods to obtain information about or
configure that instance. This Module also contains other Methods that use CM API.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_CmApi
````
typedef struct
{
    // Device Interface Guid.
    //
    GUID DeviceInterfaceGuid;
    // Callback to get device information.
    //
    EVT_DMF_CmApi_DeviceInterfaceList* CmApi_Callback_DeviceInterfaceList;
} DMF_CONFIG_CmApi;
````
Member | Description
----|----
DeviceInterfaceGuid: | This is the device interface GUID for which an arrival/removal notification is registered (optional).
CmApi_Callback_DeviceInterfaceList: | Client callback that executes when the Device Interface associated with DeviceInterfaceGuid arrives or is removed (optional).

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_CmApi_DeviceInterfaceList

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID 
EVT_DMF_CmApi_DeviceInterfaceList(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* DeviceInterfaceList,
    _In_ GUID DeviceInterfaceGuid
    );
````

This callback executes any time an instance of the given device interface appears or disappears.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CmApi Module handle.
DeviceInterfaceList:  | A string with a list of all the instances of a device interfaces of type DeviceInterfaceGuid.
DeviceInterfaceGuid: | The given device interface which was previously registered.

##### Remarks

*

-----------------------------------------------------------------------------------------------------------------------------------

##### EVT_DMF_CmApi_ParentTargetSymbolicLinkName

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN 
EVT_DMF_CmApi_ParentTargetSymbolicLinkName(_In_ DMFMODULE DmfModule,
                                           _In_ ULONG InterfaceIndex,
                                           _In_ WCHAR* InterfaceName,
                                           _In_ UNICODE_STRING* SymbolicLinkName,
                                           _In_ VOID* ClientContext);
````

This callback is called for every symbolic link instance found for the parent PDO.

##### Returns

TRUE indicates Client wants to continue enumeration.
FALSE indicates Client wants to stop enumeration.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CmApi Module handle.
InterfaceIndex | The index of the enumerated interface.
InterfaceName | The name of the enumerated interface.
SymbolicLinkName | The symbolic link name of the enumerated interface.
ClientContext | Caller's private context.

##### Remarks

* The Client may create and open the associated WDFIOTARGET using the information provided by this callback.
* The Client can receive the WDFIOTARGET handle using ClientContext.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_CmApi_DeviceInstanceIdAndHardwareIdsGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_DeviceInstanceIdAndHardwareIdsGet(
    _In_ DMFMODULE DmfModule,
    _In_ PWSTR DeviceInterface,
    _Out_ PWCHAR DeviceInstanceId,
    _In_ UINT32 DeviceInstanceIdSize,
    _Out_ PWCHAR DeviceHardwareIds,
    _In_ UINT32 DeviceHardwareIdsSize
    );
````

Given the Id of an instance of a given device interface, return the list of hardware ids associated with
with that instance.

##### Returns

NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CmApi Module handle.
DeviceInterface  | A string with a list of all the instances of a device interfaces of type DeviceInterfaceGuid.
DeviceInstanceId | The given device interface which was previously registered.
DeviceInterfaceIdSize | The size in characters of DeviceInterfaceId.
DeviceHardwareIds | The list of HWID (hardware identifier) associated with DeviceInstanceId.
DeviceHardwareIdsSize | The size in characters of DeviceHardwareIds.

##### Remarks

*

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_CmApi_DevNodeStatusAndProblemCodeGet

````
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_CmApi_DevNodeStatusAndProblemCodeGet(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* DeviceInstanceId,
    _Out_ UINT32* DevNodeStatus,
    _Out_ UINT32* ProblemCode
    );
````

Given an instance of a device interface, get its Device Node Status and Problem Code.

##### Returns

TRUE if the Problem Code and DevNode Status were read. FALSE, otherwise.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CmApi Module handle.
DeviceInstanceId | InstanceId string of the given instance.
DevNodeStatus | Returned Status of the device node.
ProblemCode | Returned Problem code for the device node.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DMF_CmApi_ParentTargetCloseAndDestroy

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_CmApi_ParentTargetCloseAndDestroy(
    _In_ DMFMODULE DmfModule,
    _In_ WDFIOTARGET ParentWdfIoTarget
    )
````

Closes and destroys a given WDFIOTARGET.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CmApi Module handle.
ParentWdfIoTarget | The WDFIOTARGET to close and destroy.

##### Remarks

* This Method is the counterpart of `DMF_CmApi_ParentTargetCreateAndOpen`.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_CmApi_ParentTargetCreateAndOpen

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_ParentTargetCreateAndOpen(
    _In_ DMFMODULE DmfModule,
    _Out_ WDFIOTARGET* ParentWdfIoTarget
    );
````

Finds the a parent device and creates/opens its associated WDFIOTARGET.

##### Returns

STATUS_SUCCESS if the parent is found and its associated WDFIIOTARGET is created and opened.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CmApi Module handle.
ParentWdfIoTarget | The returned WDFIOTARGET of the parent ready for use.

##### Remarks

* The returned WFIOTARGET must be closed and destroyed before the driver unloads.
* It is not necessary to populate this Module's Config to use this Method.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_CmApi_ParentTargetInterfacesEnumerate

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_ParentTargetInterfacesEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_CmApi_ParentTargetSymbolicLinkName ParentTargetCallback,
    _In_ GUID* GuidDevicePropertyInterface,
    _Inout_ VOID* ClientContext
    );
````

Enumerates all the interfaces associated with the Parent PDO.

##### Returns

* STATUS_SUCCESS - Enumeration was successful; otherwise, enumeration could not be performed.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CmApi Module handle.
ParentTargetCallback | Callback to Client for each interface instance enumerated.
GuidDevicePropertyInterface | The device interface GUID to find.
ClientContext | Client context passed to ParentTargetCallback.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_CmApi_PropertyUint32Get

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_PropertyUint32Get(
    _In_ DMFMODULE DmfModule,
    _In_ GUID* PropertyInterfaceGuid,
    _In_ PDEVPROPKEY PropertyKey,
    _Out_ UINT32* Value
    );
````

Given a device interface GUID, return the UINT32 value of the specified device property key.

##### Returns

* STATUS_SUCCESS - on a successfull query, and a CONFIGRET error converted to an NTSTATUS code on failure.


##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CmApi Module handle.
PropertyInterfaceGuid | The GUID specifying the device interface to query.
PropertyKey | The property key to query the value of.
Value | The UINT32 value of this property if found.

##### Remarks

* None

-----------------------------------------------------------------------------------------------------------------------------------


#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* 

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* Write a Method that returns a list that returns a list or count of interface instances.

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

