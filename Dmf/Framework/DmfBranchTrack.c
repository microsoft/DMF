/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfBranchTrack.c

Abstract:

    DMF Implementation.
    This Module contains the support that allows DMF Modules to easily use BranchTrack.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfModule.h"
#include "DmfIncludeInternal.h"

#include "DmfBranchTrack.tmh"

#pragma code_seg("PAGE")
VOID
DMF_ModuleBranchTrack_ModuleInitialize(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Initialize the BranchTrack Module of the given DMF Module as well as its children.

Arguments:

    DmfModule - The given DMF Module.
    BranchTrackEnabled - Indicates if BranchTrack is enabled.

Return Value:

    None

--*/
{
    DMF_OBJECT* childDmfObject;
    CHILD_OBJECT_INTERATION_CONTEXT childObjectIterationContext;

    ASSERT(DmfObject != NULL);
    ASSERT(DmfObject->ModuleCollection != NULL);

    PAGED_CODE();

    // Dispatch callback to Child DMF Modules first.
    //
    childDmfObject = DmfChildObjectFirstGet(DmfObject,
                                            &childObjectIterationContext);
    while (childDmfObject != NULL)
    {
        DMF_ModuleBranchTrack_ModuleInitialize(childDmfObject);
        childDmfObject = DmfChildObjectNextGet(&childObjectIterationContext);
    }

    // Dispatch callback to the given Parent DMF Module next.
    //
    if (DmfObject->ModuleDescriptor.ModuleBranchTrackInitialize != NULL)
    {
        // Child modules are passed their own DMF Module. From there the 
        // callback can the DMF BranchTrack handle.
        //
        DMFMODULE dmfModule = DMF_ObjectToModule(DmfObject);
        DmfObject->ModuleDescriptor.ModuleBranchTrackInitialize(dmfModule);
    }
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
ULONG
DMF_ModuleBranchTrack_HasClientEnabledBranchTrack(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:

    Read the Client Driver's registry settings to determine if the User has enabled BranchTrack.
    The key read is here: \HKLM\SYSTEM\CurrentControlSet\Services\[DriverName]\Parameters
    The name of the value is: "BranchTrackEnabled"

Arguments:

    Device - WDFDEVICE for Client Driver

Return Value:

    Zero - BranchTrack is not enabled.
    Non-Zero - BranchTrack is enabled.

--*/
{
    NTSTATUS ntStatus;
    WDFDRIVER driver;
    WDFKEY key;
    DECLARE_CONST_UNICODE_STRING(branchTrackEnabledString, L"BranchTrackEnabled");
    ULONG branchTrackEnabled;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // BranchTrack is disabled by default.
    //
    branchTrackEnabled = FALSE;

    driver = WdfDeviceGetDriver(Device);
    ntStatus = WdfDriverOpenParametersRegistryKey(driver,
                                                  KEY_READ,
                                                  WDF_NO_OBJECT_ATTRIBUTES,
                                                  &key);
    if (! NT_SUCCESS(ntStatus))
    {
        // This should never happen, but don't prevent driver load.
        //
        goto Exit;
    }

    ntStatus = WdfRegistryQueryULong(key,
                                     &branchTrackEnabledString,
                                     &branchTrackEnabled);
    if (! NT_SUCCESS(ntStatus))
    {
        // Ignore error, but disable BranchTrack.
        //
        branchTrackEnabled = FALSE;
        // Fall through to close key.
        //
    }

    WdfRegistryClose(key);

Exit:

    FuncExit(DMF_TRACE, "branchTrackEnabled=%d", branchTrackEnabled);

    return branchTrackEnabled;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_ModuleBranchTrack_ModuleCollectionInitialize(
    _Inout_ DMF_MODULE_COLLECTION* ModuleCollectionHandle
    )
/*++

Routine Description:

    Initialize the BranchTrack Modules for all Modules in a Module Collection.

Arguments:

    DriverModuleDescriptorTable - Table of DMF Modules and their attributes to instantiate.
    ModuleCollectionHandle - Module Collection that contains the Modules that need BranchTrack initialization.

Return Value:

    None

--*/
{
    LONG driverModuleIndex;

    PAGED_CODE();

    // Technically, BranchTrack could BranchTrack itself, but it just has a NULL handler.
    //
    for (driverModuleIndex = 0;
         driverModuleIndex < ModuleCollectionHandle->NumberOfClientDriverDmfModules;
         driverModuleIndex++)
    {
        DMF_OBJECT* dmfObject = ModuleCollectionHandle->ClientDriverDmfModules[driverModuleIndex];

        ASSERT(dmfObject != NULL);
        DMF_ModuleBranchTrack_ModuleInitialize(dmfObject);
    }
}
#pragma code_seg()

// eof: DmfBranchTrack.c
//
