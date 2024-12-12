/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_IoctlHandler.h

Abstract:

    Companion file to Dmf_IoctlHandler.c.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// Client returns STATUS_PENDING if Client retains buffer.
// Any other status causes Module to complete Request.
//
// IMPORTANT: Set *BytesReturned to zero when the Request is NOT completed by this callback. If the callback returns STATUS_PENDING,
//            the number of bytes returned can be set when the Client completes the Request using WdfRequestCompleteWithInformation().
//            If STATUS_PENDING is returned, do not store BytesReturned and write to it when Request is completed.
//

typedef
_Function_class_(EVT_DMF_IoctlHandler_Callback)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_IoctlHandler_Callback(_In_ DMFMODULE DmfModule,
                              _In_ WDFQUEUE Queue,
                              _In_ WDFREQUEST Request,
                              _In_ ULONG IoctlCode,
                              _In_reads_(InputBufferSize) VOID* InputBuffer,
                              _In_ size_t InputBufferSize,
                              _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
                              _In_ size_t OutputBufferSize,
                              _Out_ size_t* BytesReturned);

// Allows Client to filter access to the IOCTLs. Client can use the parameters to 
// decide if the connection to User-mode should be allowed. It is provided in case
// default handler does not provide needed support. Use default handler has a guide
// for how to implement the logic in this callback.
//
// Return value of TRUE indicates that this callback completed the Request.
//
typedef
_Function_class_(EVT_DMF_IoctlHandler_AccessModeFilter)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
BOOLEAN
EVT_DMF_IoctlHandler_AccessModeFilter(_In_ DMFMODULE DmfModule,
                                      _In_ WDFDEVICE Device,
                                      _In_ WDFREQUEST Request,
                                      _In_ WDFFILEOBJECT FileObject);

typedef
_Function_class_(EVT_DMF_IoctlHandler_PostDeviceInterfaceCreate)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS
EVT_DMF_IoctlHandler_PostDeviceInterfaceCreate(_In_ DMFMODULE DmfModule,
                                               _In_ GUID* DeviceInterfaceGuid,
                                               _In_opt_ UNICODE_STRING* ReferenceStringUnicodePointer);

// The descriptor for each supported IOCTL.
//
typedef struct
{
    // The IOCTL code.
    // NOTE: At this time only METHOD_BUFFERED or METHOD_DIRECT buffers are supported.
    //
    LONG IoctlCode;
    // Minimum input buffer size which is automatically validated by this Module.
    //
    ULONG InputBufferMinimumSize;
    // Minimum out buffer size which is automatically validated by this Module.
    //
    ULONG OutputBufferMinimumSize;
    // IOCTL handler callback after buffer size validation.
    //
    EVT_DMF_IoctlHandler_Callback* EvtIoctlHandlerFunction;
    // Administrator access only. This flag is used with IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl
    // to allow Administrator access on a per-IOCTL basis.
    //
    BOOLEAN AdministratorAccessOnly;
} IoctlHandler_IoctlRecord;

typedef enum
{
    // Do what WDF would normally do (allow User-mode).
    //
    IoctlHandler_AccessModeDefault,
    // Call the a Client Callback function that will decide.
    //
    IoctlHandler_AccessModeFilterClientCallback,
    // NOTE: This is currently not implemented.
    //
    IoctlHandler_AccessModeFilterDoNotAllowUserMode,
    // Only allows "Run as Administrator".
    //
    IoctlHandler_AccessModeFilterAdministratorOnly,
    // Allow access to Administrator on a per-IOCTL basis.
    //
    IoctlHandler_AccessModeFilterAdministratorOnlyPerIoctl,
    // Restrict to Kernel-mode access only.
    //
    IoctlHandler_AccessModeFilterKernelModeOnly
} IoctlHandler_AccessModeFilterType;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // GUID of the device interface that allows User-mode access.
    //
    GUID DeviceInterfaceGuid;
    // Allows Client to filter access to IOCTLs.
    //
    IoctlHandler_AccessModeFilterType AccessModeFilter;
    // This is only set if the AccessModeFilter == IoctlHandler_AccessModeFilterClientCallback.
    //
    EVT_DMF_IoctlHandler_AccessModeFilter* EvtIoctlHandlerAccessModeFilter;
    // This is a pointer to a static table in the Client.
    //
    IoctlHandler_IoctlRecord* IoctlRecords;
    // The number of records in the above table.
    //
    ULONG IoctlRecordCount;
    // FALSE (Default) means that the corresponding device interface is created when this Module opens.
    // TRUE requires that the Client call DMF_IoctlHandler_IoctlsEnable() to enable the corresponding device interface.
    //
    BOOLEAN ManualMode;
    // FALSE (Default) means that the corresponding device interface will handle all IOCTL types.
    // TRUE means that the module allows only requests from kernel mode clients.
    //
    BOOLEAN KernelModeRequestsOnly;
    // Do not use. (For backward compatibility purposes only.)
    //
    WCHAR* CustomCapabilities;
    CHAR IsRestricted;
    // Allows Client to perform actions after the Device Interface is created.
    //
    EVT_DMF_IoctlHandler_PostDeviceInterfaceCreate* PostDeviceInterfaceCreate;
    // Allows request forwarding for IOCTLs not handled by this Module.
    //
    BOOLEAN ForwardUnhandledRequests;
    // Device Interface Reference String (optional).
    //
    WCHAR* ReferenceString;
} DMF_CONFIG_IoctlHandler;

// This macro declares the following functions:
// DMF_IoctlHandler_ATTRIBUTES_INIT()
// DMF_CONFIG_IoctlHandler_AND_ATTRIBUTES_INIT()
// DMF_IoctlHandler_Create()
//
DECLARE_DMF_MODULE(IoctlHandler)

// Module Methods
//

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_IoctlHandler_IoctlChain(
    _In_ DMFMODULE DmfModule,
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoctlCode,
    _In_reads_(InputBufferSize) VOID* InputBuffer,
    _In_ size_t InputBufferSize,
    _Out_writes_(OutputBufferSize) VOID* OutputBuffer,
    _In_ size_t OutputBufferSize,
    _Out_ size_t* BytesReturned
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_IoctlHandler_IoctlStateSet(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Enable
    );

// eof: Dmf_IoctlHandler.h
//
