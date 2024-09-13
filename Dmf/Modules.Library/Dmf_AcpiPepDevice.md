## DMF_AcpiPepDevice
-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary
-----------------------------------------------------------------------------------------------------------------------------------
This Module registers root and child device tables with PEP framework. The tables register ACPI devices with PEP which include
callbacks that are called upon ACPI operations, such as EvaluateMethod. It also provides utility Methods and default callbacks for
PepDeviceClients to use. Due to PEP restrictions this Module can only be defined once per driver and hence needs the user to
explicitly provide Client device configuration before registration.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration
````
typedef struct
{
    // Client can pass an array of child PEP devices that this Module will instantiate.
    //
    AcpiPepDevice_CHILD_CONFIGURATIONS* ChildDeviceConfigurationArray;
    // Number of child configuration structures placed in ChildDeviceConfigurationArray.
    //
    ULONG ChildDeviceArraySize;
} DMF_CONFIG_AcpiPepDevice;
````
Member | Description
----|----
ChildDeviceConfigurationArray | Client can pass an array of child PEP devices that this Module will instantiate.
ChildDeviceArraySize | Number of child configuration structures placed in ChildDeviceConfigurationArray.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types
##### AcpiPepDevice_DEVICE_TYPE
````
typedef enum _AcpiPepDevice_DEVICE_TYPE
{
    AcpiPepDeviceType_Invalid = 0,
    AcpiPepDeviceType_Fan,
    AcpiPepDeviceType_Maximum,
} AcpiPepDevice_DEVICE_TYPE;
````
Used by the AcpiPepDevice_CHILD_CONFIGURATIONS structure to specify the type of child Module configuration passed.

##### PEP_NOTIFICATION_HANDLER_RESULT
````
typedef enum _PEP_NOTIFICATION_HANDLER_RESULT
{
    PEP_NOTIFICATION_HANDLER_COMPLETE,
    PEP_NOTIFICATION_HANDLER_MORE_WORK,
    PEP_NOTIFICATION_HANDLER_MAX
} PEP_NOTIFICATION_HANDLER_RESULT;
````
Used by AcpiPepDevice and its children to indicate to PEP the how the callback processed the request.

##### PEP_DEVICE_ID_MATCH
````
 typedef enum _PEP_DEVICE_ID_MATCH
 {
    // Substring match.
    //
    PepDeviceIdMatchPartial,
    // Whole string match.
    //
    PepDeviceIdMatchFull,
} PEP_DEVICE_ID_MATCH;
````
Indicates to the framework if the ACPI device ID matched fully or partially with the device ID in PEP tables.

##### PEP_NOTIFICATION_CLASS
````
typedef enum _PEP_NOTIFICATION_CLASS
{
    PEP_NOTIFICATION_CLASS_NONE = 0,
    PEP_NOTIFICATION_CLASS_ACPI = 1,
    PEP_NOTIFICATION_CLASS_DPM = 2,
    PEP_NOTIFICATION_CLASS_PPM = 4
} PEP_NOTIFICATION_CLASS;

````
This enumerator helps with registration, currently we only support ACPI class but Platform Extensions can also support DPM and PPM.

##### PEP_MAJOR_DEVICE_TYPE
````
typedef enum _PEP_MAJOR_DEVICE_TYPE
{
    PepMajorDeviceTypeProcessor,
    PepMajorDeviceTypeAcpi,
    PepMajorDeviceTypeMaximum
} PEP_MAJOR_DEVICE_TYPE;
````
This enumerator is required by the framework to identify PEP Device Major type.

##### PEP_ACPI_MINOR_DEVICE_TYPE
````
typedef enum _PEP_ACPI_MINOR_DEVICE_TYPE
{
    PepAcpiMinorTypeDevice,
    PepAcpiMinorTypePowerResource,
    PepAcpiMinorTypeThermalZone,
    PepAcpiMinorTypeMaximum
} PEP_ACPI_MINOR_DEVICE_TYPE;
````
This enumerator is required by the framework to identify PEP Device Minor type.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures
##### AcpiPepDevice_CHILD_CONFIGURATIONS
````
typedef struct _AcpiPepDevice_CHILD_CONFIGURATIONS
{
    AcpiPepDevice_DEVICE_TYPE PepDeviceType;
    VOID* PepDeviceConfiguration;
} AcpiPepDevice_CHILD_CONFIGURATIONS;
````
Structure used to pass PEP child devices to this Module.

##### PEP_ACPI_REGISTRATION_TABLES
````
typedef struct _PEP_ACPI_REGISTRATION_TABLES
{
    WDFMEMORY AcpiDefinitionTable;
    WDFMEMORY AcpiMatchTable;
} PEP_ACPI_REGISTRATION_TABLES;
````
These tables are initialized by all child PEP devices and contain all the information needed to register them with PEP.

##### PEP_ACPI_NOTIFY_CONTEXT
````
typedef struct _PEP_ACPI_NOTIFY_CONTEXT
{
    PEP_INTERNAL_DEVICE_HEADER* PepInternalDevice;
    ULONG NotifyCode;
} PEP_ACPI_NOTIFY_CONTEXT;
````
This structure is used by AcpiNotify helper Method to indicate the device and notification code.

##### PEP_ACPI_DEVICE
````
typedef struct _PEP_ACPI_DEVICE
{
    PEP_INTERNAL_DEVICE_HEADER Header;
} PEP_ACPI_DEVICE;
````
PEP_ACPI_DEVICE structure encapsulates the internal header which identifies a PEP device.

##### PEP_INTERNAL_DEVICE_HEADER
````
typedef struct _PEP_INTERNAL_DEVICE_HEADER
{
    LIST_ENTRY ListEntry;
    PEP_DEVICE_TYPE DeviceType;
    POHANDLE KernelHandle;
    PWSTR InstancePath;
    PEP_DEVICE_DEFINITION* DeviceDefinition;
    WDFMEMORY PepInternalDeviceMemory;
    DMFMODULE DmfModule;
} PEP_INTERNAL_DEVICE_HEADER;
````
This header is the sole mode of identification for a PEP device for both AcpiPepDevice and the Platform Extensions.

##### PEP_DEVICE_DEFINITION
````
typedef struct _PEP_DEVICE_DEFINITION
{
    PEP_DEVICE_TYPE Type;
    ULONG ContextSize;
    PEP_DEVICE_CONTEXT_INITIALIZE* Initialize;
    ULONG ObjectCount;
    _Field_size_(ObjectCount) PEP_OBJECT_INFORMATION* Objects;
    ULONG AcpiNotificationHandlerCount;
    _Field_size_(AcpiNotificationHandlerCount) PEP_DEVICE_NOTIFICATION_HANDLER* AcpiNotificationHandlers;
    ULONG DpmNotificationHandlerCount;
    _Field_size_(DpmNotificationHandlerCount) PEP_DEVICE_NOTIFICATION_HANDLER* DpmNotificationHandlers;
    DMFMODULE DmfModule;
} PEP_DEVICE_DEFINITION;
````
Child PEP devices need to fill out this table with routines such as ACPI Method handlers and ACPI Objects.

##### PEP_DEVICE_NOTIFICATION_HANDLER
````
typedef struct _PEP_DEVICE_NOTIFICATION_HANDLER
{
    ULONG Notification;
    PEP_NOTIFICATION_HANDLER_ROUTINE* Handler;
    PEP_NOTIFICATION_HANDLER_ROUTINE* WorkerCallbackHandler;
} PEP_DEVICE_NOTIFICATION_HANDLER;
````
This structure is what the Data void pointer can be cast to when a Method callback is called into by the framework.

##### PEP_GENERAL_NOTIFICATION_HANDLER
````
typedef struct _PEP_GENERAL_NOTIFICATION_HANDLER
{
    ULONG Notification;
    PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE* Handler;
    CHAR* Name;
} PEP_GENERAL_NOTIFICATION_HANDLER;
````
This handler is used during the device initialization.

##### PEP_OBJECT_INFORMATION
````
typedef struct _PEP_OBJECT_INFORMATION
{
    ULONG ObjectName;
    ULONG InputArgumentCount;
    ULONG OutputArgumentCount;
    PEP_ACPI_OBJECT_TYPE ObjectType;
} PEP_OBJECT_INFORMATION;
````
Part of the ACPI registration tables used by all children, this structure is used to indicate an ACPI method and its details.

##### PEP_DEVICE_MATCH

````
 typedef struct _PEP_DEVICE_MATCH
 {
    PEP_DEVICE_TYPE Type;
    PEP_NOTIFICATION_CLASS OwnedType;
    PWSTR DeviceId;
    PEP_DEVICE_ID_MATCH CompareMethod;
} PEP_DEVICE_MATCH;
````
Part of the ACPI registration tables used by all children, this structure is used to indicate ACPI device name among other details.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_AcpiPepDevice_AsyncNotifyEvent
````
PEP_NOTIFICATION_HANDLER_RESULT
DMF_AcpiPepDevice_AsyncNotifyEvent(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Data,
    _Out_opt_ PEP_WORK_INFORMATION* PoFxWorkInformation
    );
````
This Method serves AcpiPepDeviceClients as a generic callback for Acpi Notification requests.
It is scheduled to run asychronously.

##### Returns
Whether the operation is complete or needs more work.

##### Parameters
Parameter | Description
----|----
Data | Supplies a pointer to parameters buffer for this notification.
PoFxWorkInformation | Supplies a pointer to the PEP_WORK structure used to report result to PoFx.

##### DMF_AcpiPepDevice_ChildHandlesReturn

````
_Must_inspect_result_
DMFMODULE*
DMF_AcpiPepDevice_ChildHandlesReturn(
    _In_ DMFMODULE DmfModule
    );
````
This Method provides Client with a handle to all initialized Child Modules.
Clients can then use these handles to do Module specific operations, for example,
Fan Module uses this handle to send AcpiNotify to Fan device through a Method.

##### Returns
Array of Child DMF Module Handles.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiPepDevice Module handle.

##### DMF_AcpiPepDevice_PepAcpiDataReturn

````
VOID
DMF_AcpiPepDevice_PepAcpiDataReturn(
    _In_reads_bytes_(ValueLength) VOID* Value,
    _In_ USHORT ValueType,
    _In_ ULONG ValueLength,
    _In_ BOOLEAN ReturnAsPackage,
    _Inout_ PACPI_METHOD_ARGUMENT Arguments,
    _Inout_ SIZE_T* OutputArgumentSize,
    _Out_opt_ ULONG* OutputArgumentCount,
    _Out_ NTSTATUS* NtStatus,
    _In_opt_ CHAR* MethodName,
    _In_opt_ CHAR* DebugInfo,
    _Out_ PEP_NOTIFICATION_HANDLER_RESULT* CompleteResult
    );
````
This routine returns data of specific type back to PoFx.

##### Returns
None

##### Parameters
Parameter | Description
----|----
Value | Supplies the pointer to the data returned.
ValueType | Supplies the type of the data.
ValueLength | Supplies the length (raw, without ACPI method argument) of the data.
ReturnAsPackage | Supplies a flag indicating whether to return the data in a package.
Arguments | Supplies a pointer to receive the returned package.
OutputArgumentSize | Supplies a pointer to receive the returned argument size.
OutputArgumentCount | Supplies an optional pointer to receive the returned argument number.
ntStatus | Supplies a pointer to receive the evaluation result.
MethodName | Supplies an optional string that names the native method used for logging.
DebugInfo | Supplies an optional string that contains the debugging information to be included in the log.
CompleteResult | Supplies a pointer to receive the complete result.

##### DMF_AcpiPepDevice_ReportNotSupported
````
VOID
DMF_AcpiPepDevice_ReportNotSupported(
    _In_ DMFMODULE DmfModule,
    _Out_ NTSTATUS* Status,
    _Out_ ULONG* Count,
    _Out_ PEP_NOTIFICATION_HANDLER_RESULT* CompleteResult
    );
````
This Method reports to PoFx that the notification is not supported.

##### Returns
None

##### Parameters
Parameter | Description
----|----
DmfModule | Handle to this Module.
Status | Supplies a pointer to receive the evaluation status.
Count | Supplies a pointer to receive the output argument count/size.
CompleteResult | Supplies a pointer to receive the complete result.

##### DMF_AcpiPepDevice_ScheduleNotifyRequest
````
_Must_inspect_result_
NTSTATUS
DMF_AcpiPepDevice_ScheduleNotifyRequest(
    _In_ DMFMODULE DmfModule,
    _In_ PEP_ACPI_NOTIFY_CONTEXT* NotifyContext
    );
````
This Method sends AcpiNotify to the PoFx device passed in Context.

##### Returns
NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiPepDevice Module handle.
NotifyContext | Contains target device and notify code.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* Even though this Module can have at most one instance per driver, it can support any number of ACPI devices. Once the user passes
an array of child Module configs, the Module instantiates all children and establishes communication to retrieve all tables and
automatically registers them with PEP. Client can call a Method to receive all DMF Handles for children in the respective order
of their configuration array.
* Please see Windows-driver-samples\pofx\PEP\pepsamples for usage examples.
* This Module needs to be instantiated using a WDF Control Device.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

