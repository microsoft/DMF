/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    DmfModule.Library.Trace.h

Abstract:

    Trace GUID definitions specific to the "Library" DMF Library.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#pragma once

#define WPP_CONTROL_GUIDS                                                                              \
    WPP_DEFINE_CONTROL_GUID(                                                                           \
        DMF_TraceGuid_Library, (0,0,0,0,0),                                                            \
        WPP_DEFINE_BIT(DMF_TRACE_PingPongBuffer)                                                       \
        WPP_DEFINE_BIT(DMF_TRACE_AlertableSleep)                                                       \
        WPP_DEFINE_BIT(DMF_TRACE_ContinuousRequestTarget)                                              \
        WPP_DEFINE_BIT(DMF_TRACE_CrashDump)                                                            \
        WPP_DEFINE_BIT(DMF_TRACE_CDevice)                                                              \
        WPP_DEFINE_BIT(DMF_TRACE_CDeviceInterface)                                                     \
        WPP_DEFINE_BIT(DMF_TRACE_CDevicePresentNotifier)                                               \
        WPP_DEFINE_BIT(DMF_TRACE_CSystemTelemetryDevice)                                               \
        WPP_DEFINE_BIT(DMF_TRACE_DeviceInterfaceTarget)                                                \
        WPP_DEFINE_BIT(DMF_TRACE_NotifyUserWithEvent)                                                  \
        WPP_DEFINE_BIT(DMF_TRACE_NotifyUserWithRequest)                                                \
        WPP_DEFINE_BIT(DMF_TRACE_Pdo)                                                                  \
        WPP_DEFINE_BIT(DMF_TRACE_QueuedWorkItem)                                                       \
        WPP_DEFINE_BIT(DMF_TRACE_Registry)                                                             \
        WPP_DEFINE_BIT(DMF_TRACE_RequestTarget)                                                        \
        WPP_DEFINE_BIT(DMF_TRACE_ResourceHub)                                                          \
        WPP_DEFINE_BIT(DMF_TRACE_ScheduledTask)                                                        \
        WPP_DEFINE_BIT(DMF_TRACE_SelfTarget)                                                           \
        WPP_DEFINE_BIT(DMF_TRACE_Thread)                                                               \
        WPP_DEFINE_BIT(DMF_TRACE_ThreadedBufferQueue)                                                  \
        WPP_DEFINE_BIT(DMF_TRACE_Wmi)                                                                  \
        WPP_DEFINE_BIT(DMF_TRACE_AcpiNotification)                                                     \
        WPP_DEFINE_BIT(DMF_TRACE_AcpiTarget)                                                           \
        WPP_DEFINE_BIT(DMF_TRACE_VirtualHidDeviceVhf)                                                  \
        WPP_DEFINE_BIT(DMF_TRACE_GpioTarget)                                                           \
        WPP_DEFINE_BIT(DMF_TRACE_HidPortableDeviceButtons)                                             \
        WPP_DEFINE_BIT(DMF_TRACE_HidTarget)                                                            \
        WPP_DEFINE_BIT(DMF_TRACE_I2cTarget)                                                            \
        WPP_DEFINE_BIT(DMF_TRACE_SerialTarget)                                                         \
        WPP_DEFINE_BIT(DMF_TRACE_SmbiosWmi)                                                            \
        WPP_DEFINE_BIT(DMF_TRACE_SpiTarget)                                                            \
        WPP_DEFINE_BIT(DMF_TRACE_ThermalCoolingInterface)                                              \
        WPP_DEFINE_BIT(DMF_TRACE_CmApi)                                                                \
        WPP_DEFINE_BIT(DMF_TRACE_SymbolicLinkTarget)                                                   \
        )

// eof: DmfModule.Library.Trace.h
//
