## DMF_IoctlHandler

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a driver pattern that allows the Client to create a table of supported IOCTLs along with information about each IOCTL
so that the Module can perform automatic validation.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_IoctlHandler
````
typedef struct
{
  // GUID of the device interface that allows User-mode access.
  //
  GUID DeviceInterfaceGuid;
  // Allows Client to filter access to IOCTLs.
  //
  IoctlHandler_AccessModeFilterType AccessModeFilter;
  // This is only set if the AccessModeFilter == IoctlHandler_AccessModeFilterClientCallback.
  //
  EVT_DMF_IoctlHandler_AccessModeFilter* EvtIoctlHandlerAccessModeFilter;
  // This is a pointer to a static table in the Client.
  //
  IoctlHandler_IoctlRecord* IoctlRecords;
  // The number of records in the above table.
  //
  ULONG IoctlRecordCount;
  // FALSE (Default) means that the corresponding device interface is created when this Module opens.
  // TRUE requires that the Client call DMF_IoctlHandler_IoctlsEnable() to enable the corresponding device interface.
  //
  BOOLEAN ManualMode;
  // FALSE (Default) means that the corresponding device interface will handle all IOCTL types.
  // TRUE means that the module allows only requests from kernel mode clients.
  //
  BOOLEAN KernelModeRequestsOnly;
  // Windows Store App access settings.
  //
  WCHAR* CustomCapabilities;
  DEVPROP_BOOLEAN IsRestricted;
  // Allows Client to perform actions after the Device Interface is created.
  //
  EVT_DMF_IoctlHandler_PostDeviceInterfaceCreate* PostDeviceInterfaceCreate;
} DMF_CONFIG_IoctlHandler;
````
Member | Description
----|----
DeviceInterfaceGuid | The GUID of the Device Interface to expose.
AccessModeFilter | Indicates what kind of access control mechanism is used, if any. Use IoctlHandler_AccessModeDefault in most cases.
EvtIoctlHandlerAccessModeFilter | A callback that allows the Client to filter the IOCTL with Client specific logic.
IoctlRecords | A table of records that specify information about each supported IOCTL.
ManualMode | Module open configuration.
KernelModeRequestsOnly | This allows the module to handle only requests from kernel mode clients.
CustomCapabilities | Windows Store App access capabilities string.
IsRestricted | If set to DEVPROP_TRUE, sets the restricts access to the Device Interface.
PostDeviceInterfaceCreate | Allows Client to perform actions after the Device Interface is created.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------
##### IoctlHandler_AccessModeFilterType
````
typedef enum
{
  // Do what WDF would normally do (allow User-mode).
  //
  IoctlHandler_AccessModeDefault,
  // Call a Client Callback function that will decide.
  //
  IoctlHandler_AccessModeFilterClientCallback,
  // NOTE: This is currently not implemented.
  //
  IoctlHandler_AccessModeFilterDoNotAllowUserMode,
  // Only allows "Run as Administrator".
  //
  IoctlHandler_AccessModeFilterAdministratorOnly,
  // Allow access to Administrator on a per-IOCTL basis.
  //
  IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl,
  // Restrict to Kernel-mode access only.
  //
  IoctlHandler_AccessModeFilterKernelModeOnly
} IoctlHandler_AccessModeFilterType;
````
Member | Description
----|----
IoctlHandler_AccessModeDefault | Indicates that the IOCTL has no restrictions.
IoctlHandler_AccessModeFilterClientCallback | Indicates that a Client callback should be called which will determine access to the IOCTL.
IoctlHandler_AccessModeFilterDoNotAllowUserMode | Not supported.
IoctlHandler_AccessModeFilterAdministratorOnly | Indicates that only a process running "As Administrator" has access to all the IOCTLs in the table.
IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl | Indicates that the IOCTL's table entry indicates if only processes running "As Administrator" have access to the IOCTLs in the table.
IoctlHandler_AccessModeFilterKernelModeOnly | Restrict to Kernel-mode access only.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------
##### IoctlHandler_IoctlRecord
````
typedef struct
{
  // The IOCTL code.
  // NOTE: At this time only METHOD_BUFFERED or METHOD_DIRECT buffers are supported.
  //
  ULONG IoctlCode;
  // Minimum input buffer size which is automatically validated by this Module.
  //
  ULONG InputBufferMinimumSize;
  // Minimum out buffer size which is automatically validated by this Module.
  //
  ULONG OutputBufferMinimumSize;
  // IOCTL handler callback after buffer size validation.
  //
  EVT_DMF_IoctlHandler_Function* EvtIoctlHandlerFunction;
  // Administrator access only. This flag is used with IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl
  // to allow Administrator access on a per-IOCTL basis.
  //
  BOOLEAN AdministratorAccessOnly;
} IoctlHandler_IoctlRecord;
````

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_IoctlHandler_Callback
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_IoctlHandler_Callback(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoctlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    );
````

This callback is called for every IOCTL that is allowed access and has properly sized input and output buffers as
specified in the IOCTL table passed in during Module instantiation.

##### Returns

If STATUS_PENDING is returned, then the Request is not completed. If STATUS_SUCCESS or any other NTSTATUS is returned,
then the Request is completed with that NTSTATUS.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_IoctlHandler Module handle.
Queue | The WDFQUEUE handle associated with Request.
Request | The WDFREQUEST that contains the IOCTL parameters and buffers.
IoctlCode | The IOCTL code passed by the caller.
InputBuffer | The Request's input buffer, if any.
InputBufferSize | The size of the Request's input buffer.
OutputBuffer | The Request's output buffer, if any.
OutputBufferSize | The size of the Request's output buffer.
BytesReturned | The number of bytes returned to the caller. Indicates the number of bytes transferred usually.

##### Remarks

* Request is passed to the callback, but it is rarely needed because the Module has already extracted the commonly used parameters and passed them to the callback.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_IoctlHandler_AccessModeFilter
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_IoctlHandler_AccessModeFilter(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    );
````

Allows Client to filter access to the IOCTLs. Client can use the parameters to decide if the connection to User-mode should be
allowed. It is provided in case default handler does not provide needed support. Use default handler has a guide for how to
implement the logic in this callback.

##### Returns

Return value of TRUE indicates that this callback completed the Request.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_IoctlHandler Module handle.
Device | Wdf Device object associated with Queue.
Request | Incoming request.
FileObject | File object associated with the given Request.

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_IoctlHandler_PostDeviceInterfaceCreate
````
typedef
_Function_class_(EVT_DMF_IoctlHandler_PostDeviceInterfaceCreate)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_IoctlHandler_PostDeviceInterfaceCreate(
    _In_ DMFMODULE DmfModule,
    _In_ GUID DeviceInterfaceGuid,
    _In_ UNICODE_STRING* SymbolicLinkNameString,
    _In_ IoctlHandler_IO_GET_DEVICE_INTERFACE_PROPERTY_DATA* IoGetDeviceInterfaceProperty,
    _In_ IoctlHandler_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA* IoSetDeviceInterfaceProperty
    );
````
Allows the Client to perform Client specific tasks after the device interface is created.

##### Returns

NTSTATUS: If an error is returned, the Module will not open.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_IoctlHandler Module handle.
DeviceInterfaceGuid | The GUID passed in the Module's Config when the Module is instantiated.
SymbolicLinkName | The symbolic link name corresponding to the instance of the device interface.
IoGetDeviceInterfaceProperty | This is a function pointer to a function that allows the Client to get device interface properties. The Client must always check this pointer to make sure it is not NULL before using it because it is not available on all platforms. It is not available in User Mode drivers.
IoSetDeviceInterfaceProperty | This is a function pointer to a function that allows the Client to set device interface properties. The Client must always check this pointer to make sure it is not NULL before using it because it is not available on all platforms. It is not available in User Mode drivers.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_IoctlHandler_IoctlsEnable

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_IoctlHandler_IoctlStateSet(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Enable
    )
````
Allows Client to enable / disable the device interface set in the Module's Config.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_IoctlHandler Module handle.
Enable | If true, enable the device interface. Otherwise, disable the device interface.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module removes the need for the programmer to validate the input and output buffers for all IOCTLs since the Module does this work.
* In case, validation by the Module is not desired, simply pass zero as the minimum size for the input and output buffers. Then, the IOCTL callback can perform its own Client specific validation.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DMF_ResourceHub

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------


-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

