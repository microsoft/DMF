/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfIncludeInternal.h

Abstract:

    Includes definitions used by DMF internally.
    DMF Clients should not include this file, nor should they used functions referenced in this file.
    DMF Clients should only used definitions exposed by Dmf.h.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF.
//
#include "DmfModule.h"
#include "DmfModules.Core.h"
#include "DmfModules.Core.Trace.h"
#include "Dmf_Bridge.h"

// It means the Generic function is not overridden.
//
#define USE_GENERIC_ENTRYPOINT      NULL

// These are Module States that allow DMF to validate that calls happening
// in the correct order and that Client Drivers are not, for example, calling Module Methods
// without properly instantiating Modules.
//
typedef enum
{
    ModuleState_Invalid = 0,
    // The Module has been created but not opened.
    //
    ModuleState_Created,
    // The Module is in process of opening.
    //
    ModuleState_Opening,
    // The Module has been created and opened.
    //
    ModuleState_Opened,
    // The Module is in process of closing.
    //
    ModuleState_Closing,
    // The Module has been created and closed.
    //
    ModuleState_Closed,
    // The Module in process of being destroyed.
    //
    ModuleState_Destroying,
    // Sentinel.
    //
    ModuleState_Last
} ModuleStateType;

// Keep track of when the Module is opened for clean up purposes.
//
typedef enum
{
    ModuleOpenedDuringType_Invalid,
    // The Module is opened manually by Client. NOTE: This
    // setting is always set during Open. Then, if DMF.has opened
    // the Module automatically, the setting is overwritten.
    //
    ModuleOpenedDuringType_Manual,
    // The Module has been opened during Create (and not cleaned up).
    //
    ModuleOpenedDuringType_Create,
    // The Module has been opened during PrepareHardware (and not cleaned up).
    //
    ModuleOpenedDuringType_PrepareHardware,
    // The Module has been opened during D0Entry (and not cleaned up).
    //
    ModuleOpenedDuringType_D0Entry,
    // The Module has been opened during D0Entry during system power up (and not cleaned up).
    //
    ModuleOpenedDuringType_D0EntrySystemPowerUp,
    ModuleOpenedDuringType_Maximum
} ModuleOpenedDuringType;

typedef struct
{
    // DISPATCH_LEVEL Synchronization Generic Device Lock.
    //
    WDFSPINLOCK SynchronizationDispatchSpinLock;
    // PASSIVE_LEVEL Synchronization Generic Device Lock.
    //
    WDFWAITLOCK SynchronizationPassiveWaitLock;
    // For debug purposes only.
    //
    HANDLE LockHeldByThread;
} DMF_SYNCHRONIZATION;

// Maximum number of Auxiliary locks per DMF Module.
//
#define DMF_MAXIMUM_AUXILIARY_LOCKS         4

// Index of the default lock in the locks array.
//
#define DMF_DEFAULT_LOCK_INDEX              0

// Number of default locks per DMF Module.
//
#define DMF_NUMBER_OF_DEFAULT_LOCKS         1

// These are internal callbacks that may not be overridden by Modules.
//
typedef struct
{
    ULONG Size;
    // Lock Module using default lock.
    //
    DMF_Lock* DefaultLock;
    // Unlock Module using default lock.
    //
    DMF_Unlock* DefaultUnlock;
    // Lock Module using auxiliary lock.
    //
    DMF_AuxiliaryLock* AuxiliaryLock;
    // Unlock Module using auxiliary lock.
    //
    DMF_AuxiliaryLock* AuxiliaryUnlock;
} DMF_CALLBACKS_INTERNAL;

// Forward declaration for DMF Object.
//
typedef struct _DMF_OBJECT_ DMF_OBJECT;

// Forward declaration for DMF Module Collection.
//
typedef struct _DMF_MODULE_COLLECTION_ DMF_MODULE_COLLECTION;

struct _DMF_OBJECT_
{
    // This element is used to insert an instance of this structure into a list
    // when this instance is a Child Module.
    //
    LIST_ENTRY ChildListEntry;
    // Context using during Open.
    //
    VOID* ModuleConfig;
    WDFMEMORY ModuleConfigMemory;
    // For debug purposes only.
    // If Client wants allocates its own context, then
    // ModuleContext will not be the primary context of DMFMODULE.
    // Since Live kernel mini dumps, will have minimal information of
    // objects, retrieving additional contexts is not straight forward. 
    // Hence Pointer to Module's Context is stored here for easy access.
    //
    VOID* ModuleContext;
    // Reference counter for DMF Object references.
    //
    LONG ReferenceCount;
    // Associated WDF Device.
    //
    WDFDEVICE ParentDevice;
    // Handle to WDF Memory Object for this structure.
    //
    WDFMEMORY MemoryDmfObject;
    // For debug purposes only.
    //
    ModuleStateType ModuleState;
    // Keep track of when Module is opened/registered for clean up purposes.
    //
    ModuleOpenedDuringType ModuleOpenedDuring;
    ModuleOpenedDuringType ModuleNotificationRegisteredDuring;
    // For debug purposes only.
    // This allows the Client Driver to easily identify which type 
    // of DMF Module this handle is associated with.
    //
    CHAR* ModuleName;
    // For debug purposes only.
    // This allows the Client Driver to easily identify which instance
    // of a DMF Module this handle is associated with.
    //
    WDFMEMORY ClientModuleInstanceNameMemory;
    CHAR* ClientModuleInstanceName;
    // For debug purposes only.
    //
    ULONGLONG Signature;
    // Calls are always made to internal Callbacks which can then
    // filter or just call the Client Callbacks. This allows the
    // DMF Library to perform additional processing or tracking
    // as needed.
    //
    DMF_CALLBACKS_DMF InternalCallbacksDmf;
    DMF_CALLBACKS_WDF InternalCallbacksWdf;
    DMF_CALLBACKS_INTERNAL InternalCallbacksInternal;
    // DMF Module Descriptor.
    //
    DMF_MODULE_DESCRIPTOR ModuleDescriptor;
    // DMF Module Attributes.
    //
    DMF_MODULE_ATTRIBUTES ModuleAttributes;
    // DMF Module Callbacks (optional, set by Client).
    //
    DMF_MODULE_EVENT_CALLBACKS Callbacks;
    // Flag indicating that the Module close is pending.
    // This is necessary to synchronize close with Module Methods for Modules that
    // open/close in notification handlers.
    //
    BOOLEAN IsClosePending;
    // Flag indicating if PreClose callback should be called while closing this Module.
    // It is set to TRUE after this Module was successfully opened.
    //
    BOOLEAN NeedToCallPreClose;
    // Remember this Module is created directly by the Client, not part of a Collection.
    // It is important because it needs to be automatically closed prior to being destroyed.
    //
    BOOLEAN DynamicModuleImmediate;
    // List of this Module's Child Modules.
    //
    LIST_ENTRY ChildObjectList;
    // Number of Child Modules.
    //
    ULONG NumberOfChildModules;
    // Collection of Interface Bindings where this Module is either the Transport or the Protocol.
    //
    // TODO: Check if a lock is needed to protect this?
    //
    WDFCOLLECTION InterfaceBindings;
    // Spin Lock to protect access to InterfaceBindings.
    //
    WDFSPINLOCK InterfaceBindingsLock;
    // Transport Modules.
    // (NOTE: These are subset of ChildModulesVoid.)
    //
    DMF_OBJECT* TransportModule;
    // Parent Module.
    //
    DMF_OBJECT* DmfObjectParent;
    // Parent Module Collection.
    //
    DMF_MODULE_COLLECTION* ModuleCollection;
    // Synchronization Locks.
    // This includes one default lock and a number of auxiliary locks as specified by Client.
    //
    DMF_SYNCHRONIZATION Synchronizations[DMF_MAXIMUM_AUXILIARY_LOCKS + DMF_NUMBER_OF_DEFAULT_LOCKS];
    // Stores the Module's In Flight Recorder handle.
    //
    RECORDER_LOG InFlightRecorder;
    // Client Cleanup Callback (chained).
    //
    PFN_WDF_OBJECT_CONTEXT_CLEANUP ClientEvtCleanupCallback;
    // Indicates this Module is a Transport.
    //
    BOOLEAN IsTransport;
    // Transport Interface GUID for validation.
    //
    GUID DesiredTransportInterfaceGuid;
};

// DMF Object Signature.
//
#define DMF_OBJECT_SIGNATURE        (0x012345678)

// Memory Allocation Tag for Dmf. ('DmfT')
//
#define DMF_TAG                            'TfmD'

// Context used to iterate through the list of Child Modules.
//
typedef struct
{
    // Store the Parent Module so that caller does not need to pass it every subsequent time.
    //
    DMF_OBJECT* ParentObject;
    // This is the Child Module to be returned at the next iteration.
    //
    LIST_ENTRY* NextChildObjectListEntry;
    // This is the Child Module to be returned at the next iteration.
    //
    LIST_ENTRY* PreviousChildObjectListEntry;
} CHILD_OBJECT_INTERATION_CONTEXT;

// The DMF Module Collection contains information about all the instantiated
// DMF Modules. It is used for automatically dispatching various calls to
// each instance of a DMF Module.
//
struct _DMF_MODULE_COLLECTION_
{
    // WDF Memory Object corresponding to this structure.
    //
    WDFMEMORY ModuleCollectionHandleMemory;

    // The list of instantiated DMF Modules.
    // TODO: Change type to DMFMODULE*.
    //
    DMF_OBJECT** ClientDriverDmfModules;
    WDFMEMORY ClientDriverDmfModulesMemory;

    // The number of instantiated Modules.
    //
    LONG NumberOfClientDriverDmfModules;

    // Flag indicating that the manual invocation of Modules D0Entry succeeded.
    // This is necessary to ensure we don't call D0Exit on modules
    // where D0Entry were unsuccessful.
    //
    BOOLEAN ManualCallToD0EntrySucceeded;

    // Automatically created Feature Handles used by all Modules in the Module Collection.
    //
    DMF_OBJECT* DmfObjectFeature[DmfFeature_NumberOfFeatures];

    // Flags indicating if modules implement WDF callbacks.
    //
    DMF_CALLBACKS_WDF_CHECK DmfCallbacksWdfCheck;

    // Indicates that Client invoked Create callbacks manually.
    // It is necessary for the case where Module Collection Cleanup callback
    // is called, but the Client has not had a chance to call the corresponding
    // Destroy callback. (The Client cannot do so in its parent object Cleanup
    // callback as that happens after the Module Collection is destroyed since
    // the Module Collection's Cleanup callback is called first by WDF.
    //
    BOOLEAN ManualDestroyCallbackIsPending;
    WDFDEVICE ClientDevice;
};

// Represents a binding between Protocol and Transport.
//
typedef struct _DMF_INTERFACE_OBJECT
{
    // Transport Module.
    //
    DMFMODULE TransportModule;
    // Protocol Module.
    //
    DMFMODULE ProtocolModule;
    // Transport Module's Descriptor.
    //
    DMF_INTERFACE_TRANSPORT_DESCRIPTOR* TransportDescriptor;
    // Protocol Module's Descriptor.
    //
    DMF_INTERFACE_PROTOCOL_DESCRIPTOR* ProtocolDescriptor;
    // State of this Interface.
    //
    InterfaceStateType InterfaceState;
    // Reference counter for this Interface.
    //
    LONG ReferenceCount;
    // Lock to protect accesses to this structure.
    //
    WDFSPINLOCK InterfaceLock;
    // WDF Object corresponding to DMF_INTERFACE_OBJECT.
    //
    DMFINTERFACE DmfInterface;
} DMF_INTERFACE_OBJECT;

typedef struct _DMF_DEVICE_CONTEXT
{
    // Corresponding WDF Device.
    //
    WDFDEVICE WdfDevice;

    // Flag to indicate if Client Driver implements
    // an EVT_WDF_DRIVER_DEVICE_ADD callback.
    //
    BOOLEAN ClientImplementsEvtWdfDriverDeviceAdd;

    // DMF Library Dispatcher.
    //
    DMFCOLLECTION DmfCollection;

    // Control Device
    // Same as WdfDevice for Control device.
    //
    WDFDEVICE WdfControlDevice;

    // Client Driver device.
    // Same as WdfDevice for non-Control device.
    //
    WDFDEVICE WdfClientDriverDevice;

    // Indicates that the Client Driver is a Filter driver.
    //
    BOOLEAN IsFilterDevice;
} DMF_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DMF_DEVICE_CONTEXT, DmfDeviceContextGet)

typedef struct
{
    // These should only be set by DMF.
    // --------------------------------
    //
    struct
    {
        // Size of this structure.
        //
        ULONG Size;
        // Indicates if any error was encountered configuring this structure.
        //
        NTSTATUS ErrorCodeNtStatus;
        // List of all the WDFMEMORY handles that contain all the CONFIGS that will be
        // in the collection.
        //
        WDFCOLLECTION ListOfConfigs;
        // Has the Client Driver initialized a BranchTrack Module?
        //
        BOOLEAN BranchTrackEnabled;
        // Has the Client Driver initialized a LiveKernelDump Module?
        //
        BOOLEAN LiveKernelDumpEnabled;
        // Parent WDFDEVICE (The Client Driver's WDFDEVICE.)
        // 
        WDFDEVICE ClientDriverWdfDevice;
        // Parent DMFMODULE.
        //
        DMFMODULE ParentDmfModule;
        // Indicates that it is a Transport Module. This field will be copied to the
        // individual Module Attributes like other fields from this structure are
        // prior to the Module being created.
        //
        BOOLEAN IsTransportModule;
    } DmfPrivate;

    // These can be set by Client.
    // ---------------------------
    //

    // BranchTrack support.
    //
    DMF_CONFIG_BranchTrack* BranchTrackModuleConfig;

    // LiveKernelDump support.
    //
    DMF_CONFIG_LiveKernelDump* LiveKernelDumpModuleConfig;

} DMF_MODULE_COLLECTION_CONFIG;

__forceinline
VOID
DMF_MODULE_COLLECTION_CONFIG_INIT(
    _Out_ DMF_MODULE_COLLECTION_CONFIG* ModuleCollectionConfig,
    _In_opt_ DMF_CONFIG_BranchTrack* BranchTrackModuleConfig,
    _In_opt_ DMF_CONFIG_LiveKernelDump* LiveKernelDumpModuleConfig,
    _In_ WDFDEVICE ParentWdfDevice
    )
{
    RtlZeroMemory(ModuleCollectionConfig,
                  sizeof(DMF_MODULE_COLLECTION_CONFIG));
    ModuleCollectionConfig->DmfPrivate.Size = sizeof(DMF_MODULE_COLLECTION_CONFIG);
    ModuleCollectionConfig->DmfPrivate.ClientDriverWdfDevice = ParentWdfDevice;
    ModuleCollectionConfig->DmfPrivate.ErrorCodeNtStatus = STATUS_SUCCESS;
    ModuleCollectionConfig->BranchTrackModuleConfig = BranchTrackModuleConfig;
    ModuleCollectionConfig->DmfPrivate.LiveKernelDumpEnabled = FALSE;
    ModuleCollectionConfig->LiveKernelDumpModuleConfig = LiveKernelDumpModuleConfig;
}

// DmfCall.c
//

DMF_OBJECT*
DmfChildObjectFirstGet(
    _In_ DMF_OBJECT* ParentObject,
    _Out_ CHILD_OBJECT_INTERATION_CONTEXT* ChildObjectInterationContext
    );

DMF_OBJECT*
DmfChildObjectNextGet(
    _Inout_ CHILD_OBJECT_INTERATION_CONTEXT* ChildObjectInterationContext
    );

// DmfBranchTrack.h
//

// BranchTrack Module is treated like any other Module. However, it is always the
// first Module Initialized so that the rest of the driver always knows where it
// is located.
//
#define DMF_BRANCHTRACK_MODULE_INDEX    0

VOID
DMF_ModuleCollectionHandleSet(
    _In_ DMF_OBJECT* DmfObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
DMF_ModuleBranchTrack_HasClientEnabledBranchTrack(
    _In_ WDFDEVICE Device
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionHandlePropagate(
    _Inout_ DMF_MODULE_COLLECTION* ModuleCollectionHandle,
    _In_ LONG NumberOfEntries
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCollectionCreate(
    _In_opt_ PDMFDEVICE_INIT DmfDeviceInit,
    _In_ DMF_MODULE_COLLECTION_CONFIG* ModuleCollectionConfig,
    _Out_ DMFCOLLECTION* DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCollectionPostCreate(
    _In_ DMF_MODULE_COLLECTION_CONFIG* ModuleCollectionConfig,
    _In_ DMFCOLLECTION DmfCollection
    );

// Support functions for feature modules.
//

_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_OBJECT*
DMF_ModuleCollectionFeatureHandleGet(
    _In_ DMF_MODULE_COLLECTION* ModuleCollectionHandle,
    _In_ DmfFeatureType DmfFeatureType
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleBranchTrack_ModuleCollectionInitialize(
    _Inout_ DMF_MODULE_COLLECTION* ModuleCollectionHandle
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleLiveKernelDump_ModuleCollectionInitialize(
    _Inout_ DMF_MODULE_COLLECTION* ModuleCollectionHandle
    );

// DmfHelpers.h
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_SynchronizationCreate(
    _In_ DMF_OBJECT* DmfObject,
    _In_ BOOLEAN PassiveLevel
    );

// DmfValidate.c
//

VOID
DMF_HandleValidate_Create(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_Open(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsCreatedOrOpening(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsOpening(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_Close(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsClosing(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsOpenedOrClosing(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_Destroy(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsOpen(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsCreated(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsCreatedOrIsNotify(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsOpened(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsCreatedOrOpened(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsCreatedOrClosed(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsCreatedOrOpenedOrClosed(
    _In_ DMF_OBJECT* DmfObject
    );

VOID
DMF_HandleValidate_IsAvailable(
    _In_ DMF_OBJECT* DmfObject
    );

DMF_OBJECT*
DMF_FeatureHandleGetFromModuleCollection(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ DmfFeatureType DmfFeature
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RequestPassthru(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RequestPassthruWithCompletion(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine,
    __drv_aliasesMem WDFCONTEXT CompletionContext
    );

// DmfInternal.h
//

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleTreeDestroy(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleDestroy(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN DeleteMemory
    );

VOID
DMF_ModuleInterfacesUnbind(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_Destroy(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModulePrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleReleaseHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesTranslated
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_ModuleD0EntryPostInterruptsEnabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleD0ExitPreInterruptsDisabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Internal_ModuleQueueIoRead(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Internal_ModuleQueueIoWrite(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Internal_ModuleDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Internal_ModuleInternalDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleSelfManagedIoCleanup(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleSelfManagedIoFlush(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleSelfManagedIoInit(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleSelfManagedIoSuspend(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleSelfManagedIoRestart(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleSurpriseRemoval(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleQueryRemove(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleQueryStop(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleRelationsQuery(
    _In_ DMFMODULE DmfModule,
    _In_ DEVICE_RELATION_TYPE RelationType
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleUsageNotificationEx(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleArmWakeFromS0(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleDisarmWakeFromS0(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleWakeFromS0Triggered(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ModuleArmWakeFromSxWithReason(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleDisarmWakeFromSx(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_ModuleWakeFromSxTriggered(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Internal_ModuleFileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Internal_ModuleFileCleanup(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Internal_ModuleFileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Internal_ResourcesAssign(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_NotificationRegister(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_Open(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_Close(
    _In_ DMFMODULE DmfModule
    );

// DmfGeneric.h
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
EVT_DMF_MODULE_Generic_OnDeviceNotificationOpen(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_MODULE_Generic_OnDeviceNotificationPostOpen(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_MODULE_Generic_OnDeviceNotificationPreClose(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
EVT_DMF_MODULE_Generic_OnDeviceNotificationClose(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_Generic_PostBind(
    _In_ DMFINTERFACE DmfInterface
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_INTERFACE_Generic_PreUnbind(
    _In_ DMFINTERFACE DmfInterface
    );

VOID
DMF_Generic_Destroy(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_Open(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Internal_NotificationRegister(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Internal_Close(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModulePrepareHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Generic_ModuleReleaseHardware(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesTranslated
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleD0Entry(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleD0EntryPostInterruptsEnabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Generic_ModuleD0ExitPreInterruptsDisabled(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Generic_ModuleD0Exit(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Generic_ModuleQueueIoRead(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_Generic_ModuleQueueIoWrite(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Generic_ModuleDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Generic_ModuleInternalDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleSelfManagedIoCleanup(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleSelfManagedIoFlush(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleSelfManagedIoInit(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleSelfManagedIoSuspend(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleSelfManagedIoRestart(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleSurpriseRemoval(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleQueryRemove(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleQueryStop(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleRelationsQuery(
    _In_ DMFMODULE DmfModule,
    _In_ DEVICE_RELATION_TYPE RelationType
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleUsageNotificationEx(
    _In_ DMFMODULE DmfModule,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleArmWakeFromS0(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleDisarmWakeFromS0(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleWakeFromS0Triggered(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_ModuleArmWakeFromSxWithReason(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleDisarmWakeFromSx(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ModuleWakeFromSxTriggered(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Generic_ModuleFileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Generic_ModuleFileCleanup(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_Generic_ModuleFileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Generic_ResourcesAssign(
    _In_ DMFMODULE DmfModule,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_NotificationRegister(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Generic_Open(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_Close(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_AuxiliaryLock_Passive(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_AuxiliaryUnlock_Passive(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Generic_AuxiliaryLock_Dispatch(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_Generic_AuxiliaryUnlock_Dispatch(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_Lock_Passive(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_Generic_Unlock_Passive(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
#pragma warning(suppress: 28167)
#pragma warning(suppress: 28158)
DMF_Generic_Lock_Dispatch(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
#pragma warning(suppress: 28167)
DMF_Generic_Unlock_Dispatch(
    _In_ DMFMODULE DmfModule
    );

__forceinline
DMF_OBJECT*
DMF_ModuleToObject(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Converts the DMFMODULE (which is a really a WDFMEMORY object)
    to its corresponding internal DMF_OBJECT pointer.

--*/
{
    DMF_OBJECT* dmfObject;

    ASSERT(DmfModule != NULL);

    dmfObject = (DMF_OBJECT*)WdfMemoryGetBuffer((WDFMEMORY)DmfModule,
                                                NULL);
    ASSERT(dmfObject != NULL);

    return dmfObject;
}

__forceinline
DMFMODULE
DMF_ObjectToModule(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Converts the internal DMF_OBJECT pointer to its corresponding DMFMODULE.

--*/
{
    ASSERT(DmfObject != NULL);
    return (DMFMODULE)DmfObject->MemoryDmfObject;
}

__forceinline
DMF_INTERFACE_OBJECT*
DMF_InterfaceToObject(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Converts the DMFINTERFACE (which is a really a WDFMEMORY object)
    to its corresponding internal DMF_INTERFACE_OBJECT pointer.

--*/
{
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;

    ASSERT(DmfInterface != NULL);

    dmfInterfaceObject = (DMF_INTERFACE_OBJECT*)WdfMemoryGetBuffer((WDFMEMORY)DmfInterface,
                                                                    NULL);
    ASSERT(dmfInterfaceObject != NULL);

    return dmfInterfaceObject;
}

__forceinline
DMFINTERFACE
DMF_ObjectToInterface(
    _In_ DMF_INTERFACE_OBJECT* DmfInterfaceObject
    )
/*++

Routine Description:

    Converts the internal DMF_INTERFACE_OBJECT pointer to its corresponding DMFINTERFACE.

--*/
{
    ASSERT(DmfInterfaceObject != NULL);
    return (DMFINTERFACE)DmfInterfaceObject->DmfInterface;
}

__forceinline
BOOLEAN
DMF_IsObjectTypeOpenNotify(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Identify if the Module Open type is NOTIFY for a given DMF Object.

Arguments:

    DmfObject - The given DMF Object.

Return Value:

   TRUE if the open type is NOTIFY; FALSE otherwise.

--*/
{
    BOOLEAN openTypeNotify;

    if ((DmfObject->ModuleDescriptor.OpenOption == DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware) ||
        (DmfObject->ModuleDescriptor.OpenOption == DMF_MODULE_OPEN_OPTION_NOTIFY_D0Entry) ||
        (DmfObject->ModuleDescriptor.OpenOption == DMF_MODULE_OPEN_OPTION_NOTIFY_Create))
    {
        openTypeNotify = TRUE;
    }
    else
    {
        openTypeNotify = FALSE;
    }

    return openTypeNotify;
}

__forceinline
DMF_MODULE_COLLECTION*
DMF_CollectionToHandle(
    _In_ DMFCOLLECTION DmfCollection
    )
/*++

Routine Description:

    Converts the DMFCOLLECTION (which is a really a WDFMEMORY object)
    to its internal DMF_MODULE_COLLECTION pointer.

--*/
{
    DMF_MODULE_COLLECTION* dmfModuleCollection;

    ASSERT(DmfCollection != NULL);

    dmfModuleCollection = (DMF_MODULE_COLLECTION*)WdfMemoryGetBuffer((WDFMEMORY)DmfCollection,
                                                                     NULL);

    return dmfModuleCollection;
}

__forceinline
DMFCOLLECTION
DMF_ObjectToCollection(
    _In_ DMF_MODULE_COLLECTION* DmfModuleCollection
    )
/*++

Routine Description:

    Converts the internal DMF_MODULE_COLLECTION pointer to its corresponding DMFCOLLECTION.

--*/
{
    ASSERT(DmfModuleCollection != NULL);
    return (DMFCOLLECTION)DmfModuleCollection->ModuleCollectionHandleMemory;
}

NTSTATUS
DMF_Module_OpenOrRegisterNotificationOnCreate(
    _In_ DMFMODULE DmfModule
    );

VOID
DMF_Module_CloseOrUnregisterNotificationOnDestroy(
    _In_ DMFMODULE DmfModule
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleWaitForReferenceCountToClear(
    _In_ DMFMODULE DmfModule
    );

// DmfContainer.c
//

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContainerPnpPowerCallbacksInit(
    _Out_ WDF_PNPPOWER_EVENT_CALLBACKS* PnpPowerEventCallbacks
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContainerPowerPolicyCallbacksInit(
    _Out_ WDF_POWER_POLICY_EVENT_CALLBACKS* PowerPolicyCallbacks
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContainerFileObjectConfigInit(
    _Out_ WDF_FILEOBJECT_CONFIG* FileObjectConfig
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ContainerQueueConfigCallbacksInit(
    _Out_ WDF_IO_QUEUE_CONFIG* IoQueueConfig
    );

// DmfDeviceInit.c
//

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitValidate(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitIsBridgeEnabled(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_CONFIG_Bridge*
DMF_DmfDeviceInitBridgeModuleConfigGet(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitIsControlDevice(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
WDFDEVICE
DMF_DmfControlDeviceInitClientDriverDeviceGet(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitIsDefaultQueueCreated(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_CONFIG_BranchTrack*
DMF_DmfDeviceInitBranchTrackModuleConfigGet(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitIsFilterDriver(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_CONFIG_LiveKernelDump*
DMF_DmfDeviceInitLiveKernelDumpModuleConfigGet(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
DMF_EVENT_CALLBACKS*
DMF_DmfDeviceInitDmfEventCallbacksGet(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_DmfDeviceInitClientImplementsDeviceAdd(
    _In_ PDMFDEVICE_INIT DmfDeviceInit
    );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Collection
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionDestroy(
    _In_ WDFOBJECT Object
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCollectionPrepareHardware(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionReleaseHardware(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFCMRESLIST ResourcesTranslated
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCollectionD0Entry(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleCollectionD0EntryPostInterruptsEnabled(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_ModuleCollectionD0EntryCleanup(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionD0ExitPreInterruptsDisabled(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionD0Exit(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleCollectionQueueIoRead(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleCollectionQueueIoWrite(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleCollectionDeviceIoControl(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_ModuleCollectionInternalDeviceIoControl(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionSelfManagedIoCleanup(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionSelfManagedIoFlush(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionSelfManagedIoInit(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionSelfManagedIoSuspend(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionSelfManagedIoRestart(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionSurpriseRemoval(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionQueryRemove(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionQueryStop(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionRelationsQuery(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ DEVICE_RELATION_TYPE RelationType
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionUsageNotificationEx(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDF_SPECIAL_FILE_TYPE NotificationType,
    _In_ BOOLEAN IsInNotificationPath
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionArmWakeFromS0(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionDisarmWakeFromS0(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionWakeFromS0Triggered(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_ModuleCollectionArmWakeFromSxWithReason(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ BOOLEAN DeviceWakeEnabled,
    _In_ BOOLEAN ChildrenArmedForWake
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionDisarmWakeFromSx(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleCollectionWakeFromSxTriggered(
    _In_ DMFCOLLECTION DmfCollection
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_ModuleCollectionFileCreate(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_ModuleCollectionFileCleanup(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFFILEOBJECT FileObject
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_ModuleCollectionFileClose(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ WDFFILEOBJECT FileObject
    );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Transport
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

VOID
DMF_ModuleTransportSet(
    _In_ DMFMODULE DmfModule,
    _In_ DMFMODULE TransportDmfModule
    );

// eof: DmfIncludeInternal.h
//
