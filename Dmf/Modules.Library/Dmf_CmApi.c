/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_CmApi.c

Abstract:

    Support for Cm Api.

Environment:

    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_CmApi.tmh"

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
    // Device Interface arrival/removal notification handle for devices.
    //
    HCMNOTIFICATION DeviceInterfaceNotification;
} DMF_CONTEXT_CmApi;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(CmApi)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(CmApi)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#include <devpkey.h>

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
CmApi_DeviceInterfaceListGet(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG Flags
    )
/*++

Routine Description:

    Get the list of all instances of the device interface set by the Client. Then, call the Client's
    callback function with the list.

Arguments:

    DmfModule - This Module's handle.
    Flags - Indicates whether the query is for all devices or only those present.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_CmApi* moduleContext;
    DMF_CONFIG_CmApi* moduleConfig;
    DWORD cmListSize;
    WCHAR *bufferPointer;
    NTSTATUS ntStatus;
    CONFIGRET configRet;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    ASSERT(moduleContext != NULL);

    do
    {
        configRet = CM_Get_Device_Interface_List_Size(&cmListSize,
                                                      (LPGUID)&moduleConfig->DeviceInterfaceGuid,
                                                      NULL,
                                                      Flags);
        if (configRet != CR_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_Device_Interface_List_Size fails: configRet=0x%x", configRet);
            ntStatus = ERROR_NOT_FOUND;
            goto Exit;
        }

        // Add two characters for forced double null termination.
        //
        bufferPointer = (WCHAR*)malloc((cmListSize + (size_t)2) * sizeof(WCHAR));

        if (bufferPointer != NULL)
        {
            // Make sure the buffer has a double null termination.
            //
            bufferPointer[cmListSize] = L'\0';
            bufferPointer[cmListSize + 1] = L'\0';

            configRet = CM_Get_Device_Interface_List((LPGUID)&moduleConfig->DeviceInterfaceGuid,
                                                     NULL,
                                                     bufferPointer,
                                                     cmListSize,
                                                     Flags);
            if (configRet == CR_SUCCESS)
            {
                if (moduleConfig->CmApi_Callback_DeviceInterfaceList)
                {
                    moduleConfig->CmApi_Callback_DeviceInterfaceList(DmfModule,
                                                                     bufferPointer,
                                                                     moduleConfig->DeviceInterfaceGuid);
                }
            }

            free(bufferPointer);
        }
        else
        {
            ntStatus = ERROR_NOT_ENOUGH_MEMORY;
            goto Exit;
        }
    }
    while (configRet == CR_BUFFER_SMALL);

    if (configRet != CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_Device_Interface_List fails: configRet=0x%x", configRet);
        ntStatus = ERROR_NOT_FOUND;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

Exit:

    return ntStatus;
}
#pragma code_seg()

DWORD
CmApi_NotificationCallback(
    _In_ HCMNOTIFICATION hNotify,
    _In_opt_ VOID* Context,
    _In_ CM_NOTIFY_ACTION Action,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
    _In_ DWORD EventDataSize
    )
/*++

Routine Description:

    Callback called when an instance of a device interface appears or disappears.

Arguments:

    Context - This Module's handle.

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;

    UNREFERENCED_PARAMETER(hNotify);
    UNREFERENCED_PARAMETER(EventData);
    UNREFERENCED_PARAMETER(EventDataSize);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    dmfModule = DMFMODULEVOID_TO_MODULE(Context);
    ASSERT(dmfModule != NULL);

    if ((Action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL ||
         Action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
        )
    {
        ntStatus = CmApi_DeviceInterfaceListGet(dmfModule,
                                                CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

        if (ntStatus != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Error querying the device interfaces.");
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return (DWORD)ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN 
CmApi_FirstParentInterfaceOpen(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG InterfaceIndex,
    _In_ WCHAR* InterfaceName,
    _In_ UNICODE_STRING* SymbolicLinkName,
    _In_ VOID* ClientContext
    )
/*++

Routine Description:

    Called by DMF_CmApi_ParentTargetCreateAndOpen in order to receive the symbolic name each interface
    associated with the Parent PDO. When this callback is called, it creates and opens the WDFIOTARGET
    associated with the Parent PDO.

Arguments:

    DmfModule - This Module's handle.
    InterfaceIndex - The index of the enumerated interface.
    InterfaceName - The name of the enumerated interface.
    SymbolicLinkName - The symbolic link name of the enumerated interface.
    ClientContext - The returned WDFIOTARGET of the associated Parent PDO.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFIOTARGET ioTarget;
    WDF_IO_TARGET_OPEN_PARAMS openParams;
    WDFIOTARGET* wdfIoTarget;
    WDFDEVICE device;

    UNREFERENCED_PARAMETER(InterfaceIndex);
    UNREFERENCED_PARAMETER(InterfaceName);

    device = DMF_ParentDeviceGet(DmfModule);

    wdfIoTarget = (WDFIOTARGET*)ClientContext;

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams,
                                                SymbolicLinkName,
                                                GENERIC_READ| GENERIC_WRITE);
    openParams.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;

    // Create an I/O target object.
    //
    ntStatus = WdfIoTargetCreate(device,
                                 WDF_NO_OBJECT_ATTRIBUTES,
                                 &ioTarget);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "WdfIoTargetCreate fails: ntStatus=%!STATUS!", 
                    ntStatus);
        goto Exit;
    }

    ntStatus = WdfIoTargetOpen(ioTarget,
                               &openParams);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, 
                    DMF_TRACE, 
                    "WdfIoTargetOpen fails: ntStatus=%!STATUS!", 
                    ntStatus);
        
        WdfObjectDelete(ioTarget);
        goto Exit;
    }

    ASSERT(ioTarget != NULL);
    *wdfIoTarget = ioTarget;

Exit:
    ;

    // Just open the first instance.
    //
    return FALSE;
}

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
_Must_inspect_result_
static
NTSTATUS
DMF_CmApi_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type CmApi.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_CmApi* moduleContext;
    DMF_CONFIG_CmApi* moduleConfig;
    CM_NOTIFY_FILTER cmNotifyFilter;
    CONFIGRET configRet;
    GUID nullGuid;

    PAGED_CODE();
    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    RtlZeroMemory(&nullGuid,
                  sizeof(GUID));
    if (! DMF_Utility_IsEqualGUID(&nullGuid,
                                  &moduleConfig->DeviceInterfaceGuid))
    {
        cmNotifyFilter.cbSize = sizeof(CM_NOTIFY_FILTER);
        cmNotifyFilter.Flags = 0;
        cmNotifyFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
        cmNotifyFilter.u.DeviceInterface.ClassGuid = moduleConfig->DeviceInterfaceGuid;

        configRet = CM_Register_Notification(&cmNotifyFilter,
                                             (VOID*)DmfModule,
                                             (PCM_NOTIFY_CALLBACK)CmApi_NotificationCallback,
                                             &(moduleContext->DeviceInterfaceNotification));

        // Target device might already be there . So try now.
        // 
        if (configRet == CR_SUCCESS)
        {
            CmApi_DeviceInterfaceListGet(DmfModule,
                                         CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);

            ntStatus = STATUS_SUCCESS;
        }
        else
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Register_Notification fails: configRet=0x%x", configRet);
            ntStatus = ERROR_NOT_FOUND;
            goto Exit;
        }
    }
    else
    {
        // It means Client wants to use other aspects of the Module unrelated to device interfaces.
        //
        ntStatus = STATUS_SUCCESS;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

Exit:
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_CmApi_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type CmApi.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_CmApi* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    CM_Unregister_Notification(moduleContext->DeviceInterfaceNotification);
    moduleContext->DeviceInterfaceNotification = NULL;

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
DMF_CmApi_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type CmApi.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_CmApi;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_CmApi;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_CmApi);
    dmfCallbacksDmf_CmApi.DeviceOpen = DMF_CmApi_Open;
    dmfCallbacksDmf_CmApi.DeviceClose = DMF_CmApi_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_CmApi,
                                            CmApi,
                                            DMF_CONTEXT_CmApi,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_CmApi.CallbacksDmf = &dmfCallbacksDmf_CmApi;

    // ObjectAttributes must be initialized and
    // ParentObject attribute must be set to either WDFDEVICE or DMFMODULE.
    //
    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_CmApi,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_CmApi_DevNodeStatusAndProblemCodeGet(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* DeviceInstanceId,
    _Out_ UINT32* DevNodeStatus,
    _Out_ UINT32* ProblemCode
    )
/*++

Routine Description:

    Given an instance of a device interface, get its Device Node Status and Problem Code.

Arguments:

    DmfModule - This Module's handle.
    DeviceInstanceId - InstanceId string of the given instance.
    DevNodeStatus - Returned Status of the device node.
    ProblemCode - Returned Problem code for the device node.

Return Value:

    TRUE - Successfully read DevNodeStatus and ProblemCode.
    FALSE - Failed to read.

--*/
{
    DMF_CONTEXT_CmApi* moduleContext;
    DEVPROPTYPE propertyType;
    ULONG propertySize;
    DEVINST devinst;
    CONFIGRET configRet;
    BOOLEAN returnValue;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 CmApi);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    ASSERT(DeviceInstanceId != NULL);
    ASSERT(DevNodeStatus != NULL);
    *DevNodeStatus = 0;
    ASSERT(ProblemCode != NULL);
    *ProblemCode = 0;

    returnValue = FALSE;

    configRet = CM_Locate_DevNode(&devinst,
                                  DeviceInstanceId,
                                  CM_LOCATE_DEVNODE_NORMAL);

    if (configRet != CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Locate_DevNode() fails: configRet=0x%x", configRet);
        goto Exit;
    }

    // Query the dev node status property on the device.
    //
    propertySize = sizeof(*DevNodeStatus);
    configRet = CM_Get_DevNode_Property(devinst,
                                        &DEVPKEY_Device_DevNodeStatus,
                                        &propertyType,
                                        (PBYTE)DevNodeStatus,
                                        &propertySize,
                                        0);
    if (configRet != CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_DevNode_Property() fails: configRet=0x%x", configRet);
        goto Exit;
    }

    if ((propertyType & DEVPROP_MASK_TYPE) != DEVPROP_TYPE_UINT32)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Device Node Status property was not of the correct type");
        goto Exit;
    }

    // Query the dev node status property on the device.
    //
    propertySize = sizeof(*ProblemCode);
    configRet = CM_Get_DevNode_Property(devinst,
                                        &DEVPKEY_Device_ProblemCode,
                                        &propertyType,
                                        (PBYTE)ProblemCode,
                                        &propertySize,
                                        0);
    if (configRet != CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_DevNode_Property() fails: configRet=0x%x", configRet);
        goto Exit;
    }

    if ((propertyType & DEVPROP_MASK_TYPE) != DEVPROP_TYPE_UINT32)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Device Node Status property was not of the correct type");
        goto Exit;
    }

    returnValue = TRUE;

    FuncExit(DMF_TRACE, "ReturnValue=%d", returnValue);

Exit:

    return returnValue;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_DeviceInstanceIdAndHardwareIdsGet(
    _In_ DMFMODULE DmfModule,
    _In_ PWSTR DeviceInterface,
    _Out_ WCHAR* DeviceInstanceId,
    _In_ UINT32 DeviceInstanceIdSize,
    _Out_ WCHAR* DeviceHardwareIds,
    _In_ UINT32 DeviceHardwareIdsSize
    )
/*++

Routine Description:

    Given the Id of an instance of a given device interface, return the list of hardware ids associated with
    with that instance.

Arguments:

    DmfModule - This Module's handle.
    DeviceInterface - The device interface GUID of the class of device to access.
    DeviceInstanceId - The id of the instance of DeviceInterface.
    DeviceInterfaceIdSize - The size in characters of DeviceInterfaceId.
    DeviceHardwareIds - The list of HWID (hardware identifier) associated with DeviceInstanceId.
    DeviceHardwareIdsSize - The size in characters of DeviceHardwareIds.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_CmApi* moduleContext;
    DMF_CONFIG_CmApi* moduleConfig;
    DEVPROPTYPE propertyType;
    ULONG propertySize;
    DEVINST devinst;
    CONFIGRET configRet;
    NTSTATUS ntStatus;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    ASSERT(DeviceHardwareIds != NULL);

    ntStatus = STATUS_SUCCESS;

    propertySize = DeviceInstanceIdSize;
    configRet = CM_Get_Device_Interface_Property(DeviceInterface,
                                                 &DEVPKEY_Device_InstanceId,
                                                 &propertyType,
                                                 (PBYTE)DeviceInstanceId,
                                                 &propertySize,
                                                 0);
    if (configRet != CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_Device_Interface_Property() fails: configRet=0x%x", configRet);
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    if ((propertyType & DEVPROP_MASK_TYPE) != DEVPROP_TYPE_STRING)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Device instance id is not of the correct type.");
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    configRet = CM_Locate_DevNode(&devinst,
                                  DeviceInstanceId,
                                  CM_LOCATE_DEVNODE_NORMAL);

    if (configRet != CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Locate_DevNode() fails: configRet=0x%x", configRet);
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    // Query the hardware IDs property on the device.
    //
    propertySize = DeviceHardwareIdsSize;
    configRet = CM_Get_DevNode_Property(devinst,
                                        &DEVPKEY_Device_HardwareIds,
                                        &propertyType,
                                        (PBYTE)DeviceHardwareIds,
                                        &propertySize,
                                        0);

    if (configRet != CR_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_DevNode_Property() fails: configRet=0x%x", configRet);
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    if ((propertyType & DEVPROP_MASK_TYPE) != DEVPROP_TYPE_STRING)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Device hardware IDs property was not of the correct type");
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

Exit:

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_ParentDevNodeGet(
    _In_ DMFMODULE DmfModule,
    _Out_ DEVINST* ParentDevNode,
    _Out_ WCHAR* ParentDeviceInstanceId,
    _In_ ULONG ParentDeviceInstanceIdBufferSize
    )
/*++

Routine Description:

    Given a DMFMODULE, retrieve its corresponding DEVINST and Instance Id.

Arguments:

    DmfModule - This Module's handle.
    ParentDevNode - The returned DEVINST.
    ParentDeviceInstanceId - The buffer where the corresponding Instance Id is written.
    ParentDeviceInstanceIdBufferSize - The size of ParentDeviceInstanceId in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    CONFIGRET configRet;
    DWORD lastError;
    WDF_DEVICE_PROPERTY_DATA property;
    DEVPROPTYPE propertyType;
    WCHAR deviceInstanceId[MAX_DEVICE_ID_LEN];
    ULONG requiredLength;
    DEVINST devInst;
    DEVINST parentDevInst;
    PWSTR deviceInterfaceList;
    ULONG deviceInterfaceListLength;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 CmApi);

    deviceInterfaceList = NULL;
    deviceInterfaceListLength = 0;
    device = DMF_ParentDeviceGet(DmfModule);

    WDF_DEVICE_PROPERTY_DATA_INIT(&property, 
                                  &DEVPKEY_Device_InstanceId);
    propertyType = DEVPROP_TYPE_STRING;
    ntStatus = WdfDeviceQueryPropertyEx(device,
                                        &property,
                                        sizeof(deviceInstanceId),
                                        (PVOID)&deviceInstanceId,
                                        &requiredLength,
                                        &propertyType);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfDeviceQueryPropertyEx fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    configRet = CM_Locate_DevNodeW(&devInst,
                                   deviceInstanceId,
                                   CM_LOCATE_DEVNODE_NORMAL);
    if (CR_SUCCESS != configRet)
    {
        lastError = GetLastError();
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "CM_Locate_DevNodeW fails: Result=%d lastError=%!WINERROR!",
                    configRet,
                    lastError);
        ntStatus = NTSTATUS_FROM_WIN32(lastError);
        goto Exit;
    }

    configRet = CM_Get_Parent(&parentDevInst,
                              devInst, 
                              CM_LOCATE_DEVNODE_NORMAL);
    if (CR_SUCCESS != configRet)
    {
        lastError = GetLastError();
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "CM_Get_Parent fails: Result=%d lastError=%!WINERROR!",
                    configRet,
                    lastError);
        ntStatus = NTSTATUS_FROM_WIN32(lastError);
        goto Exit;
    }

    *ParentDevNode = parentDevInst;

    ULONG size = ParentDeviceInstanceIdBufferSize;
    configRet = CM_Get_DevNode_PropertyW(parentDevInst,
                                         &DEVPKEY_Device_InstanceId,
                                         &propertyType,
                                         (PBYTE)ParentDeviceInstanceId,
                                         &size,
                                         0);
    if (CR_SUCCESS != configRet)
    {
        lastError = GetLastError();
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "CM_Get_DevNode_PropertyW fails: Result=%d lastError=%!WINERROR!",
                    configRet,
                    lastError);
        ntStatus = NTSTATUS_FROM_WIN32(lastError);
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_CmApi_ParentTargetCloseAndDestroy(
    _In_ DMFMODULE DmfModule,
    _In_ WDFIOTARGET ParentWdfIoTarget
    )
/*++

Routine Description:

    Closes and destroys a given WDFIOTARGET.

Arguments:

    DmfModule - This Module's handle.
    ParentWdfIoTarget - The WDFIOTARGET to close and destroy.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    WdfIoTargetClose(ParentWdfIoTarget);
    WdfObjectDelete(ParentWdfIoTarget);

    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_ParentTargetCreateAndOpen(
    _In_ DMFMODULE DmfModule,
    _In_ GUID* GuidDevicePropertyInterface,
    _Out_ WDFIOTARGET* ParentWdfIoTarget
    )
/*++

Routine Description:

    Finds the a parent device and creates/opens its associated WDFIOTARGET.
    This function just looks for the first interface associated with the parent target.

Arguments:

    DmfModule - This Module's handle.
    GuidDevicePropertyInterface - The device interface GUID to use.
    ParentWdfIoTarget - The returned WDFIOTARGET.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 CmApi);

    // Using uninitialized memory '*ParentWdfIoTarget'
    //
    #pragma warning(suppress:6001)
    ntStatus = DMF_CmApi_ParentTargetInterfacesEnumerate(DmfModule,
                                                         CmApi_FirstParentInterfaceOpen,
                                                         GuidDevicePropertyInterface,
                                                         ParentWdfIoTarget);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_CmApi_ParentTargetInterfacesEnumerate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (*ParentWdfIoTarget == NULL)
    {
        ntStatus = STATUS_NOT_FOUND;
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_ParentTargetInterfacesEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_CmApi_ParentTargetSymbolicLinkName ParentTargetCallback,
    _In_ GUID* GuidDevicePropertyInterface,
    _Inout_ VOID* ClientContext
    )
/*++

Routine Description:

    Enumerates all the interfaces associated with the Parent PDO.

Arguments:

    DmfModule - This Module's handle.
    ParentTargetCallback - Callback to Client for each interface instance enumerated.
    GuidDevicePropertyInterface - The device interface GUID to find.
    ClientContext - Client context passed to ParentTargetCallback.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    CONFIGRET configRet;
    DWORD lastError;
    WDF_DEVICE_PROPERTY_DATA property;
    DEVPROPTYPE propertyType;
    WCHAR deviceInstanceId[MAX_DEVICE_ID_LEN];
    WCHAR parentDeviceInstanceId[MAX_DEVICE_ID_LEN];
    ULONG requiredLength;
    DEVINST devInst;
    DEVINST parentDevInst;
    PWSTR deviceInterfaceList;
    ULONG deviceInterfaceListLength;
    PWSTR currentInterface;
    DWORD interfaceIndex;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 CmApi);

    deviceInterfaceList = NULL;
    deviceInterfaceListLength = 0;
    device = DMF_ParentDeviceGet(DmfModule);

    WDF_DEVICE_PROPERTY_DATA_INIT(&property, 
                                  &DEVPKEY_Device_InstanceId);
    propertyType = DEVPROP_TYPE_STRING;
    ntStatus = WdfDeviceQueryPropertyEx(device,
                                        &property,
                                        sizeof(deviceInstanceId),
                                        (PVOID)&deviceInstanceId,
                                        &requiredLength,
                                        &propertyType);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "WdfDeviceQueryPropertyEx fails: ntStatus=%!STATUS!",
                    ntStatus);
        goto Exit;
    }

    configRet = CM_Locate_DevNodeW(&devInst,
                                   deviceInstanceId,
                                   CM_LOCATE_DEVNODE_NORMAL);
    if (CR_SUCCESS != configRet)
    {
        lastError = GetLastError();
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "CM_Locate_DevNodeW fails: Result=%d lastError=%!WINERROR!",
                    configRet,
                    lastError);
        ntStatus = NTSTATUS_FROM_WIN32(lastError);
        goto Exit;
    }

    configRet = CM_Get_Parent(&parentDevInst,
                              devInst, 
                              CM_LOCATE_DEVNODE_NORMAL);
    if (CR_SUCCESS != configRet)
    {
        lastError = GetLastError();
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "CM_Get_Parent fails: Result=%d lastError=%!WINERROR!",
                    configRet,
                    lastError);
        ntStatus = NTSTATUS_FROM_WIN32(lastError);
        goto Exit;
    }

    ULONG size = sizeof(parentDeviceInstanceId);
    configRet = CM_Get_DevNode_PropertyW(parentDevInst,
                                         &DEVPKEY_Device_InstanceId,
                                         &propertyType,
                                         (PBYTE)parentDeviceInstanceId,
                                         &size,
                                         0);
    if (CR_SUCCESS != configRet)
    {
        lastError = GetLastError();
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "CM_Get_DevNode_PropertyW fails: Result=%d lastError=%!WINERROR!",
                    configRet,
                    lastError);
        ntStatus = NTSTATUS_FROM_WIN32(lastError);
        goto Exit;
    }

    // Get the existing Device Interfaces for the given Guid.
    // It is recommended to do this in a loop, as the
    // size can change between the call to CM_Get_Device_Interface_List_Size and 
    // CM_Get_Device_Interface_List.
    //
    do
    {
        configRet = CM_Get_Device_Interface_List_Size(&deviceInterfaceListLength,
                                                      (LPGUID)GuidDevicePropertyInterface,
                                                      parentDeviceInstanceId,
                                                      CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
        if (configRet != CR_SUCCESS)
        {
            lastError = GetLastError();
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "CM_Get_Device_Interface_List_Size fails: Result=%d lastError=%!WINERROR!",
                        configRet,
                        lastError);
            ntStatus = NTSTATUS_FROM_WIN32(lastError);
            goto Exit;
        }

        if (deviceInterfaceList != NULL)
        {
            if (!HeapFree(GetProcessHeap(),
                          0,
                          deviceInterfaceList))
            {
                lastError = GetLastError();
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "HeapFree fails: lastError=%!WINERROR!",
                            lastError);
                ntStatus = NTSTATUS_FROM_WIN32(lastError);
                deviceInterfaceList = NULL;
                goto Exit;
            }
        }

        deviceInterfaceList = (PWSTR)HeapAlloc(GetProcessHeap(),
                                               HEAP_ZERO_MEMORY,
                                               deviceInterfaceListLength * sizeof(WCHAR));
        if (deviceInterfaceList == NULL)
        {
            lastError = GetLastError();
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "HeapAlloc fails: lastError=%!WINERROR!",
                        lastError);
            ntStatus = NTSTATUS_FROM_WIN32(lastError);
            goto Exit;
        }

        configRet = CM_Get_Device_Interface_List((LPGUID)GuidDevicePropertyInterface,
                                                 parentDeviceInstanceId,
                                                 deviceInterfaceList,
                                                 deviceInterfaceListLength,
                                                 CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
    } while (configRet == CR_BUFFER_SMALL);

    if (configRet != CR_SUCCESS)
    {
        lastError = GetLastError();
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "CM_Get_Device_Interface_List fails: configRet=%d lastError=%!WINERROR!",
                    configRet,
                    lastError);
        ntStatus = NTSTATUS_FROM_WIN32(lastError);
        goto Exit;
    }

    // Loop through the interfaces for a matching target and open it.
    // Ensure to return STATUS_SUCCESS only on a matched target get.
    //
    ntStatus = STATUS_NOT_FOUND;
    interfaceIndex = 0;
    UNICODE_STRING symbolicLinkName;
    for (currentInterface = deviceInterfaceList;
         *currentInterface;
         currentInterface += wcslen(currentInterface) + 1)
    {
        // At this point, enumeration has started so return STATUS_SUCCESS to
        // to indicated enumeration started. It is up to the Client to determine
        // if the data returned by enumeration from Client callback is correct.
        //
        ntStatus = STATUS_SUCCESS;

        TraceEvents(TRACE_LEVEL_VERBOSE,
                    DMF_TRACE,
                    "[interfaceIndex=%d] Checking interface=[%ws]",
                    interfaceIndex,
                    currentInterface);

        // The given interface index is found. Try to create and open it.
        //

        RtlInitUnicodeString(&symbolicLinkName,
                             currentInterface);

        BOOLEAN continueEnumeration;

        // Allow the Client to create and open the target or perform other
        // actions using the interface information.
        //
        continueEnumeration = ParentTargetCallback(DmfModule,
                                                   interfaceIndex,
                                                   currentInterface,
                                                   &symbolicLinkName,
                                                   ClientContext);
        if (!continueEnumeration)
        {
            goto Exit;
        }

        // For Client callback use only.
        //
        interfaceIndex++;
    }

Exit:

    if (deviceInterfaceList != NULL)
    {
        if (!HeapFree(GetProcessHeap(),
                      0,
                      deviceInterfaceList))
        {
            lastError = GetLastError();
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "HeapFree fails: %!WINERROR!",
                        lastError);
            ntStatus = NTSTATUS_FROM_WIN32(lastError);
            deviceInterfaceList = NULL;
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_PropertyUint32Get(
    _In_  DMFMODULE DmfModule,
    _In_  GUID* PropertyInterfaceGuid,
    _In_  PDEVPROPKEY PropertyKey,
    _Out_ UINT32* Value
    )
/*++

Routine Description:

    Gets the specified property value on the property interface.

Arguments:

    DmfModule - This Module's handle.
    PropertyInterfaceGuid - The GUID of the requested property interface.
    PropertyKey - The device's property key.
    Value - The property value if found.

Return Value:

    NTSTATUS -- STATUS_SUCCESS on a successful query, and a CONFIGRET error
                converted to an NTSTATUS code on failure.

--*/
{
    NTSTATUS ntStatus;
    CONFIGRET configRet;
    PWSTR deviceInterfaceList;
    ULONG deviceInterfaceListLength;
    PWSTR currentInterface;
    DEVPROPTYPE propertyType;
    ULONG propertySize;
    WCHAR currentDevice[MAX_DEVICE_ID_LEN];
    DEVINST deviceInstance;

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    // Device interfaces can come and go, so we will look a few times for the requested interface.
    // This also will break us out of the below loop if the interface GUID is not found.
    //
    const int MAX_SEARCH_COUNT = 5;
    int searchLoopCount  = 0;
    deviceInterfaceList = NULL;
    configRet = CR_FAILURE;
    
    if (PropertyInterfaceGuid == NULL || 
        PropertyKey == NULL || 
        Value == NULL)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    *Value = 0;
    do 
    {
        configRet = CM_Get_Device_Interface_List_Size(&deviceInterfaceListLength,
                                                      PropertyInterfaceGuid,
                                                      NULL,
                                                      CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
        if (CR_SUCCESS != configRet)
        {
            TraceEvents(TRACE_LEVEL_ERROR,
                        DMF_TRACE,
                        "CM_Get_Device_Interface_List_Size fails: 0x%x",
                        configRet);
            break;
        }

        if (NULL != deviceInterfaceList) 
        {
            free(deviceInterfaceList);
            deviceInterfaceList = NULL;
        }

        deviceInterfaceList = (PWSTR)malloc(deviceInterfaceListLength * sizeof(WCHAR));

        if (NULL == deviceInterfaceList)
        {
            configRet = CR_OUT_OF_MEMORY;
            break;
        }

        configRet = CM_Get_Device_Interface_List(PropertyInterfaceGuid,
                                                 NULL,
                                                 deviceInterfaceList,
                                                 deviceInterfaceListLength,
                                                 CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
        searchLoopCount++;
    } while (configRet == CR_BUFFER_SMALL && searchLoopCount <= MAX_SEARCH_COUNT);
    
    if (searchLoopCount >= MAX_SEARCH_COUNT)
    {
        configRet = CR_INVALID_DATA;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Did not find requested interface, config return: 0x%x",
                    configRet);
        goto Exit;
    }

    if (CR_SUCCESS != configRet)
    {
        goto Exit;
    }

    ntStatus = STATUS_NOT_FOUND;

    // Take the first interface found.
    // 
    currentInterface = deviceInterfaceList;
    if (!*currentInterface)
    {
        configRet = CR_INVALID_DATA;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Did not find requested interface: 0x%x",
                     configRet);
        goto Exit;
    }

   propertySize = sizeof(currentDevice);
   configRet = CM_Get_Device_Interface_Property(currentInterface,
                                                &DEVPKEY_Device_InstanceId,
                                                &propertyType,
                                                (PBYTE)currentDevice,
                                                &propertySize,
                                                0);
   if (CR_SUCCESS != configRet)
   {
       TraceEvents(TRACE_LEVEL_ERROR,
                   DMF_TRACE,
                   "CM_Get_Device_Interface_Property fails: 0x%x",
                   configRet);
       goto Exit;
   }
   
   if (DEVPROP_TYPE_STRING != propertyType)
   {
       configRet = CR_INVALID_DATA;
       goto Exit;
   }

   // Get the device instance, and then the device.
   // 
   propertySize = sizeof(currentDevice);
   configRet = CM_Get_Device_Interface_Property(currentInterface,
                                                &DEVPKEY_Device_InstanceId,
                                                &propertyType,
                                                (PBYTE)currentDevice,
                                                &propertySize,
                                                0);
   if (CR_SUCCESS != configRet)
   {

       TraceEvents(TRACE_LEVEL_ERROR,
                   DMF_TRACE,
                   "CM_Get_Device_Interface_Property fails: 0x%x",
                   configRet);
       goto Exit;
   }
   
   configRet = CM_Locate_DevNode(&deviceInstance,
                                 currentDevice,
                                 CM_LOCATE_DEVNODE_NORMAL);

   if (configRet != CR_SUCCESS)
   {
       TraceEvents(TRACE_LEVEL_ERROR,
                   DMF_TRACE,
                   "CM_Locate_DevNode fails: 0x%x",
                   configRet);
       goto Exit;
   }

   // Now we can query the property.
   //
   configRet = CM_Get_DevNode_Property(deviceInstance,
                                       PropertyKey,
                                       &propertyType,
                                       (BYTE*)Value,
                                       &propertySize,
                                       0);

   if (CR_SUCCESS != configRet)
   {
       TraceEvents(TRACE_LEVEL_ERROR,
                   DMF_TRACE,
                   "CM_Get_Device_Interface_Property fails: 0x%x",
                   configRet);
       goto Exit;
   }

   // Verify that the type and size is correct.
   //
   if (DEVPROP_TYPE_UINT32 != propertyType)
   {
       configRet = CR_INVALID_DATA;
       TraceEvents(TRACE_LEVEL_ERROR,
                   DMF_TRACE,
                   "Expected type : 'DEVPROP_TYPE_UINT32'");
       goto Exit;
   }
   if (sizeof(UINT32) != propertySize)
   {
       configRet = CR_INVALID_DATA;
       TraceEvents(TRACE_LEVEL_ERROR,
                   DMF_TRACE,
                   "Expected size of UINT32");
       goto Exit;
   }

   TraceEvents(TRACE_LEVEL_INFORMATION,
               DMF_TRACE,
               "Found requested property value: %d",
               *Value);

Exit:

    // Cleanup.
    //
    if (deviceInterfaceList != NULL)
    {
        free(deviceInterfaceList);
    }
    
    if (configRet == CR_SUCCESS)
    {
        ntStatus = STATUS_SUCCESS;
    }
    else
    {
        ntStatus = NTSTATUS_FROM_WIN32(configRet);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

// eof: Dmf_CmApi.c
//
