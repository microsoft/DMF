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

#endif // defined(DMF_USER_MODE)

// eof: Dmf_CmApi.h
//
