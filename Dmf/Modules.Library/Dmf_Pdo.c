/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Pdo.c

Abstract:

    Creates Physical Device Objects (PDO) connected to a Function Device Object (FDO).

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_Pdo.tmh"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This Module has no Context.
//
DMF_MODULE_DECLARE_NO_CONTEXT(Pdo)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(Pdo)

// Memory Pool Tag.
//
#define MemoryTag 'ModP'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define MAX_ID_LEN 80

typedef struct _PDO_DEVICE_DATA
{
    // Unique serial number of the device on the bus.
    //
    ULONG SerialNumber;
} PDO_DEVICE_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(PDO_DEVICE_DATA, PdoGetData)

#pragma code_seg("PAGE")
_Check_return_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Pdo_DevicePropertyTableWrite(
    _In_ DMFMODULE DmfModule,
    _In_ Pdo_DeviceProperty_Table* DevicePropertyTable
    )
/*++

Routine Description:

    This routine writes a given table of device properties to the devices's 
    proptery store.

Arguments:

    DmfModule - This Module's handle.
    DevicePropertyTable - The given table.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    Pdo_DevicePropertyEntry* entry;
    UNREFERENCED_PARAMETER(DmfModule);

    ASSERT(DevicePropertyTable);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    // Assign the properties for this device.
    //
    ntStatus = STATUS_SUCCESS;
    device = DMF_ParentDeviceGet(DmfModule);
    for (ULONG propertyIndex = 0; propertyIndex < DevicePropertyTable->ItemCount; propertyIndex++)
    {
        entry = &DevicePropertyTable->TableEntries[propertyIndex];

        // First register the device interface GUID if requested.
        //
        if (entry->RegisterDeviceInterface)
        {
            // Complain if the client requested us to register the device interface,
            // but did not provide a device interface GUID.
            //
            ASSERT(entry->DeviceInterfaceGuid != NULL);
            if (NULL == entry->DeviceInterfaceGuid)
            {
                ntStatus = STATUS_INVALID_PARAMETER;
                goto Exit;
            }

            ntStatus = WdfDeviceCreateDeviceInterface(device, 
                                                      entry->DeviceInterfaceGuid,
                                                      NULL);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "WdfDeviceCreateDeviceInterface fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }

        // Now set the properties.
        //
        ntStatus = WdfDeviceAssignProperty(device,
                                           &entry->DevicePropertyData,
                                           entry->ValueType,
                                           entry->ValueSize,
                                           entry->ValueData);
        if (!NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Check_return_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Pdo_PdoEx(
    _In_ DMFMODULE DmfModule,
    _In_ PDO_RECORD* PdoRecord,
    _In_opt_ PWDFDEVICE_INIT DeviceInit,
    _Out_opt_ WDFDEVICE* Device
    )
/*++

Routine Description:

    This routine creates and initialize a PDO for the device associated with HardwareId.

Arguments:

    Device - A handle to a framework driver object.
    PdoRecord - Pdo Record.
    DeviceInit - Pre-allocated WDFDEVICE_INIT structure.
    Device - the new PDO created (optional).

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PWDFDEVICE_INIT deviceInit;
    PDO_DEVICE_DATA* pdoData;
    WDFDEVICE child;
    WDF_OBJECT_ATTRIBUTES pdoAttributes;
    UNICODE_STRING hardwareId;
    UNICODE_STRING compatibleId;
    UNICODE_STRING deviceLocation;
    WDF_DEVICE_PNP_CAPABILITIES pnpCapabilities;
    WDF_DEVICE_POWER_CAPABILITIES powerCapabilities;
    DECLARE_UNICODE_STRING_SIZE(instanceId, MAX_ID_LEN);
    DECLARE_UNICODE_STRING_SIZE(deviceDescription, MAX_ID_LEN);
    WCHAR formattedIdBuffer[MAX_ID_LEN];
    PWCHAR idString;
    DMF_CONFIG_Pdo* moduleConfig;
    WDFDEVICE device;
    PDMFDEVICE_INIT dmfDeviceInit;
    DMF_EVENT_CALLBACKS dmfCallbacks;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);
    ASSERT(PdoRecord != NULL);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ASSERT(PdoRecord->HardwareIds != NULL);
    ASSERT(PdoRecord->HardwareIdsCount != 0);
    ASSERT(PdoRecord->CompatibleIdsCount == 0 ||
           PdoRecord->CompatibleIds != NULL);
    ASSERT(PdoRecord->Description != NULL);

    child = NULL;
    dmfDeviceInit = NULL;

    if (DeviceInit == NULL)
    {
        // Allocate a WDFDEVICE_INIT structure and set the properties
        // so that a device object for the child can be created.
        //
        deviceInit = WdfPdoInitAllocate(device);
        if (NULL == deviceInit)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit;
        }
    }
    else
    {
        deviceInit = DeviceInit;
    }

    if (PdoRecord->RawDevice)
    {
        ntStatus = WdfPdoInitAssignRawDevice(deviceInit,
                                             PdoRecord->RawDeviceClassGuid);
        if (! NT_SUCCESS(ntStatus))
        {
            goto Exit;
        }
    }

    // Create a new instance of DMF for this PDO. 
    //
    if (PdoRecord->EnableDmf)
    {
        dmfDeviceInit = DMF_DmfDeviceInitAllocate(deviceInit);

        DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit,
                                                    NULL);
        DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit,
                                              NULL);
        DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit,
                                                       NULL);
    }

    // Set DeviceType.
    //
    WdfDeviceInitSetDeviceType(deviceInit,
                               FILE_DEVICE_BUS_EXTENDER);

    // Add Each Hardware IDs one by one in the order that was specified to preserve the matching order.
    //
    for (USHORT hardwareIdIndex = 0; hardwareIdIndex < PdoRecord->HardwareIdsCount; ++hardwareIdIndex)
    {
        idString = PdoRecord->HardwareIds[hardwareIdIndex];

        if (moduleConfig->EvtPdoHardwareIdFormat != NULL)
        {
            // HardwareIds contain format strings, populate them with client's parameters
            //
            RtlZeroMemory(formattedIdBuffer,
                          _countof(formattedIdBuffer));

            ntStatus = moduleConfig->EvtPdoHardwareIdFormat(DmfModule,
                                                            formattedIdBuffer,
                                                            _countof(formattedIdBuffer),
                                                            PdoRecord->HardwareIds[hardwareIdIndex]);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtPdoHardwareIdFormat fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }

            // Use the version returned by the Client.
            //
            idString = formattedIdBuffer;
        }

        // Assign HardwareID
        //
        ntStatus = RtlUnicodeStringInit(&hardwareId,
                                        idString);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlUnicodeStringInit fails at Index %d: ntStatus=%!STATUS!", hardwareIdIndex, ntStatus);
            goto Exit;
        }

        ntStatus = WdfPdoInitAddHardwareID(deviceInit,
                                           &hardwareId);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfPdoInitAddHardwareID fails at Index %d: ntStatus=%!STATUS!", hardwareIdIndex, ntStatus);
            goto Exit;
        }

        if (0 == hardwareIdIndex)
        {
            // Pick the first item in the hardware id as the device id as recommended in MSDN.
            //
            ntStatus = WdfPdoInitAssignDeviceID(deviceInit,
                                                &hardwareId);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfPdoInitAssignDeviceID fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }
    }

    // Add Each optional Compatible IDs one by one in the order that was specified to preserve the matching order.
    //
    for (USHORT compatibleIdIndex = 0; compatibleIdIndex < PdoRecord->CompatibleIdsCount; ++compatibleIdIndex)
    {
        idString = PdoRecord->CompatibleIds[compatibleIdIndex];

        if (moduleConfig->EvtPdoCompatibleIdFormat != NULL)
        {
            // CompatibleIds contain format strings, populate them with client's parameters
            //
            RtlZeroMemory(formattedIdBuffer,
                          _countof(formattedIdBuffer));

            ntStatus = moduleConfig->EvtPdoCompatibleIdFormat(DmfModule,
                                                              formattedIdBuffer,
                                                              _countof(formattedIdBuffer),
                                                              PdoRecord->CompatibleIds[compatibleIdIndex]);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtPdoCompatibleIdFormat fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }

            // Use the version returned by the Client.
            //
            idString = formattedIdBuffer;
        }

        // Assign CompatibleId
        //
        ntStatus = RtlUnicodeStringInit(&compatibleId,
                                        idString);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlUnicodeStringInit fails at Index %d: ntStatus=%!STATUS!", compatibleIdIndex, ntStatus);
            goto Exit;
        }

        ntStatus = WdfPdoInitAddCompatibleID(deviceInit,
                                             &compatibleId);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfPdoInitAddCompatibleID fails at Index %d: ntStatus=%!STATUS!", compatibleIdIndex, ntStatus);
            goto Exit;
        }
    }

    // InstanceId.
    //
    ntStatus = RtlUnicodeStringPrintf(&instanceId,
                                      moduleConfig->InstanceIdFormatString,
                                      PdoRecord->SerialNumber);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlIntegerToUnicodeString fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = WdfPdoInitAssignInstanceID(deviceInit,
                                          &instanceId);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfPdoInitAssignInstanceID fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Provide a description about the device. This text is usually read from
    // the device. In the case of USB device, this text comes from the string
    // descriptor. This text is displayed momentarily by the PnP manager while
    // it's looking for a matching INF. If it finds one, it uses the Device
    // Description from the INF file or the friendly name created by
    // co-installers to display in the device manager. FriendlyName takes
    // precedence over the DeviceDesc from the INF file.
    //
    ntStatus = RtlUnicodeStringPrintf(&deviceDescription,
                                      PdoRecord->Description,
                                      PdoRecord->SerialNumber);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlUnicodeStringInit fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = RtlUnicodeStringInit(&deviceLocation,
                                    moduleConfig->DeviceLocation);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlUnicodeStringPrintf fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Can call WdfPdoInitAddDeviceText multiple times, adding device
    // text for multiple locales. When the system displays the text, it
    // chooses the text that matches the current locale, if available.
    // Otherwise it will use the string for the default locale.
    // The driver can specify the driver's default locale by calling
    // WdfPdoInitSetDefaultLocale.
    //
    ntStatus = WdfPdoInitAddDeviceText(deviceInit,
                                       &deviceDescription,
                                       &deviceLocation,
                                       MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                                       SORT_DEFAULT));
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfPdoInitAddDeviceText fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WdfPdoInitSetDefaultLocale(deviceInit,
                               MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                               SORT_DEFAULT));

    // Initialize the attributes to specify the size of PDO device extension.
    // All the state information private to the PDO will be tracked here.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&pdoAttributes,
                                            PDO_DEVICE_DATA);

    // Once the device is created successfully, framework frees the
    // DeviceInit memory and sets the pDeviceInit to NULL. So don't
    // call any WdfDeviceInit functions after that.
    //
    ntStatus = WdfDeviceCreate(&deviceInit,
                               &pdoAttributes,
                               &child);
    if (! NT_SUCCESS(ntStatus))
    {
        child = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // If the product has specified optional product specific properties, add them here.
    // This allows different products to specify what is supported on their platform.
    //
    if (PdoRecord->DeviceProperties != NULL)
    {
        ntStatus = Pdo_DevicePropertyTableWrite(DmfModule, 
                                                PdoRecord->DeviceProperties);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Pdo_DevicePropertyTableWrite fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    if (PdoRecord->EnableDmf)
    {
        DMF_EVENT_CALLBACKS_INIT(&dmfCallbacks);
        dmfCallbacks.EvtDmfDeviceModulesAdd = PdoRecord->EvtDmfDeviceModulesAdd;
        DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,
                                           &dmfCallbacks);

        ntStatus = DMF_ModulesCreate(child,
                                     &dmfDeviceInit);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModulesCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Get the device context.
    //
    pdoData = PdoGetData(child);

    pdoData->SerialNumber = PdoRecord->SerialNumber;

    // Set properties for the child device, all others inherit from the bus driver.
    //
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCapabilities);
    pnpCapabilities.Removable = WdfUseDefault;
    pnpCapabilities.EjectSupported = WdfUseDefault;
    pnpCapabilities.SurpriseRemovalOK = WdfUseDefault;

    pnpCapabilities.Address = PdoRecord->SerialNumber;
    pnpCapabilities.UINumber = PdoRecord->SerialNumber;

    if (moduleConfig->EvtPdoPnpCapabilities != NULL)
    {
        moduleConfig->EvtPdoPnpCapabilities(DmfModule,
                                            &pnpCapabilities);
    }

    WdfDeviceSetPnpCapabilities(child,
                                &pnpCapabilities);

    WDF_DEVICE_POWER_CAPABILITIES_INIT(&powerCapabilities);

    powerCapabilities.DeviceD1 = WdfTrue;
    powerCapabilities.WakeFromD1 = WdfTrue;
    powerCapabilities.DeviceWake = PowerDeviceD1;

    powerCapabilities.DeviceState[PowerSystemWorking] = PowerDeviceD0;
    powerCapabilities.DeviceState[PowerSystemSleeping1] = PowerDeviceD1;
    powerCapabilities.DeviceState[PowerSystemSleeping2] = PowerDeviceD3;
    powerCapabilities.DeviceState[PowerSystemSleeping3] = PowerDeviceD3;
    powerCapabilities.DeviceState[PowerSystemHibernate] = PowerDeviceD3;
    powerCapabilities.DeviceState[PowerSystemShutdown] = PowerDeviceD3;

    if (moduleConfig->EvtPdoPowerCapabilities != NULL)
    {
        moduleConfig->EvtPdoPowerCapabilities(DmfModule,
                                              &powerCapabilities);
    }

    WdfDeviceSetPowerCapabilities(child,
                                  &powerCapabilities);

    if (moduleConfig->EvtPdoQueryInterfaceAdd != NULL)
    {
        ntStatus = moduleConfig->EvtPdoQueryInterfaceAdd(DmfModule,
                                                         child);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "EvtPdoQueryInterfaceAdd fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    if (DeviceInit == NULL)
    {
        // Add this device to the FDO's collection of children.
        // 
        ntStatus = WdfFdoAddStaticChild(device,
                                        child);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfFdoAddStaticChild fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // After the child device is added to the static collection successfully,
    // driver must call WdfPdoMarkMissing to get the device deleted. It
    // should not delete the child device directly by calling WdfObjectDelete.
    //

    if (Device != NULL)
    {
        *Device = child;
    }

    child = NULL;

Exit:

    // Call WdfDeviceInitFree if an error is encountered before the
    // device is created. Once the device is created, framework
    // NULLs the pDeviceInit value.
    //
    if (deviceInit != NULL)
    {
        WdfDeviceInitFree(deviceInit);
    }

    if (dmfDeviceInit != NULL)
    {
        DMF_DmfDeviceInitFree(&dmfDeviceInit);
    }

    if (child != NULL)
    {
        WdfObjectDelete(child);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

ScheduledTask_Result_Type
Pdo_ScheduledTask(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ModuleContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Create PDO's this platform needs.

Parameters Description:

    DmfModule - The Child Module from which this callback is called.
    ModuleContext - Module Context
    PreviousState - Valid only for calls from D0Entry

Return Value:

    ScheduledTask_WorkResult_Success - Indicates the operations was successful.
    ScheduledTask_WorkResult_Fail - Indicates the operation was not successful. A retry will happen.

--*/
{
    DMFMODULE dmfModule;
    NTSTATUS ntStatus;
    DMF_CONFIG_Pdo* moduleConfig;
    ScheduledTask_Result_Type returnValue;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(ModuleContext);

    FuncEntry(DMF_TRACE);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "ScheduledTask Handler");

    // This Module is the parent of the Child Module that is passed in.
    // (Module callbacks always receive the Child Module's handle.)
    //
    dmfModule = DMF_ParentModuleGet(DmfModule);
    returnValue = ScheduledTask_WorkResult_Fail;

    moduleConfig = DMF_CONFIG_GET(dmfModule);

    for (ULONG pdoRecordIndex = 0; pdoRecordIndex < moduleConfig->PdoRecordCount; pdoRecordIndex++)
    {
        PDO_RECORD* pdoRecord;

        ntStatus = STATUS_SUCCESS;

        pdoRecord = &moduleConfig->PdoRecords[pdoRecordIndex];

        if ((pdoRecord->EvtPdoIsPdoRequired != NULL) &&
            (pdoRecord->EvtPdoIsPdoRequired(dmfModule,
                                            PreviousState) == FALSE))
        {
            // Skip PDO.
            //
            continue;
        }

        ASSERT(moduleConfig->InstanceIdFormatString != NULL);
        ASSERT(pdoRecord->HardwareIds != NULL);
        ASSERT(pdoRecord->HardwareIdsCount != 0);
        ASSERT(pdoRecord->HardwareIdsCount < PDO_RECORD_MAXIMUM_NUMBER_OF_HARDWARE_IDS);
        ASSERT(pdoRecord->CompatibleIdsCount == 0 || pdoRecord->CompatibleIds != NULL);
        ASSERT(pdoRecord->CompatibleIdsCount < PDO_RECORD_MAXIMUM_NUMBER_OF_COMPAT_IDS);
        ASSERT(pdoRecord->Description != NULL);


        // Create PDO.
        //
        ntStatus = Pdo_PdoEx(dmfModule,
                             pdoRecord,
                             NULL,
                             NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            // Driver tried to create the PDO, but was unable to. This is a an error that should be reported.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Pdo_PdoEx fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    returnValue = ScheduledTask_WorkResult_Success;

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Pdo_ChildModulesAdd(
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
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_Pdo* moduleConfig;
    DMF_CONFIG_ScheduledTask scheduledTaskModuleConfigPdo;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (moduleConfig->PdoRecordCount > 0)
    {
        // ScheduledTask
        // -------------
        //
        DMF_CONFIG_ScheduledTask_AND_ATTRIBUTES_INIT(&scheduledTaskModuleConfigPdo,
                                                     &moduleAttributes);
        scheduledTaskModuleConfigPdo.EvtScheduledTaskCallback = Pdo_ScheduledTask;
        scheduledTaskModuleConfigPdo.ExecutionMode = ScheduledTask_ExecutionMode_Immediate;
        scheduledTaskModuleConfigPdo.PersistenceType = ScheduledTask_Persistence_NotPersistentAcrossReboots;
        scheduledTaskModuleConfigPdo.ExecuteWhen = ScheduledTask_ExecuteWhen_D0Entry;
        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         NULL);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Pdo.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Pdo;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Pdo;

    PAGED_CODE();

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Pdo);
    dmfCallbacksDmf_Pdo.ChildModulesAdd = DMF_Pdo_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT(dmfModuleDescriptor_Pdo,
                               Pdo,
                               DMF_MODULE_OPTIONS_PASSIVE,
                               DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_Pdo.CallbacksDmf = &dmfCallbacksDmf_Pdo;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Pdo,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceEject(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Eject and destroy a static PDO from the Client Driver's FDO.

Arguments:

    DmfModule - This Module's handle.
    Device - PDO to be ejected.

Return Value:

    STATUS_SUCCESS upon successful ejection.
    STATUS_INVALID_PARAMETER if the ejection was unsuccessful.

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Pdo);

    device = DMF_ParentDeviceGet(DmfModule);


    WdfPdoRequestEject(Device);

    ntStatus = STATUS_SUCCESS;

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceEjectUsingSerialNumber(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG SerialNumber
    )
/*++

Routine Description:

    Eject and destroy a static PDO from the Client Driver's FDO.
    PDO is identified by matching the provided serial number.

Arguments:

    DmfModule - This Module's handle.
    SerialNumber - Serial number of the PDO to be ejected.

Return Value:

    STATUS_SUCCESS upon successful ejection.
    STATUS_INVALID_PARAMETER if the ejection was unsuccessful.

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    WDFDEVICE childDevice;
    PDO_DEVICE_DATA* pdoData;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Pdo);

    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = STATUS_NOT_FOUND;
    childDevice = NULL;

    WdfFdoLockStaticChildListForIteration(device);

    while ((childDevice = WdfFdoRetrieveNextStaticChild(device,
                                                        childDevice,
                                                        WdfRetrieveAddedChildren)) != NULL)
    {
        pdoData = PdoGetData(childDevice);

        if (SerialNumber == pdoData->SerialNumber)
        {
            WdfPdoRequestEject(childDevice);
            ntStatus = STATUS_SUCCESS;
            break;
        }
    }

    WdfFdoUnlockStaticChildListFromIteration(device);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DevicePlug(
    _In_ DMFMODULE DmfModule,
    _In_reads_(HardwareIdsCount) PWSTR HardwareIds[],
    _In_ USHORT HardwareIdsCount,
    _In_reads_opt_(CompatibleIdsCount) PWSTR CompatibleIds[],
    _In_ USHORT CompatibleIdsCount,
    _In_ PWSTR Description,
    _In_ ULONG SerialNumber,
    _Out_opt_ WDFDEVICE* Device
    )
/*++

Routine Description:

    Create and attach a static PDO to the Client Driver's FDO.

Arguments:

    DmfModule - This Module's handle.
    HardwareIds - Array of Wide string Hardware Id of the new PDO.
    HardwareIdsCount - Count of Hardware Ids in the array
    CompatibleIds - Array of Wide string Compatible Id of the new PDO.
    CompatibleIdsCount - Count of Compatible Ids in the array
    Description - Description of the new PDO.
    SerialNumber - Serial number of the new PDO.
    Device - The newly created PDO (optional).

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    WDFDEVICE childDevice;
    PDO_DEVICE_DATA* pdoData;
    BOOLEAN unique;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Pdo);

    device = DMF_ParentDeviceGet(DmfModule);

    unique = TRUE;
    childDevice = NULL;
    ntStatus = STATUS_SUCCESS;

    WdfFdoLockStaticChildListForIteration(device);

    while ((childDevice = WdfFdoRetrieveNextStaticChild(device,
                                                        childDevice,
                                                        WdfRetrieveAddedChildren)) != NULL)
    {
        // WdfFdoRetrieveNextStaticChild returns reported and to be reported
        // children (ie children who have been added but not yet reported to PNP).
        //
        // A surprise removed child will not be returned in this list.
        //
        pdoData = PdoGetData(childDevice);

        // It's okay to plug in another device with the same serial number
        // as long as the previous one is in a surprise-removed state. The
        // previous one would be in that state after the device has been
        // physically removed, if somebody has an handle open to it.
        //
        if (SerialNumber == pdoData->SerialNumber)
        {
            unique = FALSE;
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }
    }

    WdfFdoUnlockStaticChildListFromIteration(device);

    if (unique)
    {
        // Create a new child device.
        //
        PDO_RECORD pdoRecord;
        RtlZeroMemory(&pdoRecord,
                      sizeof(PDO_RECORD));

        pdoRecord.HardwareIdsCount = HardwareIdsCount;
        for (UINT hardwareIdCount = 0; hardwareIdCount < HardwareIdsCount; hardwareIdCount++)
        {
            pdoRecord.HardwareIds[hardwareIdCount] = HardwareIds[hardwareIdCount];
        }
        pdoRecord.CompatibleIdsCount = CompatibleIdsCount;
        for (UINT compatibleIdCount = 0; compatibleIdCount < CompatibleIdsCount; compatibleIdCount++)
        {
            // "Dereferencing NULL pointer 'CompatibleIds'"
            //
            #pragma warning(suppress:6011)
            pdoRecord.CompatibleIds[compatibleIdCount] = CompatibleIds[compatibleIdCount];
        }
        pdoRecord.Description = Description;
        pdoRecord.EnableDmf = FALSE;
        pdoRecord.EvtDmfDeviceModulesAdd = NULL;
        pdoRecord.EvtPdoIsPdoRequired = NULL;
        pdoRecord.RawDevice = FALSE;
        pdoRecord.SerialNumber = SerialNumber;

        ntStatus = Pdo_PdoEx(DmfModule,
                             &pdoRecord,
                             NULL,
                             Device);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DevicePlugEx(
    _In_ DMFMODULE DmfModule,
    _In_ PDO_RECORD* PdoRecord,
    _Out_opt_ WDFDEVICE* Device
    )
/*++

Routine Description:

    Create and attach a static PDO to the Client Driver's FDO. This Method allows Client
    to create PDO that uses DMF Modules.

Arguments:

    DmfModule - This Module's handle.
    PdoRecord - The parameters used to create the PDO.
    Device - The newly created PDO (optional).

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    WDFDEVICE childDevice;
    PDO_DEVICE_DATA* pdoData;
    BOOLEAN unique;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Pdo);

    device = DMF_ParentDeviceGet(DmfModule);

    unique = TRUE;
    childDevice = NULL;
    ntStatus = STATUS_SUCCESS;

    WdfFdoLockStaticChildListForIteration(device);

    while ((childDevice = WdfFdoRetrieveNextStaticChild(device,
                                                        childDevice,
                                                        WdfRetrieveAddedChildren)) != NULL)
    {
        // WdfFdoRetrieveNextStaticChild returns reported and to be reported
        // children (ie children who have been added but not yet reported to PNP).
        //
        // A surprise removed child will not be returned in this list.
        //
        pdoData = PdoGetData(childDevice);

        // It's okay to plug in another device with the same serial number
        // as long as the previous one is in a surprise-removed state. The
        // previous one would be in that state after the device has been
        // physically removed, if somebody has an handle open to it.
        //
        if (PdoRecord->SerialNumber == pdoData->SerialNumber)
        {
            unique = FALSE;
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }
    }

    WdfFdoUnlockStaticChildListFromIteration(device);

    if (unique)
    {
        // Create a new child device.
        //
        ntStatus = Pdo_PdoEx(DmfModule,
                             PdoRecord,
                             NULL,
                             Device);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceUnplug(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Unplug and destroy the given PDO from the Client Driver's FDO.

Arguments:

    DmfModule - This Module's handle.
    Device - PDO to be unplugged.

Return Value:

    STATUS_SUCCESS upon successful removal from the list
    STATUS_INVALID_PARAMETER if the removal was unsuccessful

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Pdo);

    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = STATUS_INVALID_PARAMETER;

    ntStatus = WdfPdoMarkMissing(Device);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfPdoMarkMissing fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Pdo_DeviceUnplugUsingSerialNumber(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG SerialNumber
    )
/*++

Routine Description:

    Unplug and destroy a static PDO from the Client Driver's FDO.
    PDO is identified by matching the provided serial number.

Arguments:

    DmfModule - This Module's handle.
    SerialNumber - Serial number of the PDO to be ejected.

Return Value:

    STATUS_SUCCESS upon successful removal from the list
    STATUS_INVALID_PARAMETER if the removal was unsuccessful

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    WDFDEVICE childDevice;
    PDO_DEVICE_DATA* pdoData;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Pdo);

    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = STATUS_NOT_FOUND;
    childDevice = NULL;

    WdfFdoLockStaticChildListForIteration(device);

    while ((childDevice = WdfFdoRetrieveNextStaticChild(device,
                                                        childDevice,
                                                        WdfRetrieveAddedChildren)) != NULL)
    {
        pdoData = PdoGetData(childDevice);

        if (SerialNumber == pdoData->SerialNumber)
        {
            ntStatus = WdfPdoMarkMissing(childDevice);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfPdoMarkMissing fails: ntStatus=%!STATUS!", ntStatus);
                break;
            }

            ntStatus = STATUS_SUCCESS;
            break;
        }
    }

    WdfFdoUnlockStaticChildListFromIteration(device);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_Pdo.c
//
