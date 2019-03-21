/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfValidate.c

Abstract:

    DMF Implementation:

    Contains validation helper functions used in DEBUG build only.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#include "DmfValidate.tmh"

////////////////////////////////////////////////////////////////////////////////////////////////
//
// DMF Object Validation Support
//
// NOTE: These functions are only meant for debug purposes.
//
// NOTE: Wpp Tracing is purposefully omitted to prevent any code from being generated in
//       production builds.
//
////////////////////////////////////////////////////////////////////////////////////////////////
//

VOID
DMF_HandleValidate_Create(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Validate that a DMF_OBJECT is in a proper state during a DMF Module Create
    call. In this case, the handle must be in an Invalid State.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT(ModuleState_Invalid == DmfObject->ModuleState);
}

VOID
DMF_HandleValidate_Open(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Validate that a DMF_OBJECT is in a proper state during a DMF Module Open
    call. In this case, the last state change must be either Created or Closed.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT(((ModuleState_Created == DmfObject->ModuleState) || (ModuleState_Closed == DmfObject->ModuleState)) ||
           ((DmfObject->DmfObjectParent != NULL) && (DmfObject->ModuleState == ModuleState_Opened)));
}

VOID
DMF_HandleValidate_IsCreatedOrOpening(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is either Created or Opening.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT((ModuleState_Created == DmfObject->ModuleState) ||
           (ModuleState_Opening == DmfObject->ModuleState));
}

VOID
DMF_HandleValidate_IsOpening(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is Opening.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT(ModuleState_Opening == DmfObject->ModuleState);
}

VOID
DMF_HandleValidate_Close(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Validate that a DMF_OBJECT is in a proper state during a DMF Module Close
    call. In this case, the last state change must be either Opened.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    // It is possible that all modules are created, some opened (but not all).
    // Then they are all closed. In this case, modules that are created but not closed
    // will have the close callback called.
    //
    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT(((ModuleState_Opened == DmfObject->ModuleState) || (ModuleState_Created == DmfObject->ModuleState)) ||
           ((DmfObject->DmfObjectParent != NULL) && (DmfObject->ModuleState == ModuleState_Closed))
          );
}

VOID
DMF_HandleValidate_IsClosing(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is Closing.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT(ModuleState_Closing == DmfObject->ModuleState);
}

VOID
DMF_HandleValidate_IsOpenedOrClosing(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is Opened or Closing.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT((ModuleState_Opened == DmfObject->ModuleState) ||
           (ModuleState_Closing == DmfObject->ModuleState));
}

VOID
DMF_HandleValidate_Destroy(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Validate that a DMF_OBJECT is in a proper state during a DMF Module Destroy
    call. In this case, the last state change must be either Opened.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT((ModuleState_Closed == DmfObject->ModuleState) ||
           (ModuleState_Created == DmfObject->ModuleState));
}

VOID
DMF_HandleValidate_IsOpen(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module is "Open".
    NOTE: "Open" does not map to an exact state. It is a general idea.
    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT((ModuleState_Opening == DmfObject->ModuleState) ||
           (ModuleState_Opened == DmfObject->ModuleState) ||
           (ModuleState_Closing == DmfObject->ModuleState));
}

VOID
DMF_HandleValidate_IsCreated(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is Created.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT(ModuleState_Created == DmfObject->ModuleState);
}

VOID
DMF_HandleValidate_IsCreatedOrIsNotify(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is Created or that it is of type NOTIFY and the Module
    has been opened or closed. (Open due to notification can happen at any time, often very early.)

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT((ModuleState_Created == DmfObject->ModuleState) ||
           (DMF_IsObjectTypeOpenNotify(DmfObject) &&
            ((ModuleState_Opened == DmfObject->ModuleState) ||
             (ModuleState_Closed == DmfObject->ModuleState))
           ));
}

VOID
DMF_HandleValidate_IsOpened(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is Opened.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT(ModuleState_Opened == DmfObject->ModuleState);
}

VOID
DMF_HandleValidate_IsCreatedOrOpened(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is Created or Opened.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT((ModuleState_Created == DmfObject->ModuleState) ||
           (ModuleState_Opened == DmfObject->ModuleState));
}

VOID
DMF_HandleValidate_IsCreatedOrClosed(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is Created or Closed.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT((ModuleState_Created == DmfObject->ModuleState) ||
           (ModuleState_Closed == DmfObject->ModuleState));
}

VOID
DMF_HandleValidate_IsCreatedOrOpenedOrClosed(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is Created, Opened or Closed.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT((ModuleState_Created == DmfObject->ModuleState) ||
           (ModuleState_Opened == DmfObject->ModuleState) ||
           (ModuleState_Closed == DmfObject->ModuleState));
}

VOID
DMF_HandleValidate_IsAvailable(
    _In_ DMF_OBJECT* DmfObject
    )
/*++

Routine Description:

    Verifies that a DMF_OBJECT Module State is Available. It just means that it has
    been created and is not in process of being destroyed.

    NOTE: This call is only meant for debug purposes.

Arguments:

    DmfObject - The DMF Object to validate.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfObject);

    ASSERT(DmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == DmfObject->Signature);
    ASSERT((ModuleState_Created == DmfObject->ModuleState) ||
           (ModuleState_Opening == DmfObject->ModuleState) ||
           (ModuleState_Opened == DmfObject->ModuleState) ||
           (ModuleState_Closing == DmfObject->ModuleState) ||
           (ModuleState_Closed == DmfObject->ModuleState));
}

VOID
DMF_HandleValidate_ModuleMethod(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_DESCRIPTOR* DmfModuleDescriptor
    )
/*++

Routine Description:

    Given a Module and a Module Descriptor, this function verifies that the given Module's
    descriptor matches the given Module Descriptor. Methods use this function to indicate
    when a Module's Method is called using another Module's (different type of Module) handle.
    Doing so is always a fatal error.

Arguments:

    DmfModule - The given Module handle.
    DmfModuleDescriptor - The given Module Descriptor.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(DmfModuleDescriptor);

#if defined(DEBUG)
    DMF_OBJECT* dmfObject;
    dmfObject = DMF_ModuleToObject(DmfModule);
    if (DMF_IsObjectTypeOpenNotify(dmfObject))
    {
        DMF_HandleValidate_IsAvailable(dmfObject);
    }
    else
    {
        DMF_HandleValidate_IsOpened(dmfObject);
    }
    ASSERT(dmfObject->ModuleName == DmfModuleDescriptor->ModuleName);
#endif // defined(DEBUG)
}

VOID
DMF_ObjectValidate(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Given a Module, this function validates various elements of the Module's
    corresponding data structure.

Arguments:

    DmfModule - The given Module handle.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

#if defined(DEBUG)
    DMF_OBJECT* dmfObject;
    dmfObject = DMF_ModuleToObject(DmfModule);
    ASSERT(dmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == dmfObject->Signature);
    ASSERT((dmfObject->ModuleState > ModuleState_Invalid) &&
           (dmfObject->ModuleState < ModuleState_Last));
    ASSERT(dmfObject->ModuleName != NULL);
    // TODO: Verify other fields.
    //
#endif // defined(DEBUG)
}

VOID
DMF_HandleValidate_OpeningOk(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_DESCRIPTOR* DmfModuleDescriptor
    )
/*++

Routine Description:

    Given a Module, this function validates that the Module is either
    opening or opened.

Arguments:

    DmfModule - The given Module handle.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(DmfModuleDescriptor);

#if defined(DEBUG)
    DMF_OBJECT* dmfObject;
    dmfObject = DMF_ModuleToObject(DmfModule);

    ASSERT(dmfObject != NULL);
    ASSERT(DMF_OBJECT_SIGNATURE == dmfObject->Signature);
    ASSERT((ModuleState_Opened == dmfObject->ModuleState) ||
           (ModuleState_Opening == dmfObject->ModuleState));
    ASSERT(dmfObject->ModuleName == DmfModuleDescriptor->ModuleName);
#endif // defined(DEBUG)
}

VOID
DMF_HandleValidate_ClosingOk(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_DESCRIPTOR* DmfModuleDescriptor
    )
/*++

Routine Description:

    Given a Module, this function validates that the Module is either
    opened or closing.

Arguments:

    DmfModule - The given Module handle.

Return Value:

    None. Failure causes an assert to trigger.

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(DmfModuleDescriptor);

#if defined(DEBUG)
    DMF_OBJECT* dmfObject;
    dmfObject = DMF_ModuleToObject(DmfModule);

    DMF_ObjectValidate((DMFMODULE)dmfObject->MemoryDmfObject);
    ASSERT((ModuleState_Opened == dmfObject->ModuleState) ||
        (ModuleState_Closing == dmfObject->ModuleState));
    ASSERT(dmfObject->ModuleName == DmfModuleDescriptor->ModuleName);
#endif // defined(DEBUG)
}

// eof: DmfValidate.c
//
