/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_CmApi.c

Abstract:

    Support for Cm Apis.

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

    PAGED_CODE();
    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

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
// DMF Module Descriptor
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

static DMF_MODULE_DESCRIPTOR DmfModuleDescriptor_CmApi;
static DMF_CALLBACKS_DMF DmfCallbacksDmf_CmApi;

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

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&DmfCallbacksDmf_CmApi);
    DmfCallbacksDmf_CmApi.DeviceOpen = DMF_CmApi_Open;
    DmfCallbacksDmf_CmApi.DeviceClose = DMF_CmApi_Close;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(DmfModuleDescriptor_CmApi,
                                            CmApi,
                                            DMF_CONTEXT_CmApi,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    DmfModuleDescriptor_CmApi.CallbacksDmf = &DmfCallbacksDmf_CmApi;

    // ObjectAttributes must be initialized and
    // ParentObject attribute must be set to either WDFDEVICE or DMFMODULE.
    //
    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &DmfModuleDescriptor_CmApi,
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

    DMF_HandleValidate_ModuleMethod(DmfModule,
                                    &DmfModuleDescriptor_CmApi);

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

    Given the Id of an instance of a given device interface, return the list of hardware ids accociated with
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

// eof: Dmf_CmApi.c
//
