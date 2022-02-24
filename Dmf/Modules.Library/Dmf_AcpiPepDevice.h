/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_AcpiPepDevice.h

Abstract:

    Companion file to Dmf_AcpiPepDevice.c.

Environment:

    Kernel-mode Driver Framework

--*/

#pragma once

#if !defined(DMF_USER_MODE)

// pepfx.h is not compatible with pep_x.h. Clients that use pep_x.h must:
// 
// #define DMF_DONT_INCLUDE_PEPFX
// #include <DmfModules.Library.h>
//
#if !defined(DMF_DONT_INCLUDE_PEPFX)

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define PEP_MAKE_DEVICE_TYPE(Major, Minor, UniqueId) \
    (ULONG)(((ULONG)(Major) << 24) | \
    (((ULONG)(Minor) & 0xFF) << 16) |   \
    ((ULONG)(UniqueId) & 0xFFFF))

// ACPI method names.
//
#define ACPI_OBJECT_NAME_AC0  ((ULONG)'0CA_')
#define ACPI_OBJECT_NAME_AC1  ((ULONG)'1CA_')
#define ACPI_OBJECT_NAME_AC2  ((ULONG)'2CA_')
#define ACPI_OBJECT_NAME_AC3  ((ULONG)'3CA_')
#define ACPI_OBJECT_NAME_AC4  ((ULONG)'4CA_')
#define ACPI_OBJECT_NAME_AC5  ((ULONG)'5CA_')
#define ACPI_OBJECT_NAME_AC6  ((ULONG)'6CA_')
#define ACPI_OBJECT_NAME_AC7  ((ULONG)'7CA_')
#define ACPI_OBJECT_NAME_AC8  ((ULONG)'8CA_')
#define ACPI_OBJECT_NAME_AC9  ((ULONG)'9CA_')
#define ACPI_OBJECT_NAME_ADR  ((ULONG)'RDA_')
#define ACPI_OBJECT_NAME_AL0  ((ULONG)'0LA_')
#define ACPI_OBJECT_NAME_AL1  ((ULONG)'1LA_')
#define ACPI_OBJECT_NAME_AL2  ((ULONG)'2LA_')
#define ACPI_OBJECT_NAME_AL3  ((ULONG)'3LA_')
#define ACPI_OBJECT_NAME_AL4  ((ULONG)'4LA_')
#define ACPI_OBJECT_NAME_AL5  ((ULONG)'5LA_')
#define ACPI_OBJECT_NAME_AL6  ((ULONG)'6LA_')
#define ACPI_OBJECT_NAME_AL7  ((ULONG)'7LA_')
#define ACPI_OBJECT_NAME_AL8  ((ULONG)'8LA_')
#define ACPI_OBJECT_NAME_AL9  ((ULONG)'9LA_')
#define ACPI_OBJECT_NAME_BST  ((ULONG)'TSB_')
#define ACPI_OBJECT_NAME_CCA  ((ULONG)'ACC_')
#define ACPI_OBJECT_NAME_CID  ((ULONG)'DIC_')
#define ACPI_OBJECT_NAME_CLS  ((ULONG)'SLC_')
#define ACPI_OBJECT_NAME_CRS  ((ULONG)'SRC_')
#define ACPI_OBJECT_NAME_CRT  ((ULONG)'TRC_')
#define ACPI_OBJECT_NAME_DCK  ((ULONG)'KCD_')
#define ACPI_OBJECT_NAME_DDN  ((ULONG)'NDD_')
#define ACPI_OBJECT_NAME_DEP  ((ULONG)'PED_')
#define ACPI_OBJECT_NAME_DIS  ((ULONG)'SID_')
#define ACPI_OBJECT_NAME_DLM  ((ULONG)'MLD_')
#define ACPI_OBJECT_NAME_DSM  ((ULONG)'MSD_')
#define ACPI_OBJECT_NAME_DSW  ((ULONG)'WSD_')
#define ACPI_OBJECT_NAME_DTI  ((ULONG)'ITD_')
#define ACPI_OBJECT_NAME_EJD  ((ULONG)'DJE_')
#define ACPI_OBJECT_NAME_EJ0  ((ULONG)'0JE_')
#define ACPI_OBJECT_NAME_EJ1  ((ULONG)'1JE_')
#define ACPI_OBJECT_NAME_EJ2  ((ULONG)'2JE_')
#define ACPI_OBJECT_NAME_EJ3  ((ULONG)'3JE_')
#define ACPI_OBJECT_NAME_EJ4  ((ULONG)'4JE_')
#define ACPI_OBJECT_NAME_EJ5  ((ULONG)'5JE_')
#define ACPI_OBJECT_NAME_FST  ((ULONG)'TSF_')
#define ACPI_OBJECT_NAME_GHID ((ULONG)'DIHG')
#define ACPI_OBJECT_NAME_HID  ((ULONG)'DIH_')
#define ACPI_OBJECT_NAME_HRV  ((ULONG)'VRH_')
#define ACPI_OBJECT_NAME_HOT  ((ULONG)'TOH_')
#define ACPI_OBJECT_NAME_INI  ((ULONG)'INI_')
#define ACPI_OBJECT_NAME_IRC  ((ULONG)'CRI_')
#define ACPI_OBJECT_NAME_LCK  ((ULONG)'KCL_')
#define ACPI_OBJECT_NAME_LID  ((ULONG)'DIL_')
#define ACPI_OBJECT_NAME_MAT  ((ULONG)'TAM_')
#define ACPI_OBJECT_NAME_NTT  ((ULONG)'TTN_')
#define ACPI_OBJECT_NAME_OFF  ((ULONG)'FFO_')
#define ACPI_OBJECT_NAME_ON   ((ULONG)'_NO_')
#define ACPI_OBJECT_NAME_OSC  ((ULONG)'CSO_')
#define ACPI_OBJECT_NAME_OST  ((ULONG)'TSO_')
#define ACPI_OBJECT_NAME_PCCH ((ULONG)'HCCP')
#define ACPI_OBJECT_NAME_PR0  ((ULONG)'0RP_')
#define ACPI_OBJECT_NAME_PR1  ((ULONG)'1RP_')
#define ACPI_OBJECT_NAME_PR2  ((ULONG)'2RP_')
#define ACPI_OBJECT_NAME_PR3  ((ULONG)'3RP_')
#define ACPI_OBJECT_NAME_PRS  ((ULONG)'SRP_')
#define ACPI_OBJECT_NAME_PRT  ((ULONG)'TRP_')
#define ACPI_OBJECT_NAME_PRW  ((ULONG)'WRP_')
#define ACPI_OBJECT_NAME_PS0  ((ULONG)'0SP_')
#define ACPI_OBJECT_NAME_PS1  ((ULONG)'1SP_')
#define ACPI_OBJECT_NAME_PS2  ((ULONG)'2SP_')
#define ACPI_OBJECT_NAME_PS3  ((ULONG)'3SP_')
#define ACPI_OBJECT_NAME_PSC  ((ULONG)'CSP_')
#define ACPI_OBJECT_NAME_PSL  ((ULONG)'LSP_')
#define ACPI_OBJECT_NAME_PSV  ((ULONG)'VSP_')
#define ACPI_OBJECT_NAME_PSW  ((ULONG)'WSP_')
#define ACPI_OBJECT_NAME_PTS  ((ULONG)'STP_')
#define ACPI_OBJECT_NAME_REG  ((ULONG)'GER_')
#define ACPI_OBJECT_NAME_RMV  ((ULONG)'VMR_')
#define ACPI_OBJECT_NAME_S0   ((ULONG)'_0S_')
#define ACPI_OBJECT_NAME_S0D  ((ULONG)'D0S_')
#define ACPI_OBJECT_NAME_S0W  ((ULONG)'W0S_')
#define ACPI_OBJECT_NAME_S1   ((ULONG)'_1S_')
#define ACPI_OBJECT_NAME_S1D  ((ULONG)'D1S_')
#define ACPI_OBJECT_NAME_S1W  ((ULONG)'W1S_')
#define ACPI_OBJECT_NAME_S2   ((ULONG)'_2S_')
#define ACPI_OBJECT_NAME_S2D  ((ULONG)'D2S_')
#define ACPI_OBJECT_NAME_S2W  ((ULONG)'W2S_')
#define ACPI_OBJECT_NAME_S3   ((ULONG)'_3S_')
#define ACPI_OBJECT_NAME_S3D  ((ULONG)'D3S_')
#define ACPI_OBJECT_NAME_S3W  ((ULONG)'W3S_')
#define ACPI_OBJECT_NAME_S4   ((ULONG)'_4S_')
#define ACPI_OBJECT_NAME_S4D  ((ULONG)'D4S_')
#define ACPI_OBJECT_NAME_S4W  ((ULONG)'W4S_')
#define ACPI_OBJECT_NAME_S5   ((ULONG)'_5S_')
#define ACPI_OBJECT_NAME_S5D  ((ULONG)'D5S_')
#define ACPI_OBJECT_NAME_S5W  ((ULONG)'W5S_')
#define ACPI_OBJECT_NAME_SCP  ((ULONG)'PCS_')
#define ACPI_OBJECT_NAME_SEG  ((ULONG)'GES_')
#define ACPI_OBJECT_NAME_SI   ((ULONG)'_IS_')
#define ACPI_OBJECT_NAME_SRS  ((ULONG)'SRS_')
#define ACPI_OBJECT_NAME_SST  ((ULONG)'TSS_')
#define ACPI_OBJECT_NAME_STA  ((ULONG)'ATS_')
#define ACPI_OBJECT_NAME_STD  ((ULONG)'DTS_')
#define ACPI_OBJECT_NAME_SUB  ((ULONG)'BUS_')
#define ACPI_OBJECT_NAME_SUN  ((ULONG)'NUS_')
#define ACPI_OBJECT_NAME_SWD  ((ULONG)'DWS_')
#define ACPI_OBJECT_NAME_TC1  ((ULONG)'1CT_')
#define ACPI_OBJECT_NAME_TC2  ((ULONG)'2CT_')
#define ACPI_OBJECT_NAME_TMP  ((ULONG)'PMT_')
#define ACPI_OBJECT_NAME_TSP  ((ULONG)'PST_')
#define ACPI_OBJECT_NAME_TZD  ((ULONG)'DZT_')
#define ACPI_OBJECT_NAME_UID  ((ULONG)'DIU_')
#define ACPI_OBJECT_NAME_WAK  ((ULONG)'KAW_')
#define ACPI_OBJECT_NAME_BBN  ((ULONG)'NBB_')
#define ACPI_OBJECT_NAME_PXM  ((ULONG)'MXP_')
#define ACPI_OBJECT_NAME_PLD  ((ULONG)'DLP_')
#define ACPI_OBJECT_NAME_REV  ((ULONG)'VER_')

// Used by AcpiPepDevice and its children to indicate to PEP
// how the callback processed the request.
//
typedef enum _PEP_NOTIFICATION_HANDLER_RESULT
{
    PEP_NOTIFICATION_HANDLER_COMPLETE,
    PEP_NOTIFICATION_HANDLER_MORE_WORK,
    PEP_NOTIFICATION_HANDLER_MAX
} PEP_NOTIFICATION_HANDLER_RESULT;

 typedef ULONG _PEP_DEVICE_TYPE, PEP_DEVICE_TYPE;

 // Indicates to the framework if the ACPI device ID matched fully
 // or partially with the device ID in PEP tables.
 //
 typedef enum _PEP_DEVICE_ID_MATCH
 {
    // Substring match.
    //
    PepDeviceIdMatchPartial,
    // Whole string match.
    //
    PepDeviceIdMatchFull,
} PEP_DEVICE_ID_MATCH;

// This enumerator helps with registration, currently we only support
// ACPI class but Platform Extensions can also support DPM and PPM.
//
typedef enum _PEP_NOTIFICATION_CLASS
{
    PEP_NOTIFICATION_CLASS_NONE = 0,
    PEP_NOTIFICATION_CLASS_ACPI = 1,
    PEP_NOTIFICATION_CLASS_DPM = 2,
    PEP_NOTIFICATION_CLASS_PPM = 4
} PEP_NOTIFICATION_CLASS;

// Part of the ACPI registration tables used by all children,
// this structure is used to indicate ACPI device name among other details.
//
 typedef struct _PEP_DEVICE_MATCH
 {
    PEP_DEVICE_TYPE Type;
    PEP_NOTIFICATION_CLASS OwnedType;
    PWSTR DeviceId;
    PEP_DEVICE_ID_MATCH CompareMethod;
} PEP_DEVICE_MATCH;

// Part of the ACPI registration tables used by all children,
// this structure is used to indicate an ACPI method and its details.
//
typedef struct _PEP_OBJECT_INFORMATION
{
    ULONG ObjectName;
    ULONG InputArgumentCount;
    ULONG OutputArgumentCount;
    PEP_ACPI_OBJECT_TYPE ObjectType;
} PEP_OBJECT_INFORMATION;

typedef
VOID
PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE (
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Data
    );

typedef
PEP_NOTIFICATION_HANDLER_RESULT
PEP_NOTIFICATION_HANDLER_ROUTINE (
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Data,
    _Inout_opt_ PEP_WORK_INFORMATION* PoFxWorkInformation
    );

// This handler is used during the device initialization.
//
typedef struct _PEP_GENERAL_NOTIFICATION_HANDLER
{
    ULONG Notification;
    PEP_GENERAL_NOTIFICATION_HANDLER_ROUTINE* Handler;
    CHAR* Name;
} PEP_GENERAL_NOTIFICATION_HANDLER;

// This structure is what the Data void pointer can be cast to when a
// Method callback is called into by the framework.
//
typedef struct _PEP_DEVICE_NOTIFICATION_HANDLER
{
    ULONG Notification;
    PEP_NOTIFICATION_HANDLER_ROUTINE* Handler;
    PEP_NOTIFICATION_HANDLER_ROUTINE* WorkerCallbackHandler;
} PEP_DEVICE_NOTIFICATION_HANDLER;

struct _PEP_INTERNAL_DEVICE_HEADER;

typedef
NTSTATUS
PEP_DEVICE_CONTEXT_INITIALIZE (
    _In_ DMFMODULE DmfModule,
    _In_ struct _PEP_INTERNAL_DEVICE_HEADER* Context
    );

// Child PEP devices need to fill out this table with routines such as ACPI Method handlers and ACPI Objects.
//
typedef struct _PEP_DEVICE_DEFINITION
{
    PEP_DEVICE_TYPE Type;
    ULONG ContextSize;
    PEP_DEVICE_CONTEXT_INITIALIZE* Initialize;
    ULONG ObjectCount;
    _Field_size_(ObjectCount) PEP_OBJECT_INFORMATION* Objects;
    ULONG AcpiNotificationHandlerCount;
    _Field_size_(AcpiNotificationHandlerCount) PEP_DEVICE_NOTIFICATION_HANDLER* AcpiNotificationHandlers;
    ULONG DpmNotificationHandlerCount;
    _Field_size_(DpmNotificationHandlerCount) PEP_DEVICE_NOTIFICATION_HANDLER* DpmNotificationHandlers;
    DMFMODULE DmfModule;
} PEP_DEVICE_DEFINITION;

// This enumerator is required by the framework to identify PEP Device Major type.
//
typedef enum _PEP_MAJOR_DEVICE_TYPE
{
    PepMajorDeviceTypeProcessor,
    PepMajorDeviceTypeAcpi,
    PepMajorDeviceTypeMaximum
} PEP_MAJOR_DEVICE_TYPE;

// 8 bits maximum.
//
C_ASSERT(PepMajorDeviceTypeMaximum <= 0xFF);

// This enumerator is required by the framework to identify PEP Device Minor type.
//
typedef enum _PEP_ACPI_MINOR_DEVICE_TYPE
{
    PepAcpiMinorTypeDevice,
    PepAcpiMinorTypePowerResource,
    PepAcpiMinorTypeThermalZone,
    PepAcpiMinorTypeMaximum
} PEP_ACPI_MINOR_DEVICE_TYPE;

// 8 bits maximum.
//
C_ASSERT(PepAcpiMinorTypeMaximum <= 0xFF);

#define PEP_DEVICE_TYPE_ROOT \
    PEP_MAKE_DEVICE_TYPE(PepMajorDeviceTypeAcpi, \
                         PepAcpiMinorTypeDevice, \
                         0x0)

// This header is the sole mode of identification for a PEP device for both
// AcpiPepDevice and the Platform Extensions.
//
typedef struct _PEP_INTERNAL_DEVICE_HEADER
{
    LIST_ENTRY ListEntry;
    PEP_DEVICE_TYPE DeviceType;
    POHANDLE KernelHandle;
    PWSTR InstancePath;
    PEP_DEVICE_DEFINITION* DeviceDefinition;
    WDFMEMORY PepInternalDeviceMemory;
    DMFMODULE DmfModule;
} PEP_INTERNAL_DEVICE_HEADER;

// PEP_ACPI_DEVICE structure encapsulates the internal header
// which identifies a PEP device.
//
typedef struct _PEP_ACPI_DEVICE
{
    PEP_INTERNAL_DEVICE_HEADER Header;
} PEP_ACPI_DEVICE;

// This structure is used by AcpiNotify helper Method to
// indicate the device and notification code.
//
typedef struct _PEP_ACPI_NOTIFY_CONTEXT
{
    PEP_INTERNAL_DEVICE_HEADER* PepInternalDevice;
    ULONG NotifyCode;
} PEP_ACPI_NOTIFY_CONTEXT;

// These tables are initialized by all child PEP devices and contain all the
// information needed to register them with PEP.
//
typedef struct _PEP_ACPI_REGISTRATION_TABLES
{
    WDFMEMORY AcpiDefinitionTable;
    WDFMEMORY AcpiMatchTable;
} PEP_ACPI_REGISTRATION_TABLES;

// Enumerator specifying child Module device type.
//
typedef enum _AcpiPepDevice_DEVICE_TYPE
{
    AcpiPepDeviceType_Invalid = 0,
    AcpiPepDeviceType_Fan,
    AcpiPepDeviceType_Maximum,
} AcpiPepDevice_DEVICE_TYPE;

// Structure used to pass PEP child devices to this Module.
//
typedef struct _AcpiPepDevice_CHILD_CONFIGURATIONS
{
    AcpiPepDevice_DEVICE_TYPE PepDeviceType;
    VOID* PepDeviceConfiguration;
} AcpiPepDevice_CHILD_CONFIGURATIONS;

// Client uses this structure to configure the Module specific parameters.
//
typedef struct
{
    // Client can pass an array of child PEP devices that this Module will instantiate.
    //
    AcpiPepDevice_CHILD_CONFIGURATIONS* ChildDeviceConfigurationArray;
    // Number of child configuration structures placed in ChildDeviceConfigurationArray.
    //
    ULONG ChildDeviceArraySize;
} DMF_CONFIG_AcpiPepDevice;

// This macro declares the following functions:
// DMF_AcpiPepDevice_ATTRIBUTES_INIT()
// DMF_AcpiPepDevice_Create()
//
DECLARE_DMF_MODULE(AcpiPepDevice)

// Module Methods
//

_IRQL_requires_max_(DISPATCH_LEVEL)
PEP_NOTIFICATION_HANDLER_RESULT
DMF_AcpiPepDevice_AsyncNotifyEvent(
    _In_ DMFMODULE DmfModule,
    _In_ VOID* Data,
    _Inout_opt_ PEP_WORK_INFORMATION* PoFxWorkInformation
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
DMFMODULE*
DMF_AcpiPepDevice_ChildHandlesReturn(
    _In_ DMFMODULE DmfModule
    );

_Success_(NT_SUCCESS(ntStatus))
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_AcpiPepDevice_PepAcpiDataReturn(
    _In_ VOID* Value,
    _In_ USHORT ValueType,
    _In_ ULONG ValueLength,
    _In_ BOOLEAN ReturnAsPackage,
    _Inout_ PACPI_METHOD_ARGUMENT Arguments,
    _Inout_ SIZE_T* OutputArgumentSize,
    _Out_opt_ ULONG* OutputArgumentCount,
    _Out_ NTSTATUS* NtStatus,
    _In_opt_ CHAR* MethodName,
    _In_opt_ CHAR* DebugInfo,
    _Out_ PEP_NOTIFICATION_HANDLER_RESULT* CompleteResult
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_AcpiPepDevice_ReportNotSupported(
    _In_ DMFMODULE DmfModule,
    _Out_ NTSTATUS* Status,
    _Out_ ULONG* Count,
    _Out_ PEP_NOTIFICATION_HANDLER_RESULT* CompleteResult
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_AcpiPepDevice_ScheduleNotifyRequest(
    _In_ DMFMODULE DmfModule,
    _In_ PEP_ACPI_NOTIFY_CONTEXT* NotifyContext
    );

#endif // !defined(DMF_DONT_INCLUDE_PEPFX)

#endif // defined(DMF_USER_MODE)

// eof: Dmf_AcpiPepDevice.h
//
