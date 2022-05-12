/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_VirtualHidKeyboard.c

Abstract:

    Support for creating a virtual keyboard device that "types" keys into the host.
    NOTE: See https://usb.org/sites/default/files/hut1_3_0.pdf (chapter 10) to find
          keystroke map values.

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
#include "Dmf_VirtualHidKeyboard.tmh"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_VirtualHidKeyboard
{
    // Virtual Hid Device via Vhf.
    //
    DMFMODULE DmfModuleVirtualHidDeviceVhf;

    // For Client/Server support.
    //
    PCALLBACK_OBJECT CallbackObject;
    VOID* CallbackHandle;
} DMF_CONTEXT_VirtualHidKeyboard;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(VirtualHidKeyboard)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(VirtualHidKeyboard)

// MemoryTag.
//
#define MemoryTag 'MKHV'

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// This is for test purposes only.
//
#define NO_USE_DISABLE_CALLBACK_REGISTRATION

#define REPORT_ID_KEYBOARD 0x01
#define REPORT_ID_CONSUMER 0x02

// HID Report Descriptor for a minimal keyboard.
//
static
const
UCHAR
g_VirtualHidKeyboard_HidReportDescriptor[] =
{
    /*
    Value Item
    */
    0x05, 0x01,     /* Usage Page(Generic Desktop), */
    0x09, 0x06,     /* Usage(Keyboard),*/
    0xA1, 0x01,     /* Collection(HID_FLAGS_COLLECTION_Application),*/
    0x85, REPORT_ID_KEYBOARD,  /* Report Id,*/
    0x05, 0x07,     /* Usage Page(Key Codes),*/
    0x19, 0xE0,     /* Usage Minimum(Left Ctrl),*/
    0x29, 0xE7,     /* Usage Maximum(Right Win),*/
    0x15, 0x00,     /* Logical Minimum(0),*/
    0x25, 0x01,     /* Logical Maximum(1),*/
    0x75, 0x01,     /* Report Size(1),*/
    0x95, 0x08,     /* Report Count(8),*/
    0x81, 0x02,     /* Input(Data, Variable, Absolute),*/
    0x95, 0x01,     /* Report Count(1),*/
    0x75, 0x08,     /* Report Size(8),*/
    0x25, 0x65,     /* Logical Maximum(101),*/
    0x19, 0x00,     /* Usage Minimum(0),*/
    0x29, 0x65,     /* Usage Maximum(101),*/
    0x81, 0x00,     /* Input(Data, Array),*/
    0xC0,           /* End Collection */

    0x05, 0x0C,          /* USAGE_PAGE (Consumer devices), */
    0x09, 0x01,          /* USAGE (Consumer Control) */
    0xa1, 0x01,          /* COLLECTION (HID_FLAGS_COLLECTION_Application) */
    0x85, REPORT_ID_CONSUMER,  /* Report Id, */
    0x1A, 0x00, 0x00,    /* Usage Minimum(0x0),*/
    0x2A, 0xFF, 0x03,    /* Usage Maximum(0x3FF),*/
    0x16, 0x00, 0x00,    /* Logical Minimum(0),*/
    0x26, 0xFF, 0x03,    /* Logical Maximum(1023),*/
    0x75, 0x10,          /* Report Size(16),*/
    0x95, 0x01,          /* Report Count(1),*/
    0x81, 0x00,          /* Input(Data, Array),*/
    0xc0,                /* END_COLLECTION */
};

/*
    Keyboard Report Format:
    ._______________________________________________________________________________________________________________________
    |        |           |           |             |               |            |            |              |               |
    | Input  |    D7     |    D6     |    D5       |      D4       |     D3     |     D2     |      D1      |      D0       |
    |________|___________|___________|_____________|_______________|____________|____________|______________|_______________|
    |        |                                                                                                              |
    | Byte 0 |                               Report ID (REPORT_ID_KEYBOARD)                                                 |
    |________|______________________________________________________________________________________________________________|
    |        |           |           |             |               |            |            |              |               |
    | Byte 1 | Right GUI | Right Alt | Right Shift | Right Control |  Left GUI  |  Left Alt  |  Left Shift  |  Left Control |
    |________|___________|___________|_____________|_______________|____________|____________|______________|_______________|
    |        |                                                                                                              |
    | Byte 2 |                                     Page (0x07) Usage                                                        |
    |________|______________________________________________________________________________________________________________|


    Consumer Report Format:
    ._______________________________________________________________________________________________________________________
    |        |           |           |             |               |            |            |              |               |
    | Input  |    D7     |    D6     |    D5       |      D4       |     D3     |     D2     |      D1      |      D0       |
    |________|___________|___________|_____________|_______________|____________|____________|______________|_______________|
    |        |                                                                                                              |
    | Byte 0 |                               Report ID (REPORT_ID_CONSUMER)                                                 |
    |________|______________________________________________________________________________________________________________|
    |        |                                                                                                              |
    | Byte 2 |                                Consumer Page (0x0C) Hotkey                                                   |
    |________|______________________________________________________________________________________________________________|

*/
#include <pshpack1.h>
#pragma warning(disable:4214)  // suppress bit field types other than int warning
typedef struct _VirtualHidKeyboard_INPUT_REPORT
{
    // Report Id for the collection
    //
    BYTE ReportId;
    union
    {
        // Input Report for Keyboard. 
        //
        struct
        {
            // Bits 15-8 are the modifier bits for key.
            //
            union
            {
                struct
                {
                    BYTE LeftCtrl : 1;
                    BYTE LeftShift : 1;
                    BYTE LeftAlt : 1;
                    BYTE LeftWin : 1;
                    BYTE RightCtrl : 1;
                    BYTE RightShift : 1;
                    BYTE RightAlt : 1;
                    BYTE RightWin : 1;
                } ModifierKeyBits;
                BYTE ModifierKeyByte;
            } ModifierKeys;
            // Bits 7-0 are the HID Usage Code
            //
            BYTE Key;
        } KeyboardInput;

        // Input Report for Consumer device.
        // Bits 15-0 are the HID Usage Code
        //
        USHORT ConsumerInput;
    } Input;
} VirtualHidKeyboard_INPUT_REPORT;
#include <poppack.h>

_Must_inspect_result_
NTSTATUS
VirtualHidKeyboard_Toggle(
    _In_ DMFMODULE DmfModule,
    _In_ USHORT KeyToToggle,
    _In_ USHORT UsagePage
    )
/*++

Routine Description:

    Helper function to queue up a key toggle for the keyboard to report.

Arguments:

    DmfModule - This Module's handle.
    KeyToToggle - Keys to toggle.
    UsagePage - Usage Page for KeyToToggle

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_VirtualHidKeyboard* moduleContext;
    VirtualHidKeyboard_INPUT_REPORT inputReport;
    HID_XFER_PACKET hidXferPacket;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    RtlZeroMemory(&inputReport,
                  sizeof(inputReport));

    if (UsagePage == HID_USAGE_PAGE_KEYBOARD)
    {
        // Key data is a USHORT where the high byte is the keyboard modifier bit mask and
        // low byte is a key code.
        //
        // For Input Report format, see top of this file "Keyboard Report Format".
        //

        inputReport.Input.KeyboardInput.ModifierKeys.ModifierKeyByte = (KeyToToggle & 0xFF00) >> 8;
        inputReport.Input.KeyboardInput.Key = KeyToToggle & 0x00FF;
        inputReport.ReportId = REPORT_ID_KEYBOARD;
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                    "SEND: Modifier=0x%02X Key=0x%02X",
                    inputReport.Input.KeyboardInput.ModifierKeys.ModifierKeyByte,
                    inputReport.Input.KeyboardInput.Key);
    }
    else if (UsagePage == HID_USAGE_PAGE_CONSUMER)
    {
        inputReport.Input.ConsumerInput = KeyToToggle;
        inputReport.ReportId = REPORT_ID_CONSUMER;
    }
    else
    {
        DmfAssert(FALSE);
        ntStatus = STATUS_INVALID_PARAMETER_3;
        goto Exit;
    }

    hidXferPacket.reportBuffer = (UCHAR*)&inputReport;
    hidXferPacket.reportBufferLen = sizeof(VirtualHidKeyboard_INPUT_REPORT);
    hidXferPacket.reportId = inputReport.ReportId;

    ntStatus = DMF_VirtualHidDeviceVhf_ReadReportSend(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                      &hidXferPacket);

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Must_inspect_result_
NTSTATUS
VirtualHidKeyboard_Type(
    _In_ DMFMODULE DmfModule,
    _In_ PUSHORT KeysToType,
    _In_ ULONG NumberOfKeysToType,
    _In_ USHORT UsagePage
    )
/*++

Routine Description:

    Helper function to queue up a key sequence for the keyboard to report.

Arguments:

    DmfModule - This Module's handle.
    KeysToType - Array of keys to type.
    NumberOfKeysToType - Number of keys in KeysToType
    UsagePage - Usage Page for KeyToToggle

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_VirtualHidKeyboard* moduleContext;
    VirtualHidKeyboard_INPUT_REPORT inputReport;
    HID_XFER_PACKET hidXferPacket;
    BOOLEAN isKeyDown;
    ULONG keyIndex;

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_SUCCESS;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    isKeyDown = FALSE;
    keyIndex = 0;
    while (keyIndex < NumberOfKeysToType)
    {
        RtlZeroMemory(&inputReport,
                      sizeof(inputReport));
        if (UsagePage == HID_USAGE_PAGE_KEYBOARD)
        {
            // Modifier remains set for both key up and down events.
            //
            inputReport.Input.KeyboardInput.ModifierKeys.ModifierKeyByte = (KeysToType[keyIndex] & 0xFF00) >> 8;
            if (isKeyDown)
            {
                // Release all key presses, but keep the modifier on, and then advance to next key to send.
                // (No key is set because no key is sent.)
                //
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                            "SEND: Modifier=%02X Key=%02X",
                            inputReport.Input.KeyboardInput.ModifierKeys.ModifierKeyByte,
                            inputReport.Input.KeyboardInput.Key);
                isKeyDown = FALSE;
                // Down and up-strokes have been sent.
                //
                keyIndex++;
            }
            else
            {
                // Send modifier and key value. Index is not incremented because 
                // up-stroke needs to be sent.
                //
                inputReport.Input.KeyboardInput.Key = KeysToType[keyIndex] & 0x00FF;
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                            "SEND: Modifier=0x%02X Key=0x%02X",
                            inputReport.Input.KeyboardInput.ModifierKeys.ModifierKeyByte,
                            inputReport.Input.KeyboardInput.Key);
                isKeyDown = TRUE;
            }

            inputReport.ReportId = REPORT_ID_KEYBOARD;
        }
        else if (UsagePage == HID_USAGE_PAGE_CONSUMER)
        {
            if (isKeyDown)
            {
                // Release all key presses, but keep the modifier on, and then advance to next key to send.
                // (No key is set because no key is sent.)
                //
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                            "SEND: Key up 0x%02X",
                            KeysToType[keyIndex]);
                isKeyDown = FALSE;
                // Down and up-strokes have been sent.
                //
                keyIndex++;
            }
            else
            {
                // Send modifier and key value. Index is not incremented because 
                // up-stroke needs to be sent.
                //
                inputReport.Input.ConsumerInput = KeysToType[keyIndex];
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE,
                            "SEND: Key down 0x%02X",
                            KeysToType[keyIndex]);
                isKeyDown = TRUE;
            }

            inputReport.ReportId = REPORT_ID_CONSUMER;
        }

        hidXferPacket.reportBuffer = (UCHAR*)&inputReport;
        hidXferPacket.reportBufferLen = sizeof(VirtualHidKeyboard_INPUT_REPORT);
        hidXferPacket.reportId = inputReport.ReportId;

        ntStatus = DMF_VirtualHidDeviceVhf_ReadReportSend(moduleContext->DmfModuleVirtualHidDeviceVhf,
                                                          &hidXferPacket);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_Function_class_(CALLBACK_FUNCTION)
VOID
VirtualHidKeyboard_CallbackFunction(
    _In_ VOID* CallbackContext,
    _In_ VOID* Argument1,
    _In_ VOID* Argument2
    )
/*++

Routine Description:

    Callback function from external Client.

Arguments:

    CallbackContext - This Module's handle.
    Argument1 - Array of keys to type.
    Argument2 - Address of number of keys to type (ULONG*).

Return Value:

    None

--*/
{
    NTSTATUS ntStatus;
    DMFMODULE dmfModule;
    DMF_CONTEXT_VirtualHidKeyboard* moduleContext;
    PUSHORT stringToType;
    ULONG* lengthOfStringToType;

    FuncEntry(DMF_TRACE);

    dmfModule = DMFMODULEVOID_TO_MODULE(CallbackContext);
    moduleContext = DMF_CONTEXT_GET(dmfModule);
    stringToType = (PUSHORT)Argument1;
    lengthOfStringToType = (ULONG*)Argument2;

    ntStatus = DMF_VirtualHidKeyboard_Type(dmfModule,
                                           stringToType,
                                           *lengthOfStringToType,
                                           HID_USAGE_PAGE_KEYBOARD);
    if (! NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_VirtualHidKeyboard_Type fails: ntStatus=%!STATUS!", ntStatus);
    }

    FuncExitVoid(DMF_TRACE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// WDF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

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
DMF_VirtualHidKeyboard_Open(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type VirtualHidKeyboard.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualHidKeyboard* moduleConfig;
    DMF_CONTEXT_VirtualHidKeyboard* moduleContext;
    WDFDEVICE device;
    UNICODE_STRING virtualKeyboardCallbackName;
    OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    device = DMF_ParentDeviceGet(DmfModule);

    ntStatus = STATUS_SUCCESS;

    if ((moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Server) ||
        (moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Client))
    {
        // Same callback is created for both Server/Client. Standalone does not need or expose
        // callback.
        //
        DmfAssert(moduleConfig->ClientServerCallbackName != NULL);
        RtlUnicodeStringInit(&virtualKeyboardCallbackName,
                             moduleConfig->ClientServerCallbackName);
        InitializeObjectAttributes(&objectAttributes,
                                   &virtualKeyboardCallbackName,
                                   OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                                   NULL,
                                   NULL);
        ntStatus = ExCreateCallback(&moduleContext->CallbackObject,
                                    &objectAttributes,
                                    TRUE,
                                    TRUE);
        if (! NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "ExCreateCallback fails: ntStatus=%!STATUS!", ntStatus);
            goto Exit;
        }
    }

    if ((moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Server) ||
        (moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Standalone))
    {
        if (moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Server)
        {
#if defined(USE_DISABLE_CALLBACK_REGISTRATION)
            // For test purposes only.
            //
#else
            // Only Server registers the callback handler. Client does not handle it. Standalone does not
            // expose any callback.
            //
            DmfAssert(moduleContext->CallbackHandle != NULL);
            moduleContext->CallbackHandle = ExRegisterCallback(moduleContext->CallbackObject,
                                                               VirtualHidKeyboard_CallbackFunction,
                                                               DmfModule);
#endif // defined(USE_DISABLE_CALLBACK_REGISTRATION)
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
DMF_VirtualHidKeyboard_Close(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Uninitialize an instance of a DMF Module of type VirtualHidKeyboard.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    DMF_CONTEXT_VirtualHidKeyboard* moduleContext;
    DMF_CONFIG_VirtualHidKeyboard* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    moduleConfig = DMF_CONFIG_GET(DmfModule);

    if (moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Server)
    {
#if defined(USE_DISABLE_CALLBACK_REGISTRATION)
        // For test purposes only.
        //
#else
        // Only the Server has registered the callback.
        //
        DmfAssert(moduleContext->CallbackHandle != NULL);
        ExUnregisterCallback(moduleContext->CallbackHandle);
#endif // defined(USE_DISABLE_CALLBACK_REGISTRATION)
    }

    if ((moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Server) ||
        (moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Client))
    {
        DmfAssert(moduleContext->CallbackHandle != NULL);
        ObDereferenceObject(moduleContext->CallbackHandle);
        moduleContext->CallbackHandle = NULL;
    }

    FuncExitVoid(DMF_TRACE);
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_VirtualHidKeyboard_ChildModulesAdd(
    _In_ DMFMODULE DmfModule,
    _In_ DMF_MODULE_ATTRIBUTES* DmfParentModuleAttributes,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Configure and add the required Child Modules to the given Parent Module.

Arguments:

    DmfModule - The given Parent Module.
    DmfParentModuleAttributes - Pointer to the parent DMF_MODULE_ATTRIBUTES structure.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    None

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;
    DMF_CONFIG_VirtualHidKeyboard* moduleConfig;
    DMF_CONTEXT_VirtualHidKeyboard* moduleContext;
    DMF_CONFIG_VirtualHidDeviceVhf virtualHidDeviceVhfModuleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Client)
    {
        // Client just uses the callback...it does not need thread. Server and Standalone 
        // need the child thread.
        //
    }
    else
    {
        // VirtualHidDeviceVhf
        // -------------------
        //
        DMF_CONFIG_VirtualHidDeviceVhf_AND_ATTRIBUTES_INIT(&virtualHidDeviceVhfModuleConfig,
                                                           &moduleAttributes);

        virtualHidDeviceVhfModuleConfig.VendorId = moduleConfig->VendorId;
        virtualHidDeviceVhfModuleConfig.ProductId = moduleConfig->ProductId;
        virtualHidDeviceVhfModuleConfig.VersionNumber = 0x0001;

        virtualHidDeviceVhfModuleConfig.HidReportDescriptor = g_VirtualHidKeyboard_HidReportDescriptor;
        virtualHidDeviceVhfModuleConfig.HidReportDescriptorLength = sizeof(g_VirtualHidKeyboard_HidReportDescriptor);

        // Set virtual device attributes.
        //
        virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.VendorID = moduleConfig->VendorId;
        virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.ProductID = moduleConfig->ProductId;
        virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.VersionNumber = moduleConfig->VersionNumber;
        virtualHidDeviceVhfModuleConfig.HidDeviceAttributes.Size = sizeof(virtualHidDeviceVhfModuleConfig.HidDeviceAttributes);

        virtualHidDeviceVhfModuleConfig.StartOnOpen = TRUE;
        virtualHidDeviceVhfModuleConfig.VhfClientContext = DmfModule;

        DMF_DmfModuleAdd(DmfModuleInit,
                         &moduleAttributes,
                         WDF_NO_OBJECT_ATTRIBUTES,
                         &moduleContext->DmfModuleVirtualHidDeviceVhf);
    }

    FuncExitVoid(DMF_TRACE);
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
DMF_VirtualHidKeyboard_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type VirtualHidKeyboard.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_VirtualHidKeyboard;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_VirtualHidKeyboard;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_VirtualHidKeyboard);
    dmfCallbacksDmf_VirtualHidKeyboard.DeviceOpen = DMF_VirtualHidKeyboard_Open;
    dmfCallbacksDmf_VirtualHidKeyboard.DeviceClose = DMF_VirtualHidKeyboard_Close;
    dmfCallbacksDmf_VirtualHidKeyboard.ChildModulesAdd = DMF_VirtualHidKeyboard_ChildModulesAdd;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_VirtualHidKeyboard,
                                            VirtualHidKeyboard,
                                            DMF_CONTEXT_VirtualHidKeyboard,
                                            DMF_MODULE_OPTIONS_PASSIVE,
                                            DMF_MODULE_OPEN_OPTION_OPEN_PrepareHardware);

    dmfModuleDescriptor_VirtualHidKeyboard.CallbacksDmf = &dmfCallbacksDmf_VirtualHidKeyboard;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_VirtualHidKeyboard,
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

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualHidKeyboard_Toggle(
    _In_ DMFMODULE DmfModule,
    _In_ USHORT const KeyToToggle,
    _In_ USHORT UsagePage
    )
/*++

Routine Description:

    Toggle a key using virtual keyboard.

Arguments:

    DmfModule - This Module's handle.
    KeyToTogglee - Key to toggle.
    UsagePage - Usage Page for KeyToToggle.
    __________________________________________________________________________
    |       UsagePage         |                KeysToType[x]                  |
    |_________________________|_______________________________________________|
    |                         |                                               |
    | HID_USAGE_PAGE_KEYBOARD | Bits 15-8 are the modifier bits for key.      |
    |                         | Bits 7-0 are the HID Usage Code               |
    |_________________________|_______________________________________________|
    |                         |                                               |
    | HID_USAGE_PAGE_CONSUMER | Bits 15-0 are the HID Usage Code              |
    |                         |                                               |
    |_________________________|_______________________________________________|

Return Value:

    STATUS_SUCCESS if the key was toggled.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualHidKeyboard* moduleConfig;
    DMF_CONTEXT_VirtualHidKeyboard* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 VirtualHidKeyboard);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = VirtualHidKeyboard_Toggle(DmfModule,
                                         KeyToToggle,
                                         UsagePage);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_VirtualHidKeyboard_Type(
    _In_ DMFMODULE DmfModule,
    _In_ PUSHORT const KeysToType,
    _In_ ULONG NumberOfKeys,
    _In_ USHORT UsagePage
    )
/*++

Routine Description:

    Type a series of keys using virtual keyboard.

Arguments:

    DmfModule - This Module's handle.
    KeysToType - Array of keys to type.
    NumberOfKeys - Number of keys to type.
    UsagePage - Usage Page for KeyToToggle.
    __________________________________________________________________________
    |       UsagePage         |                KeysToType[x]                  |
    |_________________________|_______________________________________________|
    |                         |                                               |
    | HID_USAGE_PAGE_KEYBOARD | Bits 15-8 are the modifier bits for key.      |
    |                         | Bits 7-0 are the HID Usage Code               |
    |_________________________|_______________________________________________|
    |                         |                                               |
    | HID_USAGE_PAGE_CONSUMER | Bits 15-0 are the HID Usage Code              |
    |                         |                                               |
    |_________________________|_______________________________________________|

Return Value:

    STATUS_SUCCESS if a buffer is added to the list.
    Other NTSTATUS if there is an error.

--*/
{
    NTSTATUS ntStatus;
    DMF_CONFIG_VirtualHidKeyboard* moduleConfig;
    DMF_CONTEXT_VirtualHidKeyboard* moduleContext;

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 VirtualHidKeyboard);
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    moduleContext = DMF_CONTEXT_GET(DmfModule);

#if defined(USE_DISABLE_CALLBACK_REGISTRATION)
    // For test purposes only.
    //
    ExNotifyCallback(moduleContext->CallbackObject,
                     (VOID*)KeysToType,
                     &NumberOfKeys);
    ntStatus = STATUS_SUCCESS;
#else
    if ((moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Server) ||
        (moduleConfig->VirtualHidKeyboardMode == VirtualHidKeyboardMode_Standalone))
    {
        // This driver can type the keys.
        //
        ntStatus = VirtualHidKeyboard_Type(DmfModule,
                                           KeysToType,
                                           NumberOfKeys,
                                           UsagePage);
    }
    else
    {
        // An external driver types the keys.
        //
        ExNotifyCallback(moduleContext->CallbackObject,
                         (VOID*)KeysToType,
                         &NumberOfKeys);
        ntStatus = STATUS_SUCCESS;
    }
#endif // defined(USE_DISABLE_CALLBACK_REGISTRATION)

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

// eof: Dmf_VirtualHidKeyboard.c
//
