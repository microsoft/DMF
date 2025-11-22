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

#if defined(DMF_INCLUDE_TMH)
#include "Dmf_MobileBroadband.tmh"
#endif

// Only support 21H1 and above because of some APIs are introduced after that.
//
#if IS_WIN10_20H2_OR_LATER

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
    // Instance of C++/WinRT SlotManager from modem.
    //
    MobileBroadbandSlotManager slotManager = nullptr;
    // MobileBroadband wireless state.
    //
    MobileBroadband_WIRELESS_STATE mobileBroadbandWirelessState = { 0 };  
    // Flag to indicate if sim/esim is present and ready.
    // 
    BOOLEAN isSimPresentAndReady = FALSE;
    // Event token for transmission state change.
    //
    event_token tokenTransmissionStateChanged;
    // Event token for transmission state change.
    //
    event_token tokenSimSlotInfoChanged;
    // IAsyncAction for modem and SAR object get.
    // 
    _IRQL_requires_max_(PASSIVE_LEVEL)
    IAsyncAction ModemAndSarResourceGetAsync();
    // IAsyncOperation to check if MobileBroadband network is connected.
    //
    _IRQL_requires_max_(PASSIVE_LEVEL)
    IAsyncOperation<bool> MobileBroadband_IsNetworkConnectedAsync();
    // IAsyncAction for timeout helper.
    // 
    _IRQL_requires_max_(PASSIVE_LEVEL)
    IAsyncAction TimeoutHelperAsync(_In_ int milliseconds);
    // IAsyncAction for timeout helper with bool return.
    // 
    _IRQL_requires_max_(PASSIVE_LEVEL)
    IAsyncOperation<bool> TimeoutHelperOperationAsync(_In_ int milliseconds);
    // Initialize route.
    //
    _IRQL_requires_max_(PASSIVE_LEVEL)
    _Must_inspect_result_
    NTSTATUS
    Initialize(_In_ DMFMODULE DmfModule);
    // DeInitialize route.
    //
    _IRQL_requires_max_(PASSIVE_LEVEL) VOID DeInitialize();
    // Check if MobileBroadband network is transmitting.
    //
    _IRQL_requires_max_(PASSIVE_LEVEL)
    _Must_inspect_result_
    BOOLEAN
    MobileBroadband_IsNetworkConnected();
    _IRQL_requires_max_(PASSIVE_LEVEL)
    _Must_inspect_result_
    BOOLEAN
    MobileBroadband_IsTransmitting();
    // Function for start monitoring MobileBroadband network transmission state.
    //
    _IRQL_requires_max_(PASSIVE_LEVEL) VOID MobileBroadbandModemDevice::MobileBroadband_TransmissionStateMonitorStart(_In_ DMFMODULE DmfModule);
    // Function for stop monitoring MobileBroadband network transmission state.
    //
    _IRQL_requires_max_(PASSIVE_LEVEL) VOID MobileBroadbandModemDevice::MobileBroadband_TransmissionStateMonitorStop();
    // Function for register MobileBroadband sim slot info change.
    //
    _IRQL_requires_max_(PASSIVE_LEVEL) VOID MobileBroadbandModemDevice::MobileBroadband_SimSlotInfoChangedEventRegister(_In_ DMFMODULE DmfModule);
    // Function for Unregister MobileBroadband sim slot info change.
    //
    _IRQL_requires_max_(PASSIVE_LEVEL) VOID MobileBroadbandModemDevice::MobileBroadband_SimSlotInfoChangedEventUnregister();


};

typedef struct _DMF_CONTEXT_MobileBroadband
{
    // DmfModuleRundown
    //
    DMFMODULE DmfModuleRundown;
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

#define AntennaBackOffTableIndexMinimum                 0
#define AntennaBackOffTableIndexMaximum                 32
#define DefaultAntennaIndex                             0
#define DefaultBackOffTableIndex                        0
#define MccMncReportLengthMinimum                       5
#define MccMncReportLengthMaximum                       6
#define RetryTimesAmount                                10
#define WaitTimeMillisecondOneSecond                    1000
#define WaitTimeMillisecondsOnInitialize                20000
#define WaitTimeMillisecondsFiveSeconds                 5000
#define MobileBroadband_AdapterWatcherEventLockIndex    0
#define MobileBroadband_AuxiliaryLockCount              1

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
inline
NTSTATUS
MobileBroadband_MobileCodeCalculate(
    _In_ hstring ProviderId,
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
    int currentPosition = 0;

    if (ProviderId.empty() ||
        StartPosition < 0 ||
        CodeLength <= 0 ||
        StartPosition + CodeLength > (int)ProviderId.size() ||
        Result == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    for (int providerIdIndex = 0; providerIdIndex < CodeLength; providerIdIndex++) 
    {
        currentPosition = providerIdIndex + StartPosition;
        if (ProviderId[currentPosition] >= '0' &&
            ProviderId[currentPosition] <= '9')
        {
            result = result * 10 + (DWORD)(ProviderId[currentPosition] - '0');
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
IAsyncAction
MobileBroadbandModemDevice::ModemAndSarResourceGetAsync()
/*++

Routine Description:

    IAsyncAction function for get MobileBroadbandModem and MobileBroadbandSarManager resource as async call.

Arguments:

    None

Return Value:

    IAsyncAction

--*/
{
    // MobileBroadbandModem is not ready right after modem interface arrived.
    // Need for wait for MobileBroadbandModem be available. Otherwise modem get will return nullptr.
    //
    for (int tryTimes = 0; tryTimes < RetryTimesAmount; tryTimes++)
    {
        try
        {
            // co_await here with a timeout, this does not create seperate thread for below task like resume_background, and can be cancelled.
            // And can create the IAsyncAction correctly.
            //
            co_await winrt::resume_after(std::chrono::milliseconds(WaitTimeMillisecondOneSecond));
            // This call might hangs here. This is a OS/WinRT bug. 
            // Here, it is being called in an async method to ensure that it does not block other threads.
            //
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
                    continue;
                }
                else
                {
                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem resource get success");
                    break;
                }
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Get MobileBroadbandModem fails");
                continue;
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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "ModemAndSarResourceGetAsync finished");
    co_return;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
IAsyncAction
MobileBroadbandModemDevice::TimeoutHelperAsync(
    _In_ int milliseconds
    )
/*++

Routine Description:

    Timeour helper function as IAsyncAction. 
    It sleeps for certain milliseconds and then return as async call.

Arguments:

    milliseconds: The time to sleep in milliseconds.

Return Value:

    IAsyncAction

--*/
{
    auto cancellation = co_await get_cancellation_token();
    cancellation.enable_propagation();

    co_await winrt::resume_after(std::chrono::milliseconds(milliseconds));

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "TimeoutHelperAsync with timeout %d ms finished", milliseconds);
    co_return;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
IAsyncOperation<bool>
MobileBroadbandModemDevice::TimeoutHelperOperationAsync(
    _In_ int milliseconds
    )
/*++

Routine Description:

    Timeout helper function as IAsyncAction. 
    It sleeps for certain milliseconds and then return as async call.

Arguments:

    milliseconds: The time to sleep in milliseconds.

Return Value:

    IAsyncOperation<bool>:  false as always to indicate timeout.

--*/
{
    auto cancellation = co_await get_cancellation_token();
    cancellation.enable_propagation();

    co_await winrt::resume_after(std::chrono::milliseconds(milliseconds));

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "TimeoutHelperOperationAsync with timeout %d ms finished", milliseconds);
    co_return false;
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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem Watcher stopped");
    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
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
    // Create device watcher according to the sensor GUID from ModuleConfig.
    //
    // Don't call CustomSensor::GetDeviceSelector(moduleConfig->DeviceGUID) as a deviceWatcher parameter. There is a bug in OS at and before 20h1.
    // This CustomSensor API call will cause appverif failure and potential pre-OOBE driver crash fail.
    // Manually construct an AQS instead and pass to deviceWatcher will resolve this issue.
    //
    hstring mbbSelector = L"System.Devices.InterfaceClassGuid:=\"{CAC88484-7515-4C03-82E6-71A87ABAC361}\""
                          L" AND System.Devices.Wwan.InterfaceGuid:-[] AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";
    // CreateWatcher API may fail during OS boot up time due to resource not ready. Retry with RetryTimesAmount times.
    //
    for (int tryTimes = 0; tryTimes < RetryTimesAmount; tryTimes++)
    {
        try {
            modemWatcher = DeviceInformation::CreateWatcher(mbbSelector, 
                                                            nullptr);
            // if success, break the loop.
            //
            break;
        }
        catch (hresult_error const& ex)
        {
            // if failed, wait for WaitTimeMillisecondsOnInitialize and retry.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Failed to create modem device watcher, %ws", ex.message().c_str());
            Sleep(WaitTimeMillisecondsOnInitialize);
        }
    }
    if (modemWatcher == nullptr)
    {
        // Last try and don't catch the exception here if fail. Let the WUDFHost handle it.
        //
        modemWatcher = DeviceInformation::CreateWatcher(mbbSelector, 
                                                        nullptr);
    }
    
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

        ntStatus = DMF_Rundown_Reference(moduleContext->DmfModuleRundown);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_ModuleReference() fails: ntStatus=%!STATUS!", ntStatus);
            return;
        }

        // New modem interface arrived.
        //
        if (modem == nullptr)
        {        
            // Use IAsyncAction and when_any to give timeout for modem resource get action to prevent hang forever.
            //
            IAsyncAction modemAndSarResourceGetAsync = ModemAndSarResourceGetAsync();
            IAsyncAction timeoutHelperAsync = TimeoutHelperAsync(WaitTimeMillisecondsOnInitialize);
            when_any(modemAndSarResourceGetAsync,
                     timeoutHelperAsync).get();
            if (modem != nullptr && sarManager != nullptr)
            {
                ntStatus = DMF_ModuleOpen(DmfModule);
                if (!NT_SUCCESS(ntStatus))
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleOpen() fails: ntStatus=%!STATUS!", ntStatus);
                    modem = nullptr;
                    sarManager = nullptr;
                }
                else
                {
                    mobileBroadbandWirelessState.IsModemValid = TRUE;
                    modemId = args.Id();
                    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem is found , device Id is %ws", modemId.c_str());
                    slotManager = modem.DeviceInformation().SlotManager();
                    MobileBroadband_SimSlotInfoChangedEventRegister(DmfModule);
                    MobileBroadband_TransmissionStateMonitorStart(DmfModule);
                    timeoutHelperAsync.Cancel();
                }
            }
            else
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem resource get failed");
                modemAndSarResourceGetAsync.Cancel();
            }
            timeoutHelperAsync.Close();
            modemAndSarResourceGetAsync.Close();
        }
        // Only one instance of mobile broad band is supported.
        //
        else
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem is found already. Only one modem is supported");
        }

        DMF_Rundown_Dereference(moduleContext->DmfModuleRundown);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Rundown deferef from add event");
    });

    // Event handler for MobileBroadband interface remove event.
    //
    TypedEventHandler deviceInfoRemovedHandler = TypedEventHandler<DeviceWatcher,
                                                                   DeviceInformationUpdate>([=](DeviceWatcher sender,
                                                                                                DeviceInformationUpdate args)
    {
        DMF_CONTEXT_MobileBroadband* moduleContext;
        NTSTATUS ntStatus;

        UNREFERENCED_PARAMETER(sender);

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Modem deviceInfoRemovedHandler triggered");

        moduleContext = DMF_CONTEXT_GET(DmfModule);
        ntStatus = DMF_Rundown_Reference(moduleContext->DmfModuleRundown);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_ModuleReference() fails: ntStatus=%!STATUS!", ntStatus);
            return;
        }

        // modem interface removed.
        //
        if (modem != nullptr)
        {
            if (modemId == args.Id())
            {
                // Removed modem interface is our modem.
                // Upon modem remove, all event callback is unregistered by c++/winrt.
                // No need to unregister them again.
                //
                // Close this Module.
                //
                DMF_ModuleClose(DmfModule);
                modem = nullptr;
                sarManager = nullptr;
                slotManager = nullptr;
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

        DMF_Rundown_Dereference(moduleContext->DmfModuleRundown);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Rundown deferef from remove event");
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
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "ModemWatcher starts");
    ntStatus = STATUS_SUCCESS;

Exit:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
IAsyncOperation<bool>
MobileBroadbandModemDevice::MobileBroadband_IsNetworkConnectedAsync()
/*++

Routine Description:

    Async operation Check if network is connected or not.

Arguments:

    None

Return Value:

    bool - TURE for connected. false for disconnected.

    --*/
{
    bool isConnected = false;
    MobileBroadbandNetwork currentNetwork = nullptr;
    NetworkRegistrationState networkRegistrationState;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    try
    {
        // co_await here with a no need timeout, this is mainly to to make this function async.
        // 
        co_await winrt::resume_background();
        currentNetwork = modem.CurrentNetwork();

        if (currentNetwork == nullptr)
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No network component found");
            goto Exit;
        }

        networkRegistrationState = currentNetwork.NetworkRegistrationState();
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "current network registration state: %d", (int)networkRegistrationState);

        if ((networkRegistrationState == NetworkRegistrationState::Denied) ||
            (networkRegistrationState == NetworkRegistrationState::Deregistered) ||
            (networkRegistrationState == NetworkRegistrationState::Searching) ||
            (networkRegistrationState == NetworkRegistrationState::None))
        {
            isConnected = false;
        }
        else
        {
            isConnected = true;
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

    co_return isConnected;

}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN
MobileBroadbandModemDevice::MobileBroadband_IsNetworkConnected()
/*++

Routine Description:

    Async operation Check if network is connected or not.

Arguments:

    None

Return Value:

    BOOLEAN - TRUE for connected. false for disconnected.

    --*/
{
    BOOLEAN isConnected = FALSE;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    try 
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Checking MobileBroadband_IsNetworkConnected");
        // Use IAsyncOperation and when_any to give timeout for async action to prevent hang forever.
        //
        IAsyncOperation<bool> timeoutHelperOperationAsync = TimeoutHelperOperationAsync(WaitTimeMillisecondsFiveSeconds);
        IAsyncOperation<bool> getIsNetworkConnectedAsync = MobileBroadband_IsNetworkConnectedAsync();
        isConnected = (BOOLEAN)when_any(getIsNetworkConnectedAsync,
                                        timeoutHelperOperationAsync).get();

        if (timeoutHelperOperationAsync.Status() == AsyncStatus::Completed)
        {
            getIsNetworkConnectedAsync.Cancel();
        }
        else
        {
            timeoutHelperOperationAsync.Cancel();
        }
        getIsNetworkConnectedAsync.Close();
        timeoutHelperOperationAsync.Close();
    }
    catch (hresult_error ex)
    {
        // At certain point. C++/WinRT sarManager component will raise 0x8007008 not enough memory exception due to a bug.
        // If that happen, don't change the IsTransmitting state.
        //
        isConnected = FALSE;
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "MobileBroadband_IsNetworkConnectedAsync fails, error code 0x%08x - %ws",
                    ex.code().value,
                    ex.message().c_str());
        goto Exit;
    }

Exit:

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "isConnected = %d", isConnected);

    return isConnected;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
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
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Checking MobileBroadband_IsTransmitting");
        // Use IAsyncOperation and when_any to give timeout for async action to prevent hang forever.
        //
        IAsyncOperation<bool> timeoutHelperOperationAsync = TimeoutHelperOperationAsync(WaitTimeMillisecondsFiveSeconds);
        IAsyncOperation<bool> getIsTransmittingAsync = sarManager.GetIsTransmittingAsync();
        isTransmitting = (BOOLEAN)when_any(getIsTransmittingAsync,
                                           timeoutHelperOperationAsync).get();

        if(timeoutHelperOperationAsync.Status() == AsyncStatus::Completed)
        {
            getIsTransmittingAsync.Cancel();
        }
        else
        {
            timeoutHelperOperationAsync.Cancel(); 
        }
        getIsTransmittingAsync.Close();
        timeoutHelperOperationAsync.Close();
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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "isTransmitting = %d", isTransmitting);

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

    DmfModule - This Module's Dmf Module.

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

        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

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
        // Call back parent Module.
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
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "MobileBroadband_TransmissionStateMonitorStop enter");
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

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
MobileBroadbandModemDevice::MobileBroadband_SimSlotInfoChangedEventRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Register sim slot info change callback.

Arguments:

    DmfModule - This Module's Dmf Module.

Return Value:

    None.

    --*/
{
    DMF_CONFIG_MobileBroadband* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // This moduleConfig get function MUST happen before Lambda function. Otherwise lambda function will have wrong config.
    //
    moduleConfig = DMF_CONFIG_GET(DmfModule);
    // Event handler lambda function for sim info change.
    // Per current 24H2 OS, this event will trigger twice for psim and esim. So perform query instead using event args is preferred.
    //
    TypedEventHandler mobileBroadbandSimSlotInfoChanged = TypedEventHandler<MobileBroadbandSlotManager,
                                                                            MobileBroadbandSlotInfoChangedEventArgs>([=](MobileBroadbandSlotManager sender,
                                                                                                                         MobileBroadbandSlotInfoChangedEventArgs args)
    {
        NTSTATUS ntStatus;

        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ntStatus = DMF_ModuleReference(DmfModule);
        if (!NT_SUCCESS(ntStatus))
        {
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not open yet.");
            return;
        }
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "mobileBroadbandSimSlotInfoChanged event triggered");
        int currentSlotIndex = slotManager.CurrentSlotIndex();
        MobileBroadbandSlotInfo slotInfo = slotManager.SlotInfos().GetAt(currentSlotIndex);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Current Sim Slot index is %d", currentSlotIndex);
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Current Sim Slot state is %d", (int)slotInfo.State());
        if (slotInfo.State() == MobileBroadbandSlotState::Active || 
            slotInfo.State() == MobileBroadbandSlotState::ActiveEsim)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Sim/eSim is present and ready");
            isSimPresentAndReady = TRUE;
        }
        else
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Sim/eSim is not present or ready");
            isSimPresentAndReady = FALSE;
        }

        // Call back parent Module.
        //
        if (moduleConfig->EvtMobileBroadbandSimReadyChangeCallback != nullptr)
        {
            moduleConfig->EvtMobileBroadbandSimReadyChangeCallback(DmfModule,
                                                                   isSimPresentAndReady);
        }
        DMF_ModuleDereference(DmfModule);
    });

    tokenSimSlotInfoChanged = slotManager.SlotInfoChanged(mobileBroadbandSimSlotInfoChanged);

    FuncExitVoid(DMF_TRACE);

}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
MobileBroadbandModemDevice::MobileBroadband_SimSlotInfoChangedEventUnregister()
/*++

Routine Description:

    Unregister sim slot info change callback.

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
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "MobileBroadband_SimSlotInfoChangedEventUnregister enter");
    try
    {
         slotManager.SlotInfoChanged(tokenSimSlotInfoChanged);
    }
    catch (hresult_error ex)
    {
        TraceEvents(TRACE_LEVEL_ERROR,
                    DMF_TRACE,
                    "MobileBroadband_SimSlotInfoChangedEventUnregister fails, error code 0x%08x - %ws",
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

    // Start Rundown module.
    //
    DMF_Rundown_Start(moduleContext->DmfModuleRundown);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Rundown starts finish");

    // Store Module config.
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

    if (!NT_SUCCESS(ntStatus))
    {
        // Unintialize C++/WinRT environment.
        //
        uninit_apartment();
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationUnregister)
_IRQL_requires_max_(PASSIVE_LEVEL)
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

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Waiting for rundown...");
    DMF_Rundown_EndAndWait(moduleContext->DmfModuleRundown);
    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Rundown satisfied.");

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

#pragma code_seg("PAGE")
_Function_class_(DMF_ChildModulesAdd)
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_MobileBroadband_ChildModulesAdd(
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
    DMF_CONTEXT_MobileBroadband* moduleContext;

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);
    UNREFERENCED_PARAMETER(DmfModuleInit);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Rundown
    // -------
    //
    DMF_Rundown_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleRundown);

    FuncExitVoid(DMF_TRACE);
}

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
    dmfCallbacksDmf_MobileBroadband.ChildModulesAdd = DMF_MobileBroadband_ChildModulesAdd;
    
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
_Must_inspect_result_
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

    DmfModule - This Module's Dmf Module.
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

    // For SAL.
    //
    *AntennaBackOffTableIndex = 0;

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
_Must_inspect_result_
NTSTATUS
DMF_MobileBroadband_AntennaBackOffTableIndexSet(
    _In_ DMFMODULE DmfModule,
    _In_reads_(AntennaCount) INT32* AntennaIndex,
    _In_reads_(AntennaCount) INT32* AntennaBackOffTableIndex,
    _In_ INT32 AntennaCount,
    _In_ BOOLEAN AbsoluteAntennaIndexMode
    )
/*++

Routine Description:

    Set desired power back off table index of specific antenna on device.

Arguments:

    DmfModule - This Module's Dmf Module.
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
        if (AntennaBackOffTableIndex[antennaIndex] > AntennaBackOffTableIndexMaximum)
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
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "antenna index from modem is %d", antenna.AntennaIndex());

            }
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Map Antenna No.%d back off index to %d success",
                                                            AntennaIndex[antennaIndex],
                                                            AntennaBackOffTableIndex[antennaIndex]);
        }

        // Use IAsyncAction and when_any to give timeout for modem resource get action to prevent hang forever.
        //
        IAsyncAction timeoutHelperAsync = moduleContext->ModemDevice->TimeoutHelperAsync(WaitTimeMillisecondsFiveSeconds);
        IAsyncAction antennasSetAsync = moduleContext->ModemDevice->sarManager.SetConfigurationAsync(std::move(antennas));
        when_any(antennasSetAsync,
                 timeoutHelperAsync).get();
        if (timeoutHelperAsync.Status() == AsyncStatus::Completed)
        {
            antennasSetAsync.Cancel();
        }
        else
        {
            timeoutHelperAsync.Cancel();
        }
        antennasSetAsync.Close();
        timeoutHelperAsync.Close();
        
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
_Must_inspect_result_
NTSTATUS
DMF_MobileBroadband_MccMncGet(
    _In_ DMFMODULE DmfModule,
    _Out_ DWORD* MobileAreaCode,
    _Out_ DWORD* MobileNetworkCode
    )
/*++

Routine Description:

    Get the Mobile Area Code and Mobile Network Code of the mobile broadband network 
    to which the device is attached from the modem.

Arguments:

    DmfModule - This Module's Dmf Module.
    MobileAreaCode - The three digit number corresponding to the area where the device is connected.
    MobileNetworkCode - The three digit number corresponding to the network where the device is connected.

Return Value:

    NTSTATUS

    --*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;
    MobileBroadbandNetwork currentNetwork = nullptr;
    hstring providerId;
    uint32_t productIdLength;
    DWORD mobileAreaCode;
    DWORD mobileNetworkCode;
    int mccLength;
    int mncLength;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    mobileAreaCode = 0;
    mobileNetworkCode = 0;
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
        providerId = currentNetwork.RegisteredProviderId();
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
    ntStatus = MobileBroadband_MobileCodeCalculate(providerId,
                                                   0, 
                                                   mccLength, 
                                                   &mobileAreaCode);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "mobileAreaCode get fails");
        goto Exit;
    }

    // Mnc value can be 2 or 3, total length divided by 2 can get the result.
    //
    mncLength = productIdLength / 2;
    ntStatus = MobileBroadband_MobileCodeCalculate(providerId,
                                                   mccLength, 
                                                   mncLength, 
                                                   &mobileNetworkCode);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "mobileAreaCode get fails");
        goto Exit;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Mcc = %d, Mnc = %d", 
                                                    mobileAreaCode, 
                                                    mobileNetworkCode);
    *MobileAreaCode = mobileAreaCode;
    *MobileNetworkCode = mobileNetworkCode;

    ntStatus = STATUS_SUCCESS;

Exit:
    
    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;

}

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_MobileBroadband_SarBackOffDisable(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Disable MobileBroadband Sar back off functionality.

Arguments:

    DmfModule - This Module's Dmf Module.

Return Value:

    NTSTATUS

    --*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Try to set SarBackOffDisable ");

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not open yet.");
        goto ExitNoRelease;
    }

    try
    {
        // Use IAsyncAction and when_any to give timeout for modem resource get action to prevent hang forever.
        //
        IAsyncAction timeoutHelperAsync = moduleContext->ModemDevice->TimeoutHelperAsync(WaitTimeMillisecondsFiveSeconds);
        IAsyncAction disableSarAsync = moduleContext->ModemDevice->sarManager.DisableBackoffAsync();
        when_any(disableSarAsync,
                 timeoutHelperAsync).get();
        if (timeoutHelperAsync.Status() == AsyncStatus::Completed)
        {
            disableSarAsync.Cancel();
        }
        else
        {
            timeoutHelperAsync.Cancel();
        }
        disableSarAsync.Close();
        timeoutHelperAsync.Close();

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
_Must_inspect_result_
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

    DmfModule - This Module's Dmf Module.

Return Value:

    NTSTATUS

    --*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_MobileBroadband* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Try to SarBackOffEnable");

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Modem is not open yet.");
        goto ExitNoRelease;
    }

    try
    {
        // Use IAsyncAction and when_any to give timeout for modem resource get action to prevent hang forever.
        //
        IAsyncAction timeoutHelperAsync = moduleContext->ModemDevice->TimeoutHelperAsync(WaitTimeMillisecondsFiveSeconds);
        IAsyncAction sarEnableAsync = moduleContext->ModemDevice->sarManager.EnableBackoffAsync();
        when_any(sarEnableAsync,
                 timeoutHelperAsync).get();
        if (timeoutHelperAsync.Status() == AsyncStatus::Completed)
        {
            sarEnableAsync.Cancel();
        }
        else
        {
            timeoutHelperAsync.Cancel();
        }
        sarEnableAsync.Close();
        timeoutHelperAsync.Close();
        
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
_Must_inspect_result_
NTSTATUS
DMF_MobileBroadband_WirelessStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ MobileBroadband_WIRELESS_STATE* MobileBroadbandWirelessState
    )
/*++

Routine Description:

    Get the current MobileBroadband wireless state.

Arguments:

    DmfModule - This Module's Dmf Module.
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
