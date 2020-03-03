/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_MobileBroadband.cpp

Abstract:

    This Module that can be used to do MobileBroadband operation.
    NOTE: This Module uses C++/WinRT so it needs RS5+ support.
          Module specific code will not be compiled in RS4 and below.

Environment:

    User-mode Driver Framework

--*/

// DMF and this Module's Library specific definitions.
//
#include "DmfModule.h"
#include "DmfModules.Library.h"
#include "DmfModules.Library.Trace.h"

#include "Dmf_MobileBroadband.tmh"

// Only support 19H1 and above because of library size limitations on RS5.
//
#if IS_WIN10_19H1_OR_LATER

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// Specific includes for this Module.
//
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Networking.NetworkOperators.h"

using namespace std;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Enumeration;
using namespace winrt::Windows::Networking::NetworkOperators;

class MobileBroadbandModemDevice 
{
private:

    // DeviceWatcher for Modem.
    //
    DeviceWatcher modemWatcher = nullptr;

    // DeviceWatcher necessary events token, need to register all these 
    // events to make it work, and use for unregister.
    //
    event_token tokenAdded;
    event_token tokenRemoved;
    event_token tokenUpdated;
    event_token tokenEnumCompleted;

public:

    // Instance of mobile broadband modem.
    //
    MobileBroadbandModem modem = nullptr;
    // Store the Device Id of modem that is found.
    //
    hstring modemId = L"";
    // Instance of C++/WinRT SarManager from modem.
    //
    MobileBroadbandSarManager sarManager = nullptr;
    // MobileBroadband wireless state.
    //
    MobileBroadband_WIRELESS_STATE mobileBroadbandWirelessState = { 0 };  
    // Event token for transmission state change.
    //
    event_token tokenTransmissionStateChanged;
    // Initialize route.
    //
    NTSTATUS Initialize(DMFMODULE DmfModule);
    // DeInitialize route.
    //
    VOID DeInitialize();
    // Check if MobileBroadband network is connected.
    //
    BOOLEAN MobileBroadband_IsNetworkConnected();
    // Check if MobileBroadband network is transmitting.
    //
    BOOLEAN MobileBroadband_IsTransmitting();
    // Function for start monitoring MobileBroadband network transmission state.
    //
    VOID MobileBroadbandModemDevice::MobileBroadband_TransmissionStateMonitorStart(DMFMODULE DmfModule);
    // Function for stop monitoring MobileBroadband network transmission state.
    //
    VOID MobileBroadbandModemDevice::MobileBroadband_TransmissionStateMonitorStop();
};

// MobileBroadband's module context.
//
typedef struct
{
    // MobileBroadbandModemDevice class instance pointer.
    //
    MobileBroadbandModemDevice* ModemDevice = nullptr;
    // Wireless state change callback.
    //
    EVT_DMF_MobileBroadband_WirelessStateChangeCallback* EvtMobileBroadbandWirelessStateChangeCallback;

} DMF_CONTEXT_MobileBroadband;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(MobileBroadband)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(MobileBroadband)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#define AntennaBackOffTableIndexMin                     0
#define AntennaBackOffTableIndexMax                     8
#define DefaultAntennaIndex                             0
#define DefaultBackOffTableIndex                        0
#define MccMncReportLengthMinimum                       5
#define MccMncReportLengthMaximum                       6
#define RetryTimesAmount                                10
#define WaitTimeMilliseconds                            1000
#define MobileBroadband_AdapterWatcherEventLockIndex    0
#define MobileBroadband_AuxiliaryLockCount              1

_IRQL_requires_max_(PASSIVE_LEVEL)
inline
NTSTATUS MobileCodeCalulate(
    _In_ const wchar_t* ProviderId,
    _In_ int StartPosition, 
    _In_ int CodeLength,
    _Out_ DWORD* Result
    )
/*++

Routine Description:

    Inline function for calculate the mobile related code.

Arguments:

    ProviderId - Pointer to provider Id const wchar array.
    StartPosition - The start index from which to start calculate.
    CodeLength - The desired length of mobile code.
    Result - The three digit number result to return.

Return Value:

    NTSTATUS

    --*/
{
    DWORD result = 0;
    for (int providerIdIndex = 0; providerIdIndex < CodeLength; providerIdIndex++) 
    {
        if (ProviderId[providerIdIndex + StartPosition] >= '0' && 
            ProviderId[providerIdIndex + StartPosition] <= '9')
        {
            result = result * 10 + (ProviderId[providerIdIndex + StartPosition] - '0');
        }
        else
        {
            return STATUS_INVALID_PARAMETER;
        }
    }
    *Result = result;
    return STATUS_SUCCESS;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
MobileBroadbandModemDevice::DeInitialize()
/*++

Routine Description:

    DeInitialize MobileBroadbandModemDevice class instance.

Arguments:

    None

Return Value:

    NTSTATUS

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // First unhook all event handlers. This ensures our
    // event handlers don't get called after stop.
    //
    modemWatcher.Added(tokenAdded);
    modemWatcher.Removed(tokenRemoved);
    modemWatcher.Updated(tokenUpdated);
    modemWatcher.EnumerationCompleted(tokenEnumCompleted);

    if (modemWatcher.Status() == DeviceWatcherStatus::Started ||
        modemWatcher.Status() == DeviceWatcherStatus::EnumerationCompleted)
    {
        modemWatcher.Stop();
    }

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
MobileBroadbandModemDevice::Initialize(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize MobileBroadbandModemDevice class instance.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Get MobileBroadbandModem specific selector for device watcher.
    //
    auto mbbSelector = MobileBroadbandModem::GetDeviceSelector();	
    modemWatcher = DeviceInformation::CreateWatcher(mbbSelector, nullptr);

    // Using lambda function is necessary here, because it need access variables that outside function scope,
    // but these callbacks don't have void pointer to pass in.
    //
    // Event handler for MobileBroadband interface add event.
    //
    TypedEventHandler deviceInfoAddedHandler = TypedEventHandler<DeviceWatcher,
                                                                 DeviceInformation>([=](DeviceWatcher sender, 
                                                                                                    DeviceInformation args)
    {
        DMF_CONTEXT_MobileBroadband* moduleContext;
        NTSTATUS ntStatus;

        UNREFERENCED_PARAMETER(sender);
        
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem deviceInfoAddedHandler triggered");

        moduleContext = DMF_CONTEXT_GET(DmfModule);
        DMF_ModuleAuxiliaryLock(DmfModule,
                                MobileBroadband_AdapterWatcherEventLockIndex);
        // New modem interface arrived.
        //
        if (modem == nullptr)
        {
            try
            {
                // MobileBroadbandModem is not ready right after modem interface arrived.
                // Need for wait for MobileBroadbandModem be available. Otherwise modem get will return nullptr.
                // This wait will not block other threads.
                //
                for (int tryTimes = 0; tryTimes < RetryTimesAmount; tryTimes++)
                {
                    Sleep(WaitTimeMilliseconds);
                    modem = MobileBroadbandModem::GetDefault();
                    if (modem != nullptr)
                    {
                        auto config = modem.GetCurrentConfigurationAsync().get();
                        // This call return success only when used in RS5 and above! Otherwise will raise 0x80070005 access deny error.
                        //
                        sarManager = config.SarManager();

                        // This check is a workaround. WinRT Api has a bug that causes above call to silently fail. Once it's fixed the
                        // call will raise hresult_error ERROR_NOT_SUPPORTED.
                        //
                        if (sarManager == nullptr)
                        {
                            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Get MobileBroadbandSarManager fails");
                            modem = nullptr;
                            break;
                        }
                        // Open this module.
                        //
                        ntStatus = DMF_ModuleOpen(DmfModule);
                        if (!NT_SUCCESS(ntStatus))
                        {
                            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleOpen fails, ntStatus = %!STATUS!", ntStatus);
                            modem = nullptr;
                            sarManager = nullptr;
                            break;
                        }
                        mobileBroadbandWirelessState.IsModemValid = TRUE;
                        modemId = args.Id();
                        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem is found , device Id is %ws", modemId.c_str());
                        MobileBroadband_TransmissionStateMonitorStart(DmfModule);
                        break;
                    }
                }
                if (modem == nullptr)
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not found");
                    sarManager = nullptr;
                    mobileBroadbandWirelessState.IsModemValid = FALSE;
                }
            }
            catch (hresult_error ex)
            {
                TraceEvents(TRACE_LEVEL_ERROR,
                            DMF_TRACE,
                            "could not get valid MobileBroadbandModem, error code 0x%08x - %ws", 
                            ex.code().value,
                            ex.message().c_str());
                modem = nullptr;
                sarManager = nullptr;
                mobileBroadbandWirelessState.IsModemValid = FALSE;
            }
        }
        // Only one instance of mobile broad band is supported.
        //
        else
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem is found already. Only one modem is supported");
        }

        DMF_ModuleAuxiliaryUnlock(DmfModule,
                                  MobileBroadband_AdapterWatcherEventLockIndex);
    });

    // Event handler for MobileBroadband interface remove event.
    //
    TypedEventHandler deviceInfoRemovedHandler = TypedEventHandler<DeviceWatcher,
                                                                   DeviceInformationUpdate>([=](DeviceWatcher sender,
                                                                                                DeviceInformationUpdate args)
    {
        DMF_CONTEXT_MobileBroadband* moduleContext;

        UNREFERENCED_PARAMETER(sender);

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem deviceInfoRemovedHandler triggered");

        moduleContext = DMF_CONTEXT_GET(DmfModule);
        DMF_ModuleAuxiliaryLock(DmfModule,
                                MobileBroadband_AdapterWatcherEventLockIndex);

        // modem interface removed.
        //
        if (modem != nullptr)
        {
            if (modemId == args.Id())
            {
                // Removed modem interface is our modem.
                // Bug in C++/WinRT, sarManager.StopTransmissionStateMonitoring() and 
                // sarManager.TransmissionStateChanged(tokenTransmissionStateChanged) should be called here,
                // but it will never return.

                // Close this module.
                //
                DMF_ModuleClose(DmfModule);
                modem = nullptr;
                sarManager = nullptr;
                mobileBroadbandWirelessState.IsModemValid = FALSE;
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem has been removed");
            }
            else
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Removed MobileBroadband interface is not our modem");
            }
        }
        else
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem is not present already");
        }

        DMF_ModuleAuxiliaryUnlock(DmfModule,
                                  MobileBroadband_AdapterWatcherEventLockIndex);

    });

    TypedEventHandler deviceInfoUpdatedHandler = TypedEventHandler<DeviceWatcher,
                                                                   DeviceInformationUpdate>([&](DeviceWatcher sender,
                                                                                                DeviceInformationUpdate args) 
    {
        // Update information is unused.
        // This function is needed for DeviceWatcher registration.
        //
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);
    });

    TypedEventHandler deviceInfoEnumCompletedHandler = TypedEventHandler<DeviceWatcher,
                                                                         IInspectable>([&](DeviceWatcher sender,
                                                                                           IInspectable args) 
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "ModemWatcher enumeration complete");
    });

    if (modemWatcher == nullptr)
    {
        ntStatus = STATUS_UNSUCCESSFUL;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Fail to create modemWatcher");
        goto Exit;
    }

    // Register event callbacks.
    //
    tokenAdded = modemWatcher.Added(deviceInfoAddedHandler);
    tokenRemoved = modemWatcher.Removed(deviceInfoRemovedHandler);
    tokenUpdated = modemWatcher.Updated(deviceInfoUpdatedHandler);
    tokenEnumCompleted = modemWatcher.EnumerationCompleted(deviceInfoEnumCompletedHandler);

    // start device watcher
    //
    modemWatcher.Start();

    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
MobileBroadbandModemDevice::MobileBroadband_IsNetworkConnected()
/*++

Routine Description:

    Check if network is connected or not.

Arguments:

    None

Return Value:

    BOOLEAN - TURE for connected. FALSE for disconnected.

    --*/
{
    BOOLEAN isConnected = FALSE;
    MobileBroadbandNetwork currentNetwork = nullptr;
    NetworkRegistrationState networkRegistrationState;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    try
    {
        currentNetwork = modem.CurrentNetwork();

        if (currentNetwork == nullptr)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No network component found");
            goto Exit;
        }

        networkRegistrationState = currentNetwork.NetworkRegistrationState();

        if ((networkRegistrationState == NetworkRegistrationState::Denied) ||
            (networkRegistrationState == NetworkRegistrationState::Deregistered) ||
            (networkRegistrationState == NetworkRegistrationState::None))
        {
            isConnected = FALSE;
        }
        else
        {
            isConnected = TRUE;
        }
    }
    catch (hresult_error ex)
    {
        // At certain point. C++/WinRT sarManager component will raise 0x8007008 not enough memory exception due to a bug.
        // If that happen, don't change the IsTransmitting state.
        //
        isConnected = FALSE;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Get CurrentNetwork fails, error code 0x%08x - %ws",
                    ex.code().value,
                    ex.message().c_str());
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "isConnected = %d", isConnected);

    return isConnected;

}

_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
MobileBroadbandModemDevice::MobileBroadband_IsTransmitting()
/*++

Routine Description:

    Check if network is transmitting data or not.

Arguments:

    None.

Return Value:

    BOOLEAN - TURE for transmitting. FALSE for not transmitting.

    --*/
{
    BOOLEAN isTransmitting = FALSE;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    try
    {
        isTransmitting = (BOOLEAN)sarManager.GetIsTransmittingAsync().get();
    }
    catch (hresult_error ex)
    {
        // At certain point. C++/WinRT sarManager component will raise 0x8007008 not enough memory exception due to a bug.
        // If that happen, don't change the IsTransmitting state.
        //
        isTransmitting = FALSE;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "GetIsTransmittingAsync fails, error code 0x%08x - %ws",
                    ex.code().value,
                    ex.message().c_str());
        goto Exit;
    }

Exit:

    FuncExit(DMF_TRACE, "isTransmitting = %d", isTransmitting);

    return isTransmitting;

}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
MobileBroadbandModemDevice::MobileBroadband_TransmissionStateMonitorStart(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Register transmission state change callback and start monitoring.

Arguments:

    DmfModule - This module's Dmf Module.

Return Value:

    None.

    --*/
{
    DMF_CONTEXT_MobileBroadband* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // This moduleContext get function MUST happen before Lambda function. Otherwise lambda function will have wrong context.
    //
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    // Event handler lambda function for transmission state change.
    // It will perform a network connection check when arrived.
    //
    TypedEventHandler mobileBroadbandTransmissionStateChanged = TypedEventHandler<MobileBroadbandSarManager,
                                                                                  MobileBroadbandTransmissionStateChangedEventArgs>([=](MobileBroadbandSarManager sender,
                                                                                                                                        MobileBroadbandTransmissionStateChangedEventArgs args)
    {
        NTSTATUS ntStatus;
        // Bug inside C++/WinRT SarManager. This event can still be called even if modem is gone.
        // Need to use DMF_ModuleReference() to protect.
        //
        ntStatus = DMF_ModuleReference(DmfModule);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not open yet.");
            return;
        }
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "mobileBroadbandWirelessState event triggered");
        mobileBroadbandWirelessState.IsTransmitting = (BOOLEAN)args.IsTransmitting();
        mobileBroadbandWirelessState.IsNetworkConnected = MobileBroadband_IsNetworkConnected();
        // Call back parent module.
        //
        if (moduleContext->EvtMobileBroadbandWirelessStateChangeCallback != nullptr) 
        {
            moduleContext->EvtMobileBroadbandWirelessStateChangeCallback(DmfModule, 
                                                                         &mobileBroadbandWirelessState);
        }
        DMF_ModuleDereference(DmfModule);
    });

    try
    {
        tokenTransmissionStateChanged = sarManager.TransmissionStateChanged(mobileBroadbandTransmissionStateChanged);
        sarManager.StartTransmissionStateMonitoring();
    }
    catch (hresult_error ex)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "StartTransmissionStateMonitoring fails, error code 0x%08x - %ws",
                    ex.code().value,
                    ex.message().c_str());
    }
    FuncExitVoid(DMF_TRACE);

}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
MobileBroadbandModemDevice::MobileBroadband_TransmissionStateMonitorStop()
/*++

Routine Description:

    Unregister transmission state change callback and stop monitoring.

Arguments:

    None.

Return Value:

    None.

    --*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Unregister callback using token.
    //
    try
    {
        sarManager.StopTransmissionStateMonitoring();
        sarManager.TransmissionStateChanged(tokenTransmissionStateChanged);
    }
    catch (hresult_error ex)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "StopTransmissionStateMonitoring fails, error code 0x%08x - %ws",
                    ex.code().value,
                    ex.message().c_str());
    }

    FuncExitVoid(DMF_TRACE);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationRegister)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
NTSTATUS
DMF_MobileBroadband_NotificationRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Register for a notification. This function create a instance of MobileBroadbandModemDevice. 
    It use modem watcher for monitoring MobileBroadband resource.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;
    DMF_CONFIG_MobileBroadband* moduleConfig;
    
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Store module config.
    //
    moduleContext->EvtMobileBroadbandWirelessStateChangeCallback = moduleConfig->EvtMobileBroadbandWirelessStateChangeCallback;

    // Necessary call for using C++/WinRT environment.
    //
    init_apartment();
    // Instantiate MobileBroadbandModemDevice class instance.
    //
    moduleContext->ModemDevice = new MobileBroadbandModemDevice();
    
    if (moduleContext->ModemDevice == nullptr)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Insufficient resource for ModemDevice instance, ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    ntStatus = moduleContext->ModemDevice->Initialize(DmfModule);

    if (!NT_SUCCESS(ntStatus))
    {
        delete moduleContext->ModemDevice;
        moduleContext->ModemDevice = nullptr;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "MobileBroadband_Initialize fails: ntStatus=%!STATUS!", ntStatus);
    }

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationUnregister)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
static
VOID
DMF_MobileBroadband_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Unregister for a notification. This function delete the instance of MobileBroadbandModemDevice. 

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None.

--*/
{
    DMF_CONTEXT_MobileBroadband* moduleContext;
    
    PAGED_CODE();
    
    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->ModemDevice != nullptr)
    {
        if (moduleContext->ModemDevice->modem != nullptr)
        {
            DMF_ModuleClose(DmfModule);
        }

        moduleContext->ModemDevice->DeInitialize();

        delete moduleContext->ModemDevice;
        moduleContext->ModemDevice = nullptr;
    }
    // Unintialize C++/WinRT environment.
    //
    uninit_apartment();

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
DMF_MobileBroadband_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
)
/*++

Routine Description:

    Create an instance of a DMF Module of type MobileBroadband.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_MobileBroadband;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_MobileBroadband;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_MobileBroadband);
    dmfCallbacksDmf_MobileBroadband.DeviceNotificationRegister = DMF_MobileBroadband_NotificationRegister;
    dmfCallbacksDmf_MobileBroadband.DeviceNotificationUnregister = DMF_MobileBroadband_NotificationUnregister;
    
    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_MobileBroadband,
                                            MobileBroadband,
                                            DMF_CONTEXT_MobileBroadband,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware);

    dmfModuleDescriptor_MobileBroadband.CallbacksDmf = &dmfCallbacksDmf_MobileBroadband;
    dmfModuleDescriptor_MobileBroadband.NumberOfAuxiliaryLocks = MobileBroadband_AuxiliaryLockCount;
    
    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_MobileBroadband,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate failed, ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);

}
#pragma code_seg()

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_AntennaBackOffTableIndexGet(
    _In_ DMFMODULE DmfModule,
    _In_ INT32 AntennaIndex,
    _Out_ INT32* AntennaBackOffTableIndex
    )
/*++

Routine Description:

    Get desired power back off table index of specific antenna on device.

Arguments:

    DmfModule - This module's Dmf Module.
    AntennaIndex - The antenna index on device to be control.
    AntennaBackOffTableIndex - The power back off table index to be get.

Return Value:

    NTSTATUS

    --*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not open yet.");
        goto ExitNoRelease;
    }

    try
    {
        for (auto antenna : moduleContext->ModemDevice->sarManager.Antennas())
        {
            if (antenna.AntennaIndex() == AntennaIndex)
            {
                *AntennaBackOffTableIndex = antenna.SarBackoffIndex();
                 TraceEvents(TRACE_LEVEL_INFORMATION,
                             DMF_TRACE,
                             "Get Antenna No.%d back off index = %d", 
                             AntennaIndex,
                             *AntennaBackOffTableIndex);
            }
        }
    }
    catch (hresult_error ex)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Get Antenna No.%d back off index fails, error code 0x%08x - %ws", 
                    AntennaIndex,
                    ex.code().value,
                    ex.message().c_str());
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;

}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_AntennaBackOffTableIndexSet(
    _In_ DMFMODULE DmfModule,
    _In_ INT32* AntennaIndex,
    _In_ INT32* AntennaBackOffTableIndex,
    _In_ INT32 AntennaCount,
    _In_ BOOLEAN AbsoluteAntennaIndexMode
    )
/*++

Routine Description:

    Set desired power back off table index of specific antenna on device.

Arguments:

    DmfModule - This module's Dmf Module.
    AntennaIndex - Index of antenna to set.
    AntennaBackOffTableIndex - Index into the power back-off table to set.
    AntennaCount - Number of antennas present.
    AbsoluteAntennaIndexMode - Indicates whether the input antenna index is an absolute index or relative index.

Return Value:

    NTSTATUS

    --*/
{
NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;

    if (AntennaCount < 1)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Antenna count is 0");
        goto ExitNoRelease;
    }

    for (INT32 antennaIndex = 0; antennaIndex < AntennaCount; antennaIndex++)
    {
        if (AntennaBackOffTableIndex[antennaIndex] > AntennaBackOffTableIndexMax)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Antenna back off index exceed the limit");
            goto ExitNoRelease;
        }
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not open yet.");
        goto ExitNoRelease;
    }

    ntStatus = STATUS_UNSUCCESSFUL;
    
    try
    {
        vector<MobileBroadbandAntennaSar> antennas;

        INT32 physicalAntennaCount = moduleContext->ModemDevice->sarManager.Antennas().Size();

        if (physicalAntennaCount < AntennaCount)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Antennas count exceed physical antenna count");
            goto Exit;
        }

        // Iterate each antenna setting.
        //
        for (INT32 antennaIndex = 0; antennaIndex < AntennaCount; antennaIndex++)
        {  
            // Antenna index absolute mode.
            //
            if (AbsoluteAntennaIndexMode == TRUE)
            {
                BOOLEAN antennaFind = FALSE;
                for (auto antenna : moduleContext->ModemDevice->sarManager.Antennas())
                {
                    if (antenna.AntennaIndex() == AntennaIndex[antennaIndex])
                    {
                        antennas.push_back({AntennaIndex[antennaIndex],
                                            AntennaBackOffTableIndex[antennaIndex]});
                        antennaFind = TRUE;
                        break;
                    }
                }
                if (!antennaFind)
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No absolute antenna index %d found", AntennaIndex[antennaIndex]);
                    goto Exit;
                }
            }
            // Antenna index relative mode.
            //
            else
            {
                auto antenna = moduleContext->ModemDevice->sarManager.Antennas().GetAt(antennaIndex);
                antennas.push_back({antenna.AntennaIndex(),
                                    AntennaBackOffTableIndex[antennaIndex]});

            }
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Map Antenna No.%d back off index to %d success",
                                                            AntennaIndex[antennaIndex],
                                                            AntennaBackOffTableIndex[antennaIndex]);
        }

        moduleContext->ModemDevice->sarManager.SetConfigurationAsync(std::move(antennas)).get();

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Set Antenna back off index success");

    }
    catch (hresult_error ex)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Set Antenna back off index fails, error code 0x%08x - %ws", 
                    ex.code().value,
                    ex.message().c_str());
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;

}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_MccMncGet(
    _In_ DMFMODULE DmfModule,
    _Out_ DWORD* MobileCountryCode,
    _Out_ DWORD* MobileNetworkCode
    )
/*++

Routine Description:

    Get the Mobile Country Code and Mobile Network Code of the mobile broadband network 
    to which the device is attached from the modem.

Arguments:

    DmfModule - This module's Dmf Module.
    MobileCountryCode - The three digit number corresponding to the country where the device is connected.
    MobileNetworkCode - The three digit number corresponding to the network where the device is connected.

Return Value:

    NTSTATUS

    --*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;
    MobileBroadbandNetwork currentNetwork = nullptr;
    const wchar_t* providerId;
    uint32_t productIdLength;
    DWORD mobileCountryCode = 0;
    DWORD mobileNetworkCode = 0;
    int mccLength;
    int mncLength;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not open yet.");
        goto ExitNoRelease;
    }

    try
    {
        currentNetwork = moduleContext->ModemDevice->modem.CurrentNetwork();
        // Product Id contains 3 digit Mcc value together with 2 or 3 digit Mnc value.
        //
        providerId = currentNetwork.RegisteredProviderId().c_str();
        productIdLength = currentNetwork.RegisteredProviderId().size();
    }
    catch (hresult_error ex)
    {
        // At certain point. C++/WinRT sarManager component will raise 0x8007008 not enough memory exception due to a bug.
        // If that happen, don't change the IsTransmitting state.
        //
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Exception occurs in DMF_MobileBroadband_MccMncGet, error code 0x%08x - %ws",
                    ex.code().value,
                    ex.message().c_str());
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    if (productIdLength < MccMncReportLengthMinimum ||
        productIdLength > MccMncReportLengthMaximum)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Get invalid info of Mcc and Mnc value, productIdLength is %d", productIdLength);
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    // Mcc is always 3 digits.
    //
    mccLength = MccMncReportLengthMaximum / 2;
    ntStatus = MobileCodeCalulate(providerId, 
                                  0, 
                                  mccLength, 
                                  &mobileCountryCode);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "mobileCountryCode get fails");
        goto Exit;
    }

    // Mnc value can be 2 or 3, total length divided by 2 can get the result.
    //
    mncLength = productIdLength / 2;
    ntStatus = MobileCodeCalulate(providerId, 
                                  mccLength, 
                                  mncLength, 
                                  &mobileNetworkCode);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "mobileCountryCode get fails");
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Mcc = %d, Mnc = %d", 
                                                    mobileCountryCode, 
                                                    mobileNetworkCode);
    *MobileCountryCode = mobileCountryCode;
    *MobileNetworkCode = mobileNetworkCode;

    ntStatus = STATUS_SUCCESS;

Exit:
    
    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;

}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_SarBackOffDisable(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Disable MobileBroadband Sar back off functionality.

Arguments:

    DmfModule - This module's Dmf Module.

Return Value:

    NTSTATUS

    --*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not open yet.");
        goto ExitNoRelease;
    }

    try
    {
        moduleContext->ModemDevice->sarManager.DisableBackoffAsync().get();
    }
    catch (hresult_error ex)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Disable SAR fails, error code 0x%08x - %ws", 
                    ex.code().value,
                    ex.message().c_str());
        goto AlternativeBackOff;
    }
    // If success, go to exit, no need alternative back off action.
    //
    goto Exit;

AlternativeBackOff:
    // Set all available antenna back off table index to DefaultBackOffTableIndex
    //
    try
    {
        for (auto antenna : moduleContext->ModemDevice->sarManager.Antennas())
        {
            INT32 lteAntennaIndex = antenna.AntennaIndex();
            INT32 lteAntennaBackoffIndex = DefaultBackOffTableIndex;
            ntStatus = DMF_MobileBroadband_AntennaBackOffTableIndexSet(DmfModule,
                                                                       &lteAntennaIndex,
                                                                       &lteAntennaBackoffIndex,
                                                                       1,
                                                                       FALSE);
            if (!NT_SUCCESS(ntStatus))
            {
                goto Exit;
            }
        }
    }
    catch (hresult_error ex)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Set Antennas to back off table fails, error code 0x%08x - %ws", 
                    ex.code().value,
                    ex.message().c_str());
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;

}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_SarBackOffEnable(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Enable MobileBroadband Sar back off functionality.
    NOTE: Caller should call DMF_MobileBroadband_AntennaBackOffTableIndexSet to set 
          antenna back off index after enable SAR back off.

Arguments:

    DmfModule - This module's Dmf Module.

Return Value:

    NTSTATUS

    --*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not open yet.");
        goto ExitNoRelease;
    }

    try
    {
        moduleContext->ModemDevice->sarManager.EnableBackoffAsync().get();
    }
    catch (hresult_error ex)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "Enable SAR fails, error code 0x%08x - %ws", 
                    ex.code().value,
                    ex.message().c_str());
        ntStatus = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

Exit:
    
    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;

}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_MobileBroadband_WirelessStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ MobileBroadband_WIRELESS_STATE* MobileBroadbandWirelessState
    )
/*++

Routine Description:

    Get the current MobileBroadband wireless state.

Arguments:

    DmfModule - This module's Dmf Module.
    MobileBroadbandWirelessState - MobileBroadband wireless state to return.

Return Value:

    NTSTATUS

    --*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not open yet.");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleContext->ModemDevice->mobileBroadbandWirelessState.IsNetworkConnected = moduleContext->ModemDevice->MobileBroadband_IsNetworkConnected();
    moduleContext->ModemDevice->mobileBroadbandWirelessState.IsTransmitting = moduleContext->ModemDevice->MobileBroadband_IsTransmitting();

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "IsNetworkConnected = %d, IsTransmitting = %d", 
                moduleContext->ModemDevice->mobileBroadbandWirelessState.IsNetworkConnected,
                moduleContext->ModemDevice->mobileBroadbandWirelessState.IsTransmitting);

    *MobileBroadbandWirelessState = moduleContext->ModemDevice->mobileBroadbandWirelessState;

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;

}

#endif

// eof: Dmf_ProximityEnvelopeSensor.cpp
//
