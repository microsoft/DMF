/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfModule.h

Abstract:

    All DMF Modules include this file. It contains access to all the Module facing DMF definitions
    as well as all definitions needed by Clients (because they are dependencies).

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Place this here so that all GUIDs defined by Modules can be compiled
// and linked without Client Driver needing to include this file.
//
#include <initguid.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Client facing definitions. These are also needed by Modules.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

#include "DmfDefinitions.h"

#if defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus)

// Declare DMFMODULE custom handle type as DMFMODULE_TYPE.
//
WDF_DECLARE_CUSTOM_TYPE(DMFMODULE_TYPE);
// Declare DMFCOLLECTION custom handle type as DMFCOLLECTION_TYPE.
//
WDF_DECLARE_CUSTOM_TYPE(DMFCOLLECTION_TYPE);
// Declare DMFINTERFACE custom handle type as DMFINTERFACE_TYPE.
//
WDF_DECLARE_CUSTOM_TYPE(DMFINTERFACE_TYPE);
// So we can set this pointer in various objects.
//
WDF_DECLARE_CONTEXT_TYPE(DMFMODULE);
// Declare an opaque handle representing a DMFCOLLECTION (DMF_MODULE_COLLECTION).
//
DECLARE_HANDLE(DMFCOLLECTION);

// DMF Module Callbacks.
// ---------------------
// These callbacks are specific to DMF. They are not related to WDF.
//

typedef
_Function_class_(DMF_ResourcesAssign)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ResourcesAssign(_In_ DMFMODULE DmfModule, 
                    _In_ WDFCMRESLIST ResourcesRaw, 
                    _In_  WDFCMRESLIST ResourcesTranslated);

typedef
_Function_class_(DMF_NotificationRegister)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_NotificationRegister(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_NotificationUnregister)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_NotificationUnregister(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_Open)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Open(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_Close)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Close(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ChildModulesAdd(_In_ DMFMODULE DmfModule,
                    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
                    _In_ PDMFMODULE_INIT DmfModuleInit);

// Internally used callbacks that cannot be overridden by Modules.
//
typedef
_Function_class_(DMF_Lock)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Lock(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_Unlock)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Unlock(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_AuxiliaryLock)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_AuxiliaryLock(_In_ DMFMODULE DmfModule, 
                  _In_ ULONG AuxiliaryLockIndex);

typedef
_Function_class_(DMF_AuxiliaryUnlock)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_AuxiliaryUnlock(_In_ DMFMODULE DmfModule, 
                    _In_ ULONG AuxiliaryLockIndex);

typedef
_Function_class_(DMF_ModuleInstanceDestroy)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleInstanceDestroy(_In_ DMFMODULE DmfModule);

// These are called by DMF internally on behalf of Client Driver.
//

typedef
_Function_class_(DMF_ModulePrepareHardware)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModulePrepareHardware(_In_ DMFMODULE DmfModule, 
                          _In_ WDFCMRESLIST ResourcesRaw, 
                          _In_ WDFCMRESLIST ResourcesTranslated);
typedef
_Function_class_(DMF_ModuleReleaseHardware)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleReleaseHardware(_In_ DMFMODULE DmfModule,
                          _In_ WDFCMRESLIST ResourcesTranslated);

typedef
_Function_class_(DMF_ModuleD0Entry)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleD0Entry(_In_ DMFMODULE DmfModule, 
                  _In_ WDF_POWER_DEVICE_STATE PreviousState);

typedef
_Function_class_(DMF_ModuleD0EntryPostInterruptsEnabled)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleD0EntryPostInterruptsEnabled(_In_ DMFMODULE DmfModule, 
                                       _In_ WDF_POWER_DEVICE_STATE PreviousState);

typedef
_Function_class_(DMF_ModuleD0Exit)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleD0Exit(_In_ DMFMODULE DmfModule, 
                 _In_ WDF_POWER_DEVICE_STATE TargetState);

typedef
_Function_class_(DMF_ModuleD0ExitPreInterruptsDisabled)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleD0ExitPreInterruptsDisabled(_In_ DMFMODULE DmfModule, 
                                      _In_ WDF_POWER_DEVICE_STATE TargetState);

typedef
_Function_class_(DMF_ModuleQueueIoRead)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleQueueIoRead(_In_ DMFMODULE DmfModule, 
                      _In_ WDFQUEUE Queue, 
                      _In_ WDFREQUEST Request, 
                      _In_ size_t Length);

typedef
_Function_class_(DMF_ModuleQueueIoWrite)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleQueueIoWrite(_In_ DMFMODULE DmfModule, 
                       _In_ WDFQUEUE Queue, 
                       _In_ WDFREQUEST Request, 
                       _In_ size_t Length);

typedef
_Function_class_(DMF_ModuleDeviceIoControl)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleDeviceIoControl(_In_ DMFMODULE DmfModule, 
                          _In_ WDFQUEUE Queue, 
                          _In_ WDFREQUEST Request, 
                          _In_ size_t OutputBufferLength, 
                          _In_ size_t InputBufferLength, 
                          _In_ ULONG IoControlCode);

typedef
_Function_class_(DMF_ModuleInternalDeviceIoControl)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleInternalDeviceIoControl(_In_ DMFMODULE DmfModule, 
                                  _In_ WDFQUEUE Queue, 
                                  _In_ WDFREQUEST Request, 
                                  _In_ size_t OutputBufferLength, 
                                  _In_ size_t InputBufferLength, 
                                  _In_ ULONG IoControlCode);

typedef
_Function_class_(DMF_ModuleSelfManagedIoCleanup)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleSelfManagedIoCleanup(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleSelfManagedIoFlush)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleSelfManagedIoFlush(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleSelfManagedIoInit)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleSelfManagedIoInit(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleSelfManagedIoSuspend)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleSelfManagedIoSuspend(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleSelfManagedIoRestart)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleSelfManagedIoRestart(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleSurpriseRemoval)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleSurpriseRemoval(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleQueryRemove)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleQueryRemove(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleQueryStop)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleQueryStop(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleRelationsQuery)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleRelationsQuery(_In_ DMFMODULE DmfModule,
                         _In_ DEVICE_RELATION_TYPE RelationType);

typedef
_Function_class_(DMF_ModuleUsageNotificationEx)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleUsageNotificationEx(_In_ DMFMODULE DmfModule,
                              _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
                              _In_ BOOLEAN IsInNotificationPath);

typedef
_Function_class_(DMF_ModuleArmWakeFromS0)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleArmWakeFromS0(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleDisarmWakeFromS0)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleDisarmWakeFromS0(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleWakeFromS0Triggered)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleWakeFromS0Triggered(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleArmWakeFromSxWithReason)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleArmWakeFromSxWithReason(_In_ DMFMODULE DmfModule,
                                  _In_ BOOLEAN DeviceWakeEnabled,
                                  _In_ BOOLEAN ChildrenArmedForWake);

typedef
_Function_class_(DMF_ModuleDisarmWakeFromSx)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleDisarmWakeFromSx(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleWakeFromSxTriggered)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleWakeFromSxTriggered(_In_ DMFMODULE DmfModule);

typedef
_Function_class_(DMF_ModuleFileCreate)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_ModuleFileCreate(_In_ DMFMODULE DmfModule, 
                     _In_ WDFDEVICE Device, 
                     _In_ WDFREQUEST Request, 
                     _In_ WDFFILEOBJECT FileObject);

typedef
_Function_class_(DMF_ModuleFileCleanup)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_ModuleFileCleanup(_In_ DMFMODULE DmfModule, 
                      _In_ WDFFILEOBJECT FileObject);

typedef
_Function_class_(DMF_ModuleFileClose)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_ModuleFileClose(_In_ DMFMODULE DmfModule, 
                    _In_ WDFFILEOBJECT FileObject);

#define DMF_MODULE_DECLARE_CONTEXT(ModuleName)                                                           \
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DMF_CONTEXT_##ModuleName##,                                           \
                                   ##ModuleName##ContextGet);                                            \
VOID                                                                                                     \
DMF_##ModuleName##_LiveKernelDumpInitialize(_In_ DMFMODULE DmfModule)                                    \
{                                                                                                        \
    DMF_CONTEXT_##ModuleName##* moduleContext;                                                           \
                                                                                                         \
    moduleContext = ##ModuleName##ContextGet(DmfModule);                                                 \
    DMF_MODULE_LIVEKERNELDUMP_POINTER_STORE(DmfModule,moduleContext,sizeof(DMF_CONTEXT_##ModuleName##)); \
}                                                                                                        \
                                                                                                         \
__forceinline                                                                                            \
DMF_CONTEXT_##ModuleName##*                                                                              \
DMF_CONTEXT_GET(                                                                                         \
    _In_ WDFOBJECT Handle                                                                                \
    )                                                                                                    \
{                                                                                                        \
    return(##ModuleName##ContextGet(Handle));                                                            \
}                                                                                                        \
                                                                                                         \

#define DMF_MODULE_DECLARE_CONFIG(ModuleName)                                                   \
                                                                                                \
__forceinline                                                                                   \
DMF_CONFIG_##ModuleName##*                                                                      \
DMF_CONFIG_GET(                                                                                 \
    _In_ DMFMODULE DmfModule                                                                    \
    )                                                                                           \
{                                                                                               \
    return (( DMF_CONFIG_##ModuleName##* )DMF_ModuleConfigGet(DmfModule));                      \
}                                                                                               \

#define DMF_MODULE_DECLARE_NO_CONTEXT(ModuleName)                                               \
                                                                                                \
VOID                                                                                            \
DMF_##ModuleName##_LiveKernelDumpInitialize(_In_ DMFMODULE DmfModule)                           \
{                                                                                               \
    UNREFERENCED_PARAMETER(DmfModule);                                                          \
}                                                                                               \
                                                                                                \

// When a Module has no Config, declare a dummy Config that is not used by Module or Clients,
// but makes it possible to easily set the size of the Config to a valid value.
//
#define DMF_MODULE_DECLARE_NO_CONFIG(ModuleName)                                                \
typedef struct                                                                                  \
{                                                                                               \
    VOID* UnusedElement;                                                                        \
} DMF_CONFIG_##ModuleName##;                                                                    \
                                                                                                \

// These are DMF specific Module callbacks.
//
typedef struct
{
    ULONG Size;
    DMF_ModuleInstanceDestroy* ModuleInstanceDestroy;
    DMF_ResourcesAssign* DeviceResourcesAssign;
    DMF_NotificationRegister* DeviceNotificationRegister;
    DMF_NotificationUnregister* DeviceNotificationUnregister;
    DMF_Open* DeviceOpen;
    DMF_Close* DeviceClose;
    DMF_ChildModulesAdd* ChildModulesAdd;
} DMF_CALLBACKS_DMF;

__forceinline
VOID
DMF_CALLBACKS_DMF_INIT(
    _Out_ DMF_CALLBACKS_DMF* CallbacksDmf
    )
{
    RtlZeroMemory(CallbacksDmf,
                  sizeof(DMF_CALLBACKS_DMF));
    CallbacksDmf->Size = sizeof(DMF_CALLBACKS_DMF);
}

typedef struct
{
    ULONG Size;
    DMF_ModulePrepareHardware* ModulePrepareHardware;
    DMF_ModuleReleaseHardware* ModuleReleaseHardware;
    DMF_ModuleD0Entry* ModuleD0Entry;
    DMF_ModuleD0EntryPostInterruptsEnabled* ModuleD0EntryPostInterruptsEnabled;
    DMF_ModuleD0ExitPreInterruptsDisabled* ModuleD0ExitPreInterruptsDisabled;
    DMF_ModuleD0Exit* ModuleD0Exit;
    DMF_ModuleQueueIoRead* ModuleQueueIoRead;
    DMF_ModuleQueueIoWrite* ModuleQueueIoWrite;
    DMF_ModuleDeviceIoControl* ModuleDeviceIoControl;
    DMF_ModuleInternalDeviceIoControl* ModuleInternalDeviceIoControl;
    DMF_ModuleSelfManagedIoCleanup* ModuleSelfManagedIoCleanup;
    DMF_ModuleSelfManagedIoFlush* ModuleSelfManagedIoFlush;
    DMF_ModuleSelfManagedIoInit* ModuleSelfManagedIoInit;
    DMF_ModuleSelfManagedIoSuspend* ModuleSelfManagedIoSuspend;
    DMF_ModuleSelfManagedIoRestart* ModuleSelfManagedIoRestart;
    DMF_ModuleSurpriseRemoval* ModuleSurpriseRemoval;
    DMF_ModuleQueryRemove* ModuleQueryRemove;
    DMF_ModuleQueryStop* ModuleQueryStop;
    DMF_ModuleRelationsQuery* ModuleRelationsQuery;
    DMF_ModuleUsageNotificationEx* ModuleUsageNotificationEx;
    DMF_ModuleArmWakeFromS0* ModuleArmWakeFromS0;
    DMF_ModuleDisarmWakeFromS0* ModuleDisarmWakeFromS0;
    DMF_ModuleWakeFromS0Triggered* ModuleWakeFromS0Triggered;
    DMF_ModuleArmWakeFromSxWithReason* ModuleArmWakeFromSxWithReason;
    DMF_ModuleDisarmWakeFromSx* ModuleDisarmWakeFromSx;
    DMF_ModuleWakeFromSxTriggered* ModuleWakeFromSxTriggered;
    DMF_ModuleFileCreate* ModuleFileCreate;
    DMF_ModuleFileCleanup* ModuleFileCleanup;
    DMF_ModuleFileClose* ModuleFileClose;
} DMF_CALLBACKS_WDF;

typedef struct
{
    ULONG Size;
    BOOLEAN ModulePrepareHardwareImplemented;
    BOOLEAN ModuleReleaseHardwareImplemented;
    BOOLEAN ModuleD0EntryImplemented;
    BOOLEAN ModuleD0EntryPostInterruptsEnabledImplemented;
    BOOLEAN ModuleD0ExitPreInterruptsDisabledImplemented;
    BOOLEAN ModuleD0ExitImplemented;
    BOOLEAN ModuleQueueIoReadImplemented;
    BOOLEAN ModuleQueueIoWriteImplemented;
    BOOLEAN ModuleDeviceIoControlImplemented;
    BOOLEAN ModuleInternalDeviceIoControlImplemented;
    BOOLEAN ModuleSelfManagedIoCleanupImplemented;
    BOOLEAN ModuleSelfManagedIoFlushImplemented;
    BOOLEAN ModuleSelfManagedIoInitImplemented;
    BOOLEAN ModuleSelfManagedIoSuspendImplemented;
    BOOLEAN ModuleSelfManagedIoRestartImplemented;
    BOOLEAN ModuleSurpriseRemovalImplemented;
    BOOLEAN ModuleQueryRemoveImplemented;
    BOOLEAN ModuleQueryStopImplemented;
    BOOLEAN ModuleRelationsQueryImplemented;
    BOOLEAN ModuleUsageNotificationExImplemented;
    BOOLEAN ModuleArmWakeFromS0Implemented;
    BOOLEAN ModuleDisarmWakeFromS0Implemented;
    BOOLEAN ModuleWakeFromS0TriggeredImplemented;
    BOOLEAN ModuleArmWakeFromSxWithReasonImplemented;
    BOOLEAN ModuleDisarmWakeFromSxImplemented;
    BOOLEAN ModuleWakeFromSxTriggeredImplemented;
    BOOLEAN ModuleFileCreateImplemented;
    BOOLEAN ModuleFileCleanupImplemented;
    BOOLEAN ModuleFileCloseImplemented;
} DMF_CALLBACKS_WDF_CHECK;

__forceinline
VOID
DMF_CALLBACKS_WDF_INIT(
    _Out_ DMF_CALLBACKS_WDF* CallbacksWdf
    )
{
    RtlZeroMemory(CallbacksWdf,
                  sizeof(DMF_CALLBACKS_WDF));
    CallbacksWdf->Size = sizeof(DMF_CALLBACKS_WDF);
}

// It means the Module's read/write/lock/unlock must be called at PASSIVE_LEVEL.
//
#define DMF_MODULE_OPTIONS_PASSIVE              0x00000001
// It means the Module's read/write/lock/unlock can be called at DISPATCH_LEVEL.
//
#define DMF_MODULE_OPTIONS_DISPATCH             0x00000002
// DMF_MODULE_OPTIONS_DISPATCH by default.
// Client can override it to DMF_MODULE_OPTIONS_PASSIVE.
//
#define DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM     0x00000004
// It means the Module requires the Client to set a Transport.
//
#define DMF_MODULE_OPTIONS_TRANSPORT_REQUIRED   0x00000008

#define DMF_MODULE_RUNS_PASSIVE(DmfObject) (DmfObject->ModuleDescriptor.ModuleOptions & DMF_MODULE_OPTIONS_PASSIVE)
#define DMF_MODULE_RUNS_DISPATCH(DmfObject) (DmfObject->ModuleDescriptor.ModuleOptions & DMF_MODULE_OPTIONS_DISPATCH)

typedef enum
{
    DMF_MODULE_OPEN_OPTION_Invalid = 0,
    // Call DMF_ModuleOpen() in EvtPrepareHardware.
    //
    DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware,
    // Call DMF_Module_RegisterNotification() in EvtPrepareHardware.
    //
    DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware,
    // Call DMF_ModuleOpen() in EvtD0Entry during system power up.
    //
    DMF_MODULE_OPEN_OPTION_OPEN_D0EntrySystemPowerUp,
    // Call DMF_ModuleOpen() in EvtD0Entry.
    //
    DMF_MODULE_OPEN_OPTION_OPEN_D0Entry,
    // Call DMF_Module_RegisterNotification() in EvtD0Entry.
    //
    DMF_MODULE_OPEN_OPTION_NOTIFY_D0Entry,
    // Call DMF_ModuleOpen() right after the Module is created.
    //
    DMF_MODULE_OPEN_OPTION_OPEN_Create,
    // Client registers for notification during create from another Modules's callback.
    //
    DMF_MODULE_OPEN_OPTION_NOTIFY_Create,
    // Sentinel.
    //
    DMF_MODULE_OPEN_OPTION_LAST,
} DmfModuleOpenOption;

typedef NTSTATUS DMF_WdfAddCustomType(_In_ WDFOBJECT Handle,
                                      _In_opt_ ULONG_PTR Data,
                                      _In_opt_ PFN_WDF_OBJECT_CONTEXT_CLEANUP EvtCleanupCallback,
                                      _In_opt_ PFN_WDF_OBJECT_CONTEXT_DESTROY EvtDestroyCallback);
// Module Descriptor.
//
typedef struct
{
    // Size of this structure.
    //
    ULONG Size;
    // Module Name (for debug purposes).
    //
    PSTR ModuleName;
    // Module options that define Module specific behavior.
    //
    ULONG ModuleOptions;
    // Module options that define how the Module opens.
    //
    DmfModuleOpenOption OpenOption;
    // Size of data passed during open.
    //
    ULONG ModuleConfigSize;
    // Module's Callbacks (DMF).
    //
    DMF_CALLBACKS_DMF* CallbacksDmf;
    // Module's Callbacks (WDF).
    //
    DMF_CALLBACKS_WDF* CallbacksWdf;
    // Module Transport Method. If this Module can act as a Transport Module, then
    // this element must be set.
    //
    DMF_ModuleTransportMethod* ModuleTransportMethod;
    // BranchTrack Initialization for the Module (optional).
    //
    EVT_DMF_BranchTrack_BranchesInitialize* ModuleBranchTrackInitialize;
    // CrashDump Feature Initialization for the Module (optional).
    //
    EVT_DMF_LiveKernelDump_Initialize* ModuleLiveKernelDumpInitialize;
    // Number of additional locks needed for the Module (optional).
    //
    ULONG NumberOfAuxiliaryLocks;
    // WDF Object attributes specifying Module Context details.
    //
    PWDF_OBJECT_ATTRIBUTES ModuleContextAttributes;
    // In Flight Recorder Size.
    // If the Module sets this to 0, its logs will be part of the default recorder buffer.
    //
    ULONG InFlightRecorderSize;
    // Transport Interface GUID supported by this Module on upper layer.
    //
    GUID SupportedTransportInterfaceGuid;
    // Transport Interface GUID required by this Module on upper layer.
    // (This field is mandatory when DMF_MODULE_OPTIONS_TRANSPORT_REQUIRED is set.)
    //
    GUID RequiredTransportInterfaceGuid;
    // When a Module is created, a custom type is assigned to the handle using
    // this method.
    //
    DMF_WdfAddCustomType* WdfAddCustomType;
} DMF_MODULE_DESCRIPTOR;

#define DMF_MODULE_DESCRIPTOR_INIT(Descriptor, Name, Module_Options, Open_Option)           \
                                                                                            \
RtlZeroMemory(&Descriptor,                                                                  \
              sizeof(DMF_MODULE_DESCRIPTOR));                                               \
                                                                                            \
Descriptor.Size                            = sizeof(DMF_MODULE_DESCRIPTOR);                 \
Descriptor.ModuleName                      = ""#Name;                                       \
Descriptor.ModuleOptions                   = Module_Options;                                \
Descriptor.OpenOption                      = Open_Option;                                   \
Descriptor.ModuleConfigSize                = sizeof(DMF_CONFIG_##Name##);                   \
Descriptor.ModuleBranchTrackInitialize     = NULL;                                          \
Descriptor.NumberOfAuxiliaryLocks          = 0;                                             \
Descriptor.CallbacksDmf                    = NULL;                                          \
Descriptor.CallbacksWdf                    = NULL;                                          \
Descriptor.ModuleLiveKernelDumpInitialize  = DMF_##Name##_LiveKernelDumpInitialize;         \
Descriptor.ModuleContextAttributes         = WDF_NO_OBJECT_ATTRIBUTES;                      \
Descriptor.WdfAddCustomType                = WDF_ADD_CUSTOM_TYPE_FUNCTION_NAME(Name);       \
                                                                                            \

#define DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(Descriptor, Name, ModuleContext, Module_Options, Open_Option)         \
                                                                                                                      \
WDF_OBJECT_ATTRIBUTES moduleContextAttributes;                                                                        \
DMF_MODULE_DESCRIPTOR_INIT(Descriptor, Name, Module_Options, Open_Option)                                             \
WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&moduleContextAttributes,                                                     \
                                        ModuleContext);                                                               \
Descriptor.ModuleContextAttributes         = &moduleContextAttributes;                                                \
                                                                                                                      \

// Method to initialize Protocol descriptor.
//
VOID
DMF_INTERFACE_PROTOCOL_DESCRIPTOR_INIT_INTERNAL(
    _Out_ DMF_INTERFACE_PROTOCOL_DESCRIPTOR* DmfProtocolDescriptor,
    _In_ PSTR InterfaceName,
    _In_ ULONG DeclarationDataSize,
    _In_ EVT_DMF_INTERFACE_ProtocolBind* EvtProtocolBind,
    _In_ EVT_DMF_INTERFACE_ProtocolUnbind* EvtProtocolUnbind,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind
    );

// Method to initialize Transport descriptor.
//
VOID
DMF_INTERFACE_TRANSPORT_DESCRIPTOR_INIT_INTERNAL(
    _Out_ DMF_INTERFACE_TRANSPORT_DESCRIPTOR* DmfTransportDescriptor,
    _In_ PSTR InterfaceName,
    _In_ ULONG DeclarationDataSize,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind
);

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCreate(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ PWDF_OBJECT_ATTRIBUTES DmfModuleObjectAttributes,
    _In_ DMF_MODULE_DESCRIPTOR* ModuleDescriptor,
    _Out_ DMFMODULE* DmfModule
    );

NTSTATUS
DMF_ModuleOpen(
    _In_ DMFMODULE DmfModule
    );

VOID
DMF_ModuleClose(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
LONG
DMF_ModuleReferenceAdd(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
LONG
DMF_ModuleReferenceDelete(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleIsInFilterDriver(
    _In_ DMFMODULE DmfModule
    );

VOID
DMF_ModuleLock(
    _In_ DMFMODULE DmfModule
    );

VOID
DMF_ModuleUnlock(
    _In_ DMFMODULE DmfModule
    );

#if defined(DEBUG)
BOOLEAN
DMF_ModuleIsLocked(
    _In_ DMFMODULE DmfModule
    );

BOOLEAN
DMF_ModuleLockIsPassive(
    _In_ DMFMODULE DmfModule
    );

BOOLEAN
DMF_IsPoolTypePassiveLevel(
    _In_ POOL_TYPE PoolType
    );
#endif // defined(DEBUG)

VOID
DMF_ModuleAuxiliaryLock(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    );

VOID
DMF_ModuleAuxiliaryUnlock(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    );

#if defined(DEBUG)
BOOLEAN
DMF_ModuleAuxiliarySynchnonizationIsLocked(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    );
#endif // defined(DEBUG)

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID*
DMF_ModuleConfigGet(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_IsModuleDynamic(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_IsModulePassiveLevel(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleReference(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ModuleDereference(
    _In_ DMFMODULE DmfModule
    );

VOID
DMF_ModuleInContextSave(
    _In_ WDFOBJECT WdfObject,
    _In_ DMFMODULE DmfModule
    );

NTSTATUS
DMF_ModuleTransportCall(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG Message,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize
    );

DMFMODULE
DMF_ModuleTransportGet(
    _In_ DMFMODULE DmfModule
    );

BOOLEAN
DMF_ModuleRequestCompleteOrForward(
    _In_ DMFMODULE DmfModule,
    _In_ WDFREQUEST Request,
    _In_ NTSTATUS NtStatus
    );

#if !defined(DMF_USER_MODE)
RECORDER_LOG
DMF_InFlightRecorderGet(
    _In_ DMFMODULE DmfModule
);
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Features
////////////////////////////////////////////////////////////////////////////////////////////////
//

DMFMODULE
DMF_FeatureModuleGetFromModule(
    _In_ DMFMODULE DmfModule,
    _In_ DmfFeatureType DmfFeature
    );

////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Object Validation Support
////////////////////////////////////////////////////////////////////////////////////////////////
//

VOID
DMF_ObjectValidate(
    _In_ DMFMODULE DmfModule
    );

VOID
DMF_HandleValidate_ModuleMethod(
    _In_ DMFMODULE DmfModule
    );

#define DMFMODULE_VALIDATE_IN_METHOD(ModuleHandle, ModuleType)                                  \
                                                                                                \
     (! WdfObjectIsCustomType(ModuleHandle, ModuleType)) ?                                      \
              (ASSERT(FALSE)) :                                                                 \
              (DMF_HandleValidate_ModuleMethod(ModuleHandle))                                   \
			  
// These two validation functions are deprecated
// Do not use it.
//
VOID
DMF_HandleValidate_OpeningOk(
    _In_ DMFMODULE DmfModule
    );

#define DMFMODULE_VALIDATE_IN_METHOD_OPENING_OK(ModuleHandle, ModuleType)                       \
                                                                                                \
    (! WdfObjectIsCustomType(ModuleHandle, ModuleType)) ?                                       \
              (ASSERT(FALSE)) :                                                                 \
              (DMF_HandleValidate_OpeningOk(ModuleHandle))                                      \
			  
VOID
DMF_HandleValidate_ClosingOk(
    _In_ DMFMODULE DmfModule
    );
	
#define DMFMODULE_VALIDATE_IN_METHOD_CLOSING_OK(ModuleHandle, ModuleType)                       \
                                                                                                \
    (! WdfObjectIsCustomType(ModuleHandle, ModuleType)) ?                                       \
              (ASSERT(FALSE)) :                                                                 \
              (DMF_HandleValidate_ClosingOk(ModuleHandle))                                      \

__forceinline
DMFMODULE
DMFMODULEVOID_TO_MODULE(
    _In_ VOID* DmfModuleVoid
    )
{
    DMFMODULE dmfModule;

    dmfModule = (DMFMODULE)DmfModuleVoid;
    DMF_ObjectValidate(dmfModule);

    return dmfModule;
}

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfModule.h
//
