/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_CmApi.h

Abstract:

    Companion file to Dmf_CmApi.c.

Environment:

    User-mode Driver Framework

--*/

#pragma once

#if defined(DMF_USER_MODE)

typedef
_Function_class_(EVT_DMF_CmApi_DeviceInterfaceList)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID 
EVT_DMF_CmApi_DeviceInterfaceList(_In_ DMFMODULE DmfModule,
                                  _In_ WCHAR* DeviceInterfaceList,
                                  _In_ GUID DeviceInterfaceGuid);

typedef
_Function_class_(EVT_DMF_CmApi_ParentTargetSymbolicLinkName)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
BOOLEAN 
EVT_DMF_CmApi_ParentTargetSymbolicLinkName(_In_ DMFMODULE DmfModule,
                                           _In_ ULONG InterfaceIndex,
                                           _In_ WCHAR* InterfaceName,
                                           _In_ UNICODE_STRING* SymbolicLinkName,
                                           _In_ VOID* ClientContext);

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Device Interface Guid.
    //
    GUID DeviceInterfaceGuid;
    // Callback to get device information.
    //
    EVT_DMF_CmApi_DeviceInterfaceList* CmApi_Callback_DeviceInterfaceList;
} DMF_CONFIG_CmApi;

// This macro declares the following functions:
// DMF_CmApi_ATTRIBUTES_INIT()
// DMF_CONFIG_CmApi_AND_ATTRIBUTES_INIT()
// DMF_CmApi_Create()
//
DECLARE_DMF_MODULE(CmApi)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_CmApi_DevNodeStatusAndProblemCodeGet(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* DeviceInstanceId,
    _Out_ UINT32* DevNodeStatus,
    _Out_ UINT32* ProblemCode
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_DeviceInstanceIdAndHardwareIdsGet(
    _In_ DMFMODULE DmfModule,
    _In_ PWSTR DeviceInterface,
    _Out_ WCHAR* DeviceInstanceId,
    _In_ UINT32 DeviceInstanceIdSize,
    _Out_ WCHAR* DeviceHardwareIds,
    _In_ UINT32 DeviceHardwareIdsSize
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_CmApi_ParentTargetCloseAndDestroy(
    _In_ DMFMODULE DmfModule,
    _In_ WDFIOTARGET ParentWdfIoTarget
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_ParentTargetCreateAndOpen(
    _In_ DMFMODULE DmfModule,
    _In_ GUID* GuidDevicePropertyInterface,
    _Out_ WDFIOTARGET* ParentWdfIoTarget
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_ParentTargetInterfacesEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ EVT_DMF_CmApi_ParentTargetSymbolicLinkName ParentTargetCallback,
    _In_ GUID* GuidDevicePropertyInterface,
    _Inout_ VOID* ClientContext
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_CmApi_PropertyUint32Get(
    _In_ DMFMODULE DmfModule,
    _In_ GUID* PropertyInterfaceGuid,
    _In_ PDEVPROPKEY PropertyKey,
    _Out_ UINT32* Value
    );


#endif // defined(DMF_USER_MODE)

// eof: Dmf_CmApi.h
//
