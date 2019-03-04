/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_ToasterBus.c

Abstract:

    DMF version of KMDF Toaster Bus WDF driver sample.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Template.h"
#include "DmfModules.Template.Trace.h"

#include "Dmf_ToasterBus.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Pdo Create.
    //
    DMFMODULE DmfModulePdo;
    // Ioctl Handler.
    //
    DMFMODULE DmfModuleIoctlHandler;
    // Registry.
    //
    DMFMODULE DmfModuleRegistry;
} DMF_CONTEXT_ToasterBus;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(ToasterBus)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(ToasterBus)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define MAX_STATICALLY_ENUMERATED_TOASTERS 10
#define MAX_ID_LEN 80

typedef
BOOLEAN
(*ToasterBus_GetCrispnessLevel)(_In_ WDFDEVICE Context,
                                _Out_ UCHAR* Level);

typedef
BOOLEAN
(*ToasterBus_SetCrispnessLevel)(_In_ WDFDEVICE Context,
                                _In_ UCHAR Level);

typedef
BOOLEAN
(*ToasterBus_IsChildProtected)(_In_ WDFDEVICE Context);

typedef struct _TOASTER_INTERFACE_STANDARD
{
    INTERFACE InterfaceHeader;
    ToasterBus_GetCrispnessLevel GetCrispinessLevel;
    ToasterBus_SetCrispnessLevel SetCrispinessLevel;
    ToasterBus_IsChildProtected IsSafetyLockEnabled;
} TOASTER_INTERFACE_STANDARD;

BOOLEAN
Bus_GetCrispinessLevel(
    _In_ WDFDEVICE ChildDevice,
    _Out_ UCHAR* Level
    )
/*++

Routine Description:

    This routine gets the current crispiness level of the toaster.

Arguments:

    Context        pointer to  PDO device extension
    Level          crispiness level of the device

Return Value:

    TRUE or FALSE

--*/
{
    UNREFERENCED_PARAMETER(ChildDevice);

    // Validate the context to see if it's really a pointer
    // to PDO's device extension. You can store some kind
    // of signature in the PDO for this purpose.
    //

    KdPrint(("BusEnum: GetCrispnessLevel\n"));

    *Level = 10;
    return TRUE;
}

BOOLEAN
Bus_SetCrispinessLevel(
    _In_ WDFDEVICE ChildDevice,
    _In_ UCHAR Level
    )
/*++

Routine Description:

    This routine sets the current crispiness level of the toaster.

Arguments:

    Context        pointer to  PDO device extension
    Level          crispiness level of the device

Return Value:

    TRUE or FALSE

--*/
{
    UNREFERENCED_PARAMETER(ChildDevice);
    UNREFERENCED_PARAMETER(Level);

    KdPrint(("BusEnum: SetCrispnessLevel\n"));

    return TRUE;
}

BOOLEAN
Bus_IsSafetyLockEnabled(
    _In_ WDFDEVICE ChildDevice
    )
/*++

Routine Description:

    Routine to check whether safety lock is enabled

Arguments:

    Context        pointer to  PDO device extension

Return Value:

    TRUE or FALSE

--*/
{
    UNREFERENCED_PARAMETER(ChildDevice);

    KdPrint(("BusEnum: IsSafetyLockEnabled\n"));

    return TRUE;
}

NTSTATUS
ToasterBus_DeviceQueryInterfaceAdd(
    DMFMODULE DmfModule,
    WDFDEVICE PdoDevice
    )
{
    NTSTATUS ntStatus;
    TOASTER_INTERFACE_STANDARD ToasterInterface;
    WDF_QUERY_INTERFACE_CONFIG qiConfig;

    UNREFERENCED_PARAMETER(DmfModule);

    // Create a custom interface so that other drivers can
    // query (IRP_MN_QUERY_INTERFACE) and use our callbacks directly.
    //
    RtlZeroMemory(&ToasterInterface, sizeof(ToasterInterface));

    ToasterInterface.InterfaceHeader.Size = sizeof(ToasterInterface);
    ToasterInterface.InterfaceHeader.Version = 1;
    ToasterInterface.InterfaceHeader.Context = (VOID*)PdoDevice;

    // Let the framework handle reference counting.
    //
    ToasterInterface.InterfaceHeader.InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
    ToasterInterface.InterfaceHeader.InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;

    ToasterInterface.GetCrispinessLevel = Bus_GetCrispinessLevel;
    ToasterInterface.SetCrispinessLevel = Bus_SetCrispinessLevel;
    ToasterInterface.IsSafetyLockEnabled = Bus_IsSafetyLockEnabled;

    WDF_QUERY_INTERFACE_CONFIG_INIT(&qiConfig,
                                    (PINTERFACE)&ToasterInterface,
                                    &GUID_TOASTER_INTERFACE_STANDARD,
                                    NULL);

    // If you have multiple interfaces, you can call WdfDeviceAddQueryInterface
    // multiple times to add additional interfaces.
    //
    ntStatus = WdfDeviceAddQueryInterface(PdoDevice, &qiConfig);

    return ntStatus;
}

NTSTATUS
ToasterBus_DoStaticEnumeration(
    _In_ DMFMODULE DmfModule
    )
/*++
Routine Description:

    The routine enables you to statically enumerate child devices
    during start instead of running the enum.exe/notify.exe to
    enumerate toaster devices.

    In order to statically enumerate, user must specify the number
    of toasters in the Toaster Bus driver's device registry. The
    default value is zero.

    HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\Root\SYSTEM\0002\
                    Device Parameters
                        NumberOfToasters:REG_DWORD:2

    You can also configure this value in the Toaster Bus Inf file.

--*/

{
    NTSTATUS ntStatus;
    ULONG numberOfToasters;
    ULONG deviceSerialNumber;
    DMF_CONTEXT_ToasterBus* moduleContext;
    DMF_CONFIG_ToasterBus* moduleConfig;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = DMF_Registry_PathAndValueReadDwordAndValidate(moduleContext->DmfModuleRegistry,
                                                             NULL,
                                                             L"NumberOfToasters",
                                                             &numberOfToasters,
                                                             0,
                                                             MAX_STATICALLY_ENUMERATED_TOASTERS);
    if (! NT_SUCCESS (ntStatus))
    {
        // Registry is an optional property.
        //
        ntStatus = STATUS_SUCCESS;

        // If the registry value doesn't exist, 
        // or If the value is not between 0 and MAX_STATICALLY_ENUMERATED_TOASTERS
        // we will use the default number.
        //
        numberOfToasters = moduleConfig->DefaultNumberOfToasters;
    }

    KdPrint(("Enumerating %d toaster devices\n", numberOfToasters));

    for (deviceSerialNumber = 1;
         deviceSerialNumber <= numberOfToasters;
         deviceSerialNumber++)
    {
        PWSTR hardwareIds[] = { moduleConfig->ToasterBusHardwareId };
        PWSTR compatibleIds[] = { moduleConfig->ToasterBusHardwareCompatibleId };
        // Value of i is used as serial number.
        //
        ntStatus = DMF_Pdo_DevicePlug(moduleContext->DmfModulePdo,
                                      hardwareIds,
                                      _countof(hardwareIds),
                                      compatibleIds,
                                      _countof(compatibleIds),
                                      moduleConfig->ToasterBusDeviceDescriptionFormat,
                                      deviceSerialNumber,
                                      NULL);
    }

    return ntStatus;
}

#pragma code_seg("PAGE")
// 'Returning uninitialized memory '*BytesReturned''
//
#pragma warning(suppress:6101)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
ToasterBus_IoctlClientCallback_DevicePlug(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoctlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    The user application has told us that a new device on the bus has arrived.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Queue - The WDFQUEUE associated with Request.
    Request - Request data, not used.
    IoctlCode - IOCTL to be used in the command.
    InputBuffer - Input data buffer.
    InputBufferSize - Input data buffer size, not used.
    OutputBuffer - Output data buffer.
    OutputBufferSize - Output data buffer size, not used.
    BytesReturned - Amount of data to be sent back.

Return Value:

    STATUS_PENDING - Request is not completed.
    Otherwise - Request is completed.

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DMF_CONTEXT_ToasterBus* moduleContext;
    DMF_CONFIG_ToasterBus* moduleConfig;
    DMFMODULE dmfModule;
    size_t length;
    BUSENUM_PLUGIN_HARDWARE* plugIn;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(IoctlCode);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);
    UNREFERENCED_PARAMETER(BytesReturned);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    dmfModule = DMF_ParentModuleGet(DmfModule);
    ASSERT(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    length = 0;
    plugIn = (BUSENUM_PLUGIN_HARDWARE*)InputBuffer;

    if (sizeof (BUSENUM_PLUGIN_HARDWARE) == plugIn->Size)
    {
        length = (InputBufferSize - sizeof (BUSENUM_PLUGIN_HARDWARE)) / sizeof(WCHAR);

        //
        // Make sure the IDs is double NULL terminated. TODO:
        //
        if ((UNICODE_NULL != plugIn->HardwareIDs[length - 1]) ||
            (UNICODE_NULL != plugIn->HardwareIDs[length - 2]))
        {

            ntStatus = STATUS_INVALID_PARAMETER;
        }
        else
        {
            PWSTR hardwareIds[] = { plugIn->HardwareIDs };
            ntStatus = DMF_Pdo_DevicePlug(moduleContext->DmfModulePdo,
                                          hardwareIds,
                                          _countof(hardwareIds),
                                          NULL,             // No compatible Id
                                          0,
                                          moduleConfig->ToasterBusDeviceDescriptionFormat,
                                          plugIn->SerialNo,
                                          NULL);
            if (! NT_SUCCESS(ntStatus))
            {
                // Complete the request even though an error happened.
                //
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                            "DMF_Pdo_PlugInDevice fails: ntStatus=%!STATUS!",
                            ntStatus);
            }
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
// 'Returning uninitialized memory '*BytesReturned''
//
#pragma warning(suppress:6101)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
ToasterBus_IoctlClientCallback_DeviceUnplug(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoctlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    The application has told us a device has departed from the bus.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Queue - The WDFQUEUE associated with Request.
    Request - Request data, not used.
    IoctlCode - IOCTL to be used in the command.
    InputBuffer - Input data buffer.
    InputBufferSize - Input data buffer size, not used.
    OutputBuffer - Output data buffer.
    OutputBufferSize - Output data buffer size, not used.
    BytesReturned - Amount of data to be sent back.

Return Value:

    STATUS_PENDING - Request is not completed.
    Otherwise - Request is completed.

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DMF_CONTEXT_ToasterBus* moduleContext;
    DMFMODULE dmfModule;
    BUSENUM_UNPLUG_HARDWARE* unPlug;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(IoctlCode);
    UNREFERENCED_PARAMETER(IoctlCode);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);
    UNREFERENCED_PARAMETER(BytesReturned);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    dmfModule = DMF_ParentModuleGet(DmfModule);
    ASSERT(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    unPlug = (BUSENUM_UNPLUG_HARDWARE*)InputBuffer;

    if (unPlug->Size == InputBufferSize)
    {
        ntStatus = DMF_Pdo_DeviceUnplugUsingSerialNumber(moduleContext->DmfModulePdo,
                                                         unPlug->SerialNo);
        if (! NT_SUCCESS(ntStatus))
        {
            // Complete the request even though an error happened.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                        "DMF_Pdo_DeviceUnplugUsingSerialNumber fails: ntStatus=%!STATUS!",
                        ntStatus);
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
// 'Returning uninitialized memory '*BytesReturned''
//
#pragma warning(suppress:6101)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
ToasterBus_IoctlClientCallback_DeviceEject(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoctlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    )
/*++

Routine Description:

    The user application has told us to eject the device from the bus.

Arguments:

    DmfModule - The Child Module from which this callback is called.
    Queue - The WDFQUEUE associated with Request.
    Request - Request data, not used.
    IoctlCode - IOCTL to be used in the command.
    InputBuffer - Input data buffer.
    InputBufferSize - Input data buffer size, not used.
    OutputBuffer - Output data buffer.
    OutputBufferSize - Output data buffer size, not used.
    BytesReturned - Amount of data to be sent back.

Return Value:

    STATUS_PENDING - Request is not completed.
    Otherwise - Request is completed.

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    DMF_CONTEXT_ToasterBus* moduleContext;
    DMFMODULE dmfModule;
    BUSENUM_EJECT_HARDWARE* eject;
    size_t length;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(IoctlCode);
    UNREFERENCED_PARAMETER(IoctlCode);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferSize);
    UNREFERENCED_PARAMETER(BytesReturned);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    dmfModule = DMF_ParentModuleGet(DmfModule);
    ASSERT(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    length = 0;
    eject = (BUSENUM_EJECT_HARDWARE*)InputBuffer;

    if (eject->Size == InputBufferSize)
    {
        ntStatus = DMF_Pdo_DeviceEjectUsingSerialNumber(moduleContext->DmfModulePdo,
                                                        eject->SerialNo);
        if (! NT_SUCCESS(ntStatus))
        {
            // Complete the request even though an error happened.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                        "DMF_Pdo_DeviceEjectUsingSerialNumber fails: ntStatus=%!STATUS!",
                        ntStatus);
        }

    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Wdf Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_ToasterBus_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type ToasterBus.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    PNP_BUS_INFORMATION busInfo;
    DMF_CONFIG_ToasterBus* moduleConfig;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    // This value is used in responding to the IRP_MN_QUERY_BUS_INFORMATION
    // for the child devices. This is an optional information provided to
    // uniquely identify the bus the device is connected.
    //
    busInfo.BusTypeGuid = moduleConfig->ToasterBusDevClassGuid;
    busInfo.LegacyBusType = PNPBus;
    busInfo.BusNumber = 0;

    WdfDeviceSetBusInformationForChildren(device, &busInfo);

    ntStatus = ToasterBus_DoStaticEnumeration(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

IoctlHandler_IoctlRecord ToasterBus_IoctlSpecification[] =
{
    { IOCTL_BUSENUM_PLUGIN_HARDWARE, sizeof (BUSENUM_PLUGIN_HARDWARE), 0, ToasterBus_IoctlClientCallback_DevicePlug, FALSE },
    { IOCTL_BUSENUM_UNPLUG_HARDWARE, sizeof (BUSENUM_UNPLUG_HARDWARE), 0, ToasterBus_IoctlClientCallback_DeviceUnplug, FALSE },
    { IOCTL_BUSENUM_EJECT_HARDWARE, sizeof (BUSENUM_EJECT_HARDWARE), 0, ToasterBus_IoctlClientCallback_DeviceEject, FALSE }
};

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ToasterBus_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_CONFIG_Pdo moduleConfigPdo;
    DMF_CONFIG_IoctlHandler moduleConfigIoctlHandler;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_ToasterBus* moduleConfig;
    DMF_CONTEXT_ToasterBus* moduleContext;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Pdo
    // ---
    //
    DMF_CONFIG_Pdo_AND_ATTRIBUTES_INIT(&moduleConfigPdo,
                                       &moduleAttributes);
    moduleConfigPdo.DeviceLocation = L"TOASTER BUS 0";
    moduleConfigPdo.InstanceIdFormatString = L"TOASTER_DEVICE_%02d";
    // Do not create any PDOs during Module create.
    // PDOs will be created dynamically through Module Method.
    //
    moduleConfigPdo.PdoRecordCount = 0;
    moduleConfigPdo.PdoRecords = NULL;
    moduleConfigPdo.EvtPdoPnpCapabilities = NULL;
    moduleConfigPdo.EvtPdoPowerCapabilities = NULL;
    moduleConfigPdo.EvtPdoQueryInterfaceAdd = ToasterBus_DeviceQueryInterfaceAdd;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModulePdo);

    // IoctlHandler
    // ------------
    //
    DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT(&moduleConfigIoctlHandler,
                                                &moduleAttributes);
    moduleConfigIoctlHandler.DeviceInterfaceGuid = GUID_DEVINTERFACE_BUSENUM_TOASTER;
    moduleConfigIoctlHandler.AccessModeFilter = IoctlHandler_AccessModeDefault;
    moduleConfigIoctlHandler.EvtIoctlHandlerAccessModeFilter = NULL;
    moduleConfigIoctlHandler.IoctlRecordCount = ARRAYSIZE(ToasterBus_IoctlSpecification);
    moduleConfigIoctlHandler.IoctlRecords = ToasterBus_IoctlSpecification;
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleIoctlHandler);

    // Registry
    // --------
    //
    DMF_Registry_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleRegistry);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_ToasterBus;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_ToasterBus;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ToasterBus_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type ToasterBus.

Arguments:

    Device - WDFDEVICE handle.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_ToasterBus);
    DmfCallbacksDmf_ToasterBus.ChildModulesAdd = DMF_ToasterBus_ChildModulesAdd;
    DmfCallbacksDmf_ToasterBus.DeviceOpen = DMF_ToasterBus_Open;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_ToasterBus,
                                            ToasterBus,
                                            DMF_CONTEXT_ToasterBus,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    DmfModuleDescriptor_ToasterBus.CallbacksDmf = &DmfCallbacksDmf_ToasterBus;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_ToasterBus,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

// eof: Dmf_ToasterBus.c
//
