/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    Dmf_SimpleOrientation.cpp

Abstract:

    This Module that can be used to get the Simple Orientation information from device.
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

#include "Dmf_SimpleOrientation.tmh"

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
class SimpleOrientationSensorReadingData
{
public:
    SimpleOrientationSensorReadingData(SimpleOrientationSensorOrientationChangedEventArgs args)
    {
        simpleOrientationSensorOrientationChangedEventArgs = args;
    };
    SimpleOrientationSensorOrientationChangedEventArgs simpleOrientationSensorOrientationChangedEventArgs = nullptr;
};

// This structure is a container that allows us to store C++/WinRT data
// in a flat buffer from DMF_BufferPool.
//
typedef struct
{
    // This is a pointer to a copy of the C++/WinRT data.
    //
    SimpleOrientationSensorReadingData* simpleOrientationSensorReadingData;
} SimpleOrientationSensorReadingDataContainer;

class SimpleOrientationDevice
{
private:

    // DeviceWatcher for Simple Orientation sensor.
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
    // SimpleOrientationSensor instance.
    //
    SimpleOrientationSensor simpleOrientationSensor = nullptr;
    // Store the Device Id of simple orientation sensor that is found.
    //
    hstring deviceId = L"";
    // CustomSensor necessary event token.
    //
    event_token tokenReadingChanged;
    // Simple orientation state, initialize with all 0s.
    //
    SIMPLE_ORIENTATION_SENSOR_STATE simpleOrientationState = { 0 };
    // This Module's handle, store it here and used for callbacks.
    //
    DMFMODULE thisModuleHandle;
    // Callback to inform Parent Module that simple orientation sensor has new changed reading.
    //
    EVT_DMF_SimpleOrientation_SimpleOrientationSensorReadingChangeCallback* EvtSimpleOrientationReadingChangeCallback;
    // Initialize route.
    //
    NTSTATUS Initialize();
    // DeInitialize function.
    //
    VOID Deinitialize();
    // Start function, start simple orientation sensor monitoring and events.
    //
    VOID Start();
    // Stop function, stop simple orientation sensor monitoring and events.
    //
    VOID Stop();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// SimpleOrientation module's context.
//
typedef struct
{
    // SimpleOrientationDevice class instance pointer.
    //
    SimpleOrientationDevice* simpleOrientationDevice = nullptr;

    // ThreadedBufferQueue for device watcher.
    //
    DMFMODULE DmfModuleThreadedBufferQueueDeviceWatcher;

    // ThreadedBufferQueue for simple orientation sensor.
    //
    DMFMODULE DmfModuleThreadedBufferQueueSimpleOrientation;
} DMF_CONTEXT_SimpleOrientation;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(SimpleOrientation)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(SimpleOrientation)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(EVT_DMF_ThreadedBufferQueue_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
ThreadedBufferQueue_BufferDisposition
SimpleOrientation_ThreadedBufferQueueDeviceWatcherWork(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* ClientWorkBuffer,
    _In_ ULONG ClientWorkBufferSize,
    _In_ VOID* ClientWorkBufferContext,
    _Out_ NTSTATUS* NtStatus
    )
/*++

Routine Description:

    Callback function of device watcher threaded buffer queue when there is work to process.
    It processes add and remove events of the simple orientation sensor from device watcher.

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
    DMFMODULE dmfModuleSimpleOrientation;
    DMF_CONTEXT_SimpleOrientation* moduleContext;

    UNREFERENCED_PARAMETER(ClientWorkBufferContext);
    UNREFERENCED_PARAMETER(ClientWorkBufferSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Initialize for SAL, but no caller will read this status.
    //
    *NtStatus = STATUS_SUCCESS;

    dmfModuleSimpleOrientation = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleSimpleOrientation);

    SimpleOrientationDevice* simpleOrientationWatcher = moduleContext->simpleOrientationDevice;

    // Event handler lambda function for simple orientation reading change.
    //
    TypedEventHandler simpleOrientationReadingChangedHandler = TypedEventHandler<SimpleOrientationSensor,
                                                                                 SimpleOrientationSensorOrientationChangedEventArgs>([moduleContext](SimpleOrientationSensor sender,
                                                                                                                                                     SimpleOrientationSensorOrientationChangedEventArgs args)
    {
        // NOTE: In order to avoid runtime exceptions with C++/WinRT, it is necessary 
        //       to declare a pointer to the "container" buffer using the "container"
        //       type and then cast to VOID* when passing it to the Fetch Method.
        //       When Fetch Method returns the buffer is ready to be used without
        //       casting. Casting a VOID* to a "container" will cause a runtime
        //       exception even though it compiles and actually works.
        //
        SimpleOrientationSensorReadingDataContainer* simpleOrientationSensorReadingDataContainer;
        NTSTATUS ntStatus;

        UNREFERENCED_PARAMETER(sender);

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "ReadingChanged event triggered from simple orientation");

        // Get a Producer buffer. It is an empty buffer big enough to store the
        // custom sensor reading data.
        //
        ntStatus = DMF_ThreadedBufferQueue_Fetch(moduleContext->DmfModuleThreadedBufferQueueSimpleOrientation,
                                                 (VOID**)&simpleOrientationSensorReadingDataContainer,
                                                 NULL);
        if (NT_SUCCESS(ntStatus))
        {
            DmfAssert(simpleOrientationSensorReadingDataContainer != NULL);

            // Copy the simple orientation data to callback buffer.
            //
            if (args != nullptr)
            {
                // Create space for a copy of the C++/WinRT data and copy it.
                //
                SimpleOrientationSensorReadingData* simpleOrientationSensorReadingData = new SimpleOrientationSensorReadingData(args);
                if (simpleOrientationSensorReadingData != NULL)
                {
                    // Set the pointer to the new created simpleOrientationSensorReadingData.
                    //
                    simpleOrientationSensorReadingDataContainer->simpleOrientationSensorReadingData = simpleOrientationSensorReadingData;
                    // Write it into the consumer buffer.
                    // (Enqueue the container structure that stores C++/WinRT data.)
                    //
                    DMF_ThreadedBufferQueue_Enqueue(moduleContext->DmfModuleThreadedBufferQueueSimpleOrientation,
                                                    (VOID*)simpleOrientationSensorReadingDataContainer);
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
    SimpleOrientationSensor simpleOrientationSensor = nullptr;

    if (deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfo != nullptr)
    {
        // Process an "Add" event.
        //
        deviceInformation = deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfo;

        // If simple orientation interface is not nullptr, means it is found and available.
        //
        if (simpleOrientationWatcher->simpleOrientationSensor != nullptr)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Simple Orientation sensor has already been found, no extra interface needed");
            goto Exit;
        }

        // Check if this device matches the one specified in the Config.
        //
        hstring deviceId = deviceInformation.Id();

        // If target device Id is not blank and didn't match current device, exit.
        //
        if (moduleContext->simpleOrientationDevice->DeviceIdToFind != L"")
        {
            string deviceIdToFind = winrt::to_string(moduleContext->simpleOrientationDevice->DeviceIdToFind);
            string currentDeviceId = winrt::to_string(deviceId);
            if (currentDeviceId.find(deviceIdToFind) == string::npos)
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Current simple orientation sensor is not the target, bypass current one");
                goto Exit;
            }
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Simple Orientation sensor found");
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Device id is %ls", deviceId.c_str());
        try
        {
            if (moduleContext->simpleOrientationDevice->DeviceIdToFind != L"")
            {
                simpleOrientationSensor = SimpleOrientationSensor::FromIdAsync(deviceId).get();
            }
            else
            {
                simpleOrientationSensor = SimpleOrientationSensor::GetDefault();
            }
        }
        catch (hresult_error const& ex)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Failed to get SimpleOrientationSensor, HRESULT=0x%08X", ex.code().value);
            goto Exit;
        }

        // Store simple orientation interface and device Id in class member.
        //
        simpleOrientationWatcher->simpleOrientationSensor = simpleOrientationSensor;
        simpleOrientationWatcher->deviceId = deviceId;
        simpleOrientationWatcher->simpleOrientationState.IsSensorValid = true;

        // Simple orientation sensor resource is ready, open this module.
        //
        *NtStatus = DMF_ModuleOpen(dmfModuleSimpleOrientation);
        if (!NT_SUCCESS(*NtStatus))
        {
            simpleOrientationWatcher->simpleOrientationSensor = nullptr;
            simpleOrientationWatcher->deviceId = L"";
            simpleOrientationWatcher->simpleOrientationState.IsSensorValid = false;
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_ModuleOpen fails: ntStatus = %!STATUS!", *NtStatus);
            goto Exit;
        }
        simpleOrientationWatcher->tokenReadingChanged = simpleOrientationWatcher->simpleOrientationSensor.OrientationChanged(simpleOrientationReadingChangedHandler);
    }
    else if (deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfoUpdate != nullptr)
    {
        // Process a "Remove" event.
        //
        deviceInformationUpdate = deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfoUpdate;

        if (deviceInformationUpdate.Id() != simpleOrientationWatcher->deviceId)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Not our simple orientation device");
            goto Exit;
        }

        // Simple orientation has been removed.
        //       
        DMF_ModuleClose(dmfModuleSimpleOrientation);
        if (simpleOrientationWatcher->simpleOrientationSensor != nullptr)
        {
            try
            {
                // Unregister reading change handler.
                //
                simpleOrientationWatcher->simpleOrientationSensor.OrientationChanged(simpleOrientationWatcher->tokenReadingChanged);
                // Dereference simple orientation interface.
                //
                simpleOrientationWatcher->simpleOrientationSensor = nullptr;
                simpleOrientationWatcher->simpleOrientationState.IsSensorValid = false;
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Simple orientation has been removed");
            }
            catch (hresult_error const& ex)
            {
                UNREFERENCED_PARAMETER(ex);
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Simple orientation has been removed before unregister callback token");
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
SimpleOrientation_ThreadedBufferQueueSimpleOrientationWork(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* ClientWorkBuffer,
    _In_ ULONG ClientWorkBufferSize,
    _In_ VOID* ClientWorkBufferContext,
    _Out_ NTSTATUS* NtStatus
    )
/*++

Routine Description:

    Callback function of simple orientation threaded buffer queue when there is a work need to process.
    It process data reading change event of simple orientation sensor.

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
    DMFMODULE dmfModuleSimpleOrientation;
    DMF_CONTEXT_SimpleOrientation* moduleContext;

    UNREFERENCED_PARAMETER(ClientWorkBufferContext);
    UNREFERENCED_PARAMETER(ClientWorkBufferSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Initialize for SAL, but no caller will read this status.
    //
    *NtStatus = STATUS_SUCCESS;

    dmfModuleSimpleOrientation = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleSimpleOrientation);

    SimpleOrientationDevice* simpleOrientationDevice = moduleContext->simpleOrientationDevice;

    SimpleOrientationSensorReadingDataContainer* simpleOrientationSensorReadingDataContainer = (SimpleOrientationSensorReadingDataContainer*)ClientWorkBuffer;

    simpleOrientationDevice->simpleOrientationState.CurrentSimpleOrientation = (SimpleOrientation_State)simpleOrientationSensorReadingDataContainer->simpleOrientationSensorReadingData->simpleOrientationSensorOrientationChangedEventArgs.Orientation();

    if (simpleOrientationDevice->EvtSimpleOrientationReadingChangeCallback != nullptr)
    {
        // callback to client, send simple orientation state data back.
        //
        simpleOrientationDevice->EvtSimpleOrientationReadingChangeCallback(simpleOrientationDevice->thisModuleHandle,
                                                                           &simpleOrientationDevice->simpleOrientationState);
    }

    FuncExit(DMF_TRACE, "returnValue=ThreadedBufferQueue_BufferDisposition_WorkComplete");

    // Tell the Child Module that this Module is not longer the owner of the buffer.
    //
    return ThreadedBufferQueue_BufferDisposition_WorkComplete;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
SimpleOrientationDevice::Initialize()
/*++

Routine Description:

    Initialize SimpleOrientationDevice class instance.

Arguments:

    None

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SimpleOrientation* moduleContext;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;
    moduleContext = DMF_CONTEXT_GET(thisModuleHandle);

    deviceWatcher = DeviceInformation::CreateWatcher(SimpleOrientationSensor::GetDeviceSelector());

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
        // simple orientation sensor reading data.
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

    // Start threaded buffer queue for simple orientation data monitoring.
    //
    ntStatus = DMF_ThreadedBufferQueue_Start(moduleContext->DmfModuleThreadedBufferQueueSimpleOrientation);
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

        // Stop simple orientation threaded buffer queue.
        //
        DMF_ThreadedBufferQueue_Stop(moduleContext->DmfModuleThreadedBufferQueueSimpleOrientation);

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
SimpleOrientationDevice::Deinitialize()
/*++

Routine Description:

    DeInitialize SimpleOrientationDevice instance.

Arguments:

    None

Return Value:

    None

--*/
{
    DMF_CONTEXT_SimpleOrientation* moduleContext;

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

    // Flush and stop simple orientation threaded buffer queue.
    //
    DMF_ThreadedBufferQueue_Flush(moduleContext->DmfModuleThreadedBufferQueueSimpleOrientation);
    DMF_ThreadedBufferQueue_Stop(moduleContext->DmfModuleThreadedBufferQueueSimpleOrientation);

    // Unregister simple orientation sensor update event handlers.
    //
    if (simpleOrientationSensor != nullptr)
    {
        simpleOrientationSensor.OrientationChanged(tokenReadingChanged);
    }

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
SimpleOrientationDevice::Start()
/*++

Routine Description:

    Start the simple orientation monitor and events, make it start to operate.

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
SimpleOrientationDevice::Stop()
/*++

Routine Description:

    Stop the simple orientation monitor and events.

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

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support functions
///////////////////////////////////////////////////////////////////////////////////////////////////////
//
#pragma code_seg("PAGE")
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
SimpleOrientation_Initialize(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type SimpleOrientation.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    FuncEntry(DMF_TRACE);

    DMF_CONTEXT_SimpleOrientation* moduleContext;
    DMF_CONFIG_SimpleOrientation* moduleConfig;
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = STATUS_UNSUCCESSFUL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Necessary call for using C++/WinRT environment.
    //
    init_apartment();

    moduleContext->simpleOrientationDevice = new SimpleOrientationDevice();
    if (moduleContext->simpleOrientationDevice == nullptr)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Insufficient resource for simpleOrientationDevice instance, ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext->simpleOrientationDevice->thisModuleHandle = DmfModule;
    moduleContext->simpleOrientationDevice->DeviceIdToFind = to_hstring(moduleConfig->DeviceId);
    moduleContext->simpleOrientationDevice->EvtSimpleOrientationReadingChangeCallback = moduleConfig->EvtSimpleOrientationReadingChangeCallback;
    ntStatus = moduleContext->simpleOrientationDevice->Initialize();

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "simpleOrientationDevice Initialize fails: ntStatus=%!STATUS!", ntStatus);
        delete moduleContext->simpleOrientationDevice;
        moduleContext->simpleOrientationDevice = nullptr;
    }

Exit:

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
DMF_SimpleOrientation_NotificationRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type SimpleOrientation.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = SimpleOrientation_Initialize(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationUnregister)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_SimpleOrientation_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Deinitialize an instance of a DMF Module of type SimpleOrientation.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_SimpleOrientation* moduleContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->simpleOrientationDevice != nullptr)
    {
        if (moduleContext->simpleOrientationDevice->simpleOrientationSensor != nullptr)
        {
            DMF_ModuleClose(DmfModule);
        }

        moduleContext->simpleOrientationDevice->Deinitialize();

        delete moduleContext->simpleOrientationDevice;
        moduleContext->simpleOrientationDevice = nullptr;
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
DMF_SimpleOrientation_ChildModulesAdd(
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
    DMF_CONTEXT_SimpleOrientation* moduleContext;
    DMF_CONFIG_ThreadedBufferQueue moduleConfigThreadedBufferQueueForDeviceWatcher;
    DMF_CONFIG_ThreadedBufferQueue moduleConfigThreadedBufferQueueForSimpleOrientation;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // ThreadedBufferQueue for device watcher.
    // ---------------------------------------
    //
    DMF_CONFIG_ThreadedBufferQueue_AND_ATTRIBUTES_INIT(&moduleConfigThreadedBufferQueueForDeviceWatcher,
                                                       &moduleAttributes);
    moduleConfigThreadedBufferQueueForDeviceWatcher.EvtThreadedBufferQueueWork = SimpleOrientation_ThreadedBufferQueueDeviceWatcherWork;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.BufferCount = 32;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.BufferContextSize = 0;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.BufferSize = sizeof(DeviceInformationAndUpdateContainer);

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher);

    // ThreadedBufferQueue for simple orientation sensor.
    // --------------------------------------------------
    //
    DMF_CONFIG_ThreadedBufferQueue_AND_ATTRIBUTES_INIT(&moduleConfigThreadedBufferQueueForSimpleOrientation,
                                                       &moduleAttributes);
    moduleConfigThreadedBufferQueueForSimpleOrientation.EvtThreadedBufferQueueWork = SimpleOrientation_ThreadedBufferQueueSimpleOrientationWork;
    moduleConfigThreadedBufferQueueForSimpleOrientation.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
    moduleConfigThreadedBufferQueueForSimpleOrientation.BufferQueueConfig.SourceSettings.BufferCount = 5;
    moduleConfigThreadedBufferQueueForSimpleOrientation.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigThreadedBufferQueueForSimpleOrientation.BufferQueueConfig.SourceSettings.BufferContextSize = 0;
    moduleConfigThreadedBufferQueueForSimpleOrientation.BufferQueueConfig.SourceSettings.BufferSize = sizeof(SimpleOrientationSensorReadingDataContainer);

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThreadedBufferQueueSimpleOrientation);

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
DMF_SimpleOrientation_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type SimpleOrientation.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_SimpleOrientation;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_SimpleOrientation;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_SimpleOrientation);
    dmfCallbacksDmf_SimpleOrientation.ChildModulesAdd = DMF_SimpleOrientation_ChildModulesAdd;
    dmfCallbacksDmf_SimpleOrientation.DeviceNotificationRegister = DMF_SimpleOrientation_NotificationRegister;
    dmfCallbacksDmf_SimpleOrientation.DeviceNotificationUnregister = DMF_SimpleOrientation_NotificationUnregister;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_SimpleOrientation,
                                            SimpleOrientation,
                                            DMF_CONTEXT_SimpleOrientation,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware);

    dmfModuleDescriptor_SimpleOrientation.CallbacksDmf = &dmfCallbacksDmf_SimpleOrientation;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_SimpleOrientation,
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
DMF_SimpleOrientation_CurrentStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ SIMPLE_ORIENTATION_SENSOR_STATE* CurrentState
    )
/*++

Routine Description:

    Get the current simple orientation state from sensor.

Arguments:

    DmfModule - This Module's handle.
    CurrentState - Current simple orientation state that this module hold.
                   Caller should only use this returned value when NTSTATUS is STATUS_SUCCESS.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SimpleOrientation* moduleContext;
    SimpleOrientation currentSimpleOrientation;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Simple orientation sensor is not found yet.");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Query sensor for current reading.
    //
    try
    {
        currentSimpleOrientation = moduleContext->simpleOrientationDevice->simpleOrientationSensor.GetCurrentOrientation();
    }
    catch (hresult_error const& ex)
    {
        ex.code();
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Query from GetCurrentOrientation fails");
        goto Exit;
    }

    moduleContext->simpleOrientationDevice->simpleOrientationState.CurrentSimpleOrientation = (SimpleOrientation_State)currentSimpleOrientation;
    *CurrentState = moduleContext->simpleOrientationDevice->simpleOrientationState;

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
DMF_SimpleOrientation_Start(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Start the simple orientation monitor and events, it puts simple orientation sensor to work.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SimpleOrientation* moduleContext;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Hinge angle module is not open yet.");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleContext->simpleOrientationDevice->Start();
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
DMF_SimpleOrientation_Stop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stop the simple orientation monitor and events.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_SimpleOrientation* moduleContext;

    UNREFERENCED_PARAMETER(DmfModule);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Hinge angle module is not open yet.");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleContext->simpleOrientationDevice->Stop();
    ntStatus = STATUS_SUCCESS;

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#endif // IS_WIN10_19H1_OR_LATER

// eof: Dmf_SimpleOrientation.cpp
//
