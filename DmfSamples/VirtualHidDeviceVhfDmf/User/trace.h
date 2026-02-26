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
// Client Driver - {D569092B-5916-4088-80ED-ABAAC262BC9E}
//
// NOTE: Every driver must define a unique GUID otherwise tracing from multiple drivers
//       that use the same GUID will appear.
//
// DMF           - {5A999273-09EF-4904-B1BB-E45E5A465288}
//

#define WPP_CONTROL_GUIDS                                                               \
   WPP_DEFINE_CONTROL_GUID(                                                             \
        DmfTraceGuid, (5A999273,09EF,4904,B1BB,E45E5A465288),                           \
        WPP_DEFINE_BIT(DMF_TRACE)                                                       \
        )                                                                               \
    WPP_DEFINE_CONTROL_GUID(                                                            \
        VHidMini2DmfK, (D569092B,5916,4088,80ED,ABAAC262BC9E),                          \
        WPP_DEFINE_BIT(MYDRIVER_ALL_INFO)                                               \
        WPP_DEFINE_BIT(TRACE_DEVICE)                                                    \
        WPP_DEFINE_BIT(TRACE_CALLBACK)                                                  \
        )                                                                               \

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level  >= lvl)

// eof: Trace.h
//