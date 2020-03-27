/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_IoctlHandler.c

Abstract:

    Creates a Device Interface and defines IOCTLs using a table. Also validates buffer sizes
    and optional access rights for IOCTLs. Then, calls a Client's callback function for each IOCTL.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Core.h"
#include "DmfModules.Core.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_IoctlHandler.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // It is a collection of all the Open File Objects that are running "As Administrator".
    //
    WDFCOLLECTION AdministratorFileObjectsCollection;
    // Access to IoGetDeviceInterfacePropertyData().
    //
    IoctlHandler_IO_GET_DEVICE_INTERFACE_PROPERTY_DATA* IoGetDeviceInterfacePropertyData;
    // Access to IoSetDeviceInterfacePropertyData().
    //
    IoctlHandler_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA* IoSetDeviceInterfacePropertyData;
} DMF_CONTEXT_IoctlHandler;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(IoctlHandler)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(IoctlHandler)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#include <devpkey.h>

#pragma code_seg("PAGE")
static
_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
IoctlHandler_PostDeviceInterfaceCreate(
    _In_ DMFMODULE DmfModule
    )
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_IoctlHandler* moduleContext;
    DMF_CONFIG_IoctlHandler* moduleConfig;
    WDFDEVICE device;
    UNICODE_STRING symbolicLinkName;
    WDFSTRING symbolicLinkNameString;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    device = DMF_ParentDeviceGet(DmfModule);
    symbolicLinkNameString = NULL;

    ntStatus = WdfStringCreate(NULL,
                               WDF_NO_OBJECT_ATTRIBUTES,
                               &symbolicLinkNameString);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfStringCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = WdfDeviceRetrieveDeviceInterfaceString(device,
                                                      &moduleConfig->DeviceInterfaceGuid,
                                                      NULL,
                                                      symbolicLinkNameString);
    if (!NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceRetrieveDeviceInterfaceString fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    WdfStringGetUnicodeString(symbolicLinkNameString, 
                              &symbolicLinkName);

#if !defined(DMF_USER_MODE)

    DEVPROP_BOOLEAN isRestricted;
    UNICODE_STRING functionName;

    // If possible, get the IoSetDeviceInterfacePropertyData.
    //
    RtlInitUnicodeString(&functionName, 
                         L"IoSetDeviceInterfacePropertyData");
    moduleContext->IoSetDeviceInterfacePropertyData = (IoctlHandler_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA*)(ULONG_PTR)MmGetSystemRoutineAddress(&functionName);

    // If possible, get the IoGetDeviceInterfacePropertyData.
    //
    RtlInitUnicodeString(&functionName, 
                         L"IoGetDeviceInterfacePropertyData");
    moduleContext->IoGetDeviceInterfacePropertyData = (IoctlHandler_IO_GET_DEVICE_INTERFACE_PROPERTY_DATA*)(ULONG_PTR)MmGetSystemRoutineAddress(&functionName);

    // If the Client has set the IsRestricted or CustomProperty fields, try to set those properties.
    //
    if ((moduleContext->IoSetDeviceInterfacePropertyData != NULL) &&
        ((moduleConfig->IsRestricted) || 
         (moduleConfig->CustomCapabilities != NULL)))
    {
        isRestricted = moduleConfig->IsRestricted;

        ntStatus = moduleContext->IoSetDeviceInterfacePropertyData(&symbolicLinkName,
                                                                   &DEVPKEY_DeviceInterface_Restricted,
                                                                   0,
                                                                   0,
                                                                   DEVPROP_TYPE_BOOLEAN,
                                                                   sizeof(isRestricted),
                                                                   &isRestricted );
        if (!NT_SUCCESS(ntStatus)) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoSetDeviceInterfacePropertyData fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

#if defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
        if (moduleConfig->CustomCapabilities != NULL)
        {
            // Adds a custom capability to device interface instance that allows a Windows
            // Store device app to access this interface using Windows.Devices.Custom namespace.

            // Get size of buffer. Add space for double \0 terminator.
            //
            ULONG stringLength = (ULONG)wcslen(moduleConfig->CustomCapabilities);
            ULONG bufferSize = (stringLength * sizeof(WCHAR)) + (2 * sizeof(WCHAR));

            ntStatus = moduleContext->IoSetDeviceInterfacePropertyData(&symbolicLinkName,
                                                                       &DEVPKEY_DeviceInterface_UnrestrictedAppCapabilities,
                                                                       0,
                                                                       0,
                                                                       DEVPROP_TYPE_STRING_LIST,
                                                                       bufferSize,
                                                                       (PVOID)moduleConfig->CustomCapabilities);
            if (!NT_SUCCESS(ntStatus)) 
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoSetDeviceInterfacePropertyData fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }
#endif // defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
    }

#endif // defined(DMF_USER_MODE)

    // Optionaly, allow Client to perform other operations. Give Client access to functions that allow query/set of
    // device interface properties. However, Client is responsible for checking if they are NULL.
    //
    if (moduleConfig->PostDeviceInterfaceCreate != NULL)
    {
        // ''symbolicLinkNameString' could be '0''
        //
        #pragma warning(suppress:6387)
        ntStatus = moduleConfig->PostDeviceInterfaceCreate(DmfModule,
                                                           moduleConfig->DeviceInterfaceGuid,
                                                           &symbolicLinkName,
                                                           moduleContext->IoGetDeviceInterfacePropertyData,
                                                           moduleContext->IoSetDeviceInterfacePropertyData);
    }

Exit:

    if (symbolicLinkNameString != NULL)
    {
        WdfObjectDelete(symbolicLinkNameString);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    // NOTE: Module will not open if this function returns an error.
    //
    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
IoctlHandler_DeviceInterfaceCreate(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Create the device interface specified by the Client. Then, perform optional predefined tasks specified by the Client.
    Then, call a callback into the Client so that Client can perform additional tasks after the device interface has been created.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_IoctlHandler* moduleContext;
    DMF_CONFIG_IoctlHandler* moduleConfig;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    // Register a device interface so applications/drivers can find and open this device.
    //   
    ntStatus = WdfDeviceCreateDeviceInterface(device,
                                              (LPGUID)&moduleConfig->DeviceInterfaceGuid,
                                              NULL);
    if (! NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfDeviceCreateDeviceInterface fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Perform some optional tasks for Client. Also, let Client know when device interface is created.
    //
    ntStatus = IoctlHandler_PostDeviceInterfaceCreate(DmfModule);
    if (! NT_SUCCESS(ntStatus)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoctlHandler_PostDeviceInterfaceCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(DMF_ModuleDeviceIoControl)
static
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DMF_IoctlHandler_ModuleDeviceIoControl(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    DmfModule - This Module's handle.

    Queue - Handle to the framework queue object that is associated
            with the I/O request.

    Request - Handle to a framework request object.

    OutputBufferLength - Length of the request's output buffer,
                         if an output buffer is available.

    InputBufferLength - Length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - The driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.

Return Value:

    TRUE if this routine handled the request.

--*/
{
    BOOLEAN handled;
    DMF_CONTEXT_IoctlHandler* moduleContext;
    size_t bytesReturned;
    NTSTATUS ntStatus;
    DMF_CONFIG_IoctlHandler* moduleConfig;
    KPROCESSOR_MODE requestSenderMode;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    // NOTE: No entry/exit logging to eliminate spurious logging.
    //

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    handled = FALSE;
    bytesReturned = 0;
    ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    // If queue is only allowed handle requests from kernel mode, reject all other types of requests.
    // 
    requestSenderMode = WdfRequestGetRequestorMode(Request);

    if (moduleConfig->KernelModeRequestsOnly &&
        requestSenderMode != KernelMode)
    {
        handled = TRUE;
        ntStatus = STATUS_ACCESS_DENIED;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "User mode access detected on kernel mode only queue.");
        goto Exit;
    }

    for (ULONG tableIndex = 0; tableIndex < moduleConfig->IoctlRecordCount; tableIndex++)
    {
        IoctlHandler_IoctlRecord* ioctlRecord;

        ioctlRecord = &moduleConfig->IoctlRecords[tableIndex];
        if ((ULONG)(ioctlRecord->IoctlCode) == IoControlCode)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                        "Matching IOCTL Found: 0x%08X tableIndex=%d",
                        IoControlCode,
                        tableIndex);

            // Always indicate handled, regardless of error.
            //
            handled = TRUE;

            // AdministratorAccessOnly can only be TRUE in the EVT_DMF_IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl mode.
            //
            DmfAssert((ioctlRecord->AdministratorAccessOnly && (moduleConfig->AccessModeFilter == IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl)) ||
                      (! (ioctlRecord->AdministratorAccessOnly)));

            // Deny access if the IOCTLs are granted access on per-IOCTL basis.
            //
            if ((moduleConfig->AccessModeFilter == IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl) &&
                (ioctlRecord->AdministratorAccessOnly))
            {
                BOOLEAN isAdministrator = FALSE;
                WDFOBJECT fileObject;
                WDFFILEOBJECT fileObjectOfRequest = WdfRequestGetFileObject(Request);
                ULONG itemIndex = 0;

                DMF_ModuleLock(DmfModule);

                fileObject = WdfCollectionGetItem(moduleContext->AdministratorFileObjectsCollection,
                                                  itemIndex);
                while (fileObject != NULL)
                {
                    if (fileObject == fileObjectOfRequest)
                    {
                        isAdministrator = TRUE;
                        break;
                    }
                    itemIndex++;
                    fileObject = WdfCollectionGetItem(moduleContext->AdministratorFileObjectsCollection,
                                                      itemIndex);
                }

                DMF_ModuleUnlock(DmfModule);

                if (! isAdministrator)
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Access denied because caller is not Administrator tableIndex=%d", tableIndex);
                    ntStatus = STATUS_ACCESS_DENIED;
                    break;
                }
            }

            VOID* inputBuffer;
            size_t inputBufferSize;
            VOID* outputBuffer;
            size_t outputBufferSize;

            // Get a pointer to the input buffer. Make sure it is big enough.
            //
            ntStatus = WdfRequestRetrieveInputBuffer(Request,
                                                     ioctlRecord->InputBufferMinimumSize,
                                                     &inputBuffer,
                                                     &inputBufferSize);
            if (! NT_SUCCESS(ntStatus))
            {
                if ((STATUS_BUFFER_TOO_SMALL == ntStatus) &&
                    (ioctlRecord->InputBufferMinimumSize == 0))
                {
                    // Fall through to handler. Let handler validate.
                    //
                    inputBuffer = NULL;
                    inputBufferSize = 0;
                }
                else
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestRetrieveInputBuffer fails: ntStatus=%!STATUS!", ntStatus);
                    break;
                }
            }

            // Get a pointer to the output buffer. Make sure it is big enough
            //
            ntStatus = WdfRequestRetrieveOutputBuffer(Request,
                                                      ioctlRecord->OutputBufferMinimumSize,
                                                      &outputBuffer,
                                                      &outputBufferSize);
            if (! NT_SUCCESS(ntStatus))
            {
                if ((STATUS_BUFFER_TOO_SMALL == ntStatus) &&
                    (ioctlRecord->OutputBufferMinimumSize == 0))
                {
                    // Fall through to handler. Let handler validate.
                    //
                    outputBuffer = NULL;
                    outputBufferSize = 0;
                }
                else
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRequestRetrieveOutputBuffer fails: ntStatus=%!STATUS!", ntStatus);
                    break;
                }
            }

            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                        "InputBufferSize=%d OutputBufferSize=%d tableIndex=%d",
                        (ULONG)inputBufferSize,
                        (ULONG)outputBufferSize,
                        tableIndex);

            // Buffer is validated. Call client handler.
            //
            ntStatus = ioctlRecord->EvtIoctlHandlerFunction(DmfModule,
                                                            Queue,
                                                            Request,
                                                            IoControlCode,
                                                            inputBuffer,
                                                            inputBufferSize,
                                                            outputBuffer,
                                                            outputBufferSize,
                                                            &bytesReturned);
            break;
        }
    }

Exit:

    if (handled)
    {
        if (STATUS_PENDING == ntStatus)
        {
            // Do not complete the request. Client is keeping it until later.
            //
        }
        else
        {
            // Complete the request.
            //
            WdfRequestCompleteWithInformation(Request,
                                              ntStatus,
                                              bytesReturned);
        }
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Handled: Request=0x%p ntStatus=%!STATUS!", Request, ntStatus);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Not Handled: Request=0x%p", Request);
    }

    // NOTE: No entry/exit logging to eliminate spurious logging.
    //

    return handled;
}

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleFileCreate)
_Must_inspect_result_
static
BOOLEAN
DMF_IoctlHandler_FileCreate(
    _In_ DMFMODULE DmfModule,
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    ModuleFileCreate callback for IoctlHandler. This callback allows the client to
    restrict access to User-mode IOCTLs.

Arguments:

    DmfModule - The given DMF Module.
    Device - WDF device object.
    Request - WDF Request with IOCTL parameters.
    FileObject - WDF file object that describes a file that is being opened for the specified request

Return Value:

    FALSE indicates this Module did not complete the Request and other modules
    should check for support.
    TRUE indicates that this Module has completed the Request.
    If you use multiple DMF Modules that support this handler, use extreme caution as only
    a single Module can complete the request.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_IoctlHandler* moduleConfig;
    DMF_CONTEXT_IoctlHandler* moduleContext;
    WDF_REQUEST_PARAMETERS requestParameters;
    BOOLEAN handled;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);

    // Assume this handler does nothing.
    // If the request is returned, handled must be set to TRUE to 
    // prevent other DMF Modules from seeing the request.
    //
    handled = FALSE;

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (IoctlHandler_AccessModeDefault == moduleConfig->AccessModeFilter ||
        IoctlHandler_AccessModeFilterKernelModeOnly == moduleConfig->AccessModeFilter)
    {
        // Callback does nothing...just do what WDF would normally do.
        // This call supports both filter and non-filter drivers correctly.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IoctlHandler_AccessModeDefault");
        if (DMF_ModuleIsInFilterDriver(DmfModule))
        {
            handled = DMF_ModuleRequestCompleteOrForward(DmfModule,
                                                         Request,
                                                         STATUS_SUCCESS);
        }
    }
    else if ((IoctlHandler_AccessModeFilterAdministratorOnly == moduleConfig->AccessModeFilter) ||
             (IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl == moduleConfig->AccessModeFilter))
    {
        // Only allow programs running "As Administrator" to open the connection 
        // to User-mode.
        //
        ntStatus = STATUS_ACCESS_DENIED;

        WDF_REQUEST_PARAMETERS_INIT(&requestParameters);
        WdfRequestGetParameters(Request,
                                &requestParameters);

#if !defined(DMF_USER_MODE)
        PIO_SECURITY_CONTEXT ioSecurityContext;
        PACCESS_TOKEN accessToken;

        // Check all the pointers because these fields are not commonly used.
        //
        ioSecurityContext = requestParameters.Parameters.Create.SecurityContext;
        if (NULL == ioSecurityContext)
        {
            DmfAssert(FALSE);
            DmfAssert(! NT_SUCCESS(ntStatus));
            goto RequestComplete;
        }

        // This is empirically determined.
        //
        accessToken = ioSecurityContext->AccessState->SubjectSecurityContext.PrimaryToken;
        if (NULL == accessToken)
        {
            DmfAssert(FALSE);
            DmfAssert(! NT_SUCCESS(ntStatus));
            goto RequestComplete;
        }

        // Check if an Administrator is creating the handle.
        //
        if (SeTokenIsAdmin(accessToken))
        {
            if (moduleConfig->AccessModeFilter == IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl)
            {
                // It is an administrator...Add to list of administrators.
                // Need to acquire the lock because other functions that run
                // asynchronously iterate through the list with the lock held.
                // (Optimize to add to list only in mode where the list is used.)
                //
                DMF_ModuleLock(DmfModule);
                ntStatus = WdfCollectionAdd(moduleContext->AdministratorFileObjectsCollection,
                                            FileObject);
                DMF_ModuleUnlock(DmfModule);
            }
            else
            {
                // Open the file...all IOCTLs are allowed so there is no need to
                // store handles.
                //
                ntStatus = STATUS_SUCCESS;
            }
        }
        else
        {
            DmfAssert(! NT_SUCCESS(ntStatus));
            if (IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl == moduleConfig->AccessModeFilter)
            {
                // Always allow open, access is checked on per-IOCTL bases later.
                //
                ntStatus = STATUS_SUCCESS;
            }
        }

RequestComplete:

#endif // !defined(DMF_USER_MODE)

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "EVT_DMF_IoctlHandler_AccessModeFilterAdministrator* ntStatus=%!STATUS!", ntStatus);
        if (!NT_SUCCESS(ntStatus))
        {
            // This call completes the request correctly for both filter and non-filter drivers.
            //
            handled = DMF_ModuleRequestCompleteOrForward(DmfModule,
                                                         Request,
                                                         ntStatus);
        }
    }
    else if (IoctlHandler_AccessModeFilterClientCallback == moduleConfig->AccessModeFilter)
    {
        // Allow the Client to determine if the connection to User-mode should be allowed.
        //
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "EVT_DMF_IoctlHandler_AccessModeFilterClientCallback");
        DmfAssert(moduleConfig->EvtIoctlHandlerAccessModeFilter != NULL);
        // NOTE: This callback must use DMF_ModuleRequestCompleteOrForward() to complete the request if the 
        //       return status is not STATUS_SUCCESS; or, return FALSE.
        //
        handled = moduleConfig->EvtIoctlHandlerAccessModeFilter(DmfModule,
                                                                Device,
                                                                Request,
                                                                FileObject);
    }
    else
    {
        // There are no other valid cases.
        //
        DmfAssert(FALSE);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoctlHandler_AccessModeInvalid");
        // WARNING: Request is not completed. This code should not run.
        //
    }

    FuncExit(DMF_TRACE, "handled=%d", handled);

    return handled;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleFileCleanup)
_Must_inspect_result_
static
BOOLEAN
DMF_IoctlHandler_FileCleanup(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    ModuleFileCleanup callback for IoctlHandler. This callback is used to remove the
    FileObject from the list of open Administrator handles.

Arguments:

    DmfModule - This Module's handle.
    FileObject - FileObject to delete from list (if it is in the list).

Return Value:

    None

--*/
{
    BOOLEAN handled;
    ULONG itemIndex;
    WDFFILEOBJECT fileObject;
    DMF_CONTEXT_IoctlHandler* moduleContext;
    DMF_CONFIG_IoctlHandler* moduleConfig;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    handled = TRUE;

    // (Optimize to add to list only in mode where the list is used.)
    //
    if (! (moduleConfig->AccessModeFilter == IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl))
    {
        goto Exit;
    }

    DMF_ModuleLock(DmfModule);

    itemIndex = 0;
    fileObject = (WDFFILEOBJECT)WdfCollectionGetItem(moduleContext->AdministratorFileObjectsCollection,
                                                     itemIndex);
    while (fileObject != NULL)
    {
        if (fileObject == FileObject)
        {
            WdfCollectionRemove(moduleContext->AdministratorFileObjectsCollection,
                                fileObject);
            break;
        }
        itemIndex++;
        fileObject = (WDFFILEOBJECT)WdfCollectionGetItem(moduleContext->AdministratorFileObjectsCollection,
                                                         itemIndex);
    }

    DMF_ModuleUnlock(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "handled=%d", handled);

    return handled;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ModuleFileClose)
_Must_inspect_result_
static
BOOLEAN
DMF_IoctlHandler_FileClose(
    _In_ DMFMODULE DmfModule,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    ModuleFileClose callback for IoctlHandler. This callback is used to remove the
    FileObject from the list of open Administrator handles.

Arguments:

    DmfModule - This Module's handle.
    FileObject - FileObject to delete from list (if it is in the list).

Return Value:

    None

--*/
{
    BOOLEAN handled;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    handled = DMF_IoctlHandler_FileCleanup(DmfModule,
                                           FileObject);

    FuncExit(DMF_TRACE, "handled=%d", handled);

    return handled;
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_IoctlHandler_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type IoctlHandler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    NT_STATUS code indicating success or failure.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_IoctlHandler* moduleContext;
    DMF_CONFIG_IoctlHandler* moduleConfig;
    WDFDEVICE device;
    GUID nullGuid;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    RtlZeroMemory(&nullGuid,
                  sizeof(GUID));
    if (! DMF_Utility_IsEqualGUID(&nullGuid,
                                  &moduleConfig->DeviceInterfaceGuid))
    {
        // Register a device interface so applications/drivers can find and open this device.
        //
        ntStatus = IoctlHandler_DeviceInterfaceCreate(DmfModule);

        if (moduleConfig->ManualMode)
        {
            WdfDeviceSetDeviceInterfaceState(device,
                                             &moduleConfig->DeviceInterfaceGuid,
                                             NULL,
                                             FALSE);
        }
    }
    else
    {
        // Target will be opened directly, not using a device interface.
        //
        ntStatus = STATUS_SUCCESS;
    }

    // (Optimize to add to list only in mode where the list is used.)
    //
    if (moduleConfig->AccessModeFilter == IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl)
    {
        // Create a collection for all the open Administrators.
        // It keeps track of all the file handles that are opened "As Administrator".
        //
        ntStatus = WdfCollectionCreate(WDF_NO_OBJECT_ATTRIBUTES,
                                       &moduleContext->AdministratorFileObjectsCollection);
        if (! NT_SUCCESS(ntStatus))
        {
            // For safety and SAL.
            //
            moduleContext->AdministratorFileObjectsCollection = NULL;
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfCollectionCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_IoctlHandler_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Destroy an instance of a DMF Module of type IoctlHandler.

Arguments:

    DmfModule - The given DMF Module.

Return Value:

    None

--*/
{
    DMF_CONTEXT_IoctlHandler* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->AdministratorFileObjectsCollection != NULL)
    {
        WdfObjectDelete(moduleContext->AdministratorFileObjectsCollection);
        moduleContext->AdministratorFileObjectsCollection = NULL;
    }

    FuncExitNoReturn(DMF_TRACE);
}
#pragma code_seg()

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_IoctlHandler_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type IoctlHandler.

Arguments:

    Device - Client Driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_IoctlHandler;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_IoctlHandler;
    DMF_CALLBACKS_WDF dmfCallbacksWdf_IoctlHandler;
    DMF_CONFIG_IoctlHandler* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_IoctlHandler);
    dmfCallbacksDmf_IoctlHandler.DeviceOpen = DMF_IoctlHandler_Open;
    dmfCallbacksDmf_IoctlHandler.DeviceClose = DMF_IoctlHandler_Close;

    DMF_CALLBACKS_WDF_INIT(&dmfCallbacksWdf_IoctlHandler);

    moduleConfig = (DMF_CONFIG_IoctlHandler*)DmfModuleAttributes->ModuleConfigPointer;

    if (moduleConfig->AccessModeFilter == IoctlHandler_AccessModeFilterKernelModeOnly)
    {
        // Only allow IOCTLs to come from other Kernel-mode components.
        //
        dmfCallbacksWdf_IoctlHandler.ModuleInternalDeviceIoControl = DMF_IoctlHandler_ModuleDeviceIoControl;
    }
    else
    {
        // Allow IOCTLs to come from User-mode applications.
        //
        dmfCallbacksWdf_IoctlHandler.ModuleDeviceIoControl = DMF_IoctlHandler_ModuleDeviceIoControl;
    }
    dmfCallbacksWdf_IoctlHandler.ModuleFileCreate = DMF_IoctlHandler_FileCreate;
    dmfCallbacksWdf_IoctlHandler.ModuleFileCleanup = DMF_IoctlHandler_FileCleanup;
    dmfCallbacksWdf_IoctlHandler.ModuleFileClose = DMF_IoctlHandler_FileClose;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_IoctlHandler,
                                            IoctlHandler,
                                            DMF_CONTEXT_IoctlHandler,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

    dmfModuleDescriptor_IoctlHandler.CallbacksDmf = &dmfCallbacksDmf_IoctlHandler;
    dmfModuleDescriptor_IoctlHandler.CallbacksWdf = &dmfCallbacksWdf_IoctlHandler;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_IoctlHandler,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}
#pragma code_seg()

// Module Methods
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_IoctlHandler_IoctlStateSet(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Enable
    )
/*++

Routine Description:

    Allows Client to enable / disable the device interface set in the Module's Config.

Arguments:

    DmfModule - This Module's handle.
    Enable - If true, enable the device interface.
             Otherwise, disable the device interface.

Return Value:

    None

--*/
{
    DMF_CONFIG_IoctlHandler* moduleConfig;
    WDFDEVICE device;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 IoctlHandler);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    WdfDeviceSetDeviceInterfaceState(device,
                                     &moduleConfig->DeviceInterfaceGuid,
                                     NULL,
                                     Enable);

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

// eof: Dmf_IoctlHandler.c
//
