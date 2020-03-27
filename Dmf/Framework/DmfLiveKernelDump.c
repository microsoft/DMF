/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfLiveKernelDump.c

Abstract:

    DMF Implementation:

    This Module contains the support that allows DMF Modules to easily use the
    LiveKernelDump Feature Module.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework

--*/

#include "DmfModule.h"
#include "DmfIncludeInternal.h"

#if defined(DMF_INCLUDE_TMH)
#include "DmfLiveKernelDump.tmh"
#endif

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_LiveKernelDump_CONFIG_INIT(
    _Out_ DMF_CONFIG_LiveKernelDump* ModuleConfig
    )
/*++

Routine Description:

    Initializes the LiveKernelDump Module's Module Config. The values are generally acceptable for most drivers.
    If the Client wishes to, Client may override.

Arguments:

    ModuleConfig - LiveKernelDump Module's Module Config to initialize.

Return Value:

    None

--*/
{
    RtlZeroMemory(ModuleConfig,
                  sizeof(DMF_CONFIG_LiveKernelDump));
}

#pragma code_seg("PAGE")
VOID
DMF_LiveKernelDump_ModuleInitialize(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    This function adds each Module's DMF structures such as DMF_OBJECT and DMF_CONFIG_Xxx
    to the Framework Ring Buffer in the LiveKernelDump Module. These structures will be
    available when a Live Kernel Memory Dump is generated.

Arguments:

    DmfObject - The Module's DMF Object used to access its structures.

Return Value:

    None

--*/
{
    DMFMODULE dmfModule;
    VOID* dmfConfig;
    VOID* clientModuleInstanceName;
    size_t dmfConfigSize;
    size_t clientModuleInstanceNameSize;
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfObject != NULL);
    DmfAssert(DmfObject->ModuleCollection != NULL);

    // Dispatch callback to Child DMF Modules first.
    //
    childDmfObject = DmfChildObjectFirstGet(DmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMF_LiveKernelDump_ModuleInitialize(childDmfObject);
        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

    dmfModule = DMF_ObjectToModule(DmfObject);

    // Store pointers to Module specific structures (DMF_OBJECT, DMF_CONFIG) to the Framework Ring Buffer.
    //
    DMF_MODULE_LIVEKERNELDUMP_POINTER_STORE(dmfModule,
                                            DmfObject,
                                            sizeof(DMF_OBJECT));

    if (DmfObject->ModuleConfigMemory != NULL)
    {
        dmfConfig = WdfMemoryGetBuffer(DmfObject->ModuleConfigMemory,
                                       &dmfConfigSize);

        DMF_MODULE_LIVEKERNELDUMP_POINTER_STORE(dmfModule,
                                                dmfConfig,
                                                dmfConfigSize);
    }

    if (DmfObject->ClientModuleInstanceNameMemory != NULL)
    {
        clientModuleInstanceName = WdfMemoryGetBuffer(DmfObject->ClientModuleInstanceNameMemory,
                                                      &clientModuleInstanceNameSize);

        DMF_MODULE_LIVEKERNELDUMP_POINTER_STORE(dmfModule,
                                                clientModuleInstanceName,
                                                clientModuleInstanceNameSize);
    }

    // Call the Module specific Initialize function where the Module can store 
    // private structures to the ring buffer.
    //
    if (DmfObject->ModuleDescriptor.ModuleLiveKernelDumpInitialize != NULL)
    {
        DmfObject->ModuleDescriptor.ModuleLiveKernelDumpInitialize(dmfModule);
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleLiveKernelDump_ModuleCollectionInitialize(
    _Inout_ DMF_MODULE_COLLECTION* ModuleCollectionHandle
    )
/*++

Routine Description:

    Initialize the LiveKernelDump Module for all Modules in a Module Collection.

Arguments:

    ModuleCollectionHandle - Module Collection that contains the Modules that need
                             LiveKernelDump initialization.

Return Value:

    None

--*/
{
    LONG driverModuleIndex;
    DMF_OBJECT* liveKernelDumpHandle;
    DMFMODULE liveKernelDumpModule;
    DMFCOLLECTION dmfCollection;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Add DMF Collection information to the Framework Ring Buffer.
    //
    liveKernelDumpHandle = DMF_ModuleCollectionFeatureHandleGet(ModuleCollectionHandle,
                                                                DmfFeature_LiveKernelDump);

    liveKernelDumpModule = DMF_ObjectToModule(liveKernelDumpHandle);

    DMF_MODULE_LIVEKERNELDUMP_POINTER_STORE(liveKernelDumpModule,
                                            ModuleCollectionHandle,
                                            sizeof(DMF_MODULE_COLLECTION));

    // Store the Module collection in the Live Kernel Dump Module.
    //
    dmfCollection = (DMFCOLLECTION)ModuleCollectionHandle->ModuleCollectionHandleMemory;

    // Store the child handle pointers in the Module collection.
    //
    DMF_MODULE_LIVEKERNELDUMP_POINTER_STORE(liveKernelDumpModule,
                                            ModuleCollectionHandle->ClientDriverDmfModules,
                                            ModuleCollectionHandle->NumberOfClientDriverDmfModules * sizeof(ULONG64));

    // Store the DMF Collection as a bugcheck parameter.
    //
    DMF_MODULE_LIVEKERNELDUMP_DMFCOLLECTION_AS_BUGCHECK_PARAMETER_STORE(liveKernelDumpModule,
                                                                        (ULONG_PTR)dmfCollection);

    // Add Module information to the Framework Ring Buffer and perform Module customizations.
    //
    for (driverModuleIndex = 0;
         driverModuleIndex < ModuleCollectionHandle->NumberOfClientDriverDmfModules;
         driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject = ModuleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];
        DmfAssert(dmfObject != NULL);

        // For each Module, call this function on all its children.
        //
        DMF_LiveKernelDump_ModuleInitialize(dmfObject);
    }

    FuncExitVoid(DMF_TRACE);

    return;
}
#pragma code_seg()

// eof: DmfLiveKernelDump.c
//
