/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_ActivitySensor.cpp

Abstract:

    This Module that can be used to get the Motion Activity information from device.
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
#include "Dmf_ActivitySensor.tmh"
#endif

// Only support 19H1 and above because of library size limitations on RS5.
//
#if defined(DMF_USER_MODE) && IS_WIN10_19H1_OR_LATER && defined(__cplusplus)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Enumerations and Structures
///////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Specific includes for this Module.
//
#include <string.h>
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Devices.Sensors.h"

using namespace std;
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Sensors;

// NOTE: In order to avoid runtime exceptions with C++/WinRT, it is necessary 
//       to declare a pointer to the "container" buffer using the "container"
//       type and then cast to VOID* when passing it to the Fetch Method.
//       When Fetch Method returns the buffer is ready to be used without
//       casting. Casting a VOID* to a "container" will cause a runtime
//       exception even though it compiles and actually works.
//

// This class allows us to copy C++/WinRT data so it can be stored
// in a flat buffer from DMF_BufferPool.
//
class DeviceInformationAndUpdateData
{
public:
    DeviceInformationAndUpdateData(DeviceInformation DeviceInfo,
        DeviceInformationUpdate DeviceInfoUpdate)
    {
        deviceInfo = DeviceInfo;
        deviceInfoUpdate = DeviceInfoUpdate;
    };
    DeviceInformation deviceInfo = nullptr;
    DeviceInformationUpdate deviceInfoUpdate = nullptr;
};

// This structure is a container that allows us to store C++/WinRT data
// in a flat buffer from DMF_BufferPool.
//
typedef struct
{
    // This is a pointer to a copy of the C++/WinRT data.
    //
    DeviceInformationAndUpdateData* deviceInformationAndUpdateData;
} DeviceInformationAndUpdateContainer;

// This class allows us to copy C++/WinRT data so it can be stored
// in a flat buffer from DMF_BufferPool.
//
class ActivitySensorReadingData
{
public:
    ActivitySensorReadingData(ActivitySensorReadingChangedEventArgs args)
    {
        activitySensorReadingChangedEventArgs = args;
    };
    ActivitySensorReadingChangedEventArgs activitySensorReadingChangedEventArgs = nullptr;
};

// This structure is a container that allows us to store C++/WinRT data
// in a flat buffer from DMF_BufferPool.
//
typedef struct
{
    // This is a pointer to a copy of the C++/WinRT data.
    //
    ActivitySensorReadingData* activitySensorReadingData;
} ActivitySensorReadingDataContainer;

class ActivitySensorDevice
{
private:

    // DeviceWatcher for Motion Activity sensor.
    //
    DeviceWatcher deviceWatcher = nullptr;

    // DeviceWatcher necessary events token, need to register all these 
    // events to make it work, and use for unregister.
    //
    event_token tokenAdded;
    event_token tokenRemoved;
    event_token tokenUpdated;
    event_token tokenEnumCompleted;

public:

    // The string for device Id to find.
    //
    hstring DeviceIdToFind = L"";
    // ActivitySensor instance.
    //
    ActivitySensor activitySensor = nullptr;
    // Store the Device Id of motion activity sensor that is found.
    //
    hstring deviceId = L"";
    // CustomSensor necessary event token.
    //
    event_token tokenReadingChanged;
    // Motion activity state, initialize with all 0s.
    //
    ACTIVITY_SENSOR_STATE activitySensorState = { 0 };
    // This Module's handle, store it here and used for callbacks.
    //
    DMFMODULE thisModuleHandle;
    // Callback to inform Parent Module that motion activity sensor has new changed reading.
    //
    EVT_DMF_ActivitySensor_EvtActivitySensorReadingChangedCallback* EvtActivitySensorReadingChangeCallback;
    // Initialize route.
    //
    NTSTATUS Initialize();
    // DeInitialize function.
    //
    VOID Deinitialize();
    // Start function, start motion activity sensor monitoring and events.
    //
    VOID Start();
    // Stop function, stop motion activity sensor monitoring and events.
    //
    VOID Stop();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef struct _DMF_CONTEXT_ActivitySensor
{
    // ActivitySensorDevice class instance pointer.
    //
    ActivitySensorDevice* activitySensorDevice = nullptr;

    // ThreadedBufferQueue for device watcher.
    //
    DMFMODULE DmfModuleThreadedBufferQueueDeviceWatcher;

    // ThreadedBufferQueue for motion activity sensor.
    //
    DMFMODULE DmfModuleThreadedBufferQueueActivitySensor;
} DMF_CONTEXT_ActivitySensor;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(ActivitySensor)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(ActivitySensor)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(EVT_DMF_ThreadedBufferQueue_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
ThreadedBufferQueue_BufferDisposition
ActivitySensor_ThreadedBufferQueueDeviceWatcherWork(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* ClientWorkBuffer,
    _In_ ULONG ClientWorkBufferSize,
    _In_ VOID* ClientWorkBufferContext,
    _Out_ NTSTATUS* NtStatus
    )
/*++

Routine Description:

    Callback function of device watcher threaded buffer queue when there is work to process.
    It processes add and remove events of the motion activity sensor from device watcher.

Arguments:

    DmfModule - ThreadedBufferQueueDeviceWatcherWork Module's handle.
    ClientWorkBuffer - Work buffer sent from the Client (DeviceInformationAndUpdateContainer).
    ClientWorkBufferSize - Size of ClientWorkBuffer.
    ClientWorkBufferContext - Not used.
    NtStatus - Not used.

Return Value:

    ThreadedBufferQueue_BufferDisposition

--*/
{
    DMFMODULE dmfModuleActivitySensor;
    DMF_CONTEXT_ActivitySensor* moduleContext;

    UNREFERENCED_PARAMETER(ClientWorkBufferContext);
    UNREFERENCED_PARAMETER(ClientWorkBufferSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Initialize for SAL, but no caller will read this status.
    //
    *NtStatus = STATUS_SUCCESS;

    dmfModuleActivitySensor = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleActivitySensor);

    ActivitySensorDevice* activitySensorWatcher = moduleContext->activitySensorDevice;

    // Event handler lambda function for motion activity reading change.
    //
    TypedEventHandler activitySensorReadingChangedHandler = TypedEventHandler<ActivitySensor,
                                                                              ActivitySensorReadingChangedEventArgs>([moduleContext](ActivitySensor sender,
                                                                                                                                     ActivitySensorReadingChangedEventArgs args)
    {
        // NOTE: In order to avoid runtime exceptions with C++/WinRT, it is necessary 
        //       to declare a pointer to the "container" buffer using the "container"
        //       type and then cast to VOID* when passing it to the Fetch Method.
        //       When Fetch Method returns the buffer is ready to be used without
        //       casting. Casting a VOID* to a "container" will cause a runtime
        //       exception even though it compiles and actually works.
        //
        ActivitySensorReadingDataContainer* activitySensorReadingDataContainer;
        NTSTATUS ntStatus;

        UNREFERENCED_PARAMETER(sender);

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "ReadingChanged event triggered from motion activity");

        // Get a Producer buffer. It is an empty buffer big enough to store the
        // custom sensor reading data.
        //
        ntStatus = DMF_ThreadedBufferQueue_Fetch(moduleContext->DmfModuleThreadedBufferQueueActivitySensor,
                                                 (VOID**)&activitySensorReadingDataContainer,
                                                 NULL);
        if (NT_SUCCESS(ntStatus))
        {
            DmfAssert(activitySensorReadingDataContainer != NULL);

            // Copy the motion activity data to callback buffer.
            //
            if (args != nullptr)
            {
                // Create space for a copy of the C++/WinRT data and copy it.
                //
                ActivitySensorReadingData* activitySensorReadingData = new ActivitySensorReadingData(args);
                if (activitySensorReadingData != NULL)
                {
                    // Set the pointer to the new created activitySensorReadingData.
                    //
                    activitySensorReadingDataContainer->activitySensorReadingData = activitySensorReadingData;
                    // Write it into the consumer buffer.
                    // (Enqueue the container structure that stores C++/WinRT data.)
                    //
                    DMF_ThreadedBufferQueue_Enqueue(moduleContext->DmfModuleThreadedBufferQueueActivitySensor,
                                                    (VOID*)activitySensorReadingDataContainer);
                }
                else
                {
                    TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Fail to allocate data");
                }
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Fail to get current reading");
            }
        }
        else
        {
            // There is no data buffer to store incoming DeviceInformationAndUpdate.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No buffer for Device Watcher event");
        }
    });

    DeviceInformation deviceInformation = nullptr;
    DeviceInformationUpdate deviceInformationUpdate = nullptr;
    DeviceInformationAndUpdateContainer* deviceInformationAndUpdate = (DeviceInformationAndUpdateContainer*)ClientWorkBuffer;
    ActivitySensor activitySensor = nullptr;

    if (deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfo != nullptr)
    {
        // Process an "Add" event.
        //
        deviceInformation = deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfo;

        // If motion activity interface is not nullptr, means it is found and available.
        //
        if (activitySensorWatcher->activitySensor != nullptr)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Motion Activity sensor has already been found, no extra interface needed");
            goto Exit;
        }

        // Check if this device matches the one specified in the Config.
        //
        hstring deviceId = deviceInformation.Id();

        // If target device Id is not blank and didn't match current device, exit.
        //
        if (moduleContext->activitySensorDevice->DeviceIdToFind != L"")
        {
            string deviceIdToFind = winrt::to_string(moduleContext->activitySensorDevice->DeviceIdToFind);
            string currentDeviceId = winrt::to_string(deviceId);
            if (currentDeviceId.find(deviceIdToFind) == string::npos)
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Current motion activity sensor is not the target, bypass current one");
                goto Exit;
            }
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Motion Activity sensor found");
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Device id is %ls", deviceId.c_str());
        try
        {
            if (moduleContext->activitySensorDevice->DeviceIdToFind != L"")
            {
                activitySensor = ActivitySensor::FromIdAsync(deviceId).get();
            }
            else
            {
                activitySensor = ActivitySensor::GetDefaultAsync().get();
            }
        }
        catch (hresult_error const& ex)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Failed to get ActivitySensor, HRESULT=0x%08X", ex.code().value);
            goto Exit;
        }

        // Store motion activity interface and device Id in class member.
        //
        activitySensorWatcher->activitySensor = activitySensor;
        activitySensorWatcher->deviceId = deviceId;
        activitySensorWatcher->activitySensorState.IsSensorValid = true;

        // Motion activity sensor resource is ready, open this module.
        //
        *NtStatus = DMF_ModuleOpen(dmfModuleActivitySensor);
        if (!NT_SUCCESS(*NtStatus))
        {
            activitySensorWatcher->activitySensor = nullptr;
            activitySensorWatcher->deviceId = L"";
            activitySensorWatcher->activitySensorState.IsSensorValid = false;
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_ModuleOpen fails: ntStatus = %!STATUS!", *NtStatus);
            goto Exit;
        }
        activitySensorWatcher->tokenReadingChanged = activitySensorWatcher->activitySensor.ReadingChanged(activitySensorReadingChangedHandler);
    }
    else if (deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfoUpdate != nullptr)
    {
        // Process a "Remove" event.
        //
        deviceInformationUpdate = deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfoUpdate;

        if (deviceInformationUpdate.Id() != activitySensorWatcher->deviceId)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Not our motion activity device");
            goto Exit;
        }

        // Motion activity has been removed.
        //       
        DMF_ModuleClose(dmfModuleActivitySensor);
        if (activitySensorWatcher->activitySensor != nullptr)
        {
            try
            {
                // Unregister reading change handler.
                //
                activitySensorWatcher->activitySensor.ReadingChanged(activitySensorWatcher->tokenReadingChanged);
                // Dereference motion activity interface.
                //
                activitySensorWatcher->activitySensor = nullptr;
                activitySensorWatcher->activitySensorState.IsSensorValid = false;
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Motion activity has been removed");
            }
            catch (hresult_error const& ex)
            {
                UNREFERENCED_PARAMETER(ex);
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Motion activity has been removed before unregister callback token");
                goto Exit;
            }
        }
    }

Exit:

    // Free the memory allocated when a copy of the C++/WinRT data was created.
    // (These pointers cannot be NULL as they are from the callback.)
    //
    delete deviceInformationAndUpdate->deviceInformationAndUpdateData;

    FuncExit(DMF_TRACE, "returnValue=ThreadedBufferQueue_BufferDisposition_WorkComplete");

    // Tell the Child Module that this Module is not longer the owner of the buffer.
    //
    return ThreadedBufferQueue_BufferDisposition_WorkComplete;
}

_Function_class_(EVT_DMF_ThreadedBufferQueue_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
ThreadedBufferQueue_BufferDisposition
ActivitySensor_ThreadedBufferQueueActivitySensorWork(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* ClientWorkBuffer,
    _In_ ULONG ClientWorkBufferSize,
    _In_ VOID* ClientWorkBufferContext,
    _Out_ NTSTATUS* NtStatus
    )
/*++

Routine Description:

    Callback function of motion activity threaded buffer queue when there is a work need to process.
    It process data reading change event of motion activity sensor.

Arguments:

    DmfModule - This Module's handle.
    ClientWorkBuffer - Work buffer send from Client (CustomSensorReadingDataContainer).
    ClientWorkBufferSize - Size of ClientWorkBuffer.
    ClientWorkBufferContext - Not used.
    NtStatus - Not used.

Return Value:

    ThreadedBufferQueue_BufferDisposition

--*/
{
    DMFMODULE dmfModuleActivitySensor;
    DMF_CONTEXT_ActivitySensor* moduleContext;

    UNREFERENCED_PARAMETER(ClientWorkBufferContext);
    UNREFERENCED_PARAMETER(ClientWorkBufferSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Initialize for SAL, but no caller will read this status.
    //
    *NtStatus = STATUS_SUCCESS;

    dmfModuleActivitySensor = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleActivitySensor);

    ActivitySensorDevice* activitySensorDevice = moduleContext->activitySensorDevice;

    ActivitySensorReadingDataContainer* activitySensorReadingDataContainer = (ActivitySensorReadingDataContainer*)ClientWorkBuffer;

    activitySensorDevice->activitySensorState.CurrentActivitySensorState = (ActivitySensor_Reading)activitySensorReadingDataContainer->activitySensorReadingData->activitySensorReadingChangedEventArgs.Reading().Activity();

    if (activitySensorDevice->EvtActivitySensorReadingChangeCallback != nullptr)
    {
        // callback to client, send motion activity state data back.
        //
        activitySensorDevice->EvtActivitySensorReadingChangeCallback(activitySensorDevice->thisModuleHandle,
                                                                     &activitySensorDevice->activitySensorState);
    }

    FuncExit(DMF_TRACE, "returnValue=ThreadedBufferQueue_BufferDisposition_WorkComplete");

    // Tell the Child Module that this Module is not longer the owner of the buffer.
    //
    return ThreadedBufferQueue_BufferDisposition_WorkComplete;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
ActivitySensorDevice::Initialize()
/*++

Routine Description:

    Initialize ActivitySensorDevice class instance.

Arguments:

    None

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ActivitySensor* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;
    moduleContext = DMF_CONTEXT_GET(thisModuleHandle);

    deviceWatcher = DeviceInformation::CreateWatcher(ActivitySensor::GetDeviceSelector());

    // Using lambda function is necessary here, because it need access variables that outside function scope,
    // but these callbacks don't have void pointer to pass in.
    //
    TypedEventHandler deviceInfoAddedHandler = TypedEventHandler<DeviceWatcher,
                                                                 DeviceInformation>([moduleContext](DeviceWatcher sender,
                                                                                                    DeviceInformation args)
    {
        // NOTE: In order to avoid runtime exceptions with C++/WinRT, it is necessary 
        //       to declare a pointer to the "container" buffer using the "container"
        //       type and then cast to VOID* when passing it to the Fetch Method.
        //       When Fetch Method returns the buffer is ready to be used without
        //       casting. Casting a VOID* to a "container" will cause a runtime
        //       exception even though it compiles and actually works.
        //
        DeviceInformationAndUpdateContainer* deviceInformationAndUpdate;
        NTSTATUS ntStatus;

        UNREFERENCED_PARAMETER(sender);

        // Get a Producer buffer. It is an empty buffer big enough to store the
        // motion activity sensor reading data.
        //
        ntStatus = DMF_ThreadedBufferQueue_Fetch(moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher,
                                                 (VOID**)&deviceInformationAndUpdate,
                                                 NULL);
        if (NT_SUCCESS(ntStatus))
        {
            DmfAssert(deviceInformationAndUpdate != NULL);

            // Create space for a copy of the C++/WinRT data and copy it.
            //
            DeviceInformationAndUpdateData* deviceInformationAndUpdateData = new DeviceInformationAndUpdateData(args,
                                                                                                                nullptr);
            if (deviceInformationAndUpdateData != NULL)
            {
                // Copy the data to enqueue.
                //
                deviceInformationAndUpdate->deviceInformationAndUpdateData = deviceInformationAndUpdateData;
                // Write it into the consumer buffer.
                //
                DMF_ThreadedBufferQueue_Enqueue(moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher,
                                                (VOID*)deviceInformationAndUpdate);
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Fail to allocate data");
            }
        }
        else
        {
            // There is no data buffer to store incoming DeviceInformationAndUpdate.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No buffer for Device Watcher event");
        }
    });

    TypedEventHandler deviceInfoRemovedHandler = TypedEventHandler<DeviceWatcher,
                                                                   DeviceInformationUpdate>([moduleContext](DeviceWatcher sender,
                                                                                                            DeviceInformationUpdate args)
    {
        // NOTE: In order to avoid runtime exceptions with C++/WinRT, it is necessary 
        //       to declare a pointer to the "container" buffer using the "container"
        //       type and then cast to VOID* when passing it to the Fetch Method.
        //       When Fetch Method returns the buffer is ready to be used without
        //       casting. Casting a VOID* to a "container" will cause a runtime
        //       exception even though it compiles and actually works.
        //
        DeviceInformationAndUpdateContainer* deviceInformationAndUpdate;
        NTSTATUS ntStatus;

        UNREFERENCED_PARAMETER(sender);

        ntStatus = DMF_ThreadedBufferQueue_Fetch(moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher,
                                                 (VOID**)&deviceInformationAndUpdate,
                                                 NULL);
        if (NT_SUCCESS(ntStatus))
        {
            DmfAssert(deviceInformationAndUpdate != NULL);

            // Create space for a copy of the C++/WinRT data and copy it.
            //
            DeviceInformationAndUpdateData* deviceInformationAndUpdateData = new DeviceInformationAndUpdateData(nullptr,
                                                                                                                args);
            if (deviceInformationAndUpdateData != NULL)
            {
                // Copy the data to enqueue.
                //
                deviceInformationAndUpdate->deviceInformationAndUpdateData = deviceInformationAndUpdateData;
                // Write it into the consumer buffer.
                //
                DMF_ThreadedBufferQueue_Enqueue(moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher,
                                                (VOID*)deviceInformationAndUpdate);
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Fail to allocate data");
            }
        }
        else
        {
            // There is no data buffer to store incoming DeviceInformationAndUpdate.
            //
            TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "No buffer for Device Watcher event");
        }
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
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DeviceWatcher enumeration complete");
    });

    if (deviceWatcher == nullptr)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Fail to create deviceWatcher");
        goto Exit;
    }

    // Register event callbacks.
    //
    tokenAdded = deviceWatcher.Added(deviceInfoAddedHandler);
    tokenRemoved = deviceWatcher.Removed(deviceInfoRemovedHandler);
    tokenUpdated = deviceWatcher.Updated(deviceInfoUpdatedHandler);
    tokenEnumCompleted = deviceWatcher.EnumerationCompleted(deviceInfoEnumCompletedHandler);

    // Start threaded buffer queue for motion activity data monitoring.
    //
    ntStatus = DMF_ThreadedBufferQueue_Start(moduleContext->DmfModuleThreadedBufferQueueActivitySensor);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // Start threaded buffer queue for device watcher.
    //
    ntStatus = DMF_ThreadedBufferQueue_Start(moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher);
    if (!NT_SUCCESS(ntStatus))
    {
        goto Exit;
    }

    // start device watcher
    //
    deviceWatcher.Start();

Exit:

    if (!NT_SUCCESS(ntStatus))
    {
        // Stop device watcher threaded buffer queue.
        //
        DMF_ThreadedBufferQueue_Stop(moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher);

        // Stop motion activity threaded buffer queue.
        //
        DMF_ThreadedBufferQueue_Stop(moduleContext->DmfModuleThreadedBufferQueueActivitySensor);

        // Close DeviceWatcher.
        //
        if (deviceWatcher != nullptr)
        {
            deviceWatcher.Added(tokenAdded);
            deviceWatcher.Removed(tokenRemoved);
            deviceWatcher.Updated(tokenUpdated);
            deviceWatcher.EnumerationCompleted(tokenEnumCompleted);
            deviceWatcher = nullptr;
        }
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
ActivitySensorDevice::Deinitialize()
/*++

Routine Description:

    DeInitialize ActivitySensorDevice instance.

Arguments:

    None

Return Value:

    None

--*/
{
    DMF_CONTEXT_ActivitySensor* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    moduleContext = DMF_CONTEXT_GET(thisModuleHandle);

    // First unhook all event handlers. This ensures our
    // event handlers don't get called after stop.
    //
    deviceWatcher.Added(tokenAdded);
    deviceWatcher.Removed(tokenRemoved);
    deviceWatcher.Updated(tokenUpdated);
    deviceWatcher.EnumerationCompleted(tokenEnumCompleted);

    if (deviceWatcher.Status() == DeviceWatcherStatus::Started ||
        deviceWatcher.Status() == DeviceWatcherStatus::EnumerationCompleted)
    {
        deviceWatcher.Stop();
    }

    // Flush and stop device watcher threaded buffer queue.
    //
    DMF_ThreadedBufferQueue_Flush(moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher);
    DMF_ThreadedBufferQueue_Stop(moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher);

    // Flush and stop motion activity threaded buffer queue.
    //
    DMF_ThreadedBufferQueue_Flush(moduleContext->DmfModuleThreadedBufferQueueActivitySensor);
    DMF_ThreadedBufferQueue_Stop(moduleContext->DmfModuleThreadedBufferQueueActivitySensor);

    // Unregister motion activity sensor update event handlers.
    //
    if (activitySensor != nullptr)
    {
        activitySensor.ReadingChanged(tokenReadingChanged);
    }

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
ActivitySensorDevice::Start()
/*++

Routine Description:

    Start the motion activity monitor and events, make it start to operate.

Arguments:

    None

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    try
    {
        deviceWatcher.Start();
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Device Watcher started");
    }
    catch (hresult_error const& ex)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Failed to start device watcher, HRESULT=0x%08X", ex.code().value);
    }

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
ActivitySensorDevice::Stop()
/*++

Routine Description:

    Stop the motion activity monitor and events.

Arguments:

    None

Return Value:

    None

--*/
{
    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Stop the device watcher.
    //
    try
    {
        deviceWatcher.Stop();
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Device Watcher stopped");
    }
    catch (hresult_error const& ex)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Failed to stop device watcher, HRESULT=0x%08X", ex.code().value);
    }
    FuncExitVoid(DMF_TRACE);
}

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
ActivitySensor_Initialize(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type ActivitySensor.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    FuncEntry(DMF_TRACE);

    DMF_CONTEXT_ActivitySensor* moduleContext;
    DMF_CONFIG_ActivitySensor* moduleConfig;
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = STATUS_UNSUCCESSFUL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Necessary call for using C++/WinRT environment.
    //
    init_apartment();

    moduleContext->activitySensorDevice = new ActivitySensorDevice();
    if (moduleContext->activitySensorDevice == nullptr)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Insufficient resource for activitySensorDevice instance, ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext->activitySensorDevice->thisModuleHandle = DmfModule;
    if (moduleConfig->DeviceId == NULL)
    {
        moduleConfig->DeviceId = L"";
    }
    moduleContext->activitySensorDevice->DeviceIdToFind = to_hstring(moduleConfig->DeviceId);
    moduleContext->activitySensorDevice->EvtActivitySensorReadingChangeCallback = moduleConfig->EvtActivitySensorReadingChangeCallback;
    ntStatus = moduleContext->activitySensorDevice->Initialize();

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "activitySensorDevice Initialize fails: ntStatus=%!STATUS!", ntStatus);
        delete moduleContext->activitySensorDevice;
        moduleContext->activitySensorDevice = nullptr;
    }

Exit:
    if (!NT_SUCCESS(ntStatus)) {
        uninit_apartment();
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

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
DMF_ActivitySensor_NotificationRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type ActivitySensor.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = ActivitySensor_Initialize(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationUnregister)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_ActivitySensor_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Deinitialize an instance of a DMF Module of type ActivitySensor.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_ActivitySensor* moduleContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->activitySensorDevice != nullptr)
    {
        if (moduleContext->activitySensorDevice->activitySensor != nullptr)
        {
            DMF_ModuleClose(DmfModule);
        }

        moduleContext->activitySensorDevice->Deinitialize();

        delete moduleContext->activitySensorDevice;
        moduleContext->activitySensorDevice = nullptr;
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
DMF_ActivitySensor_ChildModulesAdd(
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
    DMF_CONTEXT_ActivitySensor* moduleContext;
    DMF_CONFIG_ThreadedBufferQueue moduleConfigThreadedBufferQueueForDeviceWatcher;
    DMF_CONFIG_ThreadedBufferQueue moduleConfigThreadedBufferQueueForActivitySensor;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // ThreadedBufferQueue for device watcher.
    // ---------------------------------------
    //
    DMF_CONFIG_ThreadedBufferQueue_AND_ATTRIBUTES_INIT(&moduleConfigThreadedBufferQueueForDeviceWatcher,
                                                       &moduleAttributes);
    moduleConfigThreadedBufferQueueForDeviceWatcher.EvtThreadedBufferQueueWork = ActivitySensor_ThreadedBufferQueueDeviceWatcherWork;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.BufferCount = 32;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.BufferContextSize = 0;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.BufferSize = sizeof(DeviceInformationAndUpdateContainer);

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher);

    // ThreadedBufferQueue for motion activity sensor.
    // --------------------------------------------------
    //
    DMF_CONFIG_ThreadedBufferQueue_AND_ATTRIBUTES_INIT(&moduleConfigThreadedBufferQueueForActivitySensor,
                                                       &moduleAttributes);
    moduleConfigThreadedBufferQueueForActivitySensor.EvtThreadedBufferQueueWork = ActivitySensor_ThreadedBufferQueueActivitySensorWork;
    moduleConfigThreadedBufferQueueForActivitySensor.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
    moduleConfigThreadedBufferQueueForActivitySensor.BufferQueueConfig.SourceSettings.BufferCount = 32;
    moduleConfigThreadedBufferQueueForActivitySensor.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigThreadedBufferQueueForActivitySensor.BufferQueueConfig.SourceSettings.BufferContextSize = 0;
    moduleConfigThreadedBufferQueueForActivitySensor.BufferQueueConfig.SourceSettings.BufferSize = sizeof(ActivitySensorReadingDataContainer);

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThreadedBufferQueueActivitySensor);

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
DMF_ActivitySensor_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type ActivitySensor.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_ActivitySensor;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_ActivitySensor;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_ActivitySensor);
    dmfCallbacksDmf_ActivitySensor.ChildModulesAdd = DMF_ActivitySensor_ChildModulesAdd;
    dmfCallbacksDmf_ActivitySensor.DeviceNotificationRegister = DMF_ActivitySensor_NotificationRegister;
    dmfCallbacksDmf_ActivitySensor.DeviceNotificationUnregister = DMF_ActivitySensor_NotificationUnregister;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_ActivitySensor,
                                            ActivitySensor,
                                            DMF_CONTEXT_ActivitySensor,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware);

    dmfModuleDescriptor_ActivitySensor.CallbacksDmf = &dmfCallbacksDmf_ActivitySensor;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_ActivitySensor,
                                DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DMF_ModuleCreate failed, ntStatus=%!STATUS!", ntStatus);
    }

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return(ntStatus);

}
#pragma code_seg()

// Module Methods
//

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ActivitySensor_CurrentStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ ACTIVITY_SENSOR_STATE* CurrentState
    )
/*++

Routine Description:

    Get the current motion activity state from sensor.

Arguments:

    DmfModule - This Module's handle.
    CurrentState - Current motion activity state that this module hold.
                   Caller should only use this returned value when NTSTATUS is STATUS_SUCCESS.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ActivitySensor* moduleContext;
    ActivitySensorReading currentActivitySensorReading = nullptr;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ActivitySensor);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Motion activity sensor is not found.");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Query sensor for current reading.
    //
    try
    {
        currentActivitySensorReading = moduleContext->activitySensorDevice->activitySensor.GetCurrentReadingAsync().get();
    }
    catch (hresult_error const& ex)
    {
        ex.code();
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Query from GetCurrentReadingAsync fails");
        goto Exit;
    }

    moduleContext->activitySensorDevice->activitySensorState.CurrentActivitySensorState = (ActivitySensor_Reading)currentActivitySensorReading.Activity();
    *CurrentState = moduleContext->activitySensorDevice->activitySensorState;

    ntStatus = STATUS_SUCCESS;

Exit:

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ActivitySensor_Start(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Start the motion activity monitor and events, it puts motion activity sensor to work.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ActivitySensor* moduleContext;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ActivitySensor);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Motion activity module is not open.");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleContext->activitySensorDevice->Start();
    ntStatus = STATUS_SUCCESS;

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_ActivitySensor_Stop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stop the motion activity monitor and events.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_ActivitySensor* moduleContext;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMFMODULE_VALIDATE_IN_METHOD(DmfModule,
                                 ActivitySensor);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Motion activity module is not open.");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleContext->activitySensorDevice->Stop();
    ntStatus = STATUS_SUCCESS;

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#endif // IS_WIN10_19H1_OR_LATER

// eof: Dmf_ActivitySensor.cpp
//
