/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_Registry.c

Abstract:

    Performs registry operations. Currently, only registry write is supported.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_Registry.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This is the number of characters in \Registry\Machine, which is the root of all registry access.
//
#define REGISTRY_ROOT_LENGTH  (sizeof("\\Registry\\Machine") - 1)

// Context data for registry enumeration functions.
//
typedef struct
{
    // Context used by filter enumeration function.
    //
    VOID* FilterEnumeratorContext;
    // The client callback function.
    //
    EVT_DMF_Registry_KeyEnumerationCallback* RegistryKeyEnumerationFunction;
    // The client callback function context.
    //
    VOID* ClientCallbackContext;
} RegistryKeyEnumerationContextType;

// List of possible deferred operations.
//
typedef enum
{
    Registry_DeferredOperationInvalid = 0,
    Registry_DeferredOperationWrite = 1,
    // Not supported yet.
    //
    Registry_DeferredOperationDelete = 2,
} Registry_DeferredOperationType;

// There can be multiple outstanding deferred operations. Each deferred operation
// has its own context. These contexts are held in a list which is iterated through
// upon timer expiration.
//
typedef struct
{
    // The operation to perform until it is successful.
    //
    Registry_DeferredOperationType DeferredOperation;
    // The array of registry trees to perform the operation on.
    //
    Registry_Tree* RegistryTree;
    // Number of trees in the above array.
    //
    ULONG ItemCount;
    // Used for list management.
    //
    LIST_ENTRY ListEntry;
} REGISTRY_DEFERRED_CONTEXT;

// It is the time interval to use for polling (how often to attempt
// the deferred operations).
//
const ULONG Registry_DeferredRegistryWritePollingIntervalMs = 1000;

// Context for CustomActionHandler used by this Module for Registry reads.
//
typedef struct
{
    // Where the data will be written.
    //
    UCHAR* Buffer;
    // The size in bytes of Buffer.
    //
    ULONG BufferSize;

    // These are written by the Module Method.
    //

    // Number of bytes written to Buffer.
    //
    ULONG* BytesRead;
    // Indicates if the client's request succeeded.
    //
    NTSTATUS NtStatus;
} Registry_CustomActionHandler_Read_Context;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_Registry
{
#if !defined(DMF_USER_MODE)
    // Deferred Tree Write is not supported in User-mode.
    //

    // Timer for deferred operations.
    //
    WDFTIMER Timer;
    // Stores data needed to perform deferred operations.
    //
    LIST_ENTRY ListDeferredOperations;
#endif
} DMF_CONTEXT_Registry;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(Registry)

// This Module has no Config.
//
DMF_MODULE_DECLARE_NO_CONFIG(Registry)

// Memory Pool Tag.
//
#define MemoryTag 'MgeR'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")

_Function_class_(EVT_DMF_Registry_ValueComparisonCallback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
Registry_CustomActionHandler_Read(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* ClientContext,
    _In_ VOID* ValueDataInRegistry,
    _In_ ULONG ValueDataInRegistrySize,
    _In_ VOID* ClientDataInRegistry,
    _In_ ULONG ClientDataInRegistrySize
    )
{
    BOOLEAN returnValue;
    Registry_CustomActionHandler_Read_Context* customActionHandlerContextRead;

    UNREFERENCED_PARAMETER(DmfModule);
    UNREFERENCED_PARAMETER(ClientDataInRegistry);
    UNREFERENCED_PARAMETER(ClientDataInRegistrySize);

    PAGED_CODE();

    // NOTE: This context is specific to this instance of this handler.
    //
    customActionHandlerContextRead = (Registry_CustomActionHandler_Read_Context*)ClientContext;
    DmfAssert(customActionHandlerContextRead != NULL);

    // If the value matches the value defined by the caller, then caller gets
    // back TRUE, otherwise false.
    //
    if (ValueDataInRegistrySize <= customActionHandlerContextRead->BufferSize)
    {
        // The value is of the correct type. Read it.
        //
        DmfAssert(customActionHandlerContextRead->Buffer != NULL);
        RtlCopyMemory(customActionHandlerContextRead->Buffer,
                      ValueDataInRegistry,
                      ValueDataInRegistrySize);
        returnValue = TRUE;
        customActionHandlerContextRead->NtStatus = STATUS_SUCCESS;
    }
    else
    {
        // It is cleared by the caller if the parameter is present.
        //
        returnValue = FALSE;
        // Tell the caller that buffer was too small to write the read value.
        //
        customActionHandlerContextRead->NtStatus = STATUS_BUFFER_TOO_SMALL;
    }

    // NOTE: Bytes read is optional for caller.
    // In both cases, tell caller how many bytes are required. Caller may have passed
    // NULL buffer to inquire about size prior to buffer allocation.
    //
    if (customActionHandlerContextRead->BytesRead != NULL)
    {
        // It is cleared by the caller if the parameter is present.
        //
        DmfAssert(0 == *(customActionHandlerContextRead->BytesRead));
        *(customActionHandlerContextRead->BytesRead) = ValueDataInRegistrySize;
    }

    // No action is every performed by caller so this return value does not matter.
    // It tells the Client Driver if the value was read.
    //
    return returnValue;
}

#if defined(DMF_KERNEL_MODE)

NTSTATUS
Registry_DeviceInterfaceKeyOpen(
    _In_ DMFMODULE DmfModule,
    _In_ WDFSTRING DeviceLink,
    _In_ ULONG Access,
    _Out_ HANDLE* RegistryHandle
    )
{
    NTSTATUS ntStatus;
    UNICODE_STRING temporaryDeviceLink;

    UNREFERENCED_PARAMETER(DmfModule);

    WdfStringGetUnicodeString(DeviceLink,
                              &temporaryDeviceLink);
    ntStatus = IoOpenDeviceInterfaceRegistryKey(&temporaryDeviceLink,
                                                Access,
                                                RegistryHandle);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoOpenDeviceInterfaceRegistryKey fails: ntStatus=%!STATUS!", ntStatus);
    }

    return ntStatus;
}

#elif defined(DMF_USER_MODE)

NTSTATUS
Registry_DeviceInterfaceKeyOpen(
    _In_ DMFMODULE DmfModule,
    _In_ WDFSTRING DeviceLink,
    _In_ ULONG Access,
    _Out_ HANDLE* RegistryHandle
    )
{
    NTSTATUS ntStatus;
    CONFIGRET configRet;
    PWSTR deviceInterface;
    WDFMEMORY deviceInterfaceListObject;
    size_t stringSize;
    UNICODE_STRING temporaryDeviceLink;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    deviceInterfaceListObject = NULL;

    WdfStringGetUnicodeString(DeviceLink,
                              &temporaryDeviceLink);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               PagedPool,
                               MemoryTag,
                               temporaryDeviceLink.Length + sizeof(WCHAR),
                               &deviceInterfaceListObject,
                               (VOID**)&deviceInterface);

    if (!NT_SUCCESS(ntStatus))
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Copy string with a length = stringSize-1 to buffer with length = stringSize
    //
    stringSize = (temporaryDeviceLink.Length / sizeof(WCHAR)) + 1;
    wcsncpy_s(deviceInterface,
              stringSize,
              temporaryDeviceLink.Buffer,
              stringSize - 1);


    // Open registry key for device interface instance in UMDF here.
    // RegistryHandle will be closed on exit
    //
    configRet = CM_Open_Device_Interface_KeyW(deviceInterface,
                                              Access,
                                              RegDisposition_OpenExisting,
                                              (HKEY*)RegistryHandle,
                                              0);
    if (CR_SUCCESS != configRet)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Open_Device_Interface_KeyW fails");
    }

Exit:

    if (deviceInterfaceListObject)
    {
        WdfObjectDelete(deviceInterfaceListObject);
    }

    return ntStatus;
}
#endif

#if defined(DMF_KERNEL_MODE)

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
PWSTR
Registry_DeviceLinkGet(
    _In_ PWSTR DeviceInterfaces,
    _In_ int DeviceLinkIndex
    )
/*++

Routine Description:

    Find Device Interface symbolic link in the list for the given index (KMDF).

Arguments:

    DeviceInterfaces - Device interface link's list.
    DeviceLinkIndex - Instance index for Device Interface symbolic link's list.

Return Value:

    Address of found symbolic link or NULL if not found.
    The found address is pointing to substring inside DeviceInterfaces buffer.

--*/
{
    PWSTR devicePath;
    LONG index;
    size_t devicePathLength;

    NTSTATUS ntStatus;

    for (devicePath = DeviceInterfaces, index = 0; *devicePath; )
    {
        if (DeviceLinkIndex == index)
        {
            return devicePath;
        }

        ntStatus = RtlStringCchLengthW(devicePath,
                                       NTSTRSAFE_MAX_CCH,
                                       &devicePathLength);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlStringCchLengthW fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        devicePath += devicePathLength + 1;
        ++index;
    }

Exit:

    return NULL;
}

#elif defined(DMF_USER_MODE)

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
PWSTR
Registry_DeviceLinkGet(
    _In_ PWSTR DeviceInterfaces,
    _In_ int DeviceLinkIndex
    )
/*++

Routine Description:

    Find Device Interface symbolic link in the list for the given index (UMDF).

Arguments:

    DeviceInterfaces - Device interface link's list.
    DeviceLinkIndex - Instance index for Device Interface symbolic link's list.

Return Value:

    Address of found symbolic link or NULL if not found.
    The found address is pointing to substring inside DeviceInterfaces buffer.

--*/
{
    PWSTR devicePath;
    LONG index;
    size_t devicePathLength;

    for (devicePath = DeviceInterfaces, index = 0; *devicePath; )
    {
        if (DeviceLinkIndex == index)
        {
            return devicePath;
        }

        devicePathLength = wcslen(devicePath);
        devicePath += devicePathLength + 1;
        ++index;
    }

    return NULL;
}

#endif

#if defined(DMF_USER_MODE)

// Maximum number of iterations for Device Interface List allocation.
// Putting a hard limit to the number of times loop can execute to avoid any possible infinite loop.
//
#define MAXIMUM_LOOP_RETRIES 5

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_DeviceInterfaceStringGet(
    _In_ DMFMODULE DmfModule,
    _In_ CONST GUID* InterfaceGuid,
    _In_ int DeviceLinkIndex,
    _Out_ WDFSTRING* DeviceInterface
    )
/*++

Routine Description:

    Retrieve Device Interface symbolic link for UMDF.
    Allocates WDFSTRING object for the result. Must be deleted by caller.

Arguments:

    DmfModule - This Module's handle.
    InterfaceGuid - Device interface GUID.
    DeviceLinkIndex - Instance index for Device Interface symbolic link's list.
    DeviceInterface - Device interface symbolic link.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    CONFIGRET configRet;
    ULONG deviceInterfaceListLength;
    WDFMEMORY deviceInterfaceListObject;
    PWSTR deviceInterfaceList;
    PWSTR deviceLink;
    ULONG retries;
    size_t listSize;
    UNICODE_STRING temporaryDeviceLink;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    ntStatus = STATUS_UNSUCCESSFUL;
    deviceInterfaceListObject = NULL;
    deviceInterfaceListLength = 0;
    deviceInterfaceList = NULL;
    deviceLink = NULL;
    retries = 0;

    do
    {
        // Get the size of the list of installed devices.
        //
        configRet = CM_Get_Device_Interface_List_SizeW(&deviceInterfaceListLength,
                                                       (LPGUID)InterfaceGuid,
                                                       nullptr,
                                                       CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
        if (configRet == CR_SUCCESS)
        {
            if (deviceInterfaceListObject)
            {
                WdfObjectDelete(deviceInterfaceListObject);
                deviceInterfaceList = NULL;
            }

            // Allocate buffer for the list
            //
            listSize = sizeof(WCHAR) * (deviceInterfaceListLength);
            WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
            objectAttributes.ParentObject = DmfModule;
            ntStatus = WdfMemoryCreate(&objectAttributes,
                                       PagedPool,
                                       MemoryTag,
                                       listSize,
                                       &deviceInterfaceListObject,
                                       (VOID**)&deviceInterfaceList);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
            else
            {
                // Get the the list of devices installed for Device interface GUID.
                // Used CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES to retrieve disabled devices as well.
                //
                configRet = CM_Get_Device_Interface_ListW((LPGUID) InterfaceGuid,
                                                          nullptr,
                                                          deviceInterfaceList,
                                                          deviceInterfaceListLength,
                                                          CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
            }
        }
    }
    // it's possible for the interface list size to change between querying the size and getting the result.
    // So it's recommended to have this code in a while loop with a few iterations
    //
    while ((configRet == CR_BUFFER_SMALL) && (++retries <= MAXIMUM_LOOP_RETRIES));

    // CM_Get_Device_Interface_List failed.
    //
    if (configRet != CR_SUCCESS)
    {
        ntStatus = STATUS_RANGE_NOT_FOUND;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "CM_Get_Device_Interface_ListW() fails: configRet=0x%x", configRet);
        goto Exit;
    }
    // List is empty.
    //
    if (deviceInterfaceList[0] == L'\0')
    {
        ntStatus = STATUS_RANGE_NOT_FOUND;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NO Device link FOUND");
        goto Exit;
    }

    deviceLink = Registry_DeviceLinkGet(deviceInterfaceList,
                                        DeviceLinkIndex);
    if (deviceLink == NULL)
    {
        ntStatus = STATUS_RANGE_NOT_FOUND;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NO Device link FOUND");
        goto Exit;
    }
    if (deviceLink[0] == L'\0')
    {
        ntStatus = STATUS_RANGE_NOT_FOUND;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NO Device link FOUND");
        goto Exit;
    }

    // Assign temporary UNICODE_STRING to initialize WDFSTRING.
    //
    RtlInitUnicodeString(&temporaryDeviceLink,
                         deviceLink);

    // Allocate WDFSTRING and init it with device link.
    // WDFSTRING must be freed by the caller.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfStringCreate(&temporaryDeviceLink,
                               &objectAttributes,
                               DeviceInterface);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfStringCreate fails ntStatus=%!STATUS!", ntStatus);
    }

Exit:

    if (deviceInterfaceListObject)
    {
        WdfObjectDelete(deviceInterfaceListObject);
    }

    return ntStatus;
}

#elif defined(DMF_KERNEL_MODE)

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_DeviceInterfaceStringGet(
    _In_ DMFMODULE DmfModule,
    _In_ CONST GUID* InterfaceGuid,
    _In_ int DeviceLinkIndex,
    _Out_ WDFSTRING* DeviceInterface
    )
/*++

Routine Description:

    Retrieve Device Interface symbolic link for KMDF.
    Allocates WDFSTRING object for the result. Must be deleted by caller.

Arguments:

    DmfModule - This Module's handle.
    InterfaceGuid - Device interface GUID.
    DeviceLinkIndex - Instance index for Device Interface symbolic link's list.
    DeviceInterface - Device interface symbolic link.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PWSTR deviceInterfaces;
    PWSTR deviceLink;
    UNICODE_STRING temporaryDeviceLink;
    WDF_OBJECT_ATTRIBUTES objectAttributes;

    ntStatus = STATUS_UNSUCCESSFUL;
    deviceInterfaces = NULL;
    deviceLink = NULL;

    // Get the the list of device interface instances.
    //
    ntStatus = IoGetDeviceInterfaces(InterfaceGuid,
                                     NULL,
                                     DEVICE_INTERFACE_INCLUDE_NONACTIVE,
                                     &deviceInterfaces);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "IoGetDeviceInterfaces fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    if (deviceInterfaces == NULL)
    {
        ntStatus = STATUS_RANGE_NOT_FOUND;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NO INTERFACE FOUND");
        goto Exit;
    }
    if (deviceInterfaces[0] == L'\0')
    {
        ntStatus = STATUS_RANGE_NOT_FOUND;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NO INTERFACE FOUND");
        goto Exit;
    }

    deviceLink = Registry_DeviceLinkGet(deviceInterfaces,
                                        DeviceLinkIndex);
    if (deviceLink == NULL)
    {
        ntStatus = STATUS_RANGE_NOT_FOUND;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NO Device link FOUND for index = %-d", DeviceLinkIndex);
        goto Exit;
    }
    if (deviceLink[0] == L'\0')
    {
        ntStatus = STATUS_RANGE_NOT_FOUND;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NO Device link FOUND for index = %-d", DeviceLinkIndex);
        goto Exit;
    }

    // Assign UNICODE_STRING to initialize WDFSTRING.
    //
    RtlInitUnicodeString(&temporaryDeviceLink,
                         deviceLink);

    // Allocate WDFSTRING and init it with device link.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfStringCreate(&temporaryDeviceLink,
                               &objectAttributes,
                               DeviceInterface);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfStringCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:
    // Free buffer allocated by IoGetDeviceInterfaces.
    // WDFSTRING must be deleted by the caller.
    //
    if (deviceInterfaces)
    {
        ExFreePool(deviceInterfaces);
    }

    return ntStatus;
}

#endif

#if defined(DMF_USER_MODE)

// From the notes for ZwQueryKey function (wdm.h):
// If the call to this function occurs in user mode, you should use the name "NtQueryKey" instead of "ZwQueryKey".
// Nt functions could be used in UMDF instead of corresponding Nt kernel functions.
// Using Nt and Zw Versions of the Native System Services Routines.
// https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/using-nt-and-zw-versions-of-the-native-system-services-routines
//

typedef DWORD(__stdcall* NtQueryKeyType)(HANDLE  KeyHandle,
                                         int KeyInformationClass,
                                         VOID*  KeyInformation,
                                         ULONG  Length,
                                         ULONG*  ResultLength);
typedef DWORD(__stdcall* NtCloseType)(HANDLE  KeyHandle);

#define KEY_INFORMATION_CLASS_KeyNameInformation 3
#define REGISTRY_MACHINE_TEXT L"\\Registry\\Machine\\"

// KEY_NAME_INFORMATION declaration copied from ntddk.h.
//

typedef struct _KEY_NAME_INFORMATION
{
    ULONG   NameLength;
    WCHAR   Name[1];            // Variable length string
} KEY_NAME_INFORMATION, * PKEY_NAME_INFORMATION;

_IRQL_requires_max_(PASSIVE_LEVEL)
void
Registry_HKEYClose(
    _In_ HANDLE Key
    )
/*++

Routine Description:

    Wraper for NtClose function from ntdll.dll for UMDF.

Arguments:

    Key - Opened handle.

Return Value:

    None

--*/
{
    HMODULE dll;
    NtCloseType function;
    NTSTATUS ntStatus;

    DmfAssert(Key != NULL);

    dll = LoadLibrary(L"ntdll.dll");
    if (dll != NULL)
    {
        function = reinterpret_cast<NtCloseType>(::GetProcAddress(dll,
                                                                  "NtClose"));
        if (function != NULL)
        {
            ntStatus = function(Key);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "NtClose fails: ntStatus=%!STATUS!", ntStatus);
            }
        }
        FreeLibrary(dll);
    }
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_RegistryPathFromHandle(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Key,
    _Out_ WDFMEMORY* RegistryPathObject
    )
/*++

Routine Description:

    Retrieve absolute registry path from registry HEY handle for UMDF.
    Used NtQueryKey function from ntdll.dll.

Arguments:

    DmfModule - This Module's handle.
    Key - Opened registry handle.
    RegistryPathObject - Pointer to WDF Memory object to return registry path. Must be freed by caller.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WCHAR* registryPath;
    ULONG size;
    size_t bufferSize;
    size_t nameLength;
    size_t textLength;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFMEMORY nameInformationObject;
    KEY_NAME_INFORMATION* nameInformation;
    WCHAR* actualRegistryPath;
    HMODULE dll;
    NtQueryKeyType function;

    nameInformationObject = NULL;
    nameInformation = NULL;
    ntStatus = STATUS_UNSUCCESSFUL;
    registryPath = NULL;
    actualRegistryPath = NULL;
    size = 0;

    DmfAssert(Key != NULL);

    // Load ntdll.dll.
    //
    dll = LoadLibrary(L"ntdll.dll");
    if (dll != NULL)
    {
        // Retrive NtQueryKey function address.
        //
        function = reinterpret_cast<NtQueryKeyType>(::GetProcAddress(dll,
                                                                     "NtQueryKey"));
        if (function != NULL)
        {
            // Query buffer size required for registry path.
            //
            ntStatus = function(Key,
                                KEY_INFORMATION_CLASS_KeyNameInformation,
                                0,
                                0,
                                &size);
            if (ntStatus == STATUS_BUFFER_TOO_SMALL)
            {
                size = size + sizeof(WCHAR);

                // Allocate memory buffer for registry path.
                // The memory will be freed by caller.
                //
                WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
                objectAttributes.ParentObject = DmfModule;
                ntStatus = WdfMemoryCreate(&objectAttributes,
                                           PagedPool,
                                           MemoryTag,
                                           size,
                                           &nameInformationObject,
                                           (VOID**)&nameInformation);
                if (NT_SUCCESS(ntStatus))
                {
                    // Query registry path.
                    //
                    ntStatus = function(Key,
                                        KEY_INFORMATION_CLASS_KeyNameInformation,
                                        (VOID*)nameInformation,
                                        size,
                                        &size);
                }
                else
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
                }
            }
        }

        // Unload ntdll.dll
        //
        FreeLibrary(dll);

        if (NT_SUCCESS(ntStatus))
        {
            // Remove "\\Registry\\Machine\\" leading text from the registry path for UMDF
            // From https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/wdfregistry/nf-wdfregistry-wdfregistryopenkey
            // The string format specified in the KeyName parameter depends on whether the caller is a KMDF driver or a UMDF driver
            // For Kernel use path
            // L"\\Registry\\Machine\\System\\CurrentControlSet\\Control "
            // For User mode use path:
            // L"System\\CurrentControlSet\\Control\\"
            //
            textLength = wcslen(REGISTRY_MACHINE_TEXT);
            nameLength = nameInformation->NameLength/sizeof(WCHAR);
            actualRegistryPath = nameInformation->Name;

            // Check is found registry path is longer then expected "System\\CurrentControlSet\\Control\\" prefix
            //
            if (textLength < nameLength)
            {
                if (0 == _wcsnicmp(REGISTRY_MACHINE_TEXT,
                                   nameInformation->Name,
                                   textLength))
                {
                    nameLength -= textLength;
                    actualRegistryPath += textLength;
                }
            }

            bufferSize = (nameLength + 1) * sizeof(WCHAR);

            // Allocate memory buffer for registry path.
            //
            WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
            objectAttributes.ParentObject = DmfModule;
            ntStatus = WdfMemoryCreate(&objectAttributes,
                                       PagedPool,
                                       MemoryTag,
                                       bufferSize,
                                       RegistryPathObject,
                                       (VOID**)&registryPath);
            if (!NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
            else
            {
                // Copy registry path buffer to string.
                //
                wcsncpy_s(registryPath,
                          nameLength + 1,
                          actualRegistryPath,
                          nameLength);
            }
        }
    }

Exit:

    return ntStatus;
}

#elif defined(DMF_KERNEL_MODE)

_IRQL_requires_max_(PASSIVE_LEVEL)
void
Registry_HKEYClose(
    _In_ HANDLE Key
    )
/*++

Routine Description:

    Wraper for ZwClose function for KMDF.

Arguments:

    Key - Opened handle.

Return Value:

    None

--*/
{
    ZwClose(Key);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_RegistryPathFromHandle(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Key,
    _Out_ WDFMEMORY* RegistryPathObject
    )
/*++

Routine Description:

    Retrieve absolute registry path from registry HEY handle for KMDF.

Arguments:

    DmfModule - This Module's handle.
    Key - Opened registry handle.
    RegistryPathObject - Pointer to WDF Memory object to return registry path. Must be freed by caller.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG size;
    size_t bufferSize;
    WCHAR* registryPath;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    WDFMEMORY nameInformationObject;
    KEY_NAME_INFORMATION* nameInformation;

    ntStatus = STATUS_UNSUCCESSFUL;
    size = 0;
    nameInformationObject = NULL;
    nameInformation = NULL;

    // Query buffer size required for registry path.
    //
    ntStatus = ZwQueryKey(Key,
                          KeyNameInformation,
                          NULL,
                          0,
                          &size);
    if (STATUS_BUFFER_TOO_SMALL != ntStatus)
    {
        goto Exit;
    }

    // Allocate KEY_NAME_INFORMATION structure with required size buffer for registry path.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               PagedPool,
                               MemoryTag,
                               size,
                               &nameInformationObject,
                               (VOID**)&nameInformation);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = ZwQueryKey(Key,
                          KeyNameInformation,
                          nameInformation,
                          size,
                          &size);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Allocate memory buffer for registry path.
    //
    bufferSize = nameInformation->NameLength + sizeof(WCHAR);
    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
    objectAttributes.ParentObject = DmfModule;
    ntStatus = WdfMemoryCreate(&objectAttributes,
                               PagedPool,
                               MemoryTag,
                               bufferSize,
                               RegistryPathObject,
                               (VOID**)&registryPath);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Set trailing zero for registry path string.
    //
    registryPath[nameInformation->NameLength / sizeof(WCHAR)] = L'\0';

    // Copy registry path buffer to string.
    //
    RtlCopyMemory(registryPath,
                  nameInformation->Name,
                  nameInformation->NameLength);

Exit:
    // Free WDFMEMORY object created for KEY_NAME_INFORMATION structure.
    //
    if (nameInformationObject)
    {
        WdfObjectDelete(nameInformationObject);
    }

    return ntStatus;
}
#endif

//-----------------------------------------------------------------------------------------------------
// Registry Write
//-----------------------------------------------------------------------------------------------------
//

#if !defined(DMF_USER_MODE)
static
NTSTATUS
Registry_ValueSizeGet(
    _In_ Registry_Entry* Entry,
    _Out_ ULONG* ValueSize
    )
/*++

Routine Description:

    Determine the size of the value data to be written.

Arguments:

    Entry - Contains information about the value that is written.
    ValueSize - The size of the data type corresponding to the Value in Entry.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    size_t valueSize;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;

    DmfAssert(Entry != NULL);
    DmfAssert((Entry->ValueType == REG_SZ) ||
              (Entry->ValueType == REG_DWORD) ||
              (Entry->ValueType == REG_QWORD) ||
              (Entry->ValueType == REG_BINARY) ||
              (Entry->ValueType == REG_MULTI_SZ));

    switch (Entry->ValueType)
    {
        case REG_DWORD:
        {
            *ValueSize = sizeof(DWORD);
            break;
        }
        case REG_QWORD:
        {
            *ValueSize = sizeof(ULONGLONG);
            break;
        }
        case REG_SZ:
        {
            ntStatus = RtlStringCchLengthW((STRSAFE_PCNZWCH)Entry->ValueData,
                                           NTSTRSAFE_MAX_CCH,
                                           &valueSize);
            if (! NT_SUCCESS(ntStatus))
            {
                DmfAssert(FALSE);
                goto Exit;
            }
            // The above function returns the length in characters. The function
            // that writes the values requires the full size of the buffer.
            //
            *ValueSize = (ULONG)valueSize * sizeof(WCHAR);
            break;
        }
        case REG_MULTI_SZ:
        {
            PWCHAR current;
            ULONG count;
            BOOLEAN done;

            DmfAssert(Entry->ValueName != NULL);
            current = (PWCHAR)Entry->ValueData;
            count = 0;
            done = FALSE;
            while (! done)
            {
                // Loop through all the characters of the current string in the
                // set of strings in the buffer.
                //
                while (*current != L'\0')
                {
                    current++;
                    count++;
                }
                // First trailing zero is counted as part of the string.
                //
                current++;
                count++;
                // This is the check for the second consecutive \0.
                //
                if (L'\0' == *current)
                {
                    // Second consecutive \0 is found.
                    //
                    done = TRUE;
                    break;
                }
                // Second consecutive \0 is not found.
                //
            }
            // Add one WCHAR for the second /0.
            //
            count += 1;
            *ValueSize = count * sizeof(WCHAR);
            break;
        }
        case REG_BINARY:
        {
            DmfAssert(Entry->ValueSize != 0);
            *ValueSize = Entry->ValueSize;
            break;
        }
        default:
        {
            *ValueSize = 0;
            DmfAssert(FALSE);
            goto Exit;
        }
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
Registry_EntryWrite(
    _In_ PWCHAR FullPathName,
    _In_ Registry_Entry* Entry
    )
/*++

Routine Description:

    Write a single registry entry.

Arguments:

    FullPathName - This is the whole name of the path where the entry will be written.
    Entry - Contains information about the value that is written.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG valueSize;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(FullPathName != NULL);
    DmfAssert(Entry != NULL);
    DmfAssert(Entry->ValueName != NULL);
    DmfAssert((Entry->ValueType == REG_SZ) ||
              (Entry->ValueType == REG_DWORD) ||
              (Entry->ValueType == REG_QWORD) ||
              (Entry->ValueType == REG_BINARY) ||
              (Entry->ValueType == REG_MULTI_SZ));

    // Get the size of the value to be written. It depends on the kind
    // of value it is.
    //
    ntStatus = Registry_ValueSizeGet(Entry,
                                     &valueSize);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Invalid code path Registry_ValueSizeGet");
        DmfAssert(FALSE);
        goto Exit;
    }

    switch (Entry->ValueType)
    {
        case REG_DWORD:
        {
            // Write the registry entry. The data field is a DWORD so convert it to DWORD.
            //
            DWORD dword;

            dword = (DWORD)(ULONGLONG)Entry->ValueData;
            ntStatus = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
                                             FullPathName,
                                             Entry->ValueName,
                                             Entry->ValueType,
                                             &dword,
                                             valueSize);
            break;
        }
        case REG_QWORD:
        {
            ULONGLONG qword;

            // Write the registry entry. The data field is a QWORD so convert it to QWORD.
            //
            qword = (ULONGLONG)Entry->ValueData;
            ntStatus = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
                                             FullPathName,
                                             Entry->ValueName,
                                             Entry->ValueType,
                                             &qword,
                                             valueSize);
            break;
        }
        default:
        {
            // Write the registry entry. The data field is an address of the data to write.
            //
            ntStatus = RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
                                             FullPathName,
                                             Entry->ValueName,
                                             Entry->ValueType,
                                             Entry->ValueData,
                                             valueSize);
            break;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
Registry_RecursivePathCreate(
    _Inout_ PWCHAR RegistryPath
    )
/*++

Routine Description:

    Checks if the given registry path exists. If not, removes one key and tries again.
    Continues recursively until all keys of the path are created. In the event of a failure,
    RegistryPath is left modified as the minimal path that failed.

Arguments:

    RegistryPath - The path to test/create.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    size_t mainRegistryPathNameLength;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = RtlCheckRegistryKey(RTL_REGISTRY_ABSOLUTE,
                                   RegistryPath);
    if (! NT_SUCCESS(ntStatus))
    {
        // Remove one key from the path and try again. If that succeeds, create the key.
        //
        ntStatus = RtlStringCchLengthW(RegistryPath,
                                       NTSTRSAFE_MAX_CCH,
                                       &mainRegistryPathNameLength);
        if (NT_SUCCESS(ntStatus))
        {
            if (mainRegistryPathNameLength > 0)
            {
                // Counting backwards from the end of the string. Skip the null.
                //
                mainRegistryPathNameLength--;

                // Here the root of the path will be \Registry\Machine. 17 characters. 
                //
                while (mainRegistryPathNameLength > REGISTRY_ROOT_LENGTH && RegistryPath[mainRegistryPathNameLength] != '\\')
                {
                    mainRegistryPathNameLength--;
                }
                if (mainRegistryPathNameLength > REGISTRY_ROOT_LENGTH && RegistryPath[mainRegistryPathNameLength] == '\\')
                {
                    RegistryPath[mainRegistryPathNameLength] = '\0';
                    ntStatus = Registry_RecursivePathCreate(RegistryPath);
                    if (NT_SUCCESS(ntStatus))
                    {
                        // Restore the key and try to create it.
                        //
                        RegistryPath[mainRegistryPathNameLength] = '\\';
                        ntStatus = RtlCreateRegistryKey(RTL_REGISTRY_ABSOLUTE,
                                                        RegistryPath);
                    }
                }
                else
                {
                    // A poorly formatted registry path, or
                    // a misspelled hive, or
                    // the path does not start with \Registry\Machine, or
                    // registry at the specified path is not ready yet.
                    //
                    ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;
                }
            }
            else
            {
                // A poorly formatted registry path.
                //
                DmfAssert(FALSE);
                ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;
            }
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
Registry_BranchWrite(
    _In_ PWCHAR RegistryPath,
    _In_ Registry_Branch* Branches,
    _In_ ULONG NumberOfBranches
    )
/*++

Routine Description:

    Writes an array of registry branches to the registry.

Arguments:

    Branches - The array of registry branches.
    NumberOfBranches - The number of entries in the array.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG branchIndex;
    PWCHAR fullPathName;
    size_t mainRegistryPathNameLength;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;
    fullPathName = NULL;

    // Get the length of main registry path.
    //
    ntStatus = RtlStringCchLengthW(RegistryPath,
                                   NTSTRSAFE_MAX_CCH,
                                   &mainRegistryPathNameLength);
    if (! NT_SUCCESS(ntStatus))
    {
        DmfAssert(FALSE);
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlStringCchLengthW fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // For each branch, create full path that consists of the main path plus an option branch path.
    // Then, write all the values for that branch.
    //
    for (branchIndex = 0; branchIndex < NumberOfBranches; branchIndex++)
    {
        Registry_Branch* branch;
        size_t prefixPathNameLength;
        size_t fullPathNameLength;
        size_t fullPathNameSize;

        branch = &Branches[branchIndex];

        // Get the name of the prefix to append to all Value names.
        //
        ntStatus = RtlStringCchLengthW(branch->BranchValueNamePrefix,
                                       NTSTRSAFE_MAX_CCH,
                                       &prefixPathNameLength);
        if (! NT_SUCCESS(ntStatus))
        {
            DmfAssert(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlStringCchLengthW fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Calculate the full length of the path name.
        // NOTE: Trailing '/' are in the strings.
        // NOTE: Add one character from the zero-termination.
        //
        fullPathNameLength = mainRegistryPathNameLength + prefixPathNameLength + 1;

        // Calculate the size of the buffer needed for that name.
        //
        fullPathNameSize = fullPathNameLength * sizeof(WCHAR);

        // Allocate a buffer for the full path name.
        //
        fullPathName = (PWCHAR)ExAllocatePoolWithTag(PagedPool,
                                                     fullPathNameSize,
                                                     MemoryTag);
        if (NULL == fullPathName)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ExAllocatePoolWithTag fails");
            goto Exit;
        }

        // Copy the main path into the buffer.
        //
        ntStatus = RtlStringCchCopyW(fullPathName,
                                     fullPathNameSize,
                                     RegistryPath);
        if (! NT_SUCCESS(ntStatus))
        {
            DmfAssert(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlStringCchCopyW fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Copy the prefix into the full path name buffer.
        // NOTE: The prefix must have a '\' at front if there are any characters.
        //
        DmfAssert((L'\0' == branch->BranchValueNamePrefix[0]) ||
                  (L'\\' == branch->BranchValueNamePrefix[0]));
        ntStatus = RtlStringCchCatW(fullPathName,
                                    fullPathNameSize,
                                    branch->BranchValueNamePrefix);
        if (! NT_SUCCESS(ntStatus))
        {
            DmfAssert(FALSE);
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlStringCchCatW fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // Check that the RegistryPath exists in the registry and create it if it does not.
        //
        ntStatus = Registry_RecursivePathCreate(fullPathName);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Registry_RecursivePathCreate fails: RegistryPath=%S ntStatus=%!STATUS!", fullPathName, ntStatus);
            goto Exit;
        }

        for (ULONG entryIndex = 0; entryIndex < branch->ItemCount; entryIndex++)
        {
            Registry_Entry* entry;

            entry = &branch->RegistryTableEntries[entryIndex];

            // Write the value at the full path name.
            //
            ntStatus = Registry_EntryWrite(fullPathName,
                                           entry);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Registry_EntryWrite fails: ntStatus=%!STATUS!", ntStatus);
                goto Exit;
            }
        }

        // Free the buffer allocated above for the next iteration in the loop.
        //
        ExFreePoolWithTag(fullPathName,
                          MemoryTag);
        fullPathName = NULL;
    }

Exit:

    if (fullPathName != NULL)
    {
        ExFreePoolWithTag(fullPathName,
                          MemoryTag);
        fullPathName = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

static
NTSTATUS
Registry_TreeWrite(
    _In_ DMFMODULE DmfModule,
    _In_ Registry_Tree* Tree,
    _In_ ULONG NumberOfTrees
    )
/*++

Routine Description:

    Writes an array of registry trees to the registry.

Arguments:

    Tree - The array of registry trees.
    NumberOfTrees - The number of entries in the array.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG treeIndex;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;

    DmfAssert(NumberOfTrees > 0);

    for (treeIndex = 0; treeIndex < NumberOfTrees; treeIndex++)
    {
        Registry_Tree* tree;

        tree = &Tree[treeIndex];
        ntStatus = Registry_BranchWrite(tree->RegistryPath,
                                        tree->Branches,
                                        tree->NumberOfBranches);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Registry_BranchWrite fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#endif

//-----------------------------------------------------------------------------------------------------
// Registry Enumeration
//-----------------------------------------------------------------------------------------------------
//

HANDLE
Registry_HandleOpenByName(
    _In_ PWCHAR Name
    )
/*++

Routine Description:

    Open a registry key by path name.

Arguments:

    Name - Path name of the key relative to handle.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    HANDLE handle;
    NTSTATUS ntStatus;
    UNICODE_STRING nameString;
    WDFKEY key;
    ACCESS_MASK accessMask;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    RtlInitUnicodeString(&nameString,
                         Name);

    accessMask = GENERIC_ALL;
#if defined(DMF_USER_MODE)
    // For User-mode only read access works.
    //
    accessMask = KEY_READ;
#endif

    key = NULL;

    ntStatus = WdfRegistryOpenKey(NULL,
                                  &nameString,
                                  accessMask,
                                  WDF_NO_OBJECT_ATTRIBUTES, 
                                  &key);
    if (! NT_SUCCESS(ntStatus))
    {
        handle = NULL;
        goto Exit;
    }

    handle = (HANDLE)key;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return handle;
}

NTSTATUS
Registry_HandleOpenByPredefinedKey(
    _In_ WDFDEVICE Device,
    _In_ ULONG PredefinedKeyId,
    _In_ ULONG AccessMask,
    _Out_ HANDLE* RegistryHandle
    )
/*++

Routine Description:

    Open a registry key of the device.

Arguments:

    Device - Handle to Device object.
    AccessMask - The access mask to pass.
    RegistryHandle - Handle to open registry key or NULL in case of error.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFKEY key;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Open the device registry key of the instance of the device.
    //
    key = NULL;
    ntStatus = WdfDeviceOpenRegistryKey(Device,
                                        PredefinedKeyId,
                                        AccessMask,
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        &key);
    if (! NT_SUCCESS(ntStatus))
    {
        *RegistryHandle = NULL;
        goto Exit;
    }

    *RegistryHandle = (HANDLE) key;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

NTSTATUS
Registry_HandleOpenByNameEx(
    _In_ PWCHAR Name,
    _In_ ULONG AccessMask,
    _In_ BOOLEAN Create,
    _Out_ HANDLE* RegistryHandle
    )
/*++

Routine Description:

    Open a registry key by path name and access mask.

Arguments:

    Name - Path name of the key relative to handle.
    AccessMask - The access mask to pass.
    Create - Creates the key if it cannot be opened.
    RegistryHandle - Handle to open registry key or NULL in case of error.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    UNICODE_STRING nameString;
    WDFKEY key;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    RtlInitUnicodeString(&nameString,
                         Name);

    key = NULL;
    if (Create)
    {

#if defined(DMF_USER_MODE)
        // User-mode driver cannot create subkey. 
        // If the user tries to create a key, try opening instead.
        //
        ntStatus = WdfRegistryOpenKey(NULL,
                                      &nameString,
                                      AccessMask,
                                      WDF_NO_OBJECT_ATTRIBUTES, 
                                      &key);

        // If the key doesn't exist, access denied is returned.
        //
        if (STATUS_ACCESS_DENIED == ntStatus)
        {
            DmfAssert(FALSE);
            ntStatus = STATUS_NOT_SUPPORTED;
        }
#else
        // Open existing or create new.
        //
        ntStatus = WdfRegistryCreateKey(NULL,
                                        &nameString,
                                        AccessMask,
                                        REG_OPTION_NON_VOLATILE,
                                        NULL,
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        &key);
#endif
    }
    else
    {
        // Open existing.
        //
        ntStatus = WdfRegistryOpenKey(NULL,
                                      &nameString,
                                      AccessMask,
                                      WDF_NO_OBJECT_ATTRIBUTES, 
                                      &key);
    }
    if (! NT_SUCCESS(ntStatus))
    {
        *RegistryHandle = NULL;
        goto Exit;
    }

    *RegistryHandle = (HANDLE)key;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

HANDLE
Registry_HandleOpenByHandle(
    _In_ HANDLE Handle,
    _In_ PWCHAR Name,
    _In_ BOOLEAN TryToCreate
    )
/*++

Routine Description:

    Given a registry handle, open a handle relative to that handle.

Arguments:

    Handle - Handle to open registry key.
    Name - Path name of the key relative to handle.
    TryToCreate - Indicates if the function should call create instead of open.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    HANDLE handle;
    WDFKEY key;
    NTSTATUS ntStatus;
    UNICODE_STRING nameString;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    RtlInitUnicodeString(&nameString,
                         Name);

    key = NULL;
    if (TryToCreate)
    {
#if defined(DMF_USER_MODE)
        // User-mode driver cannot create subkey. 
        // If the user tries to create a key, try opening instead.
        //
        ntStatus = WdfRegistryOpenKey((WDFKEY)Handle,
                                      &nameString,
                                      KEY_READ | KEY_SET_VALUE,
                                      WDF_NO_OBJECT_ATTRIBUTES, 
                                      &key);

        // if the key doesnt exist, we get Access denied error.
        //
        if (STATUS_ACCESS_DENIED == ntStatus)
        {
            DmfAssert(FALSE);
            ntStatus = STATUS_NOT_SUPPORTED;
        }
#else
        // Try to create/open.
        //
        ntStatus = WdfRegistryCreateKey((WDFKEY)Handle,
                                        &nameString,
                                        KEY_ALL_ACCESS,
                                        REG_OPTION_NON_VOLATILE,
                                        NULL,
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        &key);
#endif
    }
    else
    {
        ACCESS_MASK accessMask = KEY_ALL_ACCESS;
#if defined(DMF_USER_MODE)
         // For User-mode only read access works.
         //
         accessMask = KEY_READ;
#endif

        // Try to open.
        //
        ntStatus = WdfRegistryOpenKey((WDFKEY)Handle,
                                      &nameString,
                                      accessMask,
                                      WDF_NO_OBJECT_ATTRIBUTES, 
                                      &key);
    }

    if (! NT_SUCCESS(ntStatus))
    {
        handle = NULL;
        goto Exit;
    }

    handle = (HANDLE)key;

Exit:

    FuncExit(DMF_TRACE, "handle=0x%p", handle);

    return handle;
}

void
Registry_HandleClose(
    _In_ HANDLE Handle
    )
/*++

Routine Description:

    Given a registry handle, close the handle.

Arguments:

    Handle - handle to open registry key.

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    WdfRegistryClose((WDFKEY)Handle);
    Handle = NULL;

    FuncExitVoid(DMF_TRACE);
}

#if !defined(DMF_USER_MODE)
BOOLEAN
Registry_SubKeysFromHandleEnumerate(
    _In_ HANDLE Handle,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* RegistryEnumerationFunction,
    _In_ VOID* Context
    )
/*++

Routine Description:

    Given a registry handle, enumerate all the sub keys and call an enumeration function for each of them.

Arguments:

    Handle - The handle the registry key.
    RegistryEnumerationFunction - The enumeration function to call for each sub key.
    Context - The client context to pass into the enumeration function.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    BOOLEAN returnValue;
    NTSTATUS ntStatus;
    ULONG resultLength;
    ULONG currentSubKeyIndex;
    BOOLEAN done;
    HANDLE handleWdm;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    returnValue = FALSE;
    done = FALSE;
    currentSubKeyIndex = 0;

    // Grab the WDM Handle. Handle that is coming in is WDFKEY.
    //
    handleWdm = WdfRegistryWdmGetHandle((WDFKEY)Handle);

    while (! done)
    {
        // If there is a key to enumerate, since the function is passed NULL and 0 as buffer and buffer
        // size the return value will indicate "buffer too small" and the resultLenght will be the 
        // amount of memory needed to read it.
        //
        resultLength = 0;
        ntStatus = ZwEnumerateKey(handleWdm,
                                  currentSubKeyIndex,
                                  KeyBasicInformation,
                                  NULL,
                                  0,
                                  &resultLength);
        if (! NT_SUCCESS(ntStatus))
        {
            // This is the expected result because the driver needs to know the length.
            //
            if ((ntStatus == STATUS_BUFFER_OVERFLOW) ||
                (ntStatus == STATUS_BUFFER_TOO_SMALL))
            {
                PKEY_BASIC_INFORMATION keyInformationBuffer;
                ULONG keyInformationBufferSize;

                // This driver needs to zero terminate the name that is returned. So, add 1 to the length
                // to the size needed to allocate.
                //
                #pragma warning(suppress: 6102)
                resultLength += sizeof(WCHAR);

                // Allocate a buffer for the path name.
                //
                keyInformationBuffer = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(PagedPool,
                                                                                     resultLength,
                                                                                     MemoryTag);
                if (NULL == keyInformationBuffer)
                {
                    returnValue = FALSE;
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ExAllocatePoolWithTag fails");
                    goto Exit;
                }

                // It is the size of the buffer passed including space for final zero.
                //
                keyInformationBufferSize = resultLength;

                // Enumerate the next key.
                //
                ntStatus = ZwEnumerateKey(handleWdm,
                                          currentSubKeyIndex,
                                          KeyBasicInformation,
                                          keyInformationBuffer,
                                          keyInformationBufferSize,
                                          &resultLength);
                if (! NT_SUCCESS(ntStatus))
                {
                    returnValue = FALSE;
                    goto Exit;
                }

                // Zero terminate the name. (It is in the buffer that was just allocated,
                // so it is OK to do so.)
                //
                keyInformationBuffer->Name[keyInformationBuffer->NameLength / sizeof(WCHAR)] = L'\0';

                // Call the client enumeration function.
                // Note: We are passing in the WDFKEY.
                //
                returnValue = RegistryEnumerationFunction(Context,
                                                          Handle,
                                                          keyInformationBuffer->Name);

                // Prepare to get next subkey.
                //
                ExFreePoolWithTag(keyInformationBuffer,
                                  MemoryTag);
                keyInformationBuffer = NULL;
                keyInformationBufferSize = 0;
                currentSubKeyIndex++;

                if (! returnValue)
                {
                    goto Exit;
                }
            }
            else
            {
                // It means there are no more entires to enumerate.
                //
                done = TRUE;
            }
        }
    }

    returnValue = TRUE;

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#else
BOOLEAN
Registry_SubKeysFromHandleEnumerate(
    _In_ HANDLE Handle,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* RegistryEnumerationFunction,
    _In_ VOID* Context
    )
/*++

Routine Description:

    Given a registry handle, enumerate all the sub keys and call an enumeration function for each of them.

Arguments:

    Handle - The handle the registry key.
    RegistryEnumerationFunction - The enumeration function to call for each sub key.
    Context - The client context to pass into the enumeration function.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    BOOLEAN returnValue;
    NTSTATUS ntStatus;
    DWORD  numberOfSubKeys;
    DWORD maximumSubKeyLength;
    HANDLE handle;
    WDFMEMORY subKeyNameMemory;
    size_t maximumBytesRequired;
    DWORD elementCountOfSubKeyName;
    LPTSTR subKeyNameMemoryBuffer;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    returnValue = FALSE;
    numberOfSubKeys = 0;
    maximumSubKeyLength = 0;
    maximumBytesRequired = 0;
    subKeyNameMemory = WDF_NO_HANDLE;
    subKeyNameMemoryBuffer = NULL;

    handle = WdfRegistryWdmGetHandle((WDFKEY) Handle);

    // Get the subkey count and maximum subkey name size.
    //
    ntStatus = RegQueryInfoKey((HKEY)handle,
                               NULL,
                               0,
                               NULL,
                               &numberOfSubKeys,
                               &maximumSubKeyLength,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RegQueryInfoKey fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (numberOfSubKeys == 0)
    {
        returnValue = TRUE;
        goto Exit;
    }

    // Enumerate the subkeys.
    //
    // Create a buffer which is big enough to hold the largest subkey.
    // Account for NULL as well. Note: No overflow check as the registry key length maximum is limited.
    //
    elementCountOfSubKeyName = maximumSubKeyLength + 1;
    maximumBytesRequired = (elementCountOfSubKeyName * sizeof(WCHAR));
    ntStatus = WdfMemoryCreate(WDF_NO_OBJECT_ATTRIBUTES,
                               PagedPool, 
                               MemoryTag, 
                               maximumBytesRequired,
                               &subKeyNameMemory,
                               (PVOID*)&subKeyNameMemoryBuffer);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    for (DWORD keyIndex = 0; keyIndex < numberOfSubKeys; keyIndex++)
    {
        elementCountOfSubKeyName = maximumSubKeyLength + 1;
        ZeroMemory(subKeyNameMemoryBuffer,
                   maximumBytesRequired);

        // Read the subkey.
        //
        ntStatus = RegEnumKeyEx((HKEY)handle,
                                keyIndex,
                                subKeyNameMemoryBuffer,
                                (LPDWORD) &elementCountOfSubKeyName,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RegEnumKeyEx fails: ntStatus=%!STATUS!", ntStatus);
            break;
        }

        // Call the client enumeration function.
        // Note: We are passing in the WDFKEY.
        //
        returnValue = RegistryEnumerationFunction(Context,
                                                  Handle,
                                                  subKeyNameMemoryBuffer);
        if (!returnValue)
        {
            goto Exit;
        }
    }

    returnValue = TRUE;

Exit:

    if (subKeyNameMemory != WDF_NO_HANDLE)
    {
        WdfObjectDelete(subKeyNameMemory);
    }

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}
#endif

// Enumeration Filter Functions. Add more here as needed for external use.
//

_Function_class_(EVT_DMF_Registry_KeyEnumerationCallback)
BOOLEAN
Registry_KeyEnumerationFilterAllSubkeys(
    _In_ VOID* ClientContext,
    _In_ HANDLE Handle,
    _In_ PWCHAR KeyName
    )
{
    RegistryKeyEnumerationContextType* context;
    BOOLEAN returnValue;
    HANDLE subKeyHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    subKeyHandle = Registry_HandleOpenByHandle(Handle,
                                               KeyName,
                                               FALSE);
    if (NULL == subKeyHandle)
    {
        // This is an error because the key was just enumerated. It should still
        // be here.
        //
        returnValue = FALSE;
        TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Registry_HandleOpenByHandle fails");
        goto Exit;
    }

    context = (RegistryKeyEnumerationContextType*)ClientContext;
    returnValue = context->RegistryKeyEnumerationFunction(context->ClientCallbackContext,
                                                          subKeyHandle,
                                                          KeyName);

    Registry_HandleClose(subKeyHandle);
    subKeyHandle = NULL;

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_Function_class_(EVT_DMF_Registry_KeyEnumerationCallback)
BOOLEAN
Registry_KeyEnumerationFilterStrstr(
    _In_ VOID* ClientContext,
    _In_ HANDLE Handle,
    _In_ PWCHAR KeyName
    )
{
    RegistryKeyEnumerationContextType* context;
    PWCHAR lookFor;
    BOOLEAN returnValue;
    HANDLE subKeyHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    returnValue = TRUE;
    context = (RegistryKeyEnumerationContextType*)ClientContext;
    lookFor = (PWCHAR)context->FilterEnumeratorContext;
    if (wcsstr(KeyName,
               lookFor))
    {
        subKeyHandle = Registry_HandleOpenByHandle(Handle,
                                                   KeyName,
                                                   FALSE);
        if (NULL == subKeyHandle)
        {
            // This is an error because the key was just enumerated. It should still
            // be here.
            //
            returnValue = FALSE;
            TraceEvents(TRACE_LEVEL_WARNING, DMF_TRACE, "Registry_HandleOpenByHandle fails");
            goto Exit;
        }

        returnValue = context->RegistryKeyEnumerationFunction(context->ClientCallbackContext,
                                                              subKeyHandle,
                                                              KeyName);

        Registry_HandleClose(subKeyHandle);
        subKeyHandle = NULL;
    }

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_ValueActionIfNeeded(
    _In_ Registry_ActionType ActionType,
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _In_opt_ VOID* ValueDataToWrite,
    _In_ ULONG ValueDataToWriteSize,
    _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
    _In_opt_ VOID* ComparisonCallbackContext,
    _In_ BOOLEAN WriteIfNotFound
    )
/*++

Routine Description:

    Perform an action to a value after calling a client comparison function to determine if that
    action should be taken.

Arguments:

    ActionType - Determines what action to take if client comparison function returns TRUE.
    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueType - The Registry Type of the value.
    ValueDataToWrite - The data to write if the value is not set to one or it does not exist.
    ValueDataToWriteSize - The size of the buffer at ValueDataToWrite
    ComparisonCallback - Caller's comparison function.
    ComparisonCallbackContext - Caller's context sent to comparison. function.
    WriteIfNotFound - Indicates if the value should be written if it does not exist.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDF_OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING valueNameString;
    BOOLEAN needsAction;
    ULONG valueLength;
    ULONG valueLengthQueried;
    ULONG valueType;
    LPTSTR valueMemoryBuffer;
    WDFMEMORY valueMemory;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(ActionType != Registry_ActionTypeInvalid);

    // Indicate if action will be taken...default is no.
    //
    needsAction = FALSE;

    RtlInitUnicodeString(&valueNameString,
                         ValueName);

    // Find out how much memory is needed to retrieve the value if it is there.
    //
    ntStatus = WdfRegistryQueryValue((WDFKEY)Handle,
                                     &valueNameString,
                                     0,
                                     NULL,
                                     &valueLengthQueried,
                                     &valueType);
    if ((STATUS_OBJECT_NAME_NOT_FOUND == ntStatus))
    {
        // The value is not there. Write it.
        //
        if (WriteIfNotFound)
        {
            needsAction = TRUE;
        }
    }
    else if (STATUS_BUFFER_OVERFLOW == ntStatus)
    {
        // We have size in bytes of the value.
        // Suppress (valueLengthQueried may not be initialized.)
        //
        #pragma warning(suppress: 6102)
        DmfAssert(valueLengthQueried > 0);
        #pragma warning(suppress: 6102)

        WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
        valueLength = valueLengthQueried;
        valueMemory = WDF_NO_HANDLE;
        ntStatus = WdfMemoryCreate(&objectAttributes,
                                   NonPagedPoolNx,
                                   MemoryTag,
                                   valueLength,
                                   &valueMemory,
                                   (PVOID*)&valueMemoryBuffer);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfMemoryCreate fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }

        // TODO: Validate the ValueType.
        //

        // Retrieve the setting of the value.
        //
        ntStatus = WdfRegistryQueryValue((WDFKEY)Handle,
                                         &valueNameString,
                                         valueLength,
                                         valueMemoryBuffer,
                                         NULL,
                                         NULL);
        if (! NT_SUCCESS(ntStatus))
        {
            // Fall through to free memory. Let the caller decide what to do.
            // Generally, this code path should not happen.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ZwQueryValueKey fails: ntStatus=%!STATUS!", ntStatus);
        }
        else
        {
            // Call the caller's comparison function.
            //
            if (ComparisonCallback(DmfModule,
                                   ComparisonCallbackContext,
                                   valueMemoryBuffer,
                                   valueLength,
                                   ValueDataToWrite,
                                   ValueDataToWriteSize))
            {
                needsAction = TRUE;
            }
        }

        WdfObjectDelete(valueMemory);
        valueMemory = WDF_NO_HANDLE;
    }
    else
    {
        // Any other status means something is wrong.
        //
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ZwQueryValueKey fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    if (needsAction)
    {
        switch (ActionType)
        {
            case Registry_ActionTypeWrite:
            {
                if (NULL == ValueDataToWrite)
                {
                    DmfAssert(FALSE);
                    ntStatus = STATUS_INVALID_PARAMETER;
                    goto Exit;
                }

                ntStatus = WdfRegistryAssignValue((WDFKEY)Handle,
                                                  &valueNameString,
                                                  ValueType,
                                                  ValueDataToWriteSize,
                                                  (VOID*)ValueDataToWrite);
                if (! NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRegistryAssignValue fails: %ws...ntStatus=%!STATUS!", ValueName, ntStatus);
                }
                break;
            }
            case Registry_ActionTypeDelete:
            {
                ntStatus = WdfRegistryRemoveValue((WDFKEY)Handle,
                                                  &valueNameString);
                if (! NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRegistryRemoveValue fails: %ws...ntStatus=%!STATUS!", ValueName, ntStatus);
                }
                break;
            }
            case Registry_ActionTypeRead:
            case Registry_ActionTypeNone:
            {
                // Action was done in the comparison function.
                //
                if (needsAction)
                {
                    // Comparison function returns success.
                    //
                    ntStatus = STATUS_SUCCESS;
                }
                else
                {
                    // Comparison function returns fail.
                    //
                    ntStatus = STATUS_UNSUCCESSFUL;
                }
                break;
            }
            default:
            {
                DmfAssert(FALSE);
                break;
            }
        }
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// "Returning uninitialized memory '*BytesRead'."
//
#pragma warning(suppress:6101)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
Registry_ValueActionAlways(
    _In_ Registry_ActionType ActionType,
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _Inout_opt_ VOID* ValueDataBuffer,
    _In_ ULONG ValueDataBufferSize,
    _Out_opt_ ULONG* BytesRead
    )
/*++

Routine Description:

    Perform an action to a registry value always. Client does not filter.

Arguments:

    ActionType - Determines what action to take if client comparison function returns TRUE.
    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueType - The Registry Type of the value.
    ValueDataBuffer - The data to write if the value is not set to one or it does not exist.
    ValueDataBufferSize - The size of the buffer at ValueDataToWrite
    BytesRead - Used for read handler to inform caller of needed size.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    UNICODE_STRING valueNameString;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(ActionType != Registry_ActionTypeInvalid);

    RtlInitUnicodeString(&valueNameString,
                         ValueName);
    // For SAL.
    //
    ntStatus = STATUS_UNSUCCESSFUL;

    switch (ActionType)
    {
        case Registry_ActionTypeWrite:
        {
            // Just perform the action now.
            //
            DmfAssert(ValueDataBuffer != NULL);
            DmfAssert(NULL == BytesRead);
            ntStatus = WdfRegistryAssignValue((WDFKEY)Handle,
                                              &valueNameString,
                                              ValueType,
                                              ValueDataBufferSize,
                                              (VOID*)ValueDataBuffer);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfRegistryAssignValue fails: %ws...ntStatus=%!STATUS!", ValueName, ntStatus);
            }
            break;
        }
        case Registry_ActionTypeDelete:
        {
            // Just perform the action now.
            //
            DmfAssert(NULL == BytesRead);
            ntStatus = WdfRegistryRemoveValue((WDFKEY)Handle,
                                              &valueNameString);
            if (! NT_SUCCESS(ntStatus))
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "RtlDeleteRegistryValue fails: %ws...ntStatus=%!STATUS!", ValueName, ntStatus);
            }
            break;
        }
        case Registry_ActionTypeRead:
        {
            // Call the "if needed" code because "always" is just a subset of "if needed".
            // The code to read the value and determine its size is already there. That
            // non-trivial code does not need to be written again.
            // NOTE: The caller can use "if needed" directly also.
            //
            Registry_CustomActionHandler_Read_Context customActionHandlerContextRead;

            DmfAssert(((ValueDataBuffer != NULL) && (ValueDataBufferSize > 0)) ||
                      ((NULL == ValueDataBuffer) && (0 == ValueDataBufferSize) && (BytesRead != NULL)));

            // Give the Custom Action Handler the information it needs.
            //
            RtlZeroMemory(&customActionHandlerContextRead,
                          sizeof(customActionHandlerContextRead));
            customActionHandlerContextRead.Buffer = (UCHAR*)ValueDataBuffer;
            customActionHandlerContextRead.BufferSize = ValueDataBufferSize;
            customActionHandlerContextRead.BytesRead = BytesRead;

            // Call the "if needed" function to do the work.
            // TODO: Validate that ValueType is the value type of the value being read.
            //
            ntStatus = Registry_ValueActionIfNeeded(Registry_ActionTypeRead,
                                                    DmfModule,
                                                    Handle,
                                                    ValueName,
                                                    ValueType,
                                                    ValueDataBuffer,
                                                    ValueDataBufferSize,
                                                    Registry_CustomActionHandler_Read,
                                                    &customActionHandlerContextRead,
                                                    FALSE);
            if (NT_SUCCESS(ntStatus))
            {
                // Override successful NTSTATUS with callback's NTSTATUS in case callback
                // indicates error.
                //
                ntStatus = customActionHandlerContextRead.NtStatus;
            }
            break;
        }
        case Registry_ActionTypeNone:
        {
            // Client has asked for no action to always be taken.
            //
            DmfAssert(FALSE);
            break;
        }
        default:
        {
            DmfAssert(FALSE);
            break;
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

//-----------------------------------------------------------------------------------------------------
// Registry Deferred Operations
//-----------------------------------------------------------------------------------------------------
//
#if !defined(DMF_USER_MODE)

_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
Registry_DeferredOperationTimerStart(
    _In_ WDFTIMER Timer
    )
/*++

Routine Description:

    Starts the deferred operation timer.

Parameters:

    Timer - The timer that will expire causing the deferred routine to run.

Return:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    WdfTimerStart(Timer,
                  WDF_REL_TIMEOUT_IN_MS(Registry_DeferredRegistryWritePollingIntervalMs));

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
Registry_DeferredOperationAdd(
    _In_ DMFMODULE DmfModule,
    _In_ Registry_Tree* RegistryTree,
    _In_ ULONG ItemCount,
    _In_ Registry_DeferredOperationType DeferredOperationType
    )
/*++

Routine Description:

    Adds a deferred operation to the deferred operation list.

Parameters:

    DmfModule - This Module's handle.
    RegistryTree - Array of trees to perform deferred operation on.
    ItemCount - Number of entries in the array.
    DeferredOperationType - The deferred operation to perform.

Return:

    STATUS_SUCCESS if successful or STATUS_INSUFFICIENT_RESOURCES if there is not
    enough memory.

--*/
{
    NTSTATUS ntStatus;
    REGISTRY_DEFERRED_CONTEXT* deferredContext;
    DMF_CONTEXT_Registry* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Allocate space for the deferred operation. If it cannot be allocated an error code
    // is returned and the operation is not deferred.
    //
    deferredContext = (REGISTRY_DEFERRED_CONTEXT*)ExAllocatePoolWithTag(PagedPool,
                                                                        sizeof(REGISTRY_DEFERRED_CONTEXT),
                                                                        MemoryTag);
    if (NULL == deferredContext)
    {
        // Out of memory.
        //
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ExAllocatePoolWithTag fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

    // Populate the deferred operation context.
    //
    RtlZeroMemory(deferredContext,
                  sizeof(REGISTRY_DEFERRED_CONTEXT));
    deferredContext->DeferredOperation = DeferredOperationType;
    deferredContext->RegistryTree = RegistryTree;
    deferredContext->ItemCount = ItemCount;

    // Add the operation to the list of operations.
    //
    DMF_ModuleLock(DmfModule);
    InsertTailList(&moduleContext->ListDeferredOperations,
                   &deferredContext->ListEntry);
    // Since there is a least one entry in the list, start the timer.
    //
    Registry_DeferredOperationTimerStart(moduleContext->Timer);
    DMF_ModuleUnlock(DmfModule);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

EVT_WDF_TIMER Registry_DeferredOperationHandler;

VOID
Registry_DeferredOperationHandler(
    _In_ WDFTIMER WdfTimer
    )
/*++

Routine Description:

Parameters:

    WdfTimer - The timer object that contains the DEVICE_CONTEXT.

Return:

    None

--*/
{
    DMFMODULE dmfModule;
    DMF_CONTEXT_Registry* moduleContext;
    REGISTRY_DEFERRED_CONTEXT* deferredContext;
    PLIST_ENTRY listEntry;
    PLIST_ENTRY nextListEntry;
    BOOLEAN needToRestartTimer;

    // 'The current function is permitted to run at an IRQ level above the maximum permitted for '__PREfastPagedCode' (1). Prior function calls or annotation are inconsistent with use of that function:  The current function may need _IRQL_requires_max_, or it may be that the limit is set by some prior call. Maximum legal IRQL was last set to 2 at line 1649.'
    // NOTE: Timer handler is set to run in PASSIVE_LEVEL.
    //
    #pragma warning(suppress:28118)
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    dmfModule = (DMFMODULE)WdfTimerGetParentObject(WdfTimer);
    DmfAssert(dmfModule != NULL);

    moduleContext = DMF_CONTEXT_GET(dmfModule);

    DMF_ModuleLock(dmfModule);

    // Point to the first entry in the list.
    //
    listEntry = moduleContext->ListDeferredOperations.Flink;
    needToRestartTimer = FALSE;

    // The loop ends when the current list entry points to the list header.
    //
    while (listEntry != &moduleContext->ListDeferredOperations)
    {
        // Get the next entry in the list now before it is removed.
        //
        nextListEntry = listEntry->Flink;

        deferredContext = CONTAINING_RECORD(listEntry,
                                            REGISTRY_DEFERRED_CONTEXT,
                                            ListEntry);
        switch (deferredContext->DeferredOperation)
        {
            case Registry_DeferredOperationWrite:
            {
                NTSTATUS ntStatus;

                DmfAssert(deferredContext->RegistryTree != NULL);
                ntStatus = Registry_TreeWrite(dmfModule,
                                              deferredContext->RegistryTree,
                                              deferredContext->ItemCount);
                if (STATUS_OBJECT_NAME_NOT_FOUND == ntStatus)
                {
                    // Leave it in the list because driver needs to try again.
                    //
                    TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "STATUS_OBJECT_NAME_NOT_FOUND...try again");
                    needToRestartTimer = TRUE;
                }
                else
                {
                    if (NT_SUCCESS(ntStatus))
                    {
                        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Registry_TreeWriteEx returns ntStatus=%!STATUS!", ntStatus);
                    }
                    else
                    {
                        TraceEvents(TRACE_LEVEL_VERBOSE, DMF_TRACE, "Registry_TreeWrite returns ntStatus=%!STATUS! (no retry)", ntStatus);
                    }
                    // Remove it from the list.
                    //
                    RemoveEntryList(listEntry);
                    ExFreePoolWithTag(deferredContext,
                                      MemoryTag);
                    deferredContext = NULL;
                }
                break;
            }
            default:
            {
                DmfAssert(FALSE);
                break;
            }
        }

        // Point to the next entry in the list.
        //
        listEntry = nextListEntry;
    }

    if (needToRestartTimer)
    {
        // It means there are still pending deferred operations to perform.
        //
        Registry_DeferredOperationTimerStart(moduleContext->Timer);
    }

    DMF_ModuleUnlock(dmfModule);

    FuncExitVoid(DMF_TRACE);
}
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#if !defined(DMF_USER_MODE)

_Function_class_(DMF_Open)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_Registry_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type Registry.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    STATUS_SUCCESS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_Registry* moduleContext;
    WDF_TIMER_CONFIG timerConfig;
    WDF_OBJECT_ATTRIBUTES timerAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Initialize the list to empty.
    //
    InitializeListHead(&moduleContext->ListDeferredOperations);

    // Create the timer for deferred operations.
    //
    WDF_TIMER_CONFIG_INIT(&timerConfig,
                          Registry_DeferredOperationHandler);
    timerConfig.AutomaticSerialization = FALSE;

    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = DmfModule;
    timerAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    ntStatus = WdfTimerCreate(&timerConfig,
                              &timerAttributes,
                              &moduleContext->Timer);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfTimerCreate fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Function_class_(DMF_Close)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_Registry_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type Registry.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_Registry* moduleContext;
    PLIST_ENTRY listEntry;
    PLIST_ENTRY nextListEntry;
    REGISTRY_DEFERRED_CONTEXT* deferredContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->Timer != NULL)
    {
        WdfTimerStop(moduleContext->Timer,
                     TRUE);
        WdfObjectDelete(moduleContext->Timer);
        moduleContext->Timer = NULL;
    }
    else
    {
        // This can happen in cases of partial initialization.
        //
    }

    // Remove all pending deferred operations.
    //
    DMF_ModuleLock(DmfModule);

    // Get the first entry in the list.
    //
    listEntry = moduleContext->ListDeferredOperations.Flink;
    if (NULL == listEntry)
    {
        // This can happen in cases of partial initialization.
        //
        goto SkipListIteration;
    }

    // Loop ends when the current entry points to the list header.
    //
    while (listEntry != &moduleContext->ListDeferredOperations)
    {
        // Get the next entry now before current entry is removed.
        //
        nextListEntry = listEntry->Flink;

        deferredContext = CONTAINING_RECORD(listEntry,
                                            REGISTRY_DEFERRED_CONTEXT,
                                            ListEntry);
        // Remove from list.
        //
        RemoveEntryList(listEntry);
#if !defined(DMF_USER_MODE)
        // Free its allocated memory.
        //
        ExFreePoolWithTag(deferredContext,
                          MemoryTag);
#endif
        deferredContext = NULL;

        // Get the next entry.
        //
        listEntry = nextListEntry;
    }

SkipListIteration:

    DMF_ModuleUnlock(DmfModule);

    FuncExitVoid(DMF_TRACE);
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Calls by Client
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_Registry_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type Registry.

Arguments:

    Device - Client driver's WDFDEVICE object.
    DmfModuleAttributes - Opaque structure that contains parameters DMF needs to initialize the Module.
    ObjectAttributes - WDF object attributes for DMFMODULE.
    DmfModule - Address of the location where the created DMFMODULE handle is returned.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_Registry;
#if !defined(DMF_USER_MODE)
    DMF_CALLBACKS_DMF dmfCallbacksDmf_Registry;
#endif

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // For user mode, Open and Close are not needed as the deferred TreeWrite is not supported.
    //
#if !defined(DMF_USER_MODE)
    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_Registry);
    dmfCallbacksDmf_Registry.DeviceOpen = DMF_Registry_Open;
    dmfCallbacksDmf_Registry.DeviceClose = DMF_Registry_Close;
#endif
    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_Registry,
                                            Registry,
                                            DMF_CONTEXT_Registry,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_Create);

#if !defined(DMF_USER_MODE)
    dmfModuleDescriptor_Registry.CallbacksDmf = &dmfCallbacksDmf_Registry;
#endif
    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_Registry,
                                DmfModule);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);
}

// Module Methods
//

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_AllSubKeysFromHandleEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    )
/*++

Routine Description:

    Given a registry handle, enumerate all the sub keys and call an enumeration function for each of them.

Arguments:

    DmfModule - This Module's handle.
    Handle - An open registry key.
    RegistryEnumerationFunction - The client's enumeration callback function.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    RegistryKeyEnumerationContextType context;
    BOOLEAN returnValue;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // There is nothing to pass in this context. (All subkeys are presented to enumerator callback.)
   //
    context.FilterEnumeratorContext = NULL;
    // For each subkey of the current key, this function will be called. It will actually create the entries.
    //
    context.RegistryKeyEnumerationFunction = ClientCallback;
    context.ClientCallbackContext = ClientCallbackContext;

    returnValue = Registry_SubKeysFromHandleEnumerate(Handle,
                                                      Registry_KeyEnumerationFilterAllSubkeys,
                                                      &context);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_CallbackWork(
    _In_ WDFDEVICE WdfDevice,
    _In_ EVT_DMF_Registry_CallbackWork* CallbackWork
    )
/*++

Routine Description:

    Create and open a Registry Module, perform work, close and destroy Registry Module.

Parameters Description:
    WdfDevice - WDFDEVICE to associate with the new Registry Module.
    CallbackWork - The function that does the work.

Return Value:

    NTSTATUS

--*/
{
    DMFMODULE dmfModuleRegistry;
    NTSTATUS ntStatus;
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;
    dmfModuleRegistry = NULL;

    // Registry
    // --------
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = WdfDevice;
    DMF_Registry_ATTRIBUTES_INIT(&moduleAttributes);
    ntStatus = DMF_Registry_Create(WdfDevice,
                                   &moduleAttributes,
                                   &attributes,
                                   &dmfModuleRegistry);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_Registry_Create fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }
    DmfAssert(NULL != dmfModuleRegistry);

    // Do the work using the Module instance.
    //
    ntStatus = CallbackWork(dmfModuleRegistry);

Exit:

    // Close and destroy the Registry Module.
    //
    if (dmfModuleRegistry != NULL)
    {
        WdfObjectDelete(dmfModuleRegistry);
    }

    FuncExit(DMF_TRACE, "returnValue=%d", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_CustomAction(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _In_opt_ VOID* ValueDataToCompare,
    _In_ ULONG ValueDataToCompareSize,
    _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
    _In_opt_ VOID* ComparisonCallbackContext
    )
/*++

Routine Description:

    Allow the caller to perform a custom action in the comparison handler for a value.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ComparisonCallback - Caller's comparison function.
    ComparisonCallbackContext - Caller's context sent to comparison. function.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // Value type is not needed for Delete.
   // ValueDataToCompare is optional...it will be passed to comparison function.
   //
    ntStatus = Registry_ValueActionIfNeeded(Registry_ActionTypeNone,
                                            DmfModule,
                                            Handle,
                                            ValueName,
                                            ValueType,
                                            ValueDataToCompare,
                                            ValueDataToCompareSize,
                                            ComparisonCallback,
                                            ComparisonCallbackContext,
                                            FALSE);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_EnumerateKeysFromName(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR RootKeyName,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    )
/*++

Routine Description:

    Given a registry path name, enumerate all the sub keys and call an enumeration function for each of them.

Arguments:

    DmfModule - This Module's handle.
    RootKeyName - Path name of the registry key.
    ClientCallback - The enumeration function to call for each sub key.
    ClientCallbackContext - The client context to pass into the enumeration function.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    BOOLEAN returnValue;
    HANDLE handle;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    returnValue = FALSE;

    handle = Registry_HandleOpenByName(RootKeyName);
    if (NULL == handle)
    {
        goto Exit;
    }

    returnValue = Registry_SubKeysFromHandleEnumerate(handle,
                                                      ClientCallback,
                                                      ClientCallbackContext);

    Registry_HandleClose(handle);
    handle = NULL;

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
void
DMF_Registry_HandleClose(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle
    )
/*++

Routine Description:

    Given a registry handle, close the handle.

Arguments:

    DmfModule - This Module's handle.
    Handle - The given registry handle to an open registry key.

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    Registry_HandleClose(Handle);
    Handle = NULL;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleDelete(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle
    )
/*++

Routine Description:

    Delete a registry key.

Arguments:

    DmfModule - This Module's handle.
    Handle - Registry key handle to delete.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    NTSTATUS ntStatus;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // Delete the key.
   //
    ntStatus = WdfRegistryRemoveKey((WDFKEY)Handle);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleOpenByDeviceInterface(
    _In_ DMFMODULE DmfModule,
    _In_ CONST GUID* InterfaceGuid,
    _In_ int DeviceLinkIndex,
    _Out_ HANDLE* RegistryHandle
    )
/*++

Routine Description:

    Open a registry key that is specific to a device interface.

Arguments:

    DmfModule - This Module's handle.
    InterfaceGuid - Device interface GUID.
    DeviceLinkIndex - Instance index for Device Interface symbolic link's list.
    RegistryHandle - Opened registry key or NULL in case of error.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WCHAR* registryPath;
    WDFMEMORY registryPathObject;
    HANDLE key;
    size_t size;
    WDFSTRING deviceInterface;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    *RegistryHandle = NULL;
    deviceInterface = NULL;

    registryPath = NULL;
    registryPathObject = NULL;
    key = NULL;

    // Step 1 - Open device interface registry key as HKEY.
    //

    // Retrieve device interface symbolic link.
    // deviceInterface is allocated in Registry_DeviceInterfaceStringGet. Will be freed on exit.
    //
    ntStatus = Registry_DeviceInterfaceStringGet(DmfModule,
                                                 InterfaceGuid,
                                                 DeviceLinkIndex,
                                                 &deviceInterface);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Registry_DeviceInterfaceStringGet fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Open registry in KMDF here.
    // key will be closed on exit.
    //
    ntStatus = Registry_DeviceInterfaceKeyOpen(DmfModule,
                                               deviceInterface,
                                               GENERIC_READ,
                                               &key);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Registry_DeviceInterfaceKeyOpen fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Step 2 - Retrieve registry path related to device interface.
    //

    // registryPathObject is allocated in Registry_RegistryPathFromHandle and will be deleted on exit.
    //
    ntStatus = Registry_RegistryPathFromHandle(DmfModule,
                                               key,
                                               &registryPathObject);
    if (NULL == registryPathObject || !NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Registry_RegistryPathFromHandle fails: ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    // Step 3 - Open device interface registry key as WDFKEY.
    //
    registryPath = (WCHAR*) WdfMemoryGetBuffer(registryPathObject,
                                               &size);

    *RegistryHandle = DMF_Registry_HandleOpenByName(DmfModule,
                                                    registryPath);
    if (!*RegistryHandle)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
    }

Exit:

    if (registryPathObject)
    {
        WdfObjectDelete(registryPathObject);
    }
    if (deviceInterface)
    {
        WdfObjectDelete(deviceInterface);
    }

    if (NULL != key)
    {
        Registry_HKEYClose(key);
    }

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
HANDLE
DMF_Registry_HandleOpenByHandle(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR Name,
    _In_ BOOLEAN TryToCreate
    )
/*++

Routine Description:

    Given a registry handle, open a handle relative to that handle.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to open registry key.
    Name - Path name of the key relative to handle.
    TryToCreate - Indicates if the function should call create instead of open.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    HANDLE handle;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    handle = Registry_HandleOpenByHandle(Handle,
                                         Name,
                                         TryToCreate);

    FuncExit(DMF_TRACE, "handle=0x%p", handle);

    return handle;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleOpenById(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG PredefinedKeyId,
    _In_ ULONG AccessMask,
    _Out_ HANDLE* RegistryHandle
    )
/*++

Routine Description:

    Open a predefined registry key.

Arguments:

    DmfModule - This Module's handle.
    PredefinedKeyId - The Id of the predefined key to open.
                      See IoOpenDeviceRegistryKey documentation for a list of Ids.
    AccessMask - Access mask to use to open the handle.
    RegistryHandle - Handle to open registry key or NULL in case of error.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = Registry_HandleOpenByPredefinedKey(device,
                                                  PredefinedKeyId,
                                                  AccessMask,
                                                  RegistryHandle);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
HANDLE
DMF_Registry_HandleOpenByName(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR Name
    )
/*++

Routine Description:

    Open a registry key by path name.

Arguments:

    DmfModule - This Module's handle.
    Name - Path name of the key relative to handle.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    HANDLE handle;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    handle = Registry_HandleOpenByName(Name);

    FuncExit(DMF_TRACE, "handle=0x%p", handle);

    return handle;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleOpenByNameEx(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR Name,
    _In_ ULONG AccessMask,
    _In_ BOOLEAN Create,
    _Out_ HANDLE* RegistryHandle
    )
/*++

Routine Description:

    Open a registry key by path name and access mask.

Arguments:

    DmfModule - This Module's handle.
    Name - Path name of the key relative to handle. NULL to open the device instance registry key.
           Note: Use DMF_Registry_HandleOpenById() instead to open the Device Key.
    AccessMask - Access mask to use to open the handle.
    Create - Creates the key if it cannot be opened.
    RegistryHandle - Handle to open registry key or NULL in case of error.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    device = DMF_ParentDeviceGet(DmfModule);

    if (Name != NULL)
    {
        // NOTE:
        // Deprecated path for WCOS compliant drivers.
        // This path will cause Verifier errors under recent versions of Windows.
        // Use DMF_Registry_HandleOpenById() or DMF_Registry_HandleOpenParametersRegistryKey()
        // instead.
        //
        ntStatus = Registry_HandleOpenByNameEx(Name,
                                               AccessMask,
                                               Create,
                                               RegistryHandle);
    }
    else
    {
        // Deprecated path. Use DMF_Registry_HandleOpenById() instead.
        //
        ntStatus = Registry_HandleOpenByPredefinedKey(device,
                                                      PLUGPLAY_REGKEY_DEVICE,
                                                      AccessMask,
                                                      RegistryHandle);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_HandleOpenParametersRegistryKey(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG DesiredAccess,
    _In_ WDF_OBJECT_ATTRIBUTES* KeyAttributes,
    _Out_ HANDLE* RegistryHandle
    )
/*++

Routine Description:

    Open the driver's "Parameters" key. This is just a wrapper around the WDF API so that
    it is not necessary to mix DMF and WDF calls.

Arguments:

    DmfModule - This Module's handle.
    DesiredAccess - Access mask to use to open the handle.
    KeyAttributes - See MSDN documentation for WdfDriverOpenParametersRegistryKey().
    RegistryHandle - Handle to open registry key or NULL in case of error.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    WDFDEVICE device;
    WDFDRIVER driver;
    WDFKEY registryHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    device = DMF_ParentDeviceGet(DmfModule);
    driver = WdfDeviceGetDriver(device);

    ntStatus = WdfDriverOpenParametersRegistryKey(driver,
                                                  DesiredAccess,
                                                  KeyAttributes,
                                                  &registryHandle);
    if (NT_SUCCESS(ntStatus))
    {
        *RegistryHandle = (HANDLE)registryHandle;
    }
    else
    {
        *RegistryHandle = NULL;
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// PathAndValue[Read | Write | Delete ][RegistryValueType]
//
// These functions work with a Path and Value. They open a handle to  the path, perform 
// the operation on the value and close the handle to the path.
//
/////////////////////////////////////////////////////////////////////////////////////////////
//

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueDelete(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName
    )
/*++

Routine Description:

    Delete a value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to delete.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    HANDLE registryPathHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_HandleOpenByNameEx(DmfModule,
                                               RegistryPathName,
                                               KEY_SET_VALUE,
                                               FALSE,
                                               &registryPathHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    DmfAssert(registryPathHandle != NULL);
    ntStatus = DMF_Registry_ValueDelete(DmfModule,
                                        registryPathHandle,
                                        ValueName);

    // Handle is no longer needed. Close it.
    //
    DMF_Registry_HandleClose(DmfModule,
                             registryPathHandle);
    registryPathHandle = NULL;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueRead(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_ ULONG RegistryType,
    _Out_writes_opt_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _Out_opt_ ULONG* BytesRead
    )
/*++

Routine Description:

    Reads a value (of any REG_* type) given a registry path and value name.
    This functions is called by other Module Methods or can be directly by
    the Client Driver.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    HANDLE registryPathHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(((Buffer != NULL) && (BufferSize > 0)) || 
              ((NULL == Buffer) && (0 == BufferSize) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_HandleOpenByNameEx(DmfModule,
                                               RegistryPathName,
                                               KEY_READ,
                                               FALSE,
                                               &registryPathHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        if (BytesRead != NULL)
        {
            // Explicitly clear here for the above failure.
            // In case the above function succeeds, it is not necessary
            // to explicitly clear *BytesRead in this function.
            //
            *BytesRead = 0;
        }
        goto Exit;
    }

    DmfAssert(registryPathHandle != NULL);
    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      registryPathHandle,
                                      ValueName,
                                      RegistryType,
                                      Buffer,
                                      BufferSize,
                                      BytesRead);

    // Handle is no longer needed. Close it.
    //
    DMF_Registry_HandleClose(DmfModule,
                             registryPathHandle);
    registryPathHandle = NULL;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadBinary(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _Out_opt_ ULONG* BytesRead
    )
/*++

Routine Description:

    Reads a REG_BINARY value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(((Buffer != NULL) && (BufferSize > 0)) || 
              ((NULL == Buffer) && (0 == BufferSize) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueRead(DmfModule,
                                             RegistryPathName,
                                             ValueName,
                                             REG_BINARY,
                                             Buffer,
                                             BufferSize,
                                             BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadDword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_ ULONG* Buffer
    )
/*++

Routine Description:

    Reads a REG_DWORD value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bytesRead;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueRead(DmfModule,
                                             RegistryPathName,
                                             ValueName,
                                             REG_DWORD,
                                             (UCHAR*)Buffer,
                                             sizeof(ULONG),
                                             &bytesRead);
    // "Using 'bytesRead' from failed function call.
    //
    #pragma warning(suppress: 6102)
    DmfAssert((NT_SUCCESS(ntStatus) && (sizeof(ULONG) == bytesRead)) || 
              (0 == bytesRead));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadDwordAndValidate(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_ ULONG* Buffer,
    _In_ ULONG Minimum,
    _In_ ULONG Maximum
    )
/*++

Routine Description:

    Reads a REG_DWORD value given a registry path and value name.
    Validate the read value against a minimum and maximum.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    Minimum - Caller's minimum expected value.
    Maximum - Caller's maximum expected value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueReadDword(DmfModule,
                                                  RegistryPathName,
                                                  ValueName,
                                                  Buffer);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (*Buffer < Minimum)
    {
        // Read value is too low.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

    if (*Buffer > Maximum)
    {
        // Read value is too high.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadMultiString(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters,
    _Out_opt_ ULONG* BytesRead
    )
/*++

Routine Description:

    Reads a REG_MULTI_SZ value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    NumberOfCharacters - Number of WCHAR pointed to by Buffer.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(((Buffer != NULL) && (NumberOfCharacters > 0)) || 
              ((NULL == Buffer) && (0 == NumberOfCharacters) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_PathAndValueRead(DmfModule,
                                             RegistryPathName,
                                             ValueName,
                                             REG_MULTI_SZ,
                                             (UCHAR*)Buffer,
                                             bufferSizeBytes,
                                             BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadQword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_ ULONGLONG* Buffer
    )
/*++

Routine Description:

    Reads a REG_QWORD value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bytesRead;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueRead(DmfModule,
                                             RegistryPathName,
                                             ValueName,
                                             REG_QWORD,
                                             (UCHAR*)Buffer,
                                             sizeof(ULONGLONG),
                                             &bytesRead);
    // "Using 'bytesRead' from failed function call.
    //
    #pragma warning(suppress: 6102)
    DmfAssert((NT_SUCCESS(ntStatus) && (sizeof(ULONGLONG) == bytesRead)) || 
              (0 == bytesRead));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadQwordAndValidate(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_ ULONGLONG* Buffer,
    _In_ ULONGLONG Minimum,
    _In_ ULONGLONG Maximum
    )
/*++

Routine Description:

    Reads a REG_QWORD value given a registry path and value name.
    Validate the read value against a minimum and maximum.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    Minimum - Caller's minimum expected value.
    Maximum - Caller's maximum expected value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueReadQword(DmfModule,
                                                  RegistryPathName,
                                                  ValueName,
                                                  Buffer);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (*Buffer < Minimum)
    {
        // Read value is too low.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

    if (*Buffer > Maximum)
    {
        // Read value is too high.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueReadString(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters,
    _Out_opt_ ULONG* BytesRead
    )
/*++

Routine Description:

    Reads a REG_SZ value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    Buffer - Where the read data is written.
    NumberOfCharacters - Number of WCHAR in the array pointed to by Buffer.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(((Buffer != NULL) && (NumberOfCharacters > 0)) || 
              ((NULL == Buffer) && (0 == NumberOfCharacters) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_PathAndValueRead(DmfModule,
                                             RegistryPathName,
                                             ValueName,
                                             REG_SZ,
                                             (UCHAR*)Buffer,
                                             bufferSizeBytes,
                                             BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWrite(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_ ULONG RegistryType,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize
    )
/*++

Routine Description:

    Write a value (of any REG_* type) given a registry path and value name.
    This functions is called by other Module Methods or can be directly by
    the Client Driver.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to write.
    Buffer - The data that is written to the value.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    HANDLE registryPathHandle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_HandleOpenByNameEx(DmfModule,
                                               RegistryPathName,
                                               KEY_SET_VALUE,
                                               TRUE,
                                               &registryPathHandle);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    DmfAssert(registryPathHandle != NULL);
    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       registryPathHandle,
                                       ValueName,
                                       RegistryType,
                                       Buffer,
                                       BufferSize);

    // Handle is no longer needed. Close it.
    //
    DMF_Registry_HandleClose(DmfModule,
                             registryPathHandle);
    registryPathHandle = NULL;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteBinary(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize
    )
/*++

Routine Description:

    Write a REG_BINARY value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to write.
    Buffer - The data that is written to value.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueWrite(DmfModule,
                                              RegistryPathName,
                                              ValueName,
                                              REG_BINARY,
                                              Buffer,
                                              BufferSize);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteDword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueData
    )
/*++

Routine Description:

    Write a REG_DWORD value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    ValueData - The data to write to the value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueWrite(DmfModule,
                                              RegistryPathName,
                                              ValueName,
                                              REG_DWORD,
                                              (UCHAR*)&ValueData,
                                              sizeof(ULONG));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteMultiString(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    )
/*++

Routine Description:

    Write a REG_MULTI_SZ value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to write.
    Buffer - The data that is written to value.
    NumberOfCharacters - Number of WCHAR pointed to by Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_PathAndValueWrite(DmfModule,
                                              RegistryPathName,
                                              ValueName,
                                              REG_MULTI_SZ,
                                              (UCHAR*)Buffer,
                                              bufferSizeBytes);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteQword(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_ ULONGLONG ValueData
    )
/*++

Routine Description:

    Write a REG_QWORD value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to read.
    ValueData - The data to write to the value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_PathAndValueWrite(DmfModule,
                                              RegistryPathName,
                                              ValueName,
                                              REG_QWORD,
                                              (UCHAR*)&ValueData,
                                              sizeof(ULONGLONG));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_PathAndValueWriteString(
    _In_ DMFMODULE DmfModule,
    _In_opt_ PWCHAR RegistryPathName,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    )
/*++

Routine Description:

    Write a REG_SZ value given a registry path and value name.

Arguments:

    DmfModule - This Module's handle.
    RegistryPathName - Registry path to ValueName.
    ValueName - Name of registry value to write.
    Buffer - The data that is written to value.
    NumberOfCharacters - Number of WCHAR pointed to by Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);

    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_PathAndValueWrite(DmfModule,
                                              RegistryPathName,
                                              ValueName,
                                              REG_SZ,
                                              (UCHAR*)Buffer,
                                              bufferSizeBytes);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_RegistryPathDelete(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR Name
    )
/*++

Routine Description:

    Delete a registry key by path name.

Arguments:

    DmfModule - This Module's handle.
    Name - Path name of the key relative to handle.

Return Value:

    Handle - Handle to open registry key or NULL in case of error.

--*/
{
    NTSTATUS ntStatus;
    HANDLE handle;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_HandleOpenByNameEx(Name,
                                           KEY_SET_VALUE,
                                           FALSE,
                                           &handle);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }
    DmfAssert(handle != NULL);

    // Delete the key.
    //
    ntStatus = DMF_Registry_HandleDelete(DmfModule,
                                         handle);

#if defined(DMF_USER_MODE)
    // Regardless of the above call, close the handle.
    // NOTE: Per MSDN, do not call this function after deleting the key.
    //
    Registry_HandleClose(handle);
#endif
    handle = NULL;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
ScheduledTask_Result_Type
DMF_Registry_ScheduledTaskCallbackContainer(
    _In_ DMFMODULE DmfScheduledTask,
    _In_ VOID* ClientCallbackContext,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
/*++

Routine Description:

    Call a number of callback functions that do work on the registry.

Parameters Description:

    DmfScheduledTask - The ScheduledTask DMF Module from which the callback is called.
    ClientCallbackContext - Client Context provided for this callback.
    PreviousState - Valid only for calls from D0Entry

Return Value:

    ScheduledTask_WorkResult_Success - Indicates the operations was successful.
    ScheduledTask_WorkResult_Fail - Indicates the operation was not successful. A retry will happen.

--*/
{
    NTSTATUS ntStatus;
    ScheduledTask_Result_Type returnValue;
    WDFDEVICE device;
    Registry_ContextScheduledTaskCallback* scheduledTaskCallbackContext;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(PreviousState);

    FuncEntry(DMF_TRACE);

    returnValue = ScheduledTask_WorkResult_FailButTryAgain;

    device = DMF_ParentDeviceGet(DmfScheduledTask);

    scheduledTaskCallbackContext = (Registry_ContextScheduledTaskCallback*)ClientCallbackContext;
    DmfAssert(scheduledTaskCallbackContext != NULL);

    for (ULONG callbackIndex = 0; callbackIndex < scheduledTaskCallbackContext->NumberOfCallbacks; callbackIndex++)
    {
        // Create and open a Registry Module, do the registry work, close and destroy the Registry Module.
        //
        ntStatus = DMF_Registry_CallbackWork(device,
                                             scheduledTaskCallbackContext->Callbacks[callbackIndex]);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE,
                        "DMF_Registry_CallbackWork fails: callbackIndex=%d ntStatus=%!STATUS!",
                        callbackIndex,
                        ntStatus);
            goto Exit;
        }
    }

    // Work is done, no need to try again.
    //
    returnValue = ScheduledTask_WorkResult_Success;

Exit:

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_SubKeysFromHandleEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    )
/*++

Routine Description:

    Given a registry handle, enumerate all the sub keys and call an enumeration function for each of them.

Arguments:

    DmfModule - This Module's handle.
    Handle - An open registry key.
    ClientCallback - The enumeration function to call for each sub key.
    ClientCallbackContext -  The client context to pass into the enumeration function.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    returnValue = Registry_SubKeysFromHandleEnumerate(Handle,
                                                      ClientCallback,
                                                      ClientCallbackContext);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
DMF_Registry_SubKeysFromPathNameContainingStringEnumerate(
    _In_ DMFMODULE DmfModule,
    _In_ PWCHAR PathName,
    _In_ PWCHAR LookFor,
    _In_ EVT_DMF_Registry_KeyEnumerationCallback* ClientCallback,
    _In_ VOID* ClientCallbackContext
    )
/*++

Routine Description:

    Given a registry path name, enumerate all the sub keys and call an enumeration function for each of them
    which looks for a particular substring.

Arguments:

    DmfModule - This Module's handle.
    PathName - Path name of the registry key.
    LookFor - The substring to search for in the sub keys.
    RegistryEnumerationFunction - The function that does the comparison for all the sub keys.

Return Value:

    TRUE on success, FALSE on error.

--*/
{
    RegistryKeyEnumerationContextType context;
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // It is the substring that is searched for inside the enumerated sub keys.
   //
    context.FilterEnumeratorContext = LookFor;

    // It is the function that the client wants called for all the sub keys that contain
    // the substring to look for.
    //
    context.RegistryKeyEnumerationFunction = ClientCallback;
    context.ClientCallbackContext = ClientCallbackContext;

    // Enumerate all the sub keys and call the function that looks for the substring in each
    // of the enumerated sub keys.
    //
    returnValue = DMF_Registry_EnumerateKeysFromName(DmfModule,
                                                     PathName,
                                                     Registry_KeyEnumerationFilterStrstr,
                                                     &context);

    FuncExit(DMF_TRACE, "returnValue=%d", returnValue);

    return returnValue;
}

#if !defined(DMF_USER_MODE)
NTSTATUS
DMF_Registry_TreeWriteDeferred(
    _In_ DMFMODULE DmfModule,
    _In_ Registry_Tree* RegistryTree,
    _In_ ULONG ItemCount
    )
/*++

Routine Description:

    Writes an array of registry trees to the registry in a deferred time. Keep retrying
    if STATUS_OBJECT_NAME_NOT_FOUND happens.

Arguments:

    DmfModule - This Module's handle.
    Tree - The array of registry trees.
    NumberOfTrees - The number of entries in the array.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_DeferredOperationAdd(DmfModule,
                                             RegistryTree,
                                             ItemCount,
                                             Registry_DeferredOperationWrite);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

NTSTATUS
DMF_Registry_TreeWriteEx(
    _In_ DMFMODULE DmfModule,
    _In_ Registry_Tree* RegistryTree,
    _In_ ULONG ItemCount
    )
/*++

Routine Description:

    Writes an array of registry trees to the registry.

Arguments:

    DmfModule - This Module's handle.
    Tree - The array of registry trees.
    NumberOfTrees - The number of entries in the array.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(DmfModule);

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_TreeWrite(DmfModule,
                                  RegistryTree,
                                  ItemCount);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#endif

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueDelete(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName
    )
/*++

Routine Description:

    Delete a value from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_ValueActionAlways(Registry_ActionTypeDelete,
                                          DmfModule,
                                          Handle,
                                          ValueName,
                                          0,
                                          NULL,
                                          0,
                                          NULL);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueDeleteIfNeeded(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_opt_ VOID* ValueDataToCompare,
    _In_ ULONG ValueDataToCompareSize,
    _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
    _In_opt_ VOID* ComparisonCallbackContext
    )
/*++

Routine Description:

    Delete a value after calling a client comparison function to determine if that
    data should be deleted.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ComparisonCallback - Caller's comparison function.
    ComparisonCallbackContext - Caller's context sent to comparison. function.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // Value type is not needed for Delete.
   // ValueDataToCompare is optional...it will be passed to comparison function.
   //
    ntStatus = Registry_ValueActionIfNeeded(Registry_ActionTypeDelete,
                                            DmfModule,
                                            Handle,
                                            ValueName,
                                            0,
                                            ValueDataToCompare,
                                            ValueDataToCompareSize,
                                            ComparisonCallback,
                                            ComparisonCallbackContext,
                                            FALSE);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueRead(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _Out_writes_opt_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _Out_opt_ ULONG* BytesRead
    )
/*++

Routine Description:

    Reads any type of value from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueType - The registry type of value to read.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    // NOTE: ValueName is NULL if Client wants to access "Default" value.
    // NOTE: "Default" value is always SZ.
    //
    DmfAssert(((Buffer != NULL) && (BufferSize > 0)) ||
              ((NULL == Buffer) && (0 == BufferSize) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

   // NOTE: Bytes read is optional. Clear in case of error.
   //
    if (BytesRead != NULL)
    {
        *BytesRead = 0;
    }

    // "Using uninitialized memory '*Buffer'.".
    //
    #pragma warning(suppress: 6001)
    ntStatus = Registry_ValueActionAlways(Registry_ActionTypeRead,
                                          DmfModule,
                                          Handle,
                                          ValueName,
                                          ValueType,
                                          Buffer,
                                          BufferSize,
                                          BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadBinary(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize,
    _Out_opt_ ULONG* BytesRead
    )
/*++

Routine Description:

    Reads a REG_BINARY from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    BytesRead - Number of bytes read from registry and written to Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(((Buffer != NULL) && (BufferSize > 0)) ||
              ((NULL == Buffer) && (0 == BufferSize) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      Handle,
                                      ValueName,
                                      REG_BINARY,
                                      (UCHAR*)Buffer,
                                      BufferSize,
                                      BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadDword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_ ULONG* Buffer
    )
/*++

Routine Description:

    Reads a REG_DWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bytesRead;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      Handle,
                                      ValueName,
                                      REG_DWORD,
                                      (UCHAR*)Buffer,
                                      sizeof(ULONG),
                                      &bytesRead);
    // "Using 'bytesRead' from failed function call.
    //
    #pragma warning(suppress: 6102)
    DmfAssert((NT_SUCCESS(ntStatus) && (sizeof(ULONG) == bytesRead)) || 
              (0 == bytesRead));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadDwordAndValidate(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_ ULONG* Buffer,
    _In_ ULONG Minimum,
    _In_ ULONG Maximum
    )
/*++

Routine Description:

    Reads a REG_DWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    Minimum - Caller's minimum expected value.
    Maximum - Caller's maximum expected value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueReadDword(DmfModule,
                                           Handle,
                                           ValueName,
                                           Buffer);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (*Buffer < Minimum)
    {
        // Read value is too low.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

    if (*Buffer > Maximum)
    {
        // Read value is too high.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadMultiString(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters,
    _Out_opt_ ULONG* BytesRead
    )
/*++

Routine Description:

    Reads a REG_MULTI_SZ from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    NumberOfCharacters - Number of WCHAR in the array pointed to by Buffer.
    BytesRead - Number of bytes read and written into Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(((Buffer != NULL) && (NumberOfCharacters > 0)) || 
              ((NULL == Buffer) && (0 == NumberOfCharacters) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      Handle,
                                      ValueName,
                                      REG_MULTI_SZ,
                                      (UCHAR*)Buffer,
                                      bufferSizeBytes,
                                      BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadQword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_ ULONGLONG* Buffer
    )
/*++

Routine Description:

    Reads a REG_QWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bytesRead;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      Handle,
                                      ValueName,
                                      REG_QWORD,
                                      (UCHAR*)Buffer,
                                      sizeof(ULONGLONG),
                                      &bytesRead);
    // "Using 'bytesRead' from failed function call.
    //
    #pragma warning(suppress: 6102)
    DmfAssert((NT_SUCCESS(ntStatus) && (sizeof(ULONGLONG) == bytesRead)) || 
              (0 == bytesRead));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadQwordAndValidate(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_ PULONGLONG Buffer,
    _In_ ULONGLONG Minimum,
    _In_ ULONGLONG Maximum
    )
/*++

Routine Description:

    Reads a REG_QWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.
    Minimum - Caller's minimum expected value.
    Maximum - Caller's maximum expected value.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueReadQword(DmfModule,
                                           Handle,
                                           ValueName,
                                           Buffer);
    if (! NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    if (*Buffer < Minimum)
    {
        // Read value is too low.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

    if (*Buffer > Maximum)
    {
        // Read value is too high.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueReadString(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _Out_writes_opt_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters,
    _Out_opt_ ULONG* BytesRead
    )
/*++

Routine Description:

    Reads a REG_SZ from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    Buffer - Where the read data is written.
    NumberOfCharacters - Number of WCHAR in the array pointed to by Buffer.
    BytesRead - Number of bytes read and written into Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    // NOTE: ValueName is NULL if Client wants to access "Default" value.
    // NOTE: "Default" value is always SZ.
    //
    DmfAssert(((Buffer != NULL) && (NumberOfCharacters > 0)) ||
              ((NULL == Buffer) && (0 == NumberOfCharacters) && (BytesRead != NULL)));

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_ValueRead(DmfModule,
                                      Handle,
                                      ValueName,
                                      REG_SZ,
                                      (UCHAR*)Buffer,
                                      bufferSizeBytes,
                                      BytesRead);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWrite(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize
    )
/*++

Routine Description:

    Writes any type of value to the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueType - The registry type of value to read.
    Buffer - Where the read data is written.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    // NOTE: ValueName is NULL if Client wants to access "Default" value.
    // NOTE: "Default" value is always SZ.
    //
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_ValueActionAlways(Registry_ActionTypeWrite,
                                          DmfModule,
                                          Handle,
                                          ValueName,
                                          ValueType,
                                          Buffer,
                                          BufferSize,
                                          NULL);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteBinary(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_reads_(BufferSize) UCHAR* Buffer,
    _In_ ULONG BufferSize
    )
/*++

Routine Description:

    Write a REG_BINARY from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueName - The name of the value that is written.
    Buffer - The string that is written.
    BufferSize - Size of buffer in bytes.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       Handle,
                                       ValueName,
                                       REG_BINARY,
                                       (UCHAR*)Buffer,
                                       BufferSize);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteDword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueData
    )
/*++

Routine Description:

    Write a REG_DWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is written.
    ValueData - The data to write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       Handle,
                                       ValueName,
                                       REG_DWORD,
                                       (UCHAR*)&ValueData,
                                       sizeof(ULONG));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteIfNeeded(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONG ValueType,
    _In_ VOID* ValueDataToWrite,
    _In_ ULONG ValueDataToWriteSize,
    _In_ EVT_DMF_Registry_ValueComparisonCallback* ComparisonCallback,
    _In_opt_ VOID* ComparisonCallbackContext,
    _In_ BOOLEAN WriteIfNotFound
    )
/*++

Routine Description:

    Write the data for a value after calling a client comparison function to determine if that
    data should be written.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is queried and set.
    ValueType - The Registry Type of the value.
    ValueDataToWrite - The data to write if the value is not set to one or it does not exist.
    ValueDataToWriteSize - The size of the buffer at ValueDataToWrite
    ComparisonCallback - Caller's comparison function.
    ComparisonCallbackContext - Caller's context sent to comparison. function.
    WriteIfNotFound - Indicates if the value should be written if it does not exist.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = Registry_ValueActionIfNeeded(Registry_ActionTypeWrite,
                                            DmfModule,
                                            Handle,
                                            ValueName,
                                            ValueType,
                                            ValueDataToWrite,
                                            ValueDataToWriteSize,
                                            ComparisonCallback,
                                            ComparisonCallbackContext,
                                            WriteIfNotFound);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteMultiString(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    )
/*++

Routine Description:

    Write a REG_MULTI_SZ to the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is written.
    Buffer - The string that is written.
    NumberOfCharacters - Number of WCHAR pointed to by Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       Handle,
                                       ValueName,
                                       REG_MULTI_SZ,
                                       (UCHAR*)Buffer,
                                       bufferSizeBytes);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteQword(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_ ULONGLONG ValueData
    )
/*++

Routine Description:

    Write a REG_QWORD from the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is written.
    ValueData - The data to write.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    DmfAssert(ValueName != NULL);
    DmfAssert(*ValueName != L'\0');

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       Handle,
                                       ValueName,
                                       REG_QWORD,
                                       (UCHAR*)&ValueData,
                                       sizeof(ULONGLONG));

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_Registry_ValueWriteString(
    _In_ DMFMODULE DmfModule,
    _In_ HANDLE Handle,
    _In_ PWCHAR ValueName,
    _In_reads_(NumberOfCharacters) PWCHAR Buffer,
    _In_ ULONG NumberOfCharacters
    )
/*++

Routine Description:

    Write a REG_SZ to the registry given a registry handle and value name.

Arguments:

    DmfModule - This Module's handle.
    Handle - Handle to the registry key where the value is located.
    ValueName - The name of the value that is written.
    Buffer - The string that is written.
    NumberOfCharacters - Size in characters pointed to by Buffer.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    ULONG bufferSizeBytes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DmfAssert(DmfModule != NULL);
    // NOTE: ValueName is NULL if Client wants to access "Default" value.
    // NOTE: "Default" value is always SZ.
    //
    DmfAssert(Buffer != NULL);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 Registry);

    bufferSizeBytes = NumberOfCharacters * sizeof(WCHAR);
    ntStatus = DMF_Registry_ValueWrite(DmfModule,
                                       Handle,
                                       ValueName,
                                       REG_SZ,
                                       (UCHAR*)Buffer,
                                       bufferSizeBytes);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_Registry.c
//
