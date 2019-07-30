/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfDefinitions.h

Abstract:

    All DMF Client Drivers include this file. It contains access to all the Client facing DMF definitions
    as well as all the Modules. This file is also automatically included by Modules and the Framework.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Compiler warning filters.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus)

// Allow DMF Modules to disable false positive SAL warnings easily when 
// compiled as C++ file.
//
// NOTE: Use with caution. First, verify there are no legitimate warnings. Then, use this option.
//
#if (defined(DMF_SAL_CPP_FILTER) && defined(__cplusplus)) || defined(DMF_USER_MODE)
    // Disable some SAL warnings because they are false positives.
    // NOTE: The pointers are checked via ASSERTs.
    //

    // 'Dereferencing NULL pointer'.
    //
    #pragma warning(disable:6011)
    // '<argument> may be NULL'.
    //
    #pragma warning(disable:6387)
#endif // (defined(DMF_SAL_CPP_FILTER) && defined(__cplusplus)) || defined(DMF_USER_MODE)

// 'warning C4201: nonstandard extension used: nameless struct/union'
//
#pragma warning(disable:4201)

// Check that the Windows version is Win10 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_OR_LATER (NTDDI_WIN10_RS3 && (NTDDI_VERSION >= NTDDI_WIN10))

// Check that the Windows version is RS3 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_RS3_OR_LATER (NTDDI_WIN10_RS3 && (NTDDI_VERSION >= NTDDI_WIN10_RS3))

// Check that the Windows version is RS5 or later. The supported versions are defined in sdkddkver.h.
//
#define IS_WIN10_RS5_OR_LATER (NTDDI_WIN10_RS5 && (NTDDI_VERSION >= NTDDI_WIN10_RS5))

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// All include files needed by all Modules and the Framework.
// This ensures that all Modules always compile together so that any Module can always be used with any other
// Module without having to deal with include file dependencies.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Some environments use DBG instead of DEBUG. DMF uses DEBUG so, define DEBUG in that case.
//
#if DBG
    #if !defined(DEBUG)
        #define DEBUG
    #endif
#endif

// All DMF Modules need these.
//
#include <stdlib.h>
#include <sal.h>
#if defined(DMF_USER_MODE)
    #include <windows.h>
    #include <stdio.h>
    #include <wdf.h>
    #include <Objbase.h>
    // NOTE: This file includes poclass.h. Do not include that again
    //       otherwise, redefinition errors will occur.
    //
    #include <batclass.h>
    #include <hidclass.h>
    #include <powrprof.h>
    #include <usbiodef.h>
    #include <cguid.h>
    #include <guiddef.h>
    #include <wdmguid.h>
    #include <cfgmgr32.h>
    #include <ndisguid.h>
    #include <strsafe.h>
    #include <ndisguid.h>
    // TODO: Add support for USB in User Mode drivers.
    //
    // Turn this on to debug asserts in UMDF.
    // Normal assert() causes a crash in UMDF which causes UMDF to just disable the driver
    // without showing what assert failed.
    //
    #if defined(DEBUG)
        #if defined(USE_ASSERT_BREAK)
            // It means, check a condition...If it is false, break into debugger.
            //
            #pragma warning(disable:4127)
            #if defined(ASSERT)
                #undef ASSERT
            #endif // defined(ASSERT)
            #define ASSERT(X)   ((! (X)) ? DebugBreak() : TRUE)
        #else
            #if !defined(ASSERT)
                // It means, use native assert().
                //
                #include <assert.h>
                #define ASSERT(X)   assert(X)
            #endif // !defined(ASSERT)
        #endif // defined(USE_ASSERT_BREAK)
    #else
        #if !defined(ASSERT)
            // It means, do not assert at all.
            //
            #define ASSERT(X) (VOID(0))
        #endif // !defined(ASSERT)
    #endif // defined(DEBUG)
#else
    #include <ntifs.h>
    #include <wdm.h>
    #include <ntddk.h>
    #include <ntstatus.h>
    #include <ntintsafe.h>
    // TODO: Add this after Dmf_Registry supports this definition properly.
    // #define NTSTRSAFE_LIB
    //
    #include <ntstrsafe.h>
    #include <wdf.h>
    // NOTE: This file has be listed here. Listing it later causes many redefinition compilation errors.
    //
    #include <acpiioct.h>
    #include <wmiguid.h>
    #include <ntddstor.h>
    #include <stdarg.h>
    #include <ntddser.h>
    #include <guiddef.h>
    #include <wdmguid.h>
    #include <ntddvdeo.h>
    #include <spb.h>
    // NOTE: This file includes poclass.h. Do not include that again
    //       otherwise, redefinition errors will occur.
    //
    #include <batclass.h>
    #include <hidport.h>
    #include "usbdi.h"
    #include "usbdlib.h"
    #include <wdfusb.h>
    #include <wpprecorder.h>
#if IS_WIN10_RS3_OR_LATER
    #include <lkmdtel.h>
#endif // IS_WIN10_RS3_OR_LATER
    #include <intrin.h>
#endif // defined(DMF_USER_MODE)
#include <hidusage.h>
#include <hidpi.h>

// NOTE: This is necessary in order to avoid redefinition errors. It is not clear why
//       this is the case.
//
#define DEVPKEY_H_INCLUDED

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Client facing DMF Objects' definitions.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Declare an opaque handle representing a DMFMODULE (DMF_OBJECT). This is an opaque handle for the Clients.
//
DECLARE_HANDLE(DMFMODULE);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Forward declarations.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Forward declaration for use in DMF_ModuleInstanceCreate.
//
struct _DMF_MODULE_ATTRIBUTES;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Definitions to initialize Modules.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// NULL Client Driver Module Instance Name (In case Client Driver does not want to populate this field.)
// In this case, DMF Framework will use the Module Name by default.
//
#define DMF_CLIENT_MODULE_INSTANCE_NAME_DEFAULT             ""

// These are callbacks in the case of Asynchronous Device Open/Close operations.
// These tell the Client that the Module is opened/closed.
//
typedef NTSTATUS EVT_DMF_MODULE_OnDeviceNotificationOpen(_In_ DMFMODULE DmfModule);
typedef VOID EVT_DMF_MODULE_OnDeviceNotificationPostOpen(_In_ DMFMODULE DmfModule);
typedef VOID EVT_DMF_MODULE_OnDeviceNotificationPreClose(_In_ DMFMODULE DmfModule);
typedef VOID EVT_DMF_MODULE_OnDeviceNotificationClose(_In_ DMFMODULE DmfModule);

// It means the Generic function is not overridden.
//
#define USE_GENERIC_CALLBACK      NULL

typedef struct
{
    // After a Module is open this callback is called.
    //
    EVT_DMF_MODULE_OnDeviceNotificationPostOpen* EvtModuleOnDeviceNotificationPostOpen;
    // Before a Module is closed this callback is called.
    //
    EVT_DMF_MODULE_OnDeviceNotificationPreClose* EvtModuleOnDeviceNotificationPreClose;
} DMF_MODULE_EVENT_CALLBACKS;

#define DECLARE_DMF_MODULE(ModuleName)                                                          \
                                                                                                \
WDF_DECLARE_CUSTOM_TYPE(ModuleName);                                                            \
                                                                                                \
_IRQL_requires_max_(PASSIVE_LEVEL)                                                              \
_Must_inspect_result_                                                                           \
NTSTATUS                                                                                        \
DMF_##ModuleName##_Create(                                                                      \
    _In_ WDFDEVICE Device,                                                                      \
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,                                            \
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,                                               \
    _Out_ DMFMODULE* DmfModule                                                                  \
    );                                                                                          \
                                                                                                \
__forceinline                                                                                   \
VOID                                                                                            \
DMF_##ModuleName##_ATTRIBUTES_INIT(                                                             \
    _Out_ volatile DMF_MODULE_ATTRIBUTES* Attributes                                            \
    )                                                                                           \
{                                                                                               \
    DMF_MODULE_ATTRIBUTES_INIT(Attributes,                                                      \
                               sizeof(DMF_CONFIG_##ModuleName##));                              \
    Attributes->InstanceCreator = DMF_##ModuleName##_Create;                                    \
}                                                                                               \
                                                                                                \
__forceinline                                                                                   \
VOID                                                                                            \
DMF_CONFIG_##ModuleName##_AND_ATTRIBUTES_INIT(                                                  \
    _Out_ DMF_CONFIG_##ModuleName##* ModuleConfig,                                              \
    _Out_ volatile DMF_MODULE_ATTRIBUTES* ModuleAttributes                                      \
    )                                                                                           \
{                                                                                               \
    RtlZeroMemory(ModuleConfig,                                                                 \
                  sizeof(DMF_CONFIG_##ModuleName##));                                           \
    DMF_##ModuleName##_ATTRIBUTES_INIT(ModuleAttributes);                                       \
    ModuleAttributes->ModuleConfigPointer = ModuleConfig;                                       \
}                                                                                               \

#define DECLARE_DMF_MODULE_NO_CONFIG(ModuleName)                                                \
                                                                                                \
WDF_DECLARE_CUSTOM_TYPE(ModuleName);                                                            \
                                                                                                \
_IRQL_requires_max_(PASSIVE_LEVEL)                                                              \
_Must_inspect_result_                                                                           \
NTSTATUS                                                                                        \
DMF_##ModuleName##_Create(                                                                      \
    _In_ WDFDEVICE Device,                                                                      \
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,                                            \
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,                                               \
    _Out_ DMFMODULE* DmfModule                                                                  \
    );                                                                                          \
                                                                                                \
__forceinline                                                                                   \
VOID                                                                                            \
DMF_##ModuleName##_ATTRIBUTES_INIT(                                                             \
    _Out_ DMF_MODULE_ATTRIBUTES* Attributes                                                     \
    )                                                                                           \
{                                                                                               \
    DMF_MODULE_ATTRIBUTES_INIT(Attributes,                                                      \
                               0);                                                              \
    Attributes->InstanceCreator = DMF_##ModuleName##_Create;                                    \
}                                                                                               \

typedef NTSTATUS DMF_ModuleInstanceCreate(_In_ WDFDEVICE Device,
                                          _In_ struct _DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
                                          _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes, 
                                          _Out_ DMFMODULE* DmfModule);

typedef VOID DMF_TransportModuleAdd(_In_ DMFMODULE DmfModule,
                                    _In_ struct _DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
                                    _In_ PVOID DmfModuleInit);

_IRQL_requires_max_(DISPATCH_LEVEL)
typedef NTSTATUS DMF_ModuleTransportMethod(_In_ DMFMODULE DmfModule,
                                           _In_ ULONG Message,
                                           _In_reads_(InputBufferSize) VOID* InputBuffer,
                                           _In_ size_t InputBufferSize,
                                           _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
                                           _In_ size_t OutputBufferSize);

typedef struct _DMF_MODULE_ATTRIBUTES
{
    // Size of this Structure.
    //
    ULONG SizeOfHeader; 
    // It is a pointer to the Module Specific Config.
    //
    VOID* ModuleConfigPointer;
    // It is the size of the Module Specific Config which is pointed to 
    // by ModuleConfigPointer.
    //
    ULONG SizeOfModuleSpecificConfig;
    // The address of the function that creates the DMF Module.
    //
    DMF_ModuleInstanceCreate* InstanceCreator;
    // Callbacks to the Client Driver (if any).
    //
    DMF_MODULE_EVENT_CALLBACKS* ClientCallbacks;
    // Address provided by the Client Driver that receives the newly created handle.
    //
    DMFMODULE* ResultantDmfModule;
    // NULL Terminated Client Driver Module Instance Name.
    //
    PSTR ClientModuleInstanceName;
    // Dynamic Module indicator.
    // Modules that are part of a Module Collection are considered to be 
    // Static Modules since they are created/destroyed by DMF and always 
    // available to the Client.
    // Dynamic Modules are created/destroyed by the Client on demand and
    // are not part of a Module Collection.
    // Dynamic Modules have special code paths so this flag is necessary.
    //
    BOOLEAN DynamicModuleImmediate;
    // A flag to indicate if the a Module in the parent path was created dynamically. 
    //
    BOOLEAN DynamicModule;
    // When a Module Transport is required, this callback must be set so that the 
    // Client can attach Transport Modules.
    //
    DMF_TransportModuleAdd* TransportModuleAdd;
    // TRUE if Client wants the Module options to be set to MODULE_OPTIONS_PASSIVE.
    // NOTE: Module Options must be set to MODULE_OPTIONS_DISPATCH_MAXIMUM.
    //
    BOOLEAN PassiveLevel;
    // Indicates that this Module is a Transport Module.
    //
    BOOLEAN IsTransportModule;
} DMF_MODULE_ATTRIBUTES;

__forceinline
VOID
DMF_MODULE_ATTRIBUTES_INIT(
    _Out_ volatile DMF_MODULE_ATTRIBUTES* Attributes,
    _In_ ULONG SizeOfModuleSpecificConfig
    ) 
{
    RtlZeroMemory((void*)Attributes, 
                  sizeof(DMF_MODULE_ATTRIBUTES));
    Attributes->SizeOfHeader = sizeof(DMF_MODULE_ATTRIBUTES);
    Attributes->SizeOfModuleSpecificConfig = SizeOfModuleSpecificConfig;
    Attributes->ClientModuleInstanceName = DMF_CLIENT_MODULE_INSTANCE_NAME_DEFAULT;
    Attributes->DynamicModuleImmediate = TRUE;
    Attributes->DynamicModule = TRUE;
    Attributes->ClientCallbacks = NULL;
}

__forceinline
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_MODULE_ATTRIBUTES_EVENT_CALLBACKS_INIT(
    _Out_ DMF_MODULE_ATTRIBUTES* ModuleAttributes,
    _Out_ DMF_MODULE_EVENT_CALLBACKS* DmfModuleEventCallbacks
    )
/*++

Routine Description:

    Initialize the DMF_MODULE_EVENT_CALLBACKS structure. Set Module Attributes to point to the callbacks so
    that the Client does not need to do so.

Arguments:

    ModuleAttributes - The Module Attributes to initialize.
    DmfModuleEventCallbacks - The Module Callbacks to initialize.

Return Value:

    None

--*/
{
    ASSERT(ModuleAttributes != NULL);
    ASSERT(DmfModuleEventCallbacks != NULL);

    RtlZeroMemory(DmfModuleEventCallbacks,
                  sizeof(DMF_MODULE_EVENT_CALLBACKS));

    ModuleAttributes->ClientCallbacks = DmfModuleEventCallbacks;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DMF Container Macro
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(DMF_USER_MODE)

#define DMF_DEFAULT_DEVICEADD_WITH_BRANCHTRACK(DmfEvtDeviceAdd, DmfDeviceModulesAdd, BranchTrackInitialize, BranchTrackName, BranchTrackEntriesOverride)   \
                                                                                                                                                           \
_Use_decl_annotations_                                                                                                                                     \
NTSTATUS                                                                                                                                                   \
DmfEvtDeviceAdd(                                                                                                                                           \
    _In_ WDFDRIVER Driver,                                                                                                                                 \
    _Inout_ PWDFDEVICE_INIT DeviceInit                                                                                                                     \
    )                                                                                                                                                      \
{                                                                                                                                                          \
    NTSTATUS ntStatus;                                                                                                                                     \
    WDFDEVICE device;                                                                                                                                      \
    PDMFDEVICE_INIT dmfDeviceInit;                                                                                                                         \
    DMF_EVENT_CALLBACKS dmfCallbacks;                                                                                                                      \
    DMF_CONFIG_BranchTrack branchTrackModuleConfig;                                                                                                        \
                                                                                                                                                           \
    UNREFERENCED_PARAMETER(Driver);                                                                                                                        \
                                                                                                                                                           \
    PAGED_CODE();                                                                                                                                          \
                                                                                                                                                           \
    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);                                                                                                 \
                                                                                                                                                           \
    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit,                                                                                             \
                                                NULL);                                                                                                     \
    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit,                                                                                                   \
                                          NULL);                                                                                                           \
    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit,                                                                                          \
                                                   NULL);                                                                                                  \
                                                                                                                                                           \
    WdfDeviceInitSetDeviceType(DeviceInit,                                                                                                                 \
                               FILE_DEVICE_UNKNOWN);                                                                                                       \
    WdfDeviceInitSetExclusive(DeviceInit,                                                                                                                  \
                              FALSE);                                                                                                                      \
    WdfDeviceInitSetIoType(DeviceInit,                                                                                                                     \
                           WdfDeviceIoBuffered);                                                                                                           \
                                                                                                                                                           \
    ntStatus = WdfDeviceCreate(&DeviceInit,                                                                                                                \
                               WDF_NO_OBJECT_ATTRIBUTES,                                                                                                   \
                               &device);                                                                                                                   \
    if (! NT_SUCCESS(ntStatus))                                                                                                                            \
    {                                                                                                                                                      \
        goto Exit;                                                                                                                                         \
    }                                                                                                                                                      \
                                                                                                                                                           \
    if (strlen(BranchTrackName) != 0)                                                                                                                      \
    {                                                                                                                                                      \
        DMF_BranchTrack_CONFIG_INIT(&branchTrackModuleConfig,                                                                                              \
                                    BranchTrackName);                                                                                                      \
        branchTrackModuleConfig.BranchesInitialize = BranchTrackInitialize;                                                                                \
        ULONG branchTrackEntriesOverride = BranchTrackEntriesOverride;                                                                                     \
        if (branchTrackEntriesOverride != 0)                                                                                                               \
        {                                                                                                                                                  \
            branchTrackModuleConfig.MaximumBranches = BranchTrackEntriesOverride;                                                                          \
        }                                                                                                                                                  \
        DMF_DmfDeviceInitSetBranchTrackConfig(dmfDeviceInit,                                                                                               \
                                              &branchTrackModuleConfig);                                                                                   \
    }                                                                                                                                                      \
                                                                                                                                                           \
    DMF_EVENT_CALLBACKS_INIT(&dmfCallbacks);                                                                                                               \
    dmfCallbacks.EvtDmfDeviceModulesAdd = DmfDeviceModulesAdd;                                                                                             \
    DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,                                                                                                      \
                                       &dmfCallbacks);                                                                                                     \
                                                                                                                                                           \
    ntStatus = DMF_ModulesCreate(device,                                                                                                                   \
                                 &dmfDeviceInit);                                                                                                          \
    if (! NT_SUCCESS(ntStatus))                                                                                                                            \
    {                                                                                                                                                      \
        goto Exit;                                                                                                                                         \
    }                                                                                                                                                      \
                                                                                                                                                           \
Exit:                                                                                                                                                      \
                                                                                                                                                           \
    if (dmfDeviceInit != NULL)                                                                                                                             \
    {                                                                                                                                                      \
        DMF_DmfDeviceInitFree(&dmfDeviceInit);                                                                                                             \
    }                                                                                                                                                      \
                                                                                                                                                           \
    return ntStatus;                                                                                                                                       \
}                                                                                                                                                          \

#define DMF_DEFAULT_DRIVERENTRY(DmfDriverEntry, DmfDriverContextCleanup, DmfEvtDeviceAdd)                     \
                                                                                                              \
_Use_decl_annotations_                                                                                        \
NTSTATUS                                                                                                      \
DmfDriverEntry(                                                                                               \
    _In_ PDRIVER_OBJECT DriverObject,                                                                         \
    _In_ PUNICODE_STRING RegistryPath                                                                         \
    )                                                                                                         \
{                                                                                                             \
    WDF_DRIVER_CONFIG config;                                                                                 \
    NTSTATUS ntStatus;                                                                                        \
    WDF_OBJECT_ATTRIBUTES attributes;                                                                         \
                                                                                                              \
    WPP_INIT_TRACING(DriverObject, RegistryPath);                                                             \
                                                                                                              \
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);                                                                  \
    attributes.EvtCleanupCallback = DmfDriverContextCleanup;                                                  \
    WDF_DRIVER_CONFIG_INIT(&config,                                                                           \
                           DmfEvtDeviceAdd);                                                                  \
                                                                                                              \
    ntStatus = WdfDriverCreate(DriverObject,                                                                  \
                               RegistryPath,                                                                  \
                               &attributes,                                                                   \
                               &config,                                                                       \
                               WDF_NO_HANDLE);                                                                \
    if (! NT_SUCCESS(ntStatus))                                                                               \
    {                                                                                                         \
        WPP_CLEANUP(DriverObject);                                                                            \
        goto Exit;                                                                                            \
    }                                                                                                         \
                                                                                                              \
Exit:                                                                                                         \
                                                                                                              \
    return ntStatus;                                                                                          \
}                                                                                                             \
                                                                                                              \

#define DMF_DEFAULT_DRIVERCLEANUP(DmfDriverContextCleanup)                                                    \
                                                                                                              \
_Use_decl_annotations_                                                                                        \
VOID                                                                                                          \
DmfDriverContextCleanup(                                                                                      \
    _In_ WDFOBJECT DriverObject                                                                               \
    )                                                                                                         \
{                                                                                                             \
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));                                        \
}                                                                                                             \
                                                                                                              \

#else

#define DMF_DEFAULT_DEVICEADD_WITH_BRANCHTRACK(DmfEvtDeviceAdd, DmfDeviceModuleAdd, BranchTrackInitialize, BranchTrackName, BranchTrackEntriesOverride)    \
                                                                                                                                                           \
_Use_decl_annotations_                                                                                                                                     \
NTSTATUS                                                                                                                                                   \
DmfEvtDeviceAdd(                                                                                                                                           \
    _In_ WDFDRIVER Driver,                                                                                                                                 \
    _Inout_ PWDFDEVICE_INIT DeviceInit                                                                                                                     \
    )                                                                                                                                                      \
{                                                                                                                                                          \
    NTSTATUS ntStatus;                                                                                                                                     \
    WDFDEVICE device;                                                                                                                                      \
    PDMFDEVICE_INIT dmfDeviceInit;                                                                                                                         \
    DMF_EVENT_CALLBACKS dmfCallbacks;                                                                                                                      \
    DMF_CONFIG_BranchTrack branchTrackModuleConfig;                                                                                                        \
                                                                                                                                                           \
    UNREFERENCED_PARAMETER(Driver);                                                                                                                        \
                                                                                                                                                           \
    PAGED_CODE();                                                                                                                                          \
                                                                                                                                                           \
    dmfDeviceInit = DMF_DmfDeviceInitAllocate(DeviceInit);                                                                                                 \
                                                                                                                                                           \
    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit,                                                                                             \
                                                NULL);                                                                                                     \
    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit,                                                                                                   \
                                          NULL);                                                                                                           \
    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit,                                                                                          \
                                                   NULL);                                                                                                  \
                                                                                                                                                           \
    WdfDeviceInitSetIoType(DeviceInit,                                                                                                                     \
                           WdfDeviceIoBuffered);                                                                                                           \
                                                                                                                                                           \
    ntStatus = WdfDeviceCreate(&DeviceInit,                                                                                                                \
                               WDF_NO_OBJECT_ATTRIBUTES,                                                                                                   \
                               &device);                                                                                                                   \
    if (! NT_SUCCESS(ntStatus))                                                                                                                            \
    {                                                                                                                                                      \
        goto Exit;                                                                                                                                         \
    }                                                                                                                                                      \
                                                                                                                                                           \
    if (strlen(BranchTrackName) != 0)                                                                                                                      \
    {                                                                                                                                                      \
        DMF_BranchTrack_CONFIG_INIT(&branchTrackModuleConfig,                                                                                              \
                                    BranchTrackName);                                                                                                      \
        branchTrackModuleConfig.BranchesInitialize = BranchTrackInitialize;                                                                                \
        ULONG branchTrackEntriesOverride = BranchTrackEntriesOverride;                                                                                     \
        if (branchTrackEntriesOverride != 0)                                                                                                               \
        {                                                                                                                                                  \
            branchTrackModuleConfig.MaximumBranches = BranchTrackEntriesOverride;                                                                          \
        }                                                                                                                                                  \
        DMF_DmfDeviceInitSetBranchTrackConfig(dmfDeviceInit,                                                                                               \
                                              &branchTrackModuleConfig);                                                                                   \
    }                                                                                                                                                      \
                                                                                                                                                           \
    DMF_EVENT_CALLBACKS_INIT(&dmfCallbacks);                                                                                                               \
    dmfCallbacks.EvtDmfDeviceModulesAdd = DmfDeviceModuleAdd;                                                                                              \
    DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,                                                                                                      \
                                       &dmfCallbacks);                                                                                                     \
                                                                                                                                                           \
    ntStatus = DMF_ModulesCreate(device,                                                                                                                   \
                                 &dmfDeviceInit);                                                                                                          \
    if (! NT_SUCCESS(ntStatus))                                                                                                                            \
    {                                                                                                                                                      \
        goto Exit;                                                                                                                                         \
    }                                                                                                                                                      \
                                                                                                                                                           \
Exit:                                                                                                                                                      \
                                                                                                                                                           \
    if (dmfDeviceInit != NULL)                                                                                                                             \
    {                                                                                                                                                      \
        DMF_DmfDeviceInitFree(&dmfDeviceInit);                                                                                                             \
    }                                                                                                                                                      \
                                                                                                                                                           \
    return ntStatus;                                                                                                                                       \
}                                                                                                                                                          \

#if (UMDF_VERSION_MAJOR > 2) || (UMDF_VERSION_MINOR >= 15)
#define DMF_DEFAULT_DRIVERENTRY(DmfDriverEntry, DmfDriverContextCleanup, DmfEvtDeviceAdd, UserModeTracingId)  \
                                                                                                              \
_Use_decl_annotations_                                                                                        \
NTSTATUS                                                                                                      \
DmfDriverEntry(                                                                                               \
    _In_ PDRIVER_OBJECT DriverObject,                                                                         \
    _In_ PUNICODE_STRING RegistryPath                                                                         \
    )                                                                                                         \
{                                                                                                             \
    WDF_DRIVER_CONFIG config;                                                                                 \
    NTSTATUS ntStatus;                                                                                        \
    WDF_OBJECT_ATTRIBUTES attributes;                                                                         \
                                                                                                              \
    WPP_INIT_TRACING(DriverObject, RegistryPath);                                                             \
                                                                                                              \
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);                                                                  \
    attributes.EvtCleanupCallback = DmfDriverContextCleanup;                                                  \
    WDF_DRIVER_CONFIG_INIT(&config,                                                                           \
                           DmfEvtDeviceAdd);                                                                  \
                                                                                                              \
    ntStatus = WdfDriverCreate(DriverObject,                                                                  \
                               RegistryPath,                                                                  \
                               &attributes,                                                                   \
                               &config,                                                                       \
                               WDF_NO_HANDLE);                                                                \
    if (! NT_SUCCESS(ntStatus))                                                                               \
    {                                                                                                         \
        WPP_CLEANUP(DriverObject);                                                                            \
        goto Exit;                                                                                            \
    }                                                                                                         \
                                                                                                              \
Exit:                                                                                                         \
                                                                                                              \
    return ntStatus;                                                                                          \
}                                                                                                             \
                                                                                                              \

#else
#define DMF_DEFAULT_DRIVERENTRY(DmfDriverEntry, DmfDriverContextCleanup, DmfEvtDeviceAdd, UserModeTracingId)  \
                                                                                                              \
_Use_decl_annotations_                                                                                        \
NTSTATUS                                                                                                      \
DmfDriverEntry(                                                                                               \
    _In_ PDRIVER_OBJECT DriverObject,                                                                         \
    _In_ PUNICODE_STRING RegistryPath                                                                         \
    )                                                                                                         \
{                                                                                                             \
    WDF_DRIVER_CONFIG config;                                                                                 \
    NTSTATUS ntStatus;                                                                                        \
    WDF_OBJECT_ATTRIBUTES attributes;                                                                         \
                                                                                                              \
    WPP_INIT_TRACING(UserModeTracingId);                                                                      \
                                                                                                              \
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);                                                                  \
    attributes.EvtCleanupCallback = DmfDriverContextCleanup;                                                  \
    WDF_DRIVER_CONFIG_INIT(&config,                                                                           \
                           DmfEvtDeviceAdd);                                                                  \
                                                                                                              \
    ntStatus = WdfDriverCreate(DriverObject,                                                                  \
                               RegistryPath,                                                                  \
                               &attributes,                                                                   \
                               &config,                                                                       \
                               WDF_NO_HANDLE);                                                                \
    if (! NT_SUCCESS(ntStatus))                                                                               \
    {                                                                                                         \
        WPP_CLEANUP();                                                                                        \
        goto Exit;                                                                                            \
    }                                                                                                         \
                                                                                                              \
Exit:                                                                                                         \
                                                                                                              \
    return ntStatus;                                                                                          \
}                                                                                                             \
                                                                                                              \

#endif // (UMDF_VERSION_MAJOR > 2) || (UMDF_VERSION_MINOR >= 15)

#if (UMDF_VERSION_MAJOR > 2) || (UMDF_VERSION_MINOR >= 15)
#define DMF_DEFAULT_DRIVERCLEANUP(DmfDriverContextCleanup)                                                    \
                                                                                                              \
_Use_decl_annotations_                                                                                        \
VOID                                                                                                          \
DmfDriverContextCleanup(                                                                                      \
    _In_ WDFOBJECT DriverObject                                                                               \
    )                                                                                                         \
{                                                                                                             \
    UNREFERENCED_PARAMETER(DriverObject);                                                                     \
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));                                        \
}                                                                                                             \

#else
#define DMF_DEFAULT_DRIVERCLEANUP(DmfDriverContextCleanup)                                                    \
                                                                                                              \
_Use_decl_annotations_                                                                                        \
VOID                                                                                                          \
DmfDriverContextCleanup(                                                                                      \
    _In_ WDFOBJECT DriverObject                                                                               \
    )                                                                                                         \
{                                                                                                             \
    UNREFERENCED_PARAMETER(DriverObject);                                                                     \
    WPP_CLEANUP();                                                                                            \
}                                                                                                             \
                                                                                                              \

#endif // (UMDF_VERSION_MAJOR > 2) || (UMDF_VERSION_MINOR >= 15)

#endif // !defined(DMF_USER_MODE)

#define DMF_DEFAULT_DEVICEADD(DmfEvtDeviceAdd, DmfDeviceModuleAdd)                                            \
DMF_DEFAULT_DEVICEADD_WITH_BRANCHTRACK(DmfEvtDeviceAdd, DmfDeviceModuleAdd, NULL, "", 0)                      \

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Client facing prototypes into DMF.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFDEVICE
DMF_ParentDeviceGet(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFDEVICE
DMF_FilterDeviceGet(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
DMFMODULE
DMF_ParentModuleGet(
    _In_ DMFMODULE DmfModule
    );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Interface Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Basic Interface Definitions
// ---------------------------
//

// Declare an opaque handle representing a DMFMODULE (DMF_OBJECT). This is an opaque handle for the Clients.
//
DECLARE_HANDLE(DMFINTERFACE);

// Structures used by DMF Core.
//

typedef enum
{
    InterfaceState_Invalid = 0,
    // The Interface has been created.
    //
    InterfaceState_Created,
    // The Interface is about to open.
    //
    InterfaceState_Opening,
    // The Interface has been opened.
    //
    InterfaceState_Opened,
    // The Interface is about to close.
    //
    InterfaceState_Closing,
    // The Interface has been closed.
    //
    InterfaceState_Closed,
    // The Interface has been destroyed.
    //
    InterfaceState_Destroyed,
    // Sentinel.
    //
    InterfaceState_Last
} InterfaceStateType;

typedef enum
{
    Interface_Invalid = 0,
    // The Interface is a Transport.
    //
    Interface_Transport,
    // The Interface is a Protocol.
    //
    Interface_Protocol,
    // Sentinel.
    //
    Interface_Last
} InterfaceType;

// Function used to initiate binding from the Interface Protocol.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_INTERFACE_ProtocolBind(
    _In_ DMFINTERFACE DmfInterface
    );

// Function used to unbind two Modules.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_ProtocolUnbind(
    _In_ DMFINTERFACE DmfInterface
    );

// Function used to Notify the Protocol and Transport Modules that binding is complete.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_PostBind(
    _In_ DMFINTERFACE DmfInterface
    );

// Function used to Notify the Protocol and Transport Modules that unbinding is about to start.
//
typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_PreUnbind(
    _In_ DMFINTERFACE DmfInterface
    );

// A generic Interface Descriptor.
//
typedef struct _DMF_INTERFACE_DESCRIPTOR
{
    // This structure is wrapped by multiple structures. 
    // The size represents the size of the outer most structure. 
    //
    ULONG Size;
    // Name of this Interface.
    //
    PSTR InterfaceName;
    // Type of this Interface.
    //
    InterfaceType InterfaceType;
    // WDF Object attributes describing the Declaration Data (Calls supported by this Interface).
    //
    WDF_OBJECT_ATTRIBUTES DeclarationDataWdfAttributes;
    // If set, it means module has set ModuleInterfaceContextWdfAttributes
    //
    BOOLEAN ModuleInterfaceContextSet;
    // WDF Object attributes describing the Module's Interface context.
    // A context will be allocated for each DMFINTERFACE instance. 
    //
    WDF_OBJECT_ATTRIBUTES ModuleInterfaceContextWdfAttributes;
    // Callback used to Notify the Module that Binding is complete.
    //
    EVT_DMF_INTERFACE_PostBind* DmfInterfacePostBind;
    // Callback used to Notify the Module that Unbinding is about to start.
    //
    EVT_DMF_INTERFACE_PreUnbind* DmfInterfacePreUnbind;
} DMF_INTERFACE_DESCRIPTOR;

// Transport specific Interface Descriptor.
//
typedef struct _DMF_INTERFACE_TRANSPORT_DESCRIPTOR
{
    // Common Interface Attributes.
    //
    DMF_INTERFACE_DESCRIPTOR GenericInterfaceDescriptor;
} DMF_INTERFACE_TRANSPORT_DESCRIPTOR;

// Protocol specific Interface Descriptor.
//
typedef struct _DMF_INTERFACE_PROTOCOL_DESCRIPTOR
{
    // Common Interface Attributes.
    //
    DMF_INTERFACE_DESCRIPTOR GenericInterfaceDescriptor;
    // Pointer to Interface Protocol's Bind function.
    //
    EVT_DMF_INTERFACE_ProtocolBind* DmfInterfaceProtocolBind;
    // Pointer to Interface Protocol's Unbind function.
    //
    EVT_DMF_INTERFACE_ProtocolUnbind* DmfInterfaceProtocolUnbind;
} DMF_INTERFACE_PROTOCOL_DESCRIPTOR;

// Module Methods.
// ---------------
#define DMF_INTERFACE_PROTOCOL_DESCRIPTOR_INIT(                                                 \
        DmfProtocolDescriptor,                                                                  \
        InterfaceName,                                                                          \
        DeclarationDataType,                                                                    \
        EvtProtocolBind,                                                                        \
        EvtProtocolUnbind,                                                                      \
        EvtPostBind,                                                                            \
        EvtPreUnbind                                                                            \
        )                                                                                       \
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR_INIT_INTERNAL(                                            \
        DmfProtocolDescriptor,                                                                  \
        InterfaceName,                                                                          \
        sizeof(DeclarationDataType),                                                            \
        EvtProtocolBind,                                                                        \
        EvtProtocolUnbind,                                                                      \
        EvtPostBind,                                                                            \
        EvtPreUnbind                                                                            \
        );                                                                                      \
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(                                                    \
        &(((DMF_INTERFACE_DESCRIPTOR*)(DmfProtocolDescriptor))->DeclarationDataWdfAttributes),  \
        DeclarationDataType);

#define DMF_INTERFACE_TRANSPORT_DESCRIPTOR_INIT(                                                \
        DmfTransportDescriptor,                                                                 \
        InterfaceName,                                                                          \
        DeclarationDataType,                                                                    \
        EvtPostBind,                                                                            \
        EvtPreUnbind                                                                            \
        )                                                                                       \
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR_INIT_INTERNAL(                                           \
        DmfTransportDescriptor,                                                                 \
        InterfaceName,                                                                          \
        sizeof(DeclarationDataType),                                                            \
        EvtPostBind,                                                                            \
        EvtPreUnbind                                                                            \
        );                                                                                      \
                                                                                                \
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(                                                    \
        &(((DMF_INTERFACE_DESCRIPTOR*)(DmfTransportDescriptor))->DeclarationDataWdfAttributes), \
        DeclarationDataType                                                                     \
        );

#define DMF_INTERFACE_DESCRIPTOR_SET_CONTEXT_TYPE(                                                      \
        DmfInterfaceDescriptor,                                                                         \
        ContextType                                                                                     \
        )                                                                                               \
    ((DMF_INTERFACE_DESCRIPTOR*)(DmfInterfaceDescriptor))->ModuleInterfaceContextSet = TRUE;            \
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(                                                            \
        &(((DMF_INTERFACE_DESCRIPTOR*)(DmfInterfaceDescriptor))->ModuleInterfaceContextWdfAttributes),  \
        ContextType);

// Method to add an Interface.
//
NTSTATUS
DMF_ModuleInterfaceDescriptorAdd(
    _Inout_ DMFMODULE DmfModule,
    _In_ DMF_INTERFACE_DESCRIPTOR* InterfaceDescriptor
    );

// Interface definitions and methods. 
// ----------------------------------
//

// Defines an accessor methods for the Protocol and Transport Declaration Data. 
//
#define DECLARE_DMF_INTERFACE(InterfaceName)                                                                                                                                                 \
                                                                                                                                                                                    \
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DMF_INTERFACE_PROTOCOL_##InterfaceName##_DECLARATION_DATA, ##InterfaceName##ProtocolDeclarationDataGet);                                             \
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DMF_INTERFACE_TRANSPORT_##InterfaceName##_DECLARATION_DATA, ##InterfaceName##TransportDeclarationDataGet);                                           \
                                                                                                                                                                                    \

// Methods used by DMF Interfaces.
//
DMFMODULE
DMF_InterfaceTransportModuleGet(
    _In_ DMFINTERFACE DmfInterface
    );

DMFMODULE
DMF_InterfaceProtocolModuleGet(
    _In_ DMFINTERFACE DmfInterface
    );

VOID*
DMF_InterfaceTransportDeclarationDataGet(
    _In_ DMFINTERFACE DmfInterface
    );

VOID*
DMF_InterfaceProtocolDeclarationDataGet(
    _In_ DMFINTERFACE DmfInterface
    );

NTSTATUS
DMF_InterfaceReference(
    _Inout_ DMFINTERFACE DmfInterface
    );

VOID
DMF_InterfaceDereference(
    _Inout_ DMFINTERFACE DmfInterface
    );

// Client Methods for Binding/Unbinding.
// -------------------------------------
//
// Bind Protocol and Transport.
//

// Method to bind Protocol and Transport.
//
NTSTATUS
DMF_ModuleInterfaceBind(
    _In_ DMFMODULE ProtocolModule,
    _In_ DMFMODULE TransportModule,
    _In_ DMF_INTERFACE_PROTOCOL_DESCRIPTOR* ProtocolDescriptor,
    _In_ DMF_INTERFACE_TRANSPORT_DESCRIPTOR* TransportDescriptor
    );

// Method to un-bind Protocol and Transport.
//
VOID
DMF_ModuleInterfaceUnbind(
    _In_ DMFMODULE ProtocolModule,
    _In_ DMFMODULE TransportModule,
    _In_ DMF_INTERFACE_PROTOCOL_DESCRIPTOR* ProtocolDescriptor,
    _In_ DMF_INTERFACE_TRANSPORT_DESCRIPTOR* TransportDescriptor
    );

#define DMF_INTERFACE_BIND(ProtocolModule,                                                                                                                                          \
                           TransportModule,                                                                                                                                         \
                           InterfaceName)                                                                                                                                           \
        DMF_ModuleInterfaceBind(ProtocolModule,                                                                                                                                     \
                                TransportModule,                                                                                                                                    \
                                (DMF_INTERFACE_PROTOCOL_DESCRIPTOR*)##InterfaceName##ProtocolDeclarationDataGet(ProtocolModule),                                                    \
                                (DMF_INTERFACE_TRANSPORT_DESCRIPTOR*)##InterfaceName##TransportDeclarationDataGet(TransportModule))                                                 \
                                                                                                                                                                                    \

// Un-bind Protocol and Transport.
//
#define DMF_INTERFACE_UNBIND(ProtocolModule,                                                                                                                                        \
                             TransportModule,                                                                                                                                       \
                             InterfaceName)                                                                                                                                         \
        DMF_ModuleInterfaceUnbind(ProtocolModule,                                                                                                                                   \
                                  TransportModule,                                                                                                                                  \
                                  (DMF_INTERFACE_PROTOCOL_DESCRIPTOR*)##InterfaceName##ProtocolDeclarationDataGet(ProtocolModule),                                                  \
                                  (DMF_INTERFACE_TRANSPORT_DESCRIPTOR*)##InterfaceName##TransportDeclarationDataGet(TransportModule))                                               \
                                                                                                                                                                                    \

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Features
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

#include "Dmf_BranchTrack.h"
#include "Dmf_LiveKernelDump.h"

// Feature Modules are treated like any other Module. However, they are always initialized and stored
// in the order specified below so that the rest of the driver always knows where to find them.
//
typedef enum
{
    DmfFeature_Invalid = 0,
    DmfFeature_BranchTrack,
    DmfFeature_LiveKernelDump,
    DmfFeature_NumberOfFeatures
} DmfFeatureType;

#define DMFFEATURE_NAME_BranchTrack    "BranchTrack"
#define DMFFEATURE_NAME_LiveKernelDump "LiveKernelDump"

DMFMODULE
DMF_FeatureModuleGetFromDevice(
    _In_ WDFDEVICE Device,
    _In_ DmfFeatureType DmfFeature
    );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Device Init 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Forward declare structures needed later header files
//
typedef struct DMFDEVICE_INIT *PDMFDEVICE_INIT;

#define PDMFMODULE_INIT PVOID

typedef
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_DEVICE_MODULES_ADD(
    _In_ WDFDEVICE Device,
    _In_ PDMFMODULE_INIT DmfModuleInit
    );

typedef EVT_DMF_DEVICE_MODULES_ADD* PFN_DMF_DEVICE_MODULES_ADD;

typedef struct _DMF_EVENT_CALLBACKS 
{
    // Size of this structure in bytes.
    //
    ULONG Size;

    PFN_DMF_DEVICE_MODULES_ADD EvtDmfDeviceModulesAdd;
} DMF_EVENT_CALLBACKS;

VOID
FORCEINLINE
DMF_EVENT_CALLBACKS_INIT(
    _Out_ DMF_EVENT_CALLBACKS* Callbacks
    )
{
    RtlZeroMemory(Callbacks, sizeof(DMF_EVENT_CALLBACKS*));

    Callbacks->Size = sizeof(DMF_EVENT_CALLBACKS*);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitSetEventCallbacks(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _In_ DMF_EVENT_CALLBACKS* DmfEventCallbacks
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitSetBranchTrackConfig(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _In_opt_ DMF_CONFIG_BranchTrack* DmfBranchTrackModuleConfig
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitSetLiveKernelDumpConfig(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _In_ DMF_CONFIG_LiveKernelDump* DmfLiveKernelDumpModuleConfig
    );

_IRQL_requires_max_(PASSIVE_LEVEL)

PDMFDEVICE_INIT 
DMF_DmfDeviceInitAllocate(
    _In_opt_ PWDFDEVICE_INIT DeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
PDMFDEVICE_INIT 
DMF_DmfControlDeviceInitAllocate(
    _In_opt_ PWDFDEVICE_INIT DeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfControlDeviceInitSetClientDriverDevice(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _In_ WDFDEVICE Device
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_DmfDeviceInitFree(
    _In_ PDMFDEVICE_INIT* DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitHookPnpPowerEventCallbacks(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _Inout_opt_ PWDF_PNPPOWER_EVENT_CALLBACKS PnpPowerEventCallbacks
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitHookFileObjectConfig(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _Inout_opt_ PWDF_FILEOBJECT_CONFIG FileObjectConfig
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _Inout_opt_ PWDF_POWER_POLICY_EVENT_CALLBACKS PowerPolicyEventCallbacks
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfDeviceInitHookQueueConfig(
    _In_ PDMFDEVICE_INIT DmfDeviceInit,
    _Inout_ PWDF_IO_QUEUE_CONFIG QueueConfig
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfFdoSetFilter(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_DmfModuleAdd(
    _Inout_ PDMFMODULE_INIT DmfModuleInit,
    _In_ DMF_MODULE_ATTRIBUTES* ModuleAttributes,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _In_opt_ DMFMODULE* ResultantDmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModulesCreate(
    _In_ WDFDEVICE Device,
    _In_ PDMFDEVICE_INIT* DmfDeviceInit
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleConfigRetrieve(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(ModuleConfigSize) PVOID ModuleConfigPointer,
    _In_ size_t ModuleConfigSize
    );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Filter Driver Support (FilterControl API)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if !defined(DMF_USER_MODE)

_IRQL_always_function_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_FilterControl_DeviceCreate(
    _In_ WDFDEVICE Device,
    _In_opt_ DMF_CONFIG_BranchTrack* FilterBranchTrackConfig,
    _In_opt_ PWDF_IO_QUEUE_CONFIG QueueConfig,
    _In_ PWCHAR ControlDeviceName
    );

_IRQL_always_function_max_(PASSIVE_LEVEL)
VOID
DMF_FilterControl_DeviceDelete(
    _In_ WDFDEVICE ControlDevice
    );

#endif // !defined(DMF_USER_MODE)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Portable Api prototypes and dependencies.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Event structure for user/Kernel-mode portable event handling.
//
typedef struct _DMF_PORTABLE_EVENT
{
#if defined(DMF_USER_MODE)
    HANDLE Handle;
#else
    KEVENT Handle;
#endif // defined(DMF_USER_MODE)
} DMF_PORTABLE_EVENT;

typedef struct _DMF_PORTABLE_LOOKASIDELIST
{
#if defined(DMF_USER_MODE)
    WDF_OBJECT_ATTRIBUTES MemoryAttributes;
    POOL_TYPE PoolType;
    ULONG PoolTag;
    size_t BufferSize;
#else
    WDFLOOKASIDE WdflookasideList;
#endif // defined(DMF_USER_MODE)
} DMF_PORTABLE_LOOKASIDELIST;

typedef struct _DMF_PORTABLE_RUNDOWN
{
#if !defined(DMF_USER_MODE)
    EX_RUNDOWN_REF RundownRef;
#else
    // TODO: Equivalent code will go here. Change is pending.
    //       Until then, Client must implement another solution
    //       in User-mode.
    //
    ULONG PlaceHolder;
#endif // !defined(DMF_USER_MODE)
} DMF_PORTABLE_RUNDOWN_REF;

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID 
DMF_Portable_EventCreate(
    _Out_ DMF_PORTABLE_EVENT* EventPointer,
    _In_ EVENT_TYPE EventType,
    _In_ BOOLEAN State
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID 
DMF_Portable_EventSet(
    _In_ DMF_PORTABLE_EVENT* EventPointer
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID 
DMF_Portable_EventReset(
    _In_ DMF_PORTABLE_EVENT* EventPointer
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
DWORD 
DMF_Portable_EventWaitForSingleObject(
    _In_ DMF_PORTABLE_EVENT* EventPointer,
    _In_ BOOLEAN Alertable,
#if defined(DMF_USER_MODE)
    _In_ ULONG TimeoutMs
#else
    _In_opt_ PLARGE_INTEGER Timeout100nsPointer
#endif // defined(DMF_USER_MODE)
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
DWORD
DMF_Portable_EventWaitForMultiple(
    _In_ ULONG EventCount,
    _In_ DMF_PORTABLE_EVENT** EventPointer,
    _In_ BOOLEAN WaitForAll,
    _In_ BOOLEAN Alertable,
#if defined(DMF_USER_MODE)
    _In_ ULONG TimeoutMs
#else
    _In_opt_ PLARGE_INTEGER Timeout100nsPointer
#endif // defined(DMF_USER_MODE)
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID 
DMF_Portable_EventClose(
    _In_ DMF_PORTABLE_EVENT* EventPointer
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS 
DMF_Portable_LookasideListCreate(
    _In_ PWDF_OBJECT_ATTRIBUTES LookasideAttributes,
    _In_ size_t BufferSize,
    _In_ POOL_TYPE PoolType,
    _In_ PWDF_OBJECT_ATTRIBUTES MemoryAttributes,
    _In_ ULONG PoolTag,
    _Out_ DMF_PORTABLE_LOOKASIDELIST* LookasidePointer
    );

#if defined(DMF_USER_MODE)
_IRQL_requires_max_(PASSIVE_LEVEL)
#else
_IRQL_requires_max_(DISPATCH_LEVEL)
#endif // defined(DMF_USER_MODE)
NTSTATUS 
DMF_Portable_LookasideListCreateMemory(
    _In_ DMF_PORTABLE_LOOKASIDELIST* LookasidePointer,
    _Out_ WDFMEMORY* Memory
    );

VOID
DMF_Portable_Rundown_Initialize(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    );

VOID
DMF_Portable_Rundown_Reinitialize(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    );

BOOLEAN
DMF_Portable_Rundown_Acquire(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    );

VOID
DMF_Portable_Rundown_Release(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    );

VOID
DMF_Portable_Rundown_WaitForRundownProtectionRelease(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    );

VOID
DMF_Portable_Rundown_Completed(
    _Inout_ DMF_PORTABLE_RUNDOWN_REF* RundownRef
    );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// "Invoke" API prototypes 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

NTSTATUS
DMF_Invoke_DeviceCallbacksCreate(
    _In_ WDFDEVICE Device,
    _In_opt_ WDFCMRESLIST ResourcesRaw,
    _In_opt_ WDFCMRESLIST ResourcesTranslated,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

NTSTATUS
DMF_Invoke_DeviceCallbacksDestroy(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Miscellaneous Utility Functions and dependencies.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Definitions used in In Flight Recording.
//
#define DMF_IN_FLIGHT_RECORDER_SIZE_DEFAULT                 (1024)

// These definitions are required to ensure UMDF drivers compile with Modules that use custom in flight recorder.
// Since in flight recording is not supported in UMDF, these defines are used to ignore the ifr field in the trace.
//
#if defined(DMF_USER_MODE)
#define RECORDER_LOG                                                HANDLE
#define WPP_RECORDER_IFRLOG_LEVEL_FLAGS_FILTER(ifr, lvl, flags)     (lvl < TRACE_LEVEL_VERBOSE || WPP_CONTROL(WPP_BIT_ ## flags).AutoLogVerboseEnabled)
#define WPP_RECORDER_IFRLOG_LEVEL_FLAGS_ARGS(ifr, lvl, flags)       WPP_RECORDER_LEVEL_FLAGS_ARGS(lvl, flags)
#endif

// Definitions used in event logging.
//
#define DMF_EVENTLOG_UNIQUE_ID_ZERO                         (0)
#define DMF_EVENTLOG_TEXT_LENGTH_ZERO                       (0)
#define DMF_EVENTLOG_NUMBER_OF_INSERTION_STRINGS_ZERO       (0)
#define DMF_EVENTLOG_NUMBER_OF_FORMAT_STRINGS_ZERO          (0)
#define DMF_EVENTLOG_FORMAT_STRINGS_NULL                    (NULL)
#define DMF_EVENTLOG_TEXT_NULL                              (NULL)
#define DMF_EVENTLOG_MAXIMUM_NUMBER_OF_INSERTION_STRINGS    (8)
#define DMF_EVENTLOG_MAXIMUM_INSERTION_STRING_LENGTH        (300)
#define DMF_EVENTLOG_MAXIMUM_BYTES_IN_INSERTION_STRING      (DMF_EVENTLOG_MAXIMUM_INSERTION_STRING_LENGTH  * sizeof(WCHAR))

_Must_inspect_result_
NTSTATUS
DMF_Utility_UserModeAccessCreate(
    _In_ WDFDEVICE Device,
    _In_opt_ const GUID* DeviceInterfaceGuid,
    _In_opt_ WCHAR* SymbolicLinkName
    );

VOID
DMF_Utility_DelayMilliseconds(
    _In_ ULONG Milliseconds
    );

_Must_inspect_result_
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Utility_AclPropagateInDeviceStack(
    _In_ WDFDEVICE Device
    );

VOID
DMF_Utility_EventLoggingNamesGet(
    _In_ WDFDEVICE Device,
    _Out_ PCWSTR* DeviceName,
    _Out_ PCWSTR* Location
    );

GUID
DMF_Utility_ActivityIdFromRequest(
    _In_ WDFREQUEST Request
    );

GUID
DMF_Utility_ActivityIdFromDevice(
    _In_ WDFDEVICE Device
    );

BOOLEAN
DMF_Utility_IsEqualGUID(
    _In_ GUID* Guid1,
    _In_ GUID* Guid2
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteDriverObject(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ NTSTATUS ErrorCode,
    _In_ NTSTATUS FinalNtStatus,
    _In_ ULONG UniqueId,
    _In_ ULONG TextLength,
    _In_opt_ PCWSTR Text,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings,
    _In_ INT NumberOfInsertionStrings,
    ...
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteDriver(
    _In_ WDFDRIVER Driver,
    _In_ NTSTATUS ErrorCode,
    _In_ NTSTATUS Status,
    _In_ ULONG UniqueId,
    _In_ ULONG TextLength,
    _In_opt_ PCWSTR Text,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings,
    _In_ INT NumberOfInsertionStrings,
    ...
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteDevice(
    _In_ WDFDEVICE Device,
    _In_ NTSTATUS ErrorCode,
    _In_ NTSTATUS Status,
    _In_ ULONG UniqueId,
    _In_ ULONG TextLength,
    _In_opt_ PCWSTR Text,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings,
    _In_ INT NumberOfInsertionStrings,
    ...
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteDmfModule(
    _In_ DMFMODULE DmfModule,
    _In_ NTSTATUS ErrorCode,
    _In_ NTSTATUS Status,
    _In_ ULONG UniqueId,
    _In_ ULONG TextLength,
    _In_opt_ PCWSTR Text,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings,
    _In_ INT NumberOfInsertionStrings,
    ...
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Utility_EventLogEntryWriteUserMode(
    _In_ PWSTR Provider,
    _In_ WORD EventType,
    _In_ DWORD EventID,
    _In_ INT NumberOfFormatStrings,
    _In_opt_ PWCHAR* FormatStrings,
    _In_ INT NumberOfInsertionStrings,
    ...
    );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// LIST_ENTRY functions for User-Mode. (These are copied as-is from Wdm.h.
// Some Modules use LIST_ENTRY. To make those Modules work with both Kernel-mode and User-mode, these
// definitions are included here.)
// LIST_ENTRY functions are defined in UMDF starting from v23
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if defined(DMF_USER_MODE) && UMDF_VERSION_MINOR < 23

FORCEINLINE
VOID
InitializeListHead(
    _Out_ PLIST_ENTRY ListHead
    )
{
    ListHead->Flink = ListHead->Blink = ListHead;
    return;
}

_Must_inspect_result_
BOOLEAN
CFORCEINLINE
IsListEmpty(
    _In_ const LIST_ENTRY * ListHead
    )
{
    return (BOOLEAN)(ListHead->Flink == ListHead);
}

//++
//VOID
//FatalListEntryError (
//    _In_ VOID* p1,
//    _In_ VOID* p2,
//    _In_ VOID* p3
//    );
//
// Routine Description:
//
//    This routine reports a fatal list entry error.  It is implemented here as a
//    wrapper around RtlFailFast so that alternative reporting mechanisms (such
//    as simply logging and trying to continue) can be easily switched in.
//
// Arguments:
//
//    p1 - Supplies the first failure parameter.
//
//    p2 - Supplies the second failure parameter.
//
//    p3 - Supplies the third failure parameter.
//
//Return Value:
//
//    None.
//--

FORCEINLINE
VOID
FatalListEntryError(
    _In_ VOID* p1,
    _In_ VOID* p2,
    _In_ VOID* p3
    )
{
    UNREFERENCED_PARAMETER(p1);
    UNREFERENCED_PARAMETER(p2);
    UNREFERENCED_PARAMETER(p3);

    // This is modified from original code in Wdm.h.
    //
    ASSERT(FALSE);
}

FORCEINLINE
VOID
RtlpCheckListEntry(
    _In_ PLIST_ENTRY Entry
    )

{
    if ((((Entry->Flink)->Blink) != Entry) || (((Entry->Blink)->Flink) != Entry))
    {
        FatalListEntryError((VOID*)(Entry),
                            (VOID*)((Entry->Flink)->Blink),
                            (VOID*)((Entry->Blink)->Flink));
    }
}

FORCEINLINE
BOOLEAN
RemoveEntryList(
    _In_ PLIST_ENTRY Entry
    )

{
    PLIST_ENTRY PrevEntry;
    PLIST_ENTRY NextEntry;

    NextEntry = Entry->Flink;
    PrevEntry = Entry->Blink;
    if ((NextEntry->Blink != Entry) || (PrevEntry->Flink != Entry))
    {
        FatalListEntryError((VOID*)PrevEntry,
                            (VOID*)Entry,
                            (VOID*)NextEntry);
    }

    PrevEntry->Flink = NextEntry;
    NextEntry->Blink = PrevEntry;
    return (BOOLEAN)(PrevEntry == NextEntry);
}

FORCEINLINE
PLIST_ENTRY
RemoveHeadList(
    _Inout_ PLIST_ENTRY ListHead
    )

{
    PLIST_ENTRY Entry;
    PLIST_ENTRY NextEntry;

    Entry = ListHead->Flink;

#if defined(DEBUG)

    RtlpCheckListEntry(ListHead);

#endif // defined(DEBUG)

    NextEntry = Entry->Flink;
    if ((Entry->Blink != ListHead) || (NextEntry->Blink != Entry))
    {
        FatalListEntryError((VOID*)ListHead,
                            (VOID*)Entry,
                            (VOID*)NextEntry);
    }

    ListHead->Flink = NextEntry;
    NextEntry->Blink = ListHead;

    return Entry;
}

FORCEINLINE
PLIST_ENTRY
RemoveTailList(
    _Inout_ PLIST_ENTRY ListHead
    )
{
    PLIST_ENTRY Entry;
    PLIST_ENTRY PrevEntry;

    Entry = ListHead->Blink;

#if defined(DEBUG)

    RtlpCheckListEntry(ListHead);

#endif // defined(DEBUG)

    PrevEntry = Entry->Blink;
    if ((Entry->Flink != ListHead) || (PrevEntry->Flink != Entry))
    {
        FatalListEntryError((VOID*)PrevEntry,
                            (VOID*)Entry,
                            (VOID*)ListHead);
    }

    ListHead->Blink = PrevEntry;
    PrevEntry->Flink = ListHead;
    return Entry;
}

FORCEINLINE
VOID
InsertTailList(
    _Inout_ PLIST_ENTRY ListHead,
    _Out_ __drv_aliasesMem PLIST_ENTRY Entry
    )
{
    PLIST_ENTRY PrevEntry;

#if defined(DEBUG)

    RtlpCheckListEntry(ListHead);

#endif // defined(DEBUG)

    PrevEntry = ListHead->Blink;
    if (PrevEntry->Flink != ListHead)
    {
        FatalListEntryError((VOID*)PrevEntry,
                            (VOID*)ListHead,
                            (VOID*)PrevEntry->Flink);
    }

    Entry->Flink = ListHead;
    Entry->Blink = PrevEntry;
    PrevEntry->Flink = Entry;
    ListHead->Blink = Entry;
    return;
}

FORCEINLINE
VOID
InsertHeadList(
    _Inout_ PLIST_ENTRY ListHead,
    _Out_ __drv_aliasesMem PLIST_ENTRY Entry
    )

{
    PLIST_ENTRY NextEntry;

#if defined(DEBUG)

    RtlpCheckListEntry(ListHead);

#endif // defined(DEBUG)

    NextEntry = ListHead->Flink;
    if (NextEntry->Blink != ListHead)
    {
        FatalListEntryError((VOID*)ListHead,
                            (VOID*)NextEntry,
                            (VOID*)NextEntry->Blink);
    }

    Entry->Flink = NextEntry;
    Entry->Blink = ListHead;
    NextEntry->Blink = Entry;
    ListHead->Flink = Entry;
    return;
}

FORCEINLINE
VOID
AppendTailList(
    _Inout_ PLIST_ENTRY ListHead,
    _Inout_ PLIST_ENTRY ListToAppend
    )
{
    PLIST_ENTRY ListEnd = ListHead->Blink;

    RtlpCheckListEntry(ListHead);
    RtlpCheckListEntry(ListToAppend);
    ListHead->Blink->Flink = ListToAppend;
    ListHead->Blink = ListToAppend->Blink;
    ListToAppend->Blink->Flink = ListHead;
    ListToAppend->Blink = ListEnd;
    return;
}

FORCEINLINE
PSINGLE_LIST_ENTRY
PopEntryList(
    _Inout_ PSINGLE_LIST_ENTRY ListHead
    )
{
    PSINGLE_LIST_ENTRY FirstEntry;

    FirstEntry = ListHead->Next;
    if (FirstEntry != NULL)
    {
        ListHead->Next = FirstEntry->Next;
    }

    return FirstEntry;
}

FORCEINLINE
VOID
PushEntryList(
    _Inout_ PSINGLE_LIST_ENTRY ListHead,
    _Inout_ __drv_aliasesMem PSINGLE_LIST_ENTRY Entry
    )

{
    Entry->Next = ListHead->Next;
    ListHead->Next = Entry;
    return;
}
#endif // defined(DMF_USER_MODE) && !defined( _ARM64_ )

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfDefinitions.h
//
