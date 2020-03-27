/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfInterface.c

Abstract:

    DMF Implementation:

    This Module contains DMF Interface implementation

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#include "DmfIncludeInternal.h"

#if defined(DMF_INCLUDE_TMH)
#include "DmfInterfaceInternal.tmh"
#endif

DMFMODULE
DMF_InterfaceTransportModuleGet(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Returns the Transport Module given a valid DMF Interface.

Arguments:

    DmfInterface - The given DMF Interface.

Return Value:

    The Transport Module.

--*/
{
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;

    dmfInterfaceObject = DMF_InterfaceToObject(DmfInterface);

    return dmfInterfaceObject->TransportModule;
}

DMFMODULE
DMF_InterfaceProtocolModuleGet(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Returns the Protocol Module given a valid DMF Interface.

Arguments:

    DmfInterface - The given DMF Interface.

Return Value:

    The Protocol Module.

--*/
{
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;

    dmfInterfaceObject = DMF_InterfaceToObject(DmfInterface);

    return dmfInterfaceObject->ProtocolModule;
}

VOID*
DMF_InterfaceTransportDeclarationDataGet(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Returns the Transport Module's Declaration Data given a valid DMF Interface.

Arguments:

    DmfInterface - The given DMF Interface.

Return Value:

    The Transport Module's Declaration Data.

--*/
{
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;

    dmfInterfaceObject = DMF_InterfaceToObject(DmfInterface);

    return (dmfInterfaceObject->TransportDescriptor);
}

VOID*
DMF_InterfaceProtocolDeclarationDataGet(
    _In_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Returns the Protocol Module's Declaration Data given a valid DMF Interface.

Arguments:

    DmfInterface - The given DMF Interface.

Return Value:

    The Protocol Module's Declaration Data.

--*/
{
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;

    dmfInterfaceObject = DMF_InterfaceToObject(DmfInterface);

    return (dmfInterfaceObject->ProtocolDescriptor);
}

VOID
DMF_INTERFACE_PROTOCOL_DESCRIPTOR_INIT_INTERNAL(
    _Out_ DMF_INTERFACE_PROTOCOL_DESCRIPTOR* DmfProtocolDescriptor,
    _In_ PSTR InterfaceName,
    _In_ ULONG DeclarationDataSize,
    _In_ EVT_DMF_INTERFACE_ProtocolBind* EvtProtocolBind,
    _In_ EVT_DMF_INTERFACE_ProtocolUnbind* EvtProtocolUnbind,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind
    )
/*++

Routine Description:

    Initializes the Protocol's Descriptor.

Arguments:

    DmfProtocolDescriptor - The Protocol Descriptor to initialize.
    InterfaceName - Name of the Interface.
    DeclarationDataSize - Protocol's Declaration Data size.
    EvtProtocolBind - The Protocol's Bind callback.
    EvtProtocolUnbind - The Protocol's Unbind callback.
    EvtPostBind - The Protocol's Post Bind callback.
    EvtPreUnbind - The Protocol's Pre Unbind callback.

Return Value:

    None

--*/
{
    RtlZeroMemory(DmfProtocolDescriptor,
                  DeclarationDataSize);

    DmfProtocolDescriptor->GenericInterfaceDescriptor.InterfaceName = InterfaceName;
    DmfProtocolDescriptor->GenericInterfaceDescriptor.InterfaceType = Interface_Protocol;
    DmfProtocolDescriptor->GenericInterfaceDescriptor.Size = DeclarationDataSize;
    DmfProtocolDescriptor->GenericInterfaceDescriptor.DmfInterfacePostBind = EvtPostBind;
    DmfProtocolDescriptor->GenericInterfaceDescriptor.DmfInterfacePreUnbind = EvtPreUnbind;
    DmfProtocolDescriptor->DmfInterfaceProtocolBind = EvtProtocolBind;
    DmfProtocolDescriptor->DmfInterfaceProtocolUnbind = EvtProtocolUnbind;
}

VOID
DMF_INTERFACE_TRANSPORT_DESCRIPTOR_INIT_INTERNAL(
    _Out_ DMF_INTERFACE_TRANSPORT_DESCRIPTOR* DmfTransportDescriptor,
    _In_ PSTR InterfaceName,
    _In_ ULONG DeclarationDataSize,
    _In_opt_ EVT_DMF_INTERFACE_PostBind* EvtPostBind,
    _In_opt_ EVT_DMF_INTERFACE_PreUnbind* EvtPreUnbind
    )
/*++

Routine Description:

    Initializes the Transport's Descriptor.

Arguments:

    DmfTransportDescriptor - The Transport Descriptor to initialize.
    InterfaceName - Name of the Interface.
    DeclarationDataSize - Transport's Declaration Data size.
    EvtPostBind - The Transport's Post Bind callback.
    EvtPreUnbind - The Transport's Pre Unbind callback.

Return Value:

    None

--*/
{
    RtlZeroMemory(DmfTransportDescriptor,
                  DeclarationDataSize);

    DmfTransportDescriptor->GenericInterfaceDescriptor.InterfaceName = InterfaceName;
    DmfTransportDescriptor->GenericInterfaceDescriptor.InterfaceType = Interface_Transport;
    DmfTransportDescriptor->GenericInterfaceDescriptor.Size = DeclarationDataSize;
    DmfTransportDescriptor->GenericInterfaceDescriptor.DmfInterfacePostBind = EvtPostBind;
    DmfTransportDescriptor->GenericInterfaceDescriptor.DmfInterfacePreUnbind = EvtPreUnbind;
}

NTSTATUS
DMF_ModuleInterfaceCreate(
    _Out_ DMF_INTERFACE_OBJECT** DmfInterfaceObject
    )
/*++

Routine Description:

    Creates the DMFINTERFACE and the DMF_INTERFACE_OBJECT associated with it.

Arguments:

    DmfInterfaceObject - A pointer that receives the address of the DMF_INTERFACE_OBJECT created.

Return Value:

    NTSTATUS indicating whether the Interface creation was successful.

--*/
{
    NTSTATUS ntStatus;
    WDFMEMORY interfaceMemory;
    DMFINTERFACE dmfInterface;
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;
    WDF_OBJECT_ATTRIBUTES attributes;

    dmfInterfaceObject = NULL;
    interfaceMemory = NULL;
    dmfInterface = NULL;

    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               NonPagedPoolNx,
                               0,
                               sizeof(DMF_INTERFACE_OBJECT),
                               &interfaceMemory,
                               (VOID**)DmfInterfaceObject);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate for DMF_INTERFACE_OBJECT fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    dmfInterface = (DMFINTERFACE)interfaceMemory;

    ntStatus = WdfObjectAddCustomType(dmfInterface,
                                      DMFINTERFACE_TYPE);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfObjectAddCustomType fails to add DMFINTERFACE_TYPE: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    dmfInterfaceObject = *DmfInterfaceObject;

    RtlZeroMemory(dmfInterfaceObject,
                  sizeof(DMF_INTERFACE_OBJECT));

    dmfInterfaceObject->InterfaceState = InterfaceState_Created;
    dmfInterfaceObject->DmfInterface = dmfInterface;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = interfaceMemory;

    ntStatus = WdfSpinLockCreate(&attributes,
                                 &dmfInterfaceObject->InterfaceLock);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfSpinLockCreate for InterfaceLock fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    return ntStatus;
}

VOID
DMF_ModuleInterfaceDestroy(
    _In_ DMF_INTERFACE_OBJECT* DmfInterfaceObject
    )
/*++

Routine Description:

    Destroys the DMFINTERFACE and it corresponding DMF_INTERFACE_OBJECT.

Arguments:

    DmfInterfaceObject - A pointer to the DMF_INTERFACE_OBJECT that must be destroyed.

Return Value:

    None

--*/
{

    // Set the Interface State to Closed.
    //
    WdfSpinLockAcquire(DmfInterfaceObject->InterfaceLock);

    DmfInterfaceObject->InterfaceState = InterfaceState_Closed;

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interface state closed. DmfInterfaceObject: 0x%p", DmfInterfaceObject);

    WdfSpinLockRelease(DmfInterfaceObject->InterfaceLock);

    WdfObjectDelete(DmfInterfaceObject->DmfInterface);
}

BOOLEAN
DMF_ModuleInterfaceFind(
    _In_ DMFMODULE ModuleToFind,
    _In_ DMFMODULE ModuleWithBindings,
    _Out_opt_ DMF_INTERFACE_OBJECT** DmfInterfaceObject
    )
/*++

Routine Description:

    Tries to find an Interface corresponding to a Module in another Module's InterfaceBindings collection.
    
Arguments:

    ModuleToFind - The Module to find.
    ModuleWithBindings - The Module with the InterfaceBindings collection.
    DmfInterfaceObject - A pointer that receives the address of the DMF_INTERFACE_OBJECT corresponding to
                         the DMFINTERFACE found.

Return Value:

    TRUE - If an Interface was found.
    FALSE - Otherwise.

    NOTE: If two Modules are connected through multiple Interfaces, this function will return the first Interface
          containing the Modules provided. If two Modules can be connected through multiple Interfaces, this 
          function should also accept an Interface Name parameter to identify the Interface being searched
          and return the appropriate Interface's memory buffer.

--*/
{
    BOOLEAN interfaceFound;
    ULONG interfaceIndex;
    DMF_OBJECT* dmfObject;

    interfaceFound = FALSE;
    dmfObject = DMF_ModuleToObject(ModuleWithBindings);

    // For SAL.
    //
    if (DmfInterfaceObject != NULL)
    {
        *DmfInterfaceObject = NULL;
    }

    WdfSpinLockAcquire(dmfObject->InterfaceBindingsLock);

    for (interfaceIndex = 0; interfaceIndex < WdfCollectionGetCount(dmfObject->InterfaceBindings); interfaceIndex++)
    {
        DMFINTERFACE dmfInterface;
        DMF_INTERFACE_OBJECT* dmfInterfaceObject;

        dmfInterface = (DMFINTERFACE)WdfCollectionGetItem(dmfObject->InterfaceBindings,
                                                          interfaceIndex);

        dmfInterfaceObject = DMF_InterfaceToObject(dmfInterface);

        DmfAssert((ModuleWithBindings == dmfInterfaceObject->ProtocolModule) ||
                  (ModuleWithBindings == dmfInterfaceObject->TransportModule));

        if (((dmfInterfaceObject->ProtocolModule == ModuleWithBindings) && (dmfInterfaceObject->TransportModule == ModuleToFind)) ||
            ((dmfInterfaceObject->TransportModule == ModuleWithBindings) && (dmfInterfaceObject->ProtocolModule == ModuleToFind)))
        {

            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interface found. DmfInterfaceObject: 0x%p", dmfInterfaceObject);

            interfaceFound = TRUE;

            if (DmfInterfaceObject != NULL)
            {
                *DmfInterfaceObject = dmfInterfaceObject;
            }

            break;
        }
    }

    WdfSpinLockRelease(dmfObject->InterfaceBindingsLock);

    return interfaceFound;
}

BOOLEAN
DMF_ModuleInterfaceFindAndRemove(
    _In_ DMFMODULE ModuleToFind,
    _In_ DMFMODULE ModuleWithBindings,
    _Out_opt_ DMF_INTERFACE_OBJECT** DmfInterfaceObject
    )
/*++

Routine Description:

    Tries to find an Interface corresponding to a Module in another Module's InterfaceBindings collection.
    Removes the Interface from the InterfaceBindings collection if found.

Arguments:

    ModuleToFind - The Module to find.
    ModuleWithBindings - The Module with the InterfaceBindings collection.
    DmfInterfaceObject - A pointer that receives the address of the DMF_INTERFACE_OBJECT corresponding to the DMFINTERFACE found.

Return Value:

    TRUE - If an Interface was found.
    FALSE - Otherwise.

    NOTE: If two Modules are connected through multiple Interfaces, this function will return the first
          Interface containing the Modules provided. If two Modules can be connected through multiple 
          Interfaces, this function should also accept an Interface Name parameter to identify the Interface
          being searched and return the appropriate Interface's memory buffer.

--*/
{
    BOOLEAN interfaceFound;
    ULONG interfaceIndex;
    DMF_OBJECT* dmfObject;
    DMFINTERFACE dmfInterface;
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;

    interfaceFound = FALSE;
    dmfInterface = NULL;
    dmfObject = DMF_ModuleToObject(ModuleWithBindings);

    // For SAL.
    //
    if (DmfInterfaceObject != NULL)
    {
        *DmfInterfaceObject = NULL;
    }

    WdfSpinLockAcquire(dmfObject->InterfaceBindingsLock);

    for (interfaceIndex = 0; interfaceIndex < WdfCollectionGetCount(dmfObject->InterfaceBindings); interfaceIndex++)
    {
        dmfInterface = (DMFINTERFACE)WdfCollectionGetItem(dmfObject->InterfaceBindings,
                                                          interfaceIndex);

        dmfInterfaceObject = DMF_InterfaceToObject(dmfInterface);

        DmfAssert((ModuleWithBindings == dmfInterfaceObject->ProtocolModule) ||
                  (ModuleWithBindings == dmfInterfaceObject->TransportModule));

        if (((dmfInterfaceObject->ProtocolModule == ModuleWithBindings) && (dmfInterfaceObject->TransportModule == ModuleToFind)) ||
            ((dmfInterfaceObject->TransportModule == ModuleWithBindings) && (dmfInterfaceObject->ProtocolModule == ModuleToFind)))
        {

            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interface found. DmfInterfaceObject: 0x%p", dmfInterfaceObject);

            interfaceFound = TRUE;

            if (DmfInterfaceObject != NULL)
            {
                *DmfInterfaceObject = dmfInterfaceObject;
            }

            break;
        }
    }

    if (interfaceFound == TRUE)
    {
        DmfAssert(dmfInterface != NULL);

        WdfCollectionRemove(dmfObject->InterfaceBindings,
                            dmfInterface);
    }

    WdfSpinLockRelease(dmfObject->InterfaceBindingsLock);

    return interfaceFound;
}

NTSTATUS
DMF_InterfaceReference(
    _Inout_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Increments the reference count corresponding to the given DMF Interface if the Interface is in Open state.

Arguments:

    DmfInterface - The given DMF Interface.

Return Value:

    STATUS_SUCCESS if the Interface reference count could be incremented.
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;
    NTSTATUS ntStatus;

    ntStatus = STATUS_SUCCESS;

    dmfInterfaceObject = DMF_InterfaceToObject(DmfInterface);

    // Modify the ReferenceCount of the DmfInterfaceObject.
    //
    WdfSpinLockAcquire(dmfInterfaceObject->InterfaceLock);

    if (dmfInterfaceObject->InterfaceState == InterfaceState_Opened)
    {
        dmfInterfaceObject->ReferenceCount++;
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interface reference added. ReferenceCount after adding: %d", dmfInterfaceObject->ReferenceCount);
    }
    else
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Interface reference add failed. DmfInterfaceObject: 0x%p, InterfaceState: %d", dmfInterfaceObject, dmfInterfaceObject->InterfaceState);
    }

    WdfSpinLockRelease(dmfInterfaceObject->InterfaceLock);

    return ntStatus;
}

VOID
DMF_InterfaceDereference(
    _Inout_ DMFINTERFACE DmfInterface
    )
/*++

Routine Description:

    Decrements the reference count corresponding to the given DMF Interface.

Arguments:

    DmfInterface - The given DMF Interface.

Return Value:

    None

--*/
{
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;

    dmfInterfaceObject = DMF_InterfaceToObject(DmfInterface);

    // Modify the ReferenceCount of the DmfInterfaceObject.
    //
    WdfSpinLockAcquire(dmfInterfaceObject->InterfaceLock);

    DmfAssert((dmfInterfaceObject->InterfaceState == InterfaceState_Opened)||
              (dmfInterfaceObject->InterfaceState == InterfaceState_Closing));

    dmfInterfaceObject->ReferenceCount--;

    DmfAssert(dmfInterfaceObject->ReferenceCount >= 0);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Interface reference count after de-reference: %d", dmfInterfaceObject->ReferenceCount);

    WdfSpinLockRelease(dmfInterfaceObject->InterfaceLock);

    return;
}

VOID
DMF_ModuleInterfaceWaitToClose(
    _Inout_ DMF_INTERFACE_OBJECT* DmfInterfaceObject
    )
/*++

Routine Description:

    Waits until the Interface's reference count decrements to zero and then sets the Interface state to Closed.

Arguments:

    DmfInterfaceObject - The DMF Interface object.

Return Value:

    None

--*/
{
    LONG referenceCount;
    ULONG referenceCountPollingIntervalMs;

    referenceCountPollingIntervalMs = 100;

    WdfSpinLockAcquire(DmfInterfaceObject->InterfaceLock);

    DmfAssert(DmfInterfaceObject->InterfaceState == InterfaceState_Opened);

    // Set the Interface State to Closed.
    // Methods or Callbacks exposed by the Interface cannot be used anymore
    // since DMF_InterfaceReference() will fail.
    //
    DmfInterfaceObject->InterfaceState = InterfaceState_Closing;
    referenceCount = DmfInterfaceObject->ReferenceCount;

    WdfSpinLockRelease(DmfInterfaceObject->InterfaceLock);

    while (referenceCount > 0)
    {
        WdfSpinLockAcquire(DmfInterfaceObject->InterfaceLock);

        referenceCount = DmfInterfaceObject->ReferenceCount;

        WdfSpinLockRelease(DmfInterfaceObject->InterfaceLock);

        if (referenceCount == 0)
        {
            break;
        }

        // Reference count > 0 means an Interface Method/Callback is running.
        // Wait for Reference count to run down to 0.
        //
        DMF_Utility_DelayMilliseconds(referenceCountPollingIntervalMs);
        TraceInformation(DMF_TRACE, "DmfInterfaceObject=0x%p Waiting to close Interface", DmfInterfaceObject);
    }

}

NTSTATUS
DMF_ModuleInterfaceDescriptorAdd(
    _Inout_ DMFMODULE DmfModule,
    _In_ DMF_INTERFACE_DESCRIPTOR* InterfaceDescriptor
    )
/*++

Routine Description:

    Associates a context containing the InterfaceDescriptor with the given DMF Module.

Arguments:

    DmfModule - The given DMF Module.
    InterfaceDescriptor - The Interface Descriptor that will be associated as a context of the DMF Module.

Return Value:

    NTSTATUS indicating if the InterfaceDescriptor could be successfully associated with the DMF Module.

--*/
{
    NTSTATUS ntStatus;
    DMF_INTERFACE_DESCRIPTOR* declarationData;

    ntStatus = STATUS_SUCCESS;

    DmfAssert(((InterfaceDescriptor->InterfaceType == Interface_Transport) && (InterfaceDescriptor->Size >= sizeof(DMF_INTERFACE_TRANSPORT_DESCRIPTOR))) ||
              ((InterfaceDescriptor->InterfaceType == Interface_Protocol) && (InterfaceDescriptor->Size >= sizeof(DMF_INTERFACE_PROTOCOL_DESCRIPTOR))));

    // Associate the Declaration Data with the DmfModule.
    //
    ntStatus = WdfObjectAllocateContext(DmfModule,
                                        &InterfaceDescriptor->DeclarationDataWdfAttributes,
                                        (VOID**)&(declarationData));
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfObjectAllocateContext for DeclarationDataWdfAttributes fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Populate the Declaration Data.
    //
    RtlCopyMemory(declarationData,
                  InterfaceDescriptor,
                  InterfaceDescriptor->Size);

    if (InterfaceDescriptor->DmfInterfacePostBind == NULL)
    {
        declarationData->DmfInterfacePostBind = EVT_DMF_INTERFACE_Generic_PostBind;
    }

    if (InterfaceDescriptor->DmfInterfacePreUnbind == NULL)
    {
        declarationData->DmfInterfacePreUnbind = EVT_DMF_INTERFACE_Generic_PreUnbind;
    }

Exit:

    return ntStatus;
}

NTSTATUS
DMF_ModuleInterfaceBind(
    _In_ DMFMODULE ProtocolModule,
    _In_ DMFMODULE TransportModule,
    _In_ DMF_INTERFACE_PROTOCOL_DESCRIPTOR* ProtocolDescriptor,
    _In_ DMF_INTERFACE_TRANSPORT_DESCRIPTOR* TransportDescriptor
    )
/*++

Routine Description:

    Creates an Interface between the given Protocol and Transport Modules.
    This Interface is based on Interface specific information provided by the ProtocolDescriptor
    and TransportDescriptor.

    NOTE: Synchronization must be considered if DMF_ModuleInterfaceBind, DMF_ModuleInterfaceUnbind and
          DMF_ModuleInterfacesUnbind calls occur simultaneously.

Arguments:

    ProtocolModule - The given Protocol Module.
    TransportModule - The given Transport Module.
    ProtocolDescriptor - The Interface Descriptor associated with the Protocol.
    TransportDescriptor - The Interface Descriptor associated with the Transport.

Return Value:

    NTSTATUS indicating if the Interface creation was successful.

--*/
{
    NTSTATUS ntStatus;
    DMF_OBJECT* dmfObject;
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;

    dmfInterfaceObject = NULL;

    DmfAssert((NULL != ProtocolModule) &&
              (NULL != TransportModule) &&
              (NULL != ProtocolDescriptor) &&
              (NULL != TransportDescriptor));

    // Lock and Check if Transport Module already contains an Interface binding with this Protocol.
    //
    if (DMF_ModuleInterfaceFind(ProtocolModule,
                                TransportModule,
                                &dmfInterfaceObject))
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_OBJECT_NAME_COLLISION;

        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Bind failed. Already found Protocol Module in Transport Module's Bindings. DmfInterfaceObject: 0x%p", dmfInterfaceObject);

        goto Exit;
    }

    // Lock and Check if Protocol Module already contains an Interface binding with this Transport.
    //
    if (DMF_ModuleInterfaceFind(TransportModule,
                                ProtocolModule,
                                &dmfInterfaceObject))
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_OBJECT_NAME_COLLISION;

        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Bind failed. Already found Transport Module in Protocol Module's Bindings. DmfInterfaceObject: 0x%p", dmfInterfaceObject);

        goto Exit;
    }

    // Create a new Interface representing the Protocol - Transport Bind.
    //
    ntStatus = DMF_ModuleInterfaceCreate(&dmfInterfaceObject);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleInterfaceCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Allocate Protocol Module's Interface Context. 
    //
    if (ProtocolDescriptor->GenericInterfaceDescriptor.ModuleInterfaceContextSet)
    {
        ntStatus = WdfObjectAllocateContext(dmfInterfaceObject->DmfInterface,
                                            &ProtocolDescriptor->GenericInterfaceDescriptor.ModuleInterfaceContextWdfAttributes,
                                            NULL);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfObjectAllocateContext for Protocol's Module Interface context fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    // Allocate Transport Module's Interface Context. 
    //
    if (TransportDescriptor->GenericInterfaceDescriptor.ModuleInterfaceContextSet)
    {
        ntStatus = WdfObjectAllocateContext(dmfInterfaceObject->DmfInterface,
                                            &TransportDescriptor->GenericInterfaceDescriptor.ModuleInterfaceContextWdfAttributes,
                                            NULL);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfObjectAllocateContext for Transport's Module Interface context fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    dmfInterfaceObject->ProtocolModule = ProtocolModule;
    dmfInterfaceObject->TransportModule = TransportModule;
    dmfInterfaceObject->ProtocolDescriptor = ProtocolDescriptor;
    dmfInterfaceObject->TransportDescriptor = TransportDescriptor;

    // Add this Interface to Protocol Module's and Transport Module's Interface Binding collections.
    //
    dmfObject = DMF_ModuleToObject(ProtocolModule);

    WdfSpinLockAcquire(dmfObject->InterfaceBindingsLock);

    ntStatus = WdfCollectionAdd(dmfObject->InterfaceBindings,
                                dmfInterfaceObject->DmfInterface);

    WdfSpinLockRelease(dmfObject->InterfaceBindingsLock);

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfCollectionAdd fails to add DmfInterface to Protocol's binding collection: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    dmfObject = DMF_ModuleToObject(TransportModule);

    WdfSpinLockAcquire(dmfObject->InterfaceBindingsLock);

    ntStatus = WdfCollectionAdd(dmfObject->InterfaceBindings,
                                dmfInterfaceObject->DmfInterface);

    WdfSpinLockRelease(dmfObject->InterfaceBindingsLock);

    if (!NT_SUCCESS(ntStatus))
    {
        // Remove the Interface from Protocol Module's collection.
        //
        dmfObject = DMF_ModuleToObject(ProtocolModule);

        WdfSpinLockAcquire(dmfObject->InterfaceBindingsLock);

        WdfCollectionRemove(dmfObject->InterfaceBindings,
                            dmfInterfaceObject->DmfInterface);

        WdfSpinLockRelease(dmfObject->InterfaceBindingsLock);

        goto Exit;
    }

    WdfSpinLockAcquire(dmfInterfaceObject->InterfaceLock);

    dmfInterfaceObject->InterfaceState = InterfaceState_Opening;

    WdfSpinLockRelease(dmfInterfaceObject->InterfaceLock);

    // Interface state is set to Opening when Bind call is made.
    //
    // Call the Protocol's Bind callback that will initiate exchange of Bind-time declarationData
    // between the two Modules. Note: this function is unique per interface.
    //
    ntStatus = ProtocolDescriptor->DmfInterfaceProtocolBind(dmfInterfaceObject->DmfInterface);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfInterfaceProtocolBind fails: ntStatus=%!STATUS!, DmfInterfaceObject=0x%p", ntStatus, dmfInterfaceObject);

        // Unbind the Modules as Bind failed.
        //
        DMF_ModuleInterfaceUnbind(ProtocolModule,
                                  TransportModule,
                                  ProtocolDescriptor,
                                  TransportDescriptor);
        goto Exit;
    }

    // Set the Interface State to Open.
    //
    WdfSpinLockAcquire(dmfInterfaceObject->InterfaceLock);
    dmfInterfaceObject->InterfaceState = InterfaceState_Opened;
    WdfSpinLockRelease(dmfInterfaceObject->InterfaceLock);

    // Interface state is set to Opened when PostBind callbacks are called.
    //
    // Call the Post Bind Callback for Transport Module.
    //
    TransportDescriptor->GenericInterfaceDescriptor.DmfInterfacePostBind(dmfInterfaceObject->DmfInterface);

    // Call the Post Bind Callback for Protocol Module.
    //
    ProtocolDescriptor->GenericInterfaceDescriptor.DmfInterfacePostBind(dmfInterfaceObject->DmfInterface);

Exit:

    return ntStatus;
}

VOID
DMF_ModuleInterfaceUnbind(
    _In_ DMFMODULE ProtocolModule,
    _In_ DMFMODULE TransportModule,
    _In_ DMF_INTERFACE_PROTOCOL_DESCRIPTOR* ProtocolDescriptor,
    _In_ DMF_INTERFACE_TRANSPORT_DESCRIPTOR* TransportDescriptor
    )
/*++

Routine Description:

    Destroys the Interface between the given Protocol and Transport Modules.
    This Interface is based on Interface specific information provided by the ProtocolDescriptor
    and TransportDescriptor.

    NOTE: Synchronization must be considered if DMF_ModuleInterfaceBind, DMF_ModuleInterfaceUnbind and
          DMF_ModuleInterfacesUnbind calls occur simultaneously.

Arguments:

    ProtocolModule - The given Protocol Module.
    TransportModule - The given Transport Module.
    ProtocolDescriptor - The Interface Descriptor associated with the Protocol.
    TransportDescriptor - The Interface Descriptor associated with the Transport.

Return Value:

    None

--*/

{
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;
    DMF_INTERFACE_OBJECT* dmfInterfaceObjectTemp;

    DmfAssert((NULL != ProtocolModule) &&
              (NULL != TransportModule) &&
              (NULL != ProtocolDescriptor) &&
              (NULL != TransportDescriptor));

    // Find the Interface Handle in Transport Module.
    //
    if (!DMF_ModuleInterfaceFindAndRemove(ProtocolModule,
                                          TransportModule,
                                          &dmfInterfaceObject))
    {
        // This could happen so no DmfAssert(FALSE) here.
        // This happens if both Protocol and Transport are getting destroyed simultaneously.
        // Both will call the Unbind function and depending on who calls it first, the other caller will
        // enter this code path.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleInterfaceFind failed to find ProtocolModule=0x%p in TransportModule=0x%p Bindings", ProtocolModule, TransportModule);
        goto Exit;
    }

    // Find the Interface Handle in Protocol Module.
    //
    if (!DMF_ModuleInterfaceFindAndRemove(TransportModule,
                                          ProtocolModule,
                                          &dmfInterfaceObjectTemp))
    {
        // This could happen so no DmfAssert(FALSE) here.
        // This happens if both Protocol and Transport are getting destroyed simultaneously.
        // Both will call the Unbind function and depending on who calls it first, the other caller will
        // enter this code path.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleInterfaceFind failed to find TransportModule=0x%p in ProtocolModule=0x%p Bindings", TransportModule, ProtocolModule);
        goto Exit;
    }

    DmfAssert(dmfInterfaceObject == dmfInterfaceObjectTemp);

    // Interface state is set to Opened when PreUnbind callbacks are called.
    //
    // Call the Pre Unbind Callback for Protocol Module.
    //
    ProtocolDescriptor->GenericInterfaceDescriptor.DmfInterfacePreUnbind(dmfInterfaceObject->DmfInterface);

    // Call the Pre Unbind Callback for Transport Module.
    //
    TransportDescriptor->GenericInterfaceDescriptor.DmfInterfacePreUnbind(dmfInterfaceObject->DmfInterface);

    // Wait for the Interface reference count to become 0.
    //
    DMF_ModuleInterfaceWaitToClose(dmfInterfaceObject);

    // Interface state is set to Closing when Unbind call is made.
    //
    // Call the Unbind function.
    //
    ProtocolDescriptor->DmfInterfaceProtocolUnbind(dmfInterfaceObject->DmfInterface);

    // Destroy the DMF Interface.
    //
    DMF_ModuleInterfaceDestroy(dmfInterfaceObject);

Exit:

    return;
}

VOID
DMF_ModuleInterfacesUnbind(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Unbinds all the Interfaces associated with a given DMF Module.

    NOTE: Synchronization must be considered if DMF_ModuleInterfaceBind, DMF_ModuleInterfaceUnbind and
          DMF_ModuleInterfacesUnbind calls occur simultaneously.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMFINTERFACE dmfInterface;
    DMF_INTERFACE_OBJECT* dmfInterfaceObject;
    DMF_OBJECT* dmfObject;

    dmfObject = DMF_ModuleToObject(DmfModule);

    // Unbind all interface bindings of this Module.
    // NOTE: Generally speaking this loop should not execute.
    // This will happen in the case of non-PnP Client drivers.
    //
    WdfSpinLockAcquire(dmfObject->InterfaceBindingsLock);

    dmfInterface = (DMFINTERFACE)WdfCollectionGetFirstItem(dmfObject->InterfaceBindings);

    WdfSpinLockRelease(dmfObject->InterfaceBindingsLock);

    while (dmfInterface)
    {
        dmfInterfaceObject = DMF_InterfaceToObject(dmfInterface);

        DmfAssert((DmfModule == dmfInterfaceObject->ProtocolModule) || (DmfModule == dmfInterfaceObject->TransportModule));

        DMF_ModuleInterfaceUnbind(dmfInterfaceObject->ProtocolModule,
                                  dmfInterfaceObject->TransportModule,
                                  dmfInterfaceObject->ProtocolDescriptor,
                                  dmfInterfaceObject->TransportDescriptor);

        WdfSpinLockAcquire(dmfObject->InterfaceBindingsLock);

        dmfInterface = (DMFINTERFACE)WdfCollectionGetFirstItem(dmfObject->InterfaceBindings);

        WdfSpinLockRelease(dmfObject->InterfaceBindingsLock);
    }
}

// eof: DmfInterfaceInternal.c
//
