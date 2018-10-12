/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    Trace.h

Abstract:

    Header file for the debug tracing related function definitions and macros.

Environment:

    Kernel mode

--*/

#pragma once

// Define the tracing flags.
// Tracing GUIDs:
//
// Client Driver - {296D475F-89B6-4470-B943-023A0BEAFD04}
//
// NOTE: Every driver must define a unique GUID otherwise tracing from multiple drivers
//       that use the same GUID will appear.
//
// DMF           - {83029B7A-5AE6-4672-A171-32C6695B8735}
//

#define WPP_CONTROL_GUIDS                                                               \
   WPP_DEFINE_CONTROL_GUID(                                                             \
        DmfTraceGuid, (83029B7A,5AE6,4672,A171,32C6695B8735),                           \
        WPP_DEFINE_BIT(DMF_TRACE)                                                       \
        )                                                                               \
    WPP_DEFINE_CONTROL_GUID(                                                            \
        SwitchBarTraceGuid, (296D475F,89B6,4470,B943,023A0BEAFD04),                     \
        WPP_DEFINE_BIT(MYDRIVER_ALL_INFO)                                               \
        WPP_DEFINE_BIT(TRACE_DEVICE)                                                    \
        WPP_DEFINE_BIT(TRACE_CALLBACK)                                                  \
        )                                                                               \

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level  >= lvl)

// eof: Trace.h
//