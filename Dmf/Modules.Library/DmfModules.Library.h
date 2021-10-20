/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfModules.Library.h

Abstract:

    Definitions specific for the "Library" DMF Library. This Library contains the Modules
    that are released as part of DMF.

    NOTE: All other Libraries must include this file to access these Modules and, importantly,
          include DMF itself. Every Library has a corresponding DmfModules.[LibraryName].h file
          which should include this file.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

// NOTE: The definitions in this file must be surrounded by this annotation to ensure
//       that both C and C++ Clients can easily compile and link with Modules in this Library.
//
#if defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus)

// Include DMF. This is the API that is accessible to Modules (but not Client Drivers).
// It also includes all the Modules that are part of the Framework itself.
//
#include "..\Framework\Modules.Core\DmfModules.Core.h"
#include "DmfModules.Library.Public.h"

// Interfaces in this Library.
//
#include "Dmf_Interface_ComponentFirmwareUpdate.h"
#include "Dmf_Interface_BusTarget.h"
#include "Dmf_Interface_SystemManagementFramework.h"

// All the Modules in this Library.
//

// Protocols
//
#include "Dmf_ComponentFirmwareUpdate.h"

// Task Execution
//
#include "Dmf_Doorbell.h"
#include "Dmf_ScheduledTask.h"
#include "Dmf_QueuedWorkItem.h"
#include "Dmf_Thread.h"

// Driver Patterns
//
#include "Dmf_AcpiNotification.h"
#include "Dmf_AcpiPepDevice.h"
#include "Dmf_AcpiPepDeviceFan.h"
#include "Dmf_AlertableSleep.h"
#include "Dmf_CrashDump.h"
#include "Dmf_CmApi.h"
#include "Dmf_File.h"
#include "Dmf_InterruptResource.h"
#include "Dmf_IoctlHandler.h"
#include "Dmf_NotifyUserWithEvent.h"
#include "Dmf_NotifyUserWithRequest.h"
#include "Dmf_NotifyUserWithRequestMultiple.h"
#include "Dmf_Pdo.h"
#include "Dmf_Registry.h"
#include "Dmf_Rundown.h"
#include "Dmf_String.h"
#include "Dmf_ThermalCoolingInterface.h"
#include "Dmf_UdeClient.h"
#include "Dmf_UefiOperation.h"

// Buffers
//
#include "Dmf_BufferPool.h"
#include "Dmf_BufferQueue.h"
#include "Dmf_PingPongBuffer.h"
#include "Dmf_RingBuffer.h"
#include "Dmf_ThreadedBufferQueue.h"

// Data Structures
//
#include "Dmf_HashTable.h"
#include "Dmf_Stack.h"

// Targets
//
#include "Dmf_GpioTarget.h"
#include "Dmf_I2cTarget.h"
#include "Dmf_ContinuousRequestTarget.h"
#include "Dmf_DefaultTarget.h"
#include "Dmf_RequestTarget.h"
#include "Dmf_DeviceInterfaceTarget.h"
#include "Dmf_DeviceInterfaceMultipleTarget.h"
#include "Dmf_AcpiTarget.h"
#include "Dmf_SelfTarget.h"
#include "Dmf_SmbiosWmi.h"
#include "Dmf_SpbTarget.h"
#include "Dmf_SpiTarget.h"
#include "Dmf_SymbolicLinkTarget.h"

// Hid
//
#include "Dmf_EyeGazeIoctl.h"
#include "Dmf_HidPortableDeviceButtons.h"
#include "Dmf_VirtualEyeGaze.h"
#include "Dmf_VirtualHidAmbientColorSensor.h"
#include "Dmf_VirtualHidAmbientLightSensor.h"
#include "Dmf_VirtualHidDeviceVhf.h"
#include "Dmf_VirtualHidKeyboard.h"

// Sensors
//
#include "Dmf_ActivitySensor.h"
#include "Dmf_HingeAngle.h"
#include "Dmf_SimpleOrientation.h"

// Network
//
#include "Dmf_MobileBroadband.h"

#if defined(DMF_WDF_DRIVER)
#include "Dmf_Wmi.h"
#endif

#if defined(DMF_KERNEL_MODE) || defined(DMF_USER_MODE)
#include "Dmf_HidTarget.h"
#include "Dmf_ResourceHub.h"
#include "Dmf_SerialTarget.h"
#include "Dmf_VirtualHidMini.h"
#include "Dmf_ComponentFirmwareUpdateHidTransport.h"
#endif

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfModules.Library.h
//
