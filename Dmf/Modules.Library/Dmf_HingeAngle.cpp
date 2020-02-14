/*++

    Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    Dmf_HingeAngle.cpp

Abstract:

    This Module that can be used to get the Hinge Angle information from device.
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

#include "Dmf_HingeAngle.tmh"

// This Module uses C++/WinRT so it needs RS5+ support. 
// This code will not be compiled in RS4 and below.
//
#if defined(DMF_USER_MODE) && IS_WIN10_RS5_OR_LATER && defined(__cplusplus)

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
class HingeAngleSensorReadingData
{
public:
    HingeAngleSensorReadingData(HingeAngleSensorReadingChangedEventArgs args)
    {
        hingeAngleSensorReadingChangedEventArgs = args;
    };
    HingeAngleSensorReadingChangedEventArgs hingeAngleSensorReadingChangedEventArgs = nullptr;
};

// This structure is a container that allows us to store C++/WinRT data
// in a flat buffer from DMF_BufferPool.
//
typedef struct
{
    // This is a pointer to a copy of the C++/WinRT data.
    //
    HingeAngleSensorReadingData* hingeAngleSensorReadingData;
} HingeAngleSensorReadingDataContainer;

class HingeAngleDevice
{
private:

    // DeviceWatcher for Hinge Angle sensor.
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
    // HingeAngleSensor instance.
    //
    HingeAngleSensor hingeAngleSensor = nullptr;
    // Store the Device Id of hinge angle sensor that is found.
    //
    hstring deviceId = L"";
    // CustomSensor necessary event token.
    //
    event_token tokenReadingChanged;
    // Hinge angle state, initialize with all 0s.
    //
    HINGE_ANGLE_SENSOR_STATE hingeAngleState = { 0 };
    // This Module's handle, store it here and used for callbacks.
    //
    DMFMODULE thisModuleHandle;
    // Callback to inform Parent Module that hinge angle sensor has new changed reading.
    //
    EVT_DMF_HingeAngle_HingeAngleSensorReadingChangeCallback* EvtHingeAngleReadingChangeCallback;
    // Initialize route.
    //
    NTSTATUS Initialize();
    // DeInitialize function.
    //
    VOID Deinitialize();
    // Start function, start hinge angle sensor monitoring and events.
    //
    VOID Start();
    // Stop function, stop hinge angle sensor monitoring and events.
    //
    VOID Stop();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Module Private Context
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

// HingeAngle module's context.
//
typedef struct
{
    // HingeAngleDevice class instance pointer.
    //
    HingeAngleDevice* hingeAngleDevice = nullptr;

    // ThreadedBufferQueue for device watcher.
    //
    DMFMODULE DmfModuleThreadedBufferQueueDeviceWatcher;

    // ThreadedBufferQueue for hinge angle sensor.
    //
    DMFMODULE DmfModuleThreadedBufferQueueHingeAngle;
} DMF_CONTEXT_HingeAngle;

// This macro declares the following function:
// DMF_CONTEXT_GET()
//
DMF_MODULE_DECLARE_CONTEXT(HingeAngle)

// This macro declares the following function:
// DMF_CONFIG_GET()
//
DMF_MODULE_DECLARE_CONFIG(HingeAngle)

///////////////////////////////////////////////////////////////////////////////////////////////////////
// DMF Module Support Code
///////////////////////////////////////////////////////////////////////////////////////////////////////
//

_Function_class_(EVT_DMF_ThreadedBufferQueue_Callback)
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
ThreadedBufferQueue_BufferDisposition
HingeAngle_ThreadedBufferQueueDeviceWatcherWork(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* ClientWorkBuffer,
    _In_ ULONG ClientWorkBufferSize,
    _In_ VOID* ClientWorkBufferContext,
    _Out_ NTSTATUS* NtStatus
    )
/*++

Routine Description:

    Callback function of device watcher threaded buffer queue when there is work to process.
    It processes add and remove events of the hinge angle sensor from device watcher.

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
    DMFMODULE dmfModuleHingeAngle;
    DMF_CONTEXT_HingeAngle* moduleContext;
    DMF_CONFIG_HingeAngle* moduleConfig;

    UNREFERENCED_PARAMETER(ClientWorkBufferContext);
    UNREFERENCED_PARAMETER(ClientWorkBufferSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Initialize for SAL, but no caller will read this status.
    //
    *NtStatus = STATUS_SUCCESS;

    dmfModuleHingeAngle = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleHingeAngle);
    moduleConfig = DMF_CONFIG_GET(dmfModuleHingeAngle);

    HingeAngleDevice* hingeAngleWatcher = moduleContext->hingeAngleDevice;

    // Event handler lambda function for hinge angle reading change.
    //
    TypedEventHandler hingeAngleReadingChangedHandler = TypedEventHandler<HingeAngleSensor,
                                                                          HingeAngleSensorReadingChangedEventArgs>([moduleContext](HingeAngleSensor sender,
                                                                                                                                   HingeAngleSensorReadingChangedEventArgs args)
    {
        // NOTE: In order to avoid runtime exceptions with C++/WinRT, it is necessary 
        //       to declare a pointer to the "container" buffer using the "container"
        //       type and then cast to VOID* when passing it to the Fetch Method.
        //       When Fetch Method returns the buffer is ready to be used without
        //       casting. Casting a VOID* to a "container" will cause a runtime
        //       exception even though it compiles and actually works.
        //
        HingeAngleSensorReadingDataContainer* hingeAngleSensorReadingDataContainer;
        NTSTATUS ntStatus;

        UNREFERENCED_PARAMETER(sender);

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "ReadingChanged event triggered from hinge angle");

        // Get a Producer buffer. It is an empty buffer big enough to store the
        // custom sensor reading data.
        //
        ntStatus = DMF_ThreadedBufferQueue_Fetch(moduleContext->DmfModuleThreadedBufferQueueHingeAngle,
                                                 (VOID**)&hingeAngleSensorReadingDataContainer,
                                                 NULL);
        if (NT_SUCCESS(ntStatus))
        {
            DmfAssert(hingeAngleSensorReadingDataContainer != NULL);

            // Copy the hinge angle data to callback buffer.
            //
            if (args != nullptr)
            {
                // Create space for a copy of the C++/WinRT data and copy it.
                //
                HingeAngleSensorReadingData* hingeAngleSensorReadingData = new HingeAngleSensorReadingData(args);
                if (hingeAngleSensorReadingData != NULL)
                {
                    // Set the pointer to the new created hingeAngleSensorReadingData.
                    //
                    hingeAngleSensorReadingDataContainer->hingeAngleSensorReadingData = hingeAngleSensorReadingData;
                    // Write it into the consumer buffer.
                    // (Enqueue the container structure that stores C++/WinRT data.)
                    //
                    DMF_ThreadedBufferQueue_Enqueue(moduleContext->DmfModuleThreadedBufferQueueHingeAngle,
                                                    (VOID*)hingeAngleSensorReadingDataContainer);
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
    HingeAngleSensor hingeAngleSensor = nullptr;

    if (deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfo != nullptr)
    {
        // Process an "Add" event.
        //
        deviceInformation = deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfo;

        // If hinge angle interface is not nullptr, means it is found and available.
        //
        if (hingeAngleWatcher->hingeAngleSensor != nullptr)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Hinge Angle sensor has already been found, no extra interface needed");
            goto Exit;
        }

        // Check if this device matches the one specified in the Config.
        //
        hstring deviceId = deviceInformation.Id();

        // If target device Id is not blank and didn't match current device, exit.
        //
        if (moduleContext->hingeAngleDevice->DeviceIdToFind != L"")
        {
            string deviceIdToFind = winrt::to_string(moduleContext->hingeAngleDevice->DeviceIdToFind);
            string currentDeviceId = winrt::to_string(deviceId);
            if (currentDeviceId.find(deviceIdToFind) == string::npos)
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Current hinge angle sensor is not the target, bypass current one");
                goto Exit;
            }
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Hinge Angle sensor found");
        TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Device id is %ls", deviceId.c_str());
        try
        {
            if (moduleContext->hingeAngleDevice->DeviceIdToFind != L"")
            {
                hingeAngleSensor = HingeAngleSensor::FromIdAsync(deviceId).get();
            }
            else
            {
                hingeAngleSensor = HingeAngleSensor::GetDefaultAsync().get();
            }
        }
        catch (hresult_error const& ex)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Failed to get HingeAngleSensor, HRESULT=%!HRESULT!", ex.code().value);
            goto Exit;
        }

        // Store hinge angle interface and device Id in class member.
        //
        hingeAngleWatcher->hingeAngleSensor = hingeAngleSensor;
        hingeAngleWatcher->deviceId = deviceId;
        hingeAngleWatcher->hingeAngleState.IsSensorValid = true;

        // Hinge angle sensor resource is ready, open this module.
        //
        *NtStatus = DMF_ModuleOpen(dmfModuleHingeAngle);
        if (!NT_SUCCESS(*NtStatus))
        {
            hingeAngleWatcher->hingeAngleSensor = nullptr;
            hingeAngleWatcher->deviceId = L"";
            hingeAngleWatcher->hingeAngleState.IsSensorValid = false;
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "DMF_ModuleOpen fails: ntStatus = %!STATUS!", *NtStatus);
            goto Exit;
        }
        if (moduleConfig->ReportThresholdInDegrees > 0) 
        {
            hingeAngleSensor.ReportThresholdInDegrees(moduleConfig->ReportThresholdInDegrees);
        }
        hingeAngleWatcher->tokenReadingChanged = hingeAngleWatcher->hingeAngleSensor.ReadingChanged(hingeAngleReadingChangedHandler);
    }
    else if (deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfoUpdate != nullptr)
    {
        // Process a "Remove" event.
        //
        deviceInformationUpdate = deviceInformationAndUpdate->deviceInformationAndUpdateData->deviceInfoUpdate;

        if (deviceInformationUpdate.Id() != hingeAngleWatcher->deviceId)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Not our hinge angle device");
            goto Exit;
        }

        // Hinge angle has been removed.
        //       
        DMF_ModuleClose(dmfModuleHingeAngle);
        if (hingeAngleWatcher->hingeAngleSensor != nullptr)
        {
            try
            {
                // Unregister reading change handler.
                //
                hingeAngleWatcher->hingeAngleSensor.ReadingChanged(hingeAngleWatcher->tokenReadingChanged);
                // Dereference hinge angle interface.
                //
                hingeAngleWatcher->hingeAngleSensor = nullptr;
                hingeAngleWatcher->hingeAngleState.IsSensorValid = false;
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Hinge angle has been removed");
            }
            catch (hresult_error const& ex)
            {
                UNREFERENCED_PARAMETER(ex);
                TraceEvents(TRACE_LEVEL_INFORMATION, DMF_TRACE, "Hinge angle has been removed before unregister callback token");
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
HingeAngle_ThreadedBufferQueueHingeAngleWork(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* ClientWorkBuffer,
    _In_ ULONG ClientWorkBufferSize,
    _In_ VOID* ClientWorkBufferContext,
    _Out_ NTSTATUS* NtStatus
    )
/*++

Routine Description:

    Callback function of hinge angle threaded buffer queue when there is a work need to process.
    It process data reading change event of hinge angle sensor.

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
    DMFMODULE dmfModuleHingeAngle;
    DMF_CONTEXT_HingeAngle* moduleContext;

    UNREFERENCED_PARAMETER(ClientWorkBufferContext);
    UNREFERENCED_PARAMETER(ClientWorkBufferSize);

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    // Initialize for SAL, but no caller will read this status.
    //
    *NtStatus = STATUS_SUCCESS;

    dmfModuleHingeAngle = DMF_ParentModuleGet(DmfModule);
    moduleContext = DMF_CONTEXT_GET(dmfModuleHingeAngle);

    HingeAngleDevice* hingeAngleDevice = moduleContext->hingeAngleDevice;

    HingeAngleSensorReadingDataContainer* hingeAngleSensorReadingDataContainer = (HingeAngleSensorReadingDataContainer*)ClientWorkBuffer;

    hingeAngleDevice->hingeAngleState.AngleInDegrees = hingeAngleSensorReadingDataContainer->hingeAngleSensorReadingData->hingeAngleSensorReadingChangedEventArgs.Reading().AngleInDegrees();

    if (hingeAngleDevice->EvtHingeAngleReadingChangeCallback != nullptr)
    {
        // callback to client, send hinge angle state data back.
        //
        hingeAngleDevice->EvtHingeAngleReadingChangeCallback(hingeAngleDevice->thisModuleHandle,
                                                             &hingeAngleDevice->hingeAngleState);
    }

    FuncExit(DMF_TRACE, "returnValue=ThreadedBufferQueue_BufferDisposition_WorkComplete");

    // Tell the Child Module that this Module is not longer the owner of the buffer.
    //
    return ThreadedBufferQueue_BufferDisposition_WorkComplete;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
HingeAngleDevice::Initialize()
/*++

Routine Description:

    Initialize HingeAngleDevice class instance.

Arguments:

    None

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HingeAngle* moduleContext;
    DMF_CONFIG_HingeAngle* moduleConfig;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = STATUS_UNSUCCESSFUL;
    moduleContext = DMF_CONTEXT_GET(thisModuleHandle);
    moduleConfig = DMF_CONFIG_GET(thisModuleHandle);

    // Create device watcher according to the sensor GUID from ModuleConfig.
    //
    deviceWatcher = DeviceInformation::CreateWatcher(HingeAngleSensor::GetDeviceSelector());

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
        // hinge angle sensor reading data.
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

    // Start threaded buffer queue for hinge angle data monitoring.
    //
    ntStatus = DMF_ThreadedBufferQueue_Start(moduleContext->DmfModuleThreadedBufferQueueHingeAngle);
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

        // Stop hinge angle threaded buffer queue.
        //
        DMF_ThreadedBufferQueue_Stop(moduleContext->DmfModuleThreadedBufferQueueHingeAngle);

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
HingeAngleDevice::Deinitialize()
/*++

Routine Description:

    DeInitialize HingeAngleDevice instance.

Arguments:

    None

Return Value:

    None

--*/
{
    DMF_CONTEXT_HingeAngle* moduleContext;

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

    // Flush and stop hinge angle threaded buffer queue.
    //
    DMF_ThreadedBufferQueue_Flush(moduleContext->DmfModuleThreadedBufferQueueHingeAngle);
    DMF_ThreadedBufferQueue_Stop(moduleContext->DmfModuleThreadedBufferQueueHingeAngle);

    // Unregister hinge angle sensor update event handlers.
    //
    if (hingeAngleSensor != nullptr)
    {
        hingeAngleSensor.ReadingChanged(tokenReadingChanged);
    }

    FuncExitVoid(DMF_TRACE);
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
HingeAngleDevice::Start()
/*++

Routine Description:

    Start the hinge angle monitor and events, make it start to operate.

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
HingeAngleDevice::Stop()
/*++

Routine Description:

    Stop the hinge angle monitor and events.

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
HingeAngle_Initialize(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type HingeAngle.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    FuncEntry(DMF_TRACE);

    DMF_CONTEXT_HingeAngle* moduleContext;
    DMF_CONFIG_HingeAngle* moduleConfig;
    NTSTATUS ntStatus;

    PAGED_CODE();

    ntStatus = STATUS_UNSUCCESSFUL;
    moduleContext = DMF_CONTEXT_GET(DmfModule);
    moduleConfig = DMF_CONFIG_GET(DmfModule);

    // Necessary call for using C++/WinRT environment.
    //
    init_apartment();

    moduleContext->hingeAngleDevice = new HingeAngleDevice();
    if (moduleContext->hingeAngleDevice == nullptr)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Insufficient resource for hingeAngleDevice instance, ntStatus=%!STATUS!", ntStatus);
        goto Exit;
    }

    moduleContext->hingeAngleDevice->thisModuleHandle = DmfModule;
    moduleContext->hingeAngleDevice->DeviceIdToFind = moduleConfig->DeviceId;
    moduleContext->hingeAngleDevice->EvtHingeAngleReadingChangeCallback = moduleConfig->EvtHingeAngleReadingChangeCallback;
    ntStatus = moduleContext->hingeAngleDevice->Initialize();

    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "hingeAngleDevice Initialize fails: ntStatus=%!STATUS!", ntStatus);
        delete moduleContext->hingeAngleDevice;
        moduleContext->hingeAngleDevice = nullptr;
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
DMF_HingeAngle_NotificationRegister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Initialize an instance of a DMF Module of type HingeAngle.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = HingeAngle_Initialize(DmfModule);

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#pragma code_seg("PAGE")
_Function_class_(DMF_NotificationUnregister)
_IRQL_requires_max_(PASSIVE_LEVEL)
static
VOID
DMF_HingeAngle_NotificationUnregister(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Deinitialize an instance of a DMF Module of type HingeAngle.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    None

--*/
{
    DMF_CONTEXT_HingeAngle* moduleContext;

    FuncEntry(DMF_TRACE);

    PAGED_CODE();

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    if (moduleContext->hingeAngleDevice != nullptr)
    {
        if (moduleContext->hingeAngleDevice->hingeAngleSensor != nullptr)
        {
            DMF_ModuleClose(DmfModule);
        }

        moduleContext->hingeAngleDevice->Deinitialize();

        delete moduleContext->hingeAngleDevice;
        moduleContext->hingeAngleDevice = nullptr;
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
DMF_HingeAngle_ChildModulesAdd(
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
    DMF_CONTEXT_HingeAngle* moduleContext;
    DMF_CONFIG_ThreadedBufferQueue moduleConfigThreadedBufferQueueForDeviceWatcher;
    DMF_CONFIG_ThreadedBufferQueue moduleConfigThreadedBufferQueueForHingeAngle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    UNREFERENCED_PARAMETER(DmfParentModuleAttributes);

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // ThreadedBufferQueue for device watcher.
    // ---------------------------------------
    //
    DMF_CONFIG_ThreadedBufferQueue_AND_ATTRIBUTES_INIT(&moduleConfigThreadedBufferQueueForDeviceWatcher,
                                                       &moduleAttributes);
    moduleConfigThreadedBufferQueueForDeviceWatcher.EvtThreadedBufferQueueWork = HingeAngle_ThreadedBufferQueueDeviceWatcherWork;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.BufferCount = 32;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.BufferContextSize = 0;
    moduleConfigThreadedBufferQueueForDeviceWatcher.BufferQueueConfig.SourceSettings.BufferSize = sizeof(DeviceInformationAndUpdateContainer);

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThreadedBufferQueueDeviceWatcher);

    // ThreadedBufferQueue for hinge angle sensor.
    // --------------------------------------------------
    //
    DMF_CONFIG_ThreadedBufferQueue_AND_ATTRIBUTES_INIT(&moduleConfigThreadedBufferQueueForHingeAngle,
                                                       &moduleAttributes);
    moduleConfigThreadedBufferQueueForHingeAngle.EvtThreadedBufferQueueWork = HingeAngle_ThreadedBufferQueueHingeAngleWork;
    moduleConfigThreadedBufferQueueForHingeAngle.BufferQueueConfig.SourceSettings.EnableLookAside = TRUE;
    moduleConfigThreadedBufferQueueForHingeAngle.BufferQueueConfig.SourceSettings.BufferCount = 5;
    moduleConfigThreadedBufferQueueForHingeAngle.BufferQueueConfig.SourceSettings.PoolType = NonPagedPoolNx;
    moduleConfigThreadedBufferQueueForHingeAngle.BufferQueueConfig.SourceSettings.BufferContextSize = 0;
    moduleConfigThreadedBufferQueueForHingeAngle.BufferQueueConfig.SourceSettings.BufferSize = sizeof(HingeAngleSensorReadingDataContainer);

    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     &moduleContext->DmfModuleThreadedBufferQueueHingeAngle);

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
DMF_HingeAngle_Create(
    _In_ WDFDEVICE Device,
    _In_ DMF_MODULE_ATTRIBUTES* DmfModuleAttributes,
    _In_ WDF_OBJECT_ATTRIBUTES* ObjectAttributes,
    _Out_ DMFMODULE* DmfModule
    )
/*++

Routine Description:

    Create an instance of a DMF Module of type HingeAngle.

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
    DMF_MODULE_DESCRIPTOR dmfModuleDescriptor_HingeAngle;
    DMF_CALLBACKS_DMF dmfCallbacksDmf_HingeAngle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    DMF_CALLBACKS_DMF_INIT(&dmfCallbacksDmf_HingeAngle);
    dmfCallbacksDmf_HingeAngle.ChildModulesAdd = DMF_HingeAngle_ChildModulesAdd;
    dmfCallbacksDmf_HingeAngle.DeviceNotificationRegister = DMF_HingeAngle_NotificationRegister;
    dmfCallbacksDmf_HingeAngle.DeviceNotificationUnregister = DMF_HingeAngle_NotificationUnregister;

    DMF_MODULE_DESCRIPTOR_INIT_CONTEXT_TYPE(dmfModuleDescriptor_HingeAngle,
                                            HingeAngle,
                                            DMF_CONTEXT_HingeAngle,
                                            DMF_MODULE_OPTIONS_DISPATCH,
                                            DMF_MODULE_OPEN_OPTION_NOTIFY_PrepareHardware);

    dmfModuleDescriptor_HingeAngle.CallbacksDmf = &dmfCallbacksDmf_HingeAngle;

    ntStatus = DMF_ModuleCreate(Device,
                                DmfModuleAttributes,
                                ObjectAttributes,
                                &dmfModuleDescriptor_HingeAngle,
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
DMF_HingeAngle_CurrentStateGet(
    _In_ DMFMODULE DmfModule,
    _Out_ HINGE_ANGLE_SENSOR_STATE* CurrentState
    )
/*++

Routine Description:

    Get the current hinge angle state from sensor.

Arguments:

    DmfModule - This Module's handle.
    CurrentState - Current hinge angle state that this module hold.
                   Caller should only use this returned value when NTSTATUS is STATUS_SUCCESS.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HingeAngle* moduleContext;
    double currentHingeAngle;

    PAGED_CODE();

    FuncEntry(DMF_TRACE);

    ntStatus = DMF_ModuleReference(DmfModule);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Hinge angle sensor is not found yet.");
        goto ExitNoRelease;
    }

    moduleContext = DMF_CONTEXT_GET(DmfModule);

    // Query sensor for current reading.
    //
    try
    {
        currentHingeAngle = moduleContext->hingeAngleDevice->hingeAngleSensor.GetCurrentReadingAsync().get().AngleInDegrees();
    }
    catch (hresult_error const& ex)
    {
        ex.code();
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "Query from GetCurrentHingeAngle fails");
        goto Exit;
    }

    moduleContext->hingeAngleDevice->hingeAngleState.AngleInDegrees = currentHingeAngle;
    *CurrentState = moduleContext->hingeAngleDevice->hingeAngleState;

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
DMF_HingeAngle_Start(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Start the hinge angle monitor and events, it puts hinge angle sensor to work.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HingeAngle* moduleContext;

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
    moduleContext->hingeAngleDevice->Start();
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
DMF_HingeAngle_Stop(
    _In_ DMFMODULE DmfModule
    )
/*++

Routine Description:

    Stop the hinge angle monitor and events.

Arguments:

    DmfModule - This Module's handle.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_CONTEXT_HingeAngle* moduleContext;

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
    moduleContext->hingeAngleDevice->Stop();
    ntStatus = STATUS_SUCCESS;

    DMF_ModuleDereference(DmfModule);

ExitNoRelease:

    FuncExit(DMF_TRACE, "ntStatus=%!STATUS!", ntStatus);

    return ntStatus;
}
#pragma code_seg()

#endif // defined(DMF_USER_MODE) && defined(IS_WIN10_RS5_OR_LATER) && defined(__cplusplus)

// eof: Dmf_HingeAngle.cpp
//
