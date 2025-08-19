/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfHelpers.c

Abstract:

    DMF Implementation:

    This Module contains helper functions. Some are used by Clients and DMF, others are used
    only by the DMF.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#if defined(DMF_INCLUDE_TMH)
#include "DmfHelpers.tmh"
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// These are used by both DMF and DMF Clients.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFDEVICE
DMF_ParentDeviceGet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Returns the WDFDEVICE that contains the given DmfModule.

Arguments:

    DmfModule: The given DMF Module.

Return Value:

    WDFDEVICE.

--*/
{
    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //

    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    DMF_HandleValidate_IsAvailable(dmfObject);

    DmfAssert(dmfObject != NULL);
    DmfAssert(dmfObject->ParentDevice != NULL);

    return dmfObject->ParentDevice;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFDEVICE
DMF_FilterDeviceGet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Returns the WDFDEVICE Object that corresponds to the Client (Filter) Driver's FDO.
    This function should only be used by Filter drivers.

Arguments:

    DmfModule: This Module's Instance DMF Module.

Return Value:

    WDFDEVICE that corresponds to the Client Driver's FDO.

--*/
{
    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //
    DMF_DEVICE_CONTEXT* dmfDeviceContext;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    DMF_HandleValidate_IsAvailable(dmfObject);

    DmfAssert(dmfObject != NULL);
    DmfAssert(dmfObject->ParentDevice != NULL);

    // ParentDevice can be either Client Driver device or the Control device 
    // (in the case when the Client Driver is a filter driver and DmfModule is added 
    // to the Control device.)
    // Since Client will need access to Client Driver device,
    // return the Client Driver device stored in ParentDevice's dmf context.
    //
    dmfDeviceContext = DmfDeviceContextGet(dmfObject->ParentDevice);

    DmfAssert(dmfDeviceContext != NULL);
    DmfAssert(dmfDeviceContext->WdfClientDriverDevice != NULL);

    return dmfDeviceContext->WdfClientDriverDevice;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
DMFMODULE
DMF_ParentModuleGet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Given a DMFMODULE, returns its Parent DMFMODULE.

Arguments:

    DmfModule - The given DMFMODULE.

Return Value:

    Parent DMFMODULE of the given DMFMODULE.
    NULL if no parent. (It is in the Module Collection array.)

--*/
{
    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //

    DMF_OBJECT* dmfObject;
    DMFMODULE dmfModuleParent;

    dmfObject = DMF_ModuleToObject(DmfModule);

    DMF_HandleValidate_IsAvailable(dmfObject);

    if (dmfObject->DmfObjectParent != NULL)
    {
        dmfModuleParent = (DMFMODULE)(dmfObject->DmfObjectParent->MemoryDmfObject);
    }
    else
    {
        dmfModuleParent = NULL;
    }

    return dmfModuleParent;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID*
DMF_ModuleConfigGet(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Returns the Client Driver's Module Config for use by the Module when the Module is opened.
    The Module Config allows the Module to initialize itself with Module specific parameters that
    are set by the Client Driver.

Arguments:

    DmfModule: This Module's Instance DMF Module.

Return Value:

    The Module's Module Config structure. Each Module casts the pointer to its own known
    structure type.

--*/
{
    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //
    DMF_OBJECT* DmfObject;

    DmfObject = DMF_ModuleToObject(DmfModule);

    DMF_HandleValidate_IsAvailable(DmfObject);

    DmfAssert(DmfObject != NULL);

    return DmfObject->ModuleConfig;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_IsModuleDynamic(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Returns TRUE if 
        - the given DMF Module was created dynamically. or
        - the given DMF Module is part of a dynamic Module tree.
    FALSE if the given DMF Module is part of Module Collection. 

Arguments:

    DmfModule: The given DMF Module.

Return Value:

    TRUE if 
        - the given DMF Module was created dynamically. or
        - the given DMF Module is part of a dynamic Module tree.
    FALSE if the given DMF Module is part of Module Collection.

--*/
{
    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //
    DMF_OBJECT* DmfObject;

    DmfObject = DMF_ModuleToObject(DmfModule);

    DMF_HandleValidate_IsAvailable(DmfObject);

    DmfAssert(DmfObject != NULL);

    return DmfObject->ModuleAttributes.DynamicModule;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_IsModulePassiveLevel(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Allows caller to access PassiveLevel field of a given Module's Attributes.

Arguments:

    DmfModule: The given DMF Module.

Return Value:

    Returns TRUE if the given DMF Module was created with PassiveLevel = TRUE; otherwise returns FALSE.

--*/
{
    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //
    DMF_OBJECT* DmfObject;

    DmfObject = DMF_ModuleToObject(DmfModule);

    DMF_HandleValidate_IsAvailable(DmfObject);

    DmfAssert(DmfObject != NULL);

    return DmfObject->ModuleAttributes.PassiveLevel;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ModuleConfigRetrieve(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(ModuleConfigSize) PVOID ModuleConfigPointer,
    _In_ size_t ModuleConfigSize
    )
/*++

Routine Description:

    Returns a copy of the given Module's Config for use by the Client.

Arguments:

    DmfModule: The given DMF Module.
    ModuleConfigPointer: Address where the Module's config is copied to.
    ModuleConfigSize: Size of Module config. Used for validation.

Return Value:

    NTSTATUS

--*/
{
    // NOTE: No FuncEntry/Exit logging. It is too much and it is not necessary for this
    // simple function.
    //
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    ntStatus = STATUS_SUCCESS;

    DMF_HandleValidate_IsAvailable(dmfObject);

    DmfAssert(dmfObject != NULL);

    if (dmfObject->ModuleConfig != NULL)
    {
        if (dmfObject->ModuleConfigSize > ModuleConfigSize)
        {
            ntStatus = STATUS_INVALID_BUFFER_SIZE;
        }
        else
        {
            RtlCopyMemory(ModuleConfigPointer,
                          dmfObject->ModuleConfig,
                          dmfObject->ModuleConfigSize);
        }
    }
    else
    {
        // This API should not be called in this case
        // because Client should know that no Config was set.
        //
        DmfAssert(FALSE);
        ntStatus = STATUS_NOT_FOUND;
    }

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN
DMF_ModuleIsInFilterDriver(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Indicates if the given Module is executing in a filter driver.

Arguments:

    DmfModule - The given Module.

Return Value:

    TRUE indicates the Client Driver is a filter driver.
    FALSE indicates the Client Driver is not filter driver.

--*/
{
    DMF_DEVICE_CONTEXT* deviceContext;
    WDFDEVICE device;

    DMF_ObjectValidate(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);
    deviceContext = DmfDeviceContextGet(device);

    return deviceContext->IsFilterDevice;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// These are used by only by DMF.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef
PVOID
(*InternalExAllocatePoolWithTag)(
    _In_ POOL_TYPE PoolType,
    _In_ SIZE_T NumberOfBytes,
    _In_ ULONG Tag
    );

typedef
PVOID
(*InternalExAllocatePool2)(
    _In_ ULONG64 Flags,
    _In_ SIZE_T NumberOfBytes,
    _In_ ULONG Tag
    );

#if !defined(DMF_USER_MODE)
    VOID*
    DMF_GenericMemoryAllocate(
        _In_ ULONG64 PoolFlags,
        _In_ size_t Size,
        _In_ ULONG Tag
        )
/*++

Routine Description:

    Allocates memory in Kernel-mode using native NT API instead of WDF.
    For some memory allocations it is not necessary to incur the overhead
    of WDF handles. In those cases, use this function instead of 
    WdfMemoryCreate() to avoid creating a new handle.

    NOTE: This code needs support legacy Windows which does not support 
          ExAllocatePool2. Therefore, the checks below are necessary.
          This code only executes internally in DMF and performance is not
          a consideration since it only executes while creating a Module.

Arguments:

    PoolFlags - Type of pool to allocate NonPagedPoolNx or PagedPool.
    Size - Number of bytes to allocate.
    Tag - Tag to assign for debug purposes.

Return Value:

    NULL if memory cannot be allocated.
    If not NULL, it is the address of allocated memory

--*/
    {
        VOID* returnValue;
        UNICODE_STRING functionName;
        static InternalExAllocatePool2 internalExAllocatePool2;
        static InternalExAllocatePoolWithTag internalExAllocatePoolWithTag;

        if (internalExAllocatePool2 == NULL)
        {
            if (internalExAllocatePoolWithTag != NULL)
            {
                goto Legacy;
            }
            // Search for the function the first time.
            //
            RtlInitUnicodeString(&functionName,
                                 L"ExAllocatePool2");
            internalExAllocatePool2 = (InternalExAllocatePool2)MmGetSystemRoutineAddress(&functionName);
            if (internalExAllocatePool2 != NULL)
            {
                goto NonLegacy;
            }
            else
            {
                if (internalExAllocatePoolWithTag == NULL)
                {
                    // Search for the function the first time.
                    // NOTE: To pass CodeQL, code cannot reference ExAllocatePoolWithTag directly.
                    //
                    RtlInitUnicodeString(&functionName,
                                         L"ExAllocatePoolWithTag");
                    internalExAllocatePoolWithTag = (InternalExAllocatePoolWithTag)MmGetSystemRoutineAddress(&functionName);
                    if (internalExAllocatePoolWithTag != NULL)
                    {
                        goto Legacy;
                    }
                    else
                    {
                        // This should never happen.
                        //
                        DmfAssert(FALSE);
                        returnValue = NULL;
                    }
                }
                else
                {
Legacy:
                    POOL_TYPE poolType = NonPagedPoolNx;

                    if (PoolFlags == POOL_FLAG_PAGED)
                    {
                        poolType = PagedPool;
                    }
                    returnValue = internalExAllocatePoolWithTag(poolType,
                                                                Size,
                                                                Tag);
                }
            }
        }
        else
        {
NonLegacy:
            returnValue = internalExAllocatePool2(PoolFlags,
                                                  Size,
                                                  Tag);
        }

        return returnValue;
    }

    VOID
    DMF_GenericMemoryFree(
        _In_ VOID* Pointer,
        _In_ ULONG Tag
        )
    /*++

    Routine Description:

        Free memory allocated by DMF_GenericMemoryAllocate().

    Arguments:

        Pointer - Address of memory to free.
        Tag - Tag to assign for debug purposes.

    Return Value:

        NULL if memory cannot be allocated.
        If not NULL, it is the address of allocated memory

    --*/
    {
        ExFreePoolWithTag(Pointer,
                          Tag);
    }
#else
    VOID*
    DMF_GenericMemoryAllocate(
        _In_ ULONG64 PoolFlags,
        _In_ size_t Size,
        _In_ ULONG Tag
        )
    /*++

    Routine Description:

        Allocates memory in User-mode using native C runtime instead of WDF.
        For some memory allocations it is not necessary to incur the overhead
        of WDF handles. In those cases, use this function instead of 
        WdfMemoryCreate() to avoid creating a new handle.

    Arguments:

        PoolFlags - Type of pool to allocate NonPagedPoolNx or PagedPool. (Not used).
        Size - Number of bytes to allocate.
        Tag - Tag to assign for debug purposes. (Not used).

    Return Value:

        NULL if memory cannot be allocated.
        If not NULL, it is the address of allocated memory

    --*/
    {
        VOID* returnValue;

        UNREFERENCED_PARAMETER(PoolFlags);
        UNREFERENCED_PARAMETER(Tag);

        returnValue = malloc(Size);

        return returnValue;
    }

    VOID
    DMF_GenericMemoryFree(
        _In_ VOID* Pointer,
        _In_ ULONG Tag
        )
    /*++

    Routine Description:

        Free memory allocated by DMF_GenericMemoryAllocate().

    Arguments:

        Pointer - Address of memory to free.
        Tag - Tag to assign for debug purposes. (Not used.)

    Return Value:

        NULL if memory cannot be allocated.
        If not NULL, it is the address of allocated memory

    --*/
    {
        UNREFERENCED_PARAMETER(Tag);

        free(Pointer);
    }
#endif

#if defined(DMF_ALWAYS_USE_WDF_HANDLES)
    NTSTATUS
    DMF_GenericSpinLockCreate(
        _In_ GENERIC_SPINLOCK_CREATE_CONTEXT* NativeLockCreateContext,
        _Out_ DMF_GENERIC_SPINLOCK* GenericSpinLock
        )
    /*++

    Routine Description:

        Allocates a spinlock using WDF API.

    Arguments:

        NativeLockCreateContext - WDF_OBJECT_ATTRIBUTES*.
        GenericSpinLock - The returned spinlock.

    Return Value:

        NTSTATUS

    --*/
    {
        NTSTATUS ntStatus;

        ntStatus = WdfSpinLockCreate(NativeLockCreateContext,
                                     GenericSpinLock);

        return ntStatus;
    }

    _Acquires_lock_(*GenericSpinLock)
    VOID
    DMF_GenericSpinLockAcquire(
        _In_ DMF_GENERIC_SPINLOCK* GenericSpinLock,
        _Out_ GENERIC_SPINLOCK_CONTEXT *NativeLockContext
        )
    /*++

    Routine Description:

        Acquire a spinlock created by its corresponding DMF_GenericSpinLockCreate();.

    Arguments:

        GenericSpinLock - The returned spinlock to acquire.
        NativeLockCreateContext - Not used.

    Return Value:

        None

    --*/
    {
        UNREFERENCED_PARAMETER(NativeLockContext);

        WdfSpinLockAcquire(*GenericSpinLock);
    }

    _Releases_lock_(*GenericSpinLock)
    VOID
    DMF_GenericSpinLockRelease(
        _In_ DMF_GENERIC_SPINLOCK* GenericSpinLock,
        _In_ GENERIC_SPINLOCK_CONTEXT NativeLockContext
        )
    /*++

    Routine Description:

        Release a spinlock created by its corresponding DMF_GenericSpinLockCreate();.

    Arguments:

        GenericSpinLock - The returned spinlock to release.
        NativeLockCreateContext - Not used.

    Return Value:

        None

    --*/
    {
        UNREFERENCED_PARAMETER(NativeLockContext);

        WdfSpinLockRelease(*GenericSpinLock);
    }

    VOID
    DMF_GenericSpinLockDestroy(
        _In_ DMF_GENERIC_SPINLOCK* GenericSpinLock
        )
    /*++

    Routine Description:

        Destroy a spinlock created by its corresponding DMF_GenericSpinLockCreate();.

    Arguments:

        GenericSpinLock - The returned spinlock to destroy..

    Return Value:

        None

    --*/
    {
        // Do not call "WdfObjectDelete(*GenericSpinLock);" here to maintain same code path
        // with historical code. This lock is created by the DMF and used only by DMF and it
        // is deleted automatically because its parent is the DMFMODULE.
        //
        UNREFERENCED_PARAMETER(GenericSpinLock);
    }
#else
    #if !defined(DMF_USER_MODE)
        NTSTATUS
        DMF_GenericSpinLockCreate(
            _In_ GENERIC_SPINLOCK_CREATE_CONTEXT* NativeLockCreateContext,
            _Out_ DMF_GENERIC_SPINLOCK* GenericSpinLock
            )
        /*++

        Routine Description:

            Allocates a spinlock using NT API.

        Arguments:

            NativeLockCreateContext - Not used.
            GenericSpinLock - The returned spinlock.

        Return Value:

            STATUS_SUCCESS

        --*/
        {
            UNREFERENCED_PARAMETER(NativeLockCreateContext);

            KeInitializeSpinLock(GenericSpinLock);

            return STATUS_SUCCESS;
        }

        _Acquires_lock_(*GenericSpinLock)
        VOID
        DMF_GenericSpinLockAcquire(
            _In_ DMF_GENERIC_SPINLOCK* GenericSpinLock,
            _Out_ GENERIC_SPINLOCK_CONTEXT *NativeLockContext
            )
        /*++

        Routine Description:

            Acquire a spinlock created by its corresponding DMF_GenericSpinLockCreate();.

        Arguments:

            GenericSpinLock - The returned spinlock to acquire.
            NativeLockCreateContext - Old IRQL before spinlock was acquired.

        Return Value:

            None

        --*/
        {
            KeAcquireSpinLock(GenericSpinLock,
                              NativeLockContext);
        }

        _Releases_lock_(*GenericSpinLock)
        VOID
        DMF_GenericSpinLockRelease(
            _In_ DMF_GENERIC_SPINLOCK* GenericSpinLock,
            _In_ GENERIC_SPINLOCK_CONTEXT NativeLockContext
            )
        /*++

        Routine Description:

            Release a spinlock created by its corresponding DMF_GenericSpinLockCreate();.

        Arguments:

            GenericSpinLock - The returned spinlock to acquire.
            NativeLockCreateContext - Old IRQL before spinlock was acquired.

        Return Value:

            None

        --*/
        {
            KeReleaseSpinLock(GenericSpinLock,
                              NativeLockContext);
        }

        VOID
        DMF_GenericSpinLockDestroy(
            _In_ DMF_GENERIC_SPINLOCK* GenericSpinLock
            )
        /*++

        Routine Description:

            Destroy a spinlock created by its corresponding DMF_GenericSpinLockCreate();.

        Arguments:

            GenericSpinLock - The returned spinlock to destroy.

        Return Value:

            None

        --*/
        {
            UNREFERENCED_PARAMETER(GenericSpinLock);
        }
    #else
        NTSTATUS
        DMF_GenericSpinLockCreate(
            _In_ GENERIC_SPINLOCK_CREATE_CONTEXT* NativeLockCreateContext,
            _Out_ DMF_GENERIC_SPINLOCK* GenericSpinLock
            )
        /*++

        Routine Description:

            Allocates a spinlock using Win32 API.

        Arguments:

            NativeLockCreateContext - Not used.
            GenericSpinLock - The returned spinlock.

        Return Value:

            STATUS_SUCCESS

        --*/
        {
            UNREFERENCED_PARAMETER(NativeLockCreateContext);

            InitializeCriticalSection(GenericSpinLock);

            return STATUS_SUCCESS;
        }

        _Acquires_lock_(*GenericSpinLock)
        VOID
        DMF_GenericSpinLockAcquire(
            _In_ DMF_GENERIC_SPINLOCK* GenericSpinLock,
            _Out_ GENERIC_SPINLOCK_CONTEXT *NativeLockContext
            )
        /*++

        Routine Description:

            Acquire a spinlock created by its corresponding DMF_GenericSpinLockCreate();.

        Arguments:

            GenericSpinLock - The returned spinlock to acquire.
            NativeLockCreateContext - Not used.

        Return Value:

            None

        --*/
        {
            UNREFERENCED_PARAMETER(NativeLockContext);

            EnterCriticalSection(GenericSpinLock);
        }

        _Releases_lock_(*GenericSpinLock)
        VOID
        DMF_GenericSpinLockRelease(
            _In_ DMF_GENERIC_SPINLOCK* GenericSpinLock,
            _In_ GENERIC_SPINLOCK_CONTEXT NativeLockContext
            )
        /*++

        Routine Description:

            Release a spinlock created by its corresponding DMF_GenericSpinLockCreate();.

        Arguments:

            GenericSpinLock - The returned spinlock to acquire.
            NativeLockCreateContext - Not used.

        Return Value:

            None

        --*/
        {
            UNREFERENCED_PARAMETER(NativeLockContext);

            LeaveCriticalSection(GenericSpinLock);
        }

        VOID
        DMF_GenericSpinLockDestroy(
            _In_ DMF_GENERIC_SPINLOCK* GenericSpinLock
            )
        /*++

        Routine Description:

            Destroy a spinlock created by its corresponding DMF_GenericSpinLockCreate();.

        Arguments:

            GenericSpinLock - The returned spinlock to destroy.

        Return Value:

            None

        --*/
        {
            DeleteCriticalSection(GenericSpinLock);
        }
    #endif
#endif

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_SynchronizationCreate(
    _In_ DMF_OBJECT* DmfObject,
    _In_ BOOLEAN PassiveLevel
    )
/*++

Routine Description:

    Create a set of Locks for a given DMF Module.
    PassiveLevel - TRUE if Client wants the Module options to be set to MODULE_OPTIONS_PASSIVE.
                   NOTE: Module Options must be set to MODULE_OPTIONS_DISPATCH_MAXIMUM. 

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    STATUS_SUCCESS - locks are created.
    STATUS_UNSUCCESSFUL - locks were not created.

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES attributes;
    ULONG lockIndex;
    DMF_MODULE_DESCRIPTOR* moduleDescriptor;

    PAGED_CODE();

    FuncEntryArguments(DMF_TRACE, "DmfObject=0x%p [%s]", DmfObject, DmfObject->ClientModuleInstanceName);

    moduleDescriptor = &DmfObject->ModuleDescriptor;

    DmfAssert(moduleDescriptor->NumberOfAuxiliaryLocks <= DMF_MAXIMUM_AUXILIARY_LOCKS);

    if (moduleDescriptor->ModuleOptions & DMF_MODULE_OPTIONS_DISPATCH_MAXIMUM)
    {
        if (PassiveLevel)
        {
            moduleDescriptor->ModuleOptions |= DMF_MODULE_OPTIONS_PASSIVE;
        }
        else
        {
            moduleDescriptor->ModuleOptions |= DMF_MODULE_OPTIONS_DISPATCH;
        }
    }
    else
    {
        if ((moduleDescriptor->ModuleOptions & DMF_MODULE_OPTIONS_DISPATCH) &&
            PassiveLevel)
        {
            DmfAssert(FALSE);
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
            goto Exit;
        }
    }

    // Create the locking mechanism based on Module Options.
    //
    if (moduleDescriptor->ModuleOptions & DMF_MODULE_OPTIONS_PASSIVE)
    {
        TraceVerbose(DMF_TRACE, "DMF_MODULE_OPTIONS_PASSIVE");

        DmfAssert(! (moduleDescriptor->ModuleOptions & DMF_MODULE_OPTIONS_DISPATCH));

        // Create the Generic PASSIVE_LEVEL Lock for the Auxiliary Synchronization and one device lock.
        //
        for (lockIndex = 0; lockIndex < moduleDescriptor->NumberOfAuxiliaryLocks + DMF_NUMBER_OF_DEFAULT_LOCKS; lockIndex++)
        {
            WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
            attributes.ParentObject = DmfObject->MemoryDmfObject;
            ntStatus = WdfWaitLockCreate(&attributes,
                                         &DmfObject->Synchronizations[lockIndex].SynchronizationPassiveWaitLock);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfSpinLockCreate fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }
    }
    else
    {
        TraceVerbose(DMF_TRACE, "DMF_MODULE_OPTIONS_DISPATCH");

        // Create the Generic DISPATCH_LEVEL Lock for the Auxiliary Synchronization and one device lock.
        //
        for (lockIndex = 0; lockIndex < moduleDescriptor->NumberOfAuxiliaryLocks + DMF_NUMBER_OF_DEFAULT_LOCKS; lockIndex++)
        {
            WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
            attributes.ParentObject = DmfObject->MemoryDmfObject;
            ntStatus = WdfSpinLockCreate(&attributes,
                                         &DmfObject->Synchronizations[lockIndex].SynchronizationDispatchSpinLock);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfSpinLockCreate fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "DmfObject=0x%p [%s] ntStatus=%!STATUS!", DmfObject, DmfObject->ClientModuleInstanceName, ntStatus);

    return ntStatus;
}
#pragma code_seg()

// TODO: This function should not reference DMF_OBJECT nor DMF_MODULE_COLLECTION.
//       Currently it is an exception because it is an "internal" Module. But, this
//       needs to be fixed.
//
DMF_OBJECT*
DMF_FeatureHandleGetFromModuleCollection(
    _In_ DMFCOLLECTION DmfCollection,
    _In_ DmfFeatureType DmfFeature
    )
/*++

Routine Description:

    Get the DMF Object from the DMF Collection of the specified DMF Feature.

Arguments:

    DmfCollection - The given DMF Collection.
    DmfFeatureType - The required DMF Feature identifier.

Return Value:

    DMF Object of the required DMF Feature.

--*/
{
    DMF_MODULE_COLLECTION* moduleCollectionHandle;
    DMF_OBJECT* dmfObjectFeature;

    DmfAssert(DmfCollection != NULL);

    moduleCollectionHandle = DMF_CollectionToHandle(DmfCollection);

    DmfAssert((DmfFeature > DmfFeature_Invalid) && 
              (DmfFeature < DmfFeature_NumberOfFeatures));
    __analysis_assume((DmfFeature > DmfFeature_Invalid) && 
                      (DmfFeature < DmfFeature_NumberOfFeatures));

    dmfObjectFeature = moduleCollectionHandle->DmfObjectFeature[DmfFeature];
    // It can be NULL if this feature is not running.
    //

    return dmfObjectFeature;
}

DMFMODULE
DMF_FeatureModuleGetFromModule(
    _In_ DMFMODULE DmfModule,
    _In_ DmfFeatureType DmfFeature
    )
/*++

Routine Description:

    Given a DMF Module and a feature identifier, return the corresponding Feature Handle.

Arguments:

    DmfModule - The given DMF Module.
    DmfFeature - The required DMF feature identifier.

Return Value:

    The Feature Module that was created automatically for this Module.

--*/
{
    DMF_OBJECT* dmfObjectFeature;
    DMF_OBJECT* dmfObject;
    DMFMODULE dmfModuleFeature;

    dmfObject = DMF_ModuleToObject(DmfModule);
    DmfAssert(dmfObject != NULL);
    dmfObjectFeature = NULL;

    if (dmfObject->ModuleCollection != NULL)
    {
        DMFCOLLECTION dmfCollection = DMF_ObjectToCollection(dmfObject->ModuleCollection);

        dmfObjectFeature = DMF_FeatureHandleGetFromModuleCollection(dmfCollection,
                                                                    DmfFeature);
    }

    if (dmfObjectFeature != NULL)
    {
        dmfModuleFeature = DMF_ObjectToModule(dmfObjectFeature);
        DMF_ObjectValidate(dmfModuleFeature);
    }
    else
    {
        // The Client Driver has not enabled this DMF Feature.
        // Dynamic Modules do not support DMF Features.
        //
        dmfModuleFeature = NULL;
    }

    return dmfModuleFeature;
}

DMFMODULE
DMF_FeatureModuleGetFromDevice(
    _In_ WDFDEVICE Device,
    _In_ DmfFeatureType DmfFeature
    )
/*++

Routine Description:

    Given a WDF Device and a feature identifier, return the corresponding Feature Module.

Arguments:

    Device - The given WDFDEVICE handle.
    DmfFeature - The required DMF feature identifier.

Return Value:

    The Feature Module that was created automatically for the given WDF Device.

--*/
{
    DMF_OBJECT* dmfObjectFeature;
    DMFMODULE dmfModuleFeature;
    DMF_DEVICE_CONTEXT* dmfDeviceContext;

    DmfAssert(Device != NULL);

    dmfDeviceContext = DmfDeviceContextGet(Device);
    DmfAssert(dmfDeviceContext != NULL);

    dmfObjectFeature = DMF_FeatureHandleGetFromModuleCollection(dmfDeviceContext->DmfCollection,
                                                                DmfFeature);
    if (dmfObjectFeature != NULL)
    {
        dmfModuleFeature = DMF_ObjectToModule(dmfObjectFeature);
        DMF_ObjectValidate(dmfModuleFeature);
    }
    else
    {
        // The Client Driver has not enabled Automatic BranchTrack.
        //
        dmfModuleFeature = NULL;
    }

    return dmfModuleFeature;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RequestPassthru(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request
    )
/*++

Routine Description:

    Forward the given request to next lower driver.

Arguments:

    DmfModule - This Module's handle.

    Request - The given request.

Return Value:

    None. If the given request cannot be forwarded, the request is completed with error.

--*/
{
    WDFIOTARGET ioTarget;
    WDF_REQUEST_SEND_OPTIONS sendOptions; 

    ioTarget = WdfDeviceGetIoTarget(Device);

    WdfRequestFormatRequestUsingCurrentType(Request);
    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions,
                                  WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
    if (! WdfRequestSend(Request,
                         ioTarget,
                         &sendOptions))
    {
        // This is an error that generally should not happen.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Unable to Passthru Request: Request=%p", Request);

        // It could not be passed down, so just complete it with an error.
        //
        WdfRequestComplete(Request,
                           STATUS_INVALID_DEVICE_STATE);
    }
    else
    {
        // Request will be completed by the target.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Passthru Request: Request=%p", Request);
    }
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_RequestPassthruWithCompletion(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ PFN_WDF_REQUEST_COMPLETION_ROUTINE CompletionRoutine,
    __drv_aliasesMem WDFCONTEXT CompletionContext
    )
/*++

Routine Description:

    Forward the given request to next lower driver. Sets a completion routine so that the WDFREQUEST can
    be post-processed.

Arguments:

    DmfModule - This Module's handle.

    Request - The given request.

Return Value:

    None. If the given request cannot be forwarded, the request is completed with error.

--*/
{
    WDFIOTARGET ioTarget;

    ioTarget = WdfDeviceGetIoTarget(Device);

    WdfRequestFormatRequestUsingCurrentType(Request);
    WdfRequestSetCompletionRoutine(Request,
                                   CompletionRoutine,
                                   CompletionContext);
    if (! WdfRequestSend(Request,
                         ioTarget,
                         NULL))
    {
        // This is an error that generally should not happen.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Unable to Passthru Request: Request=%p", Request);

        // It could not be passed down, so just complete it with an error.
        //
        WdfRequestComplete(Request,
                           STATUS_INVALID_DEVICE_STATE);
    }
    else
    {
        // Request will be completed by the target.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Passthru Request: Request=%p", Request);
    }
}

HANDLE 
DmfGetCurrentThreadId(
    )
/*++

Routine Description:

    Helper routine to get current thread for kernel and user mode.

Arguments:

    None

Return Value:

    ID of current thread

--*/
{
    HANDLE currentThreadId;

#if defined(DMF_USER_MODE)
    #if defined(DMF_WIN32_MODE)
        // 'type cast': conversion from 'DWORD' to 'HANDLE' of greater size
        //
        #pragma warning(push)
        #pragma warning(disable:4312)
    #endif
    currentThreadId = (HANDLE)GetCurrentThreadId();
    #if defined(DMF_WIN32_MODE)
        // 'type cast': conversion from 'DWORD' to 'HANDLE' of greater size
        //
        #pragma warning(pop)
    #endif
#else
    currentThreadId = PsGetCurrentThread();
#endif

    DmfAssert(currentThreadId != NULL);
    return currentThreadId;
}

VOID
DMF_ModuleLockPrivate(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Acquire a Module's primary lock.

    NOTE: This function should only be called from a Module and that Module must be
          the creator of this lock. This function is called indirectly
          after proper ownership is verified.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    DmfAssert(dmfObject != NULL);
    DmfAssert(dmfObject->InternalCallbacksInternal.AuxiliaryLock != NULL);
    (dmfObject->InternalCallbacksInternal.AuxiliaryLock)(DmfModule,
                                                         DMF_DEFAULT_LOCK_INDEX);
    DmfAssert(NULL == dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread);

    dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread = DmfGetCurrentThreadId();
}

VOID
DMF_ModuleUnlockPrivate(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Release a Module's primary lock.

    NOTE: This function should only be called from a Module and that Module must be
          the creator of this lock. This function is called indirectly
          after proper ownership is verified.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    DmfAssert(dmfObject != NULL);
    
    DmfAssert(DmfGetCurrentThreadId() == dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread);

    dmfObject->Synchronizations[DMF_DEFAULT_LOCK_INDEX].LockHeldByThread = NULL;
    DmfAssert(dmfObject->InternalCallbacksInternal.AuxiliaryUnlock != NULL);
    (dmfObject->InternalCallbacksInternal.AuxiliaryUnlock)(DmfModule,
                                                           DMF_DEFAULT_LOCK_INDEX);
}

VOID
DMF_ModuleAuxiliaryLockPrivate(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    )
/*++

Routine Description:

    Invoke the Lock Callback for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    AuxiliaryLockIndex - Index of the auxiliary lock object

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    DmfAssert(dmfObject != NULL);
    DmfAssert(dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks <= DMF_MAXIMUM_AUXILIARY_LOCKS);
    DmfAssert(AuxiliaryLockIndex < dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks);
    DmfAssert(dmfObject->InternalCallbacksInternal.AuxiliaryLock != NULL);

    // Device lock is at 0. Auxiliary locks start from 1.
    // AuxiliaryLockIndex is 0 based.
    //
    (dmfObject->InternalCallbacksInternal.AuxiliaryLock)(DmfModule,
                                                         AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS);

    // This check is required for SAL.
    //
    if (AuxiliaryLockIndex < DMF_MAXIMUM_AUXILIARY_LOCKS)
    {
        DmfAssert(NULL == dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread);
        dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread = DmfGetCurrentThreadId();
    }
    else
    {
        DmfAssert(FALSE);
    }
}

VOID
DMF_ModuleAuxiliaryUnlockPrivate(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG AuxiliaryLockIndex
    )
/*++

Routine Description:

    Invoke the Unlock Callback for a given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    AuxiliaryLockIndex - Index of the auxiliary lock object

Return Value:

    None

--*/
{
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);
    DmfAssert(dmfObject != NULL);
    DmfAssert(dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks <= DMF_MAXIMUM_AUXILIARY_LOCKS);
    DmfAssert(AuxiliaryLockIndex < dmfObject->ModuleDescriptor.NumberOfAuxiliaryLocks);

    // This check is required for SAL.
    //
    if (AuxiliaryLockIndex < DMF_MAXIMUM_AUXILIARY_LOCKS)
    {
        // Device lock is at 0. Auxiliary locks starts from 1.
        // AuxiliaryLockIndex is 0 based.
        //
        DmfAssert(DmfGetCurrentThreadId() == dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread);

        dmfObject->Synchronizations[AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS].LockHeldByThread = NULL;

        DmfAssert(dmfObject->InternalCallbacksInternal.AuxiliaryUnlock != NULL);
        (dmfObject->InternalCallbacksInternal.AuxiliaryUnlock)(DmfModule,
                                                               AuxiliaryLockIndex + DMF_NUMBER_OF_DEFAULT_LOCKS);
    }
    else
    {
        DmfAssert(FALSE);
    }
}

// eof: DmfHelpers.c
//
