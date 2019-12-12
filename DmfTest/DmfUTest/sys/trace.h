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
//
// DmfUTest Tracing GUID - {EAF8F31E-30E1-499B-ABEB-8E6B051E1A75}
// DMF Tracing GUID - {E028F9D9-8BD5-494F-B288-CF1C049F4BD1}
//

#define WPP_CONTROL_GUIDS                                                               \
   WPP_DEFINE_CONTROL_GUID(                                                             \
        DmfTraceGuid, (E028F9D9,8BD5,494F,B288,CF1C049F4BD1),                           \
        WPP_DEFINE_BIT(DMF_TRACE)                                                       \
        )                                                                               \
    WPP_DEFINE_CONTROL_GUID(                                                            \
        DmfUTestDriverTraceGuid, (EAF8F31E,30E1,499B,ABEB,8E6B051E1A75),                \
        WPP_DEFINE_BIT(MYDRIVER_ALL_INFO)                                               \
        WPP_DEFINE_BIT(TRACE_DEVICE)                                                    \
        WPP_DEFINE_BIT(TRACE_CALLBACK)                                                  \
        )                                                                               \

//
// This comment block is scanned by the trace preprocessor to define our
// Trace function.
//
// USEPREFIX and USESUFFIX strip all trailing whitespace, so we need to surround
// FuncExit messages with brackets 
//
// begin_wpp config
// FUNC Trace{FLAG=MYDRIVER_ALL_INFO}(LEVEL, MSG, ...);
// FUNC TraceEvents(LEVEL, FLAGS, MSG, ...);
// FUNC FuncEntry{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// FUNC FuncEntryArguments{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS, MSG, ...);
// FUNC FuncExit{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS, MSG, ...);
// FUNC FuncExitVoid{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// FUNC TraceError{LEVEL=TRACE_LEVEL_ERROR}(FLAGS, MSG, ...);
// FUNC TraceInformation{LEVEL=TRACE_LEVEL_INFORMATION}(FLAGS, MSG, ...);
// FUNC TraceVerbose{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS, MSG, ...);
// FUNC FuncExitNoReturn{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// USEPREFIX(FuncEntry, "%!STDPREFIX! [%!FUNC!] --> Entry");
// USEPREFIX(FuncEntryArguments, "%!STDPREFIX! [%!FUNC!] --> Entry");
// USEPREFIX(FuncExit, "%!STDPREFIX! [%!FUNC!] <-- Exit <");
// USESUFFIX(FuncExit, ">");
// USEPREFIX(FuncExitVoid, "%!STDPREFIX! [%!FUNC!] <-- Exit");
// USEPREFIX(TraceError, "%!STDPREFIX! [%!FUNC!] ERROR:");
// USEPREFIX(TraceEvents, "%!STDPREFIX! [%!FUNC!]");
// USEPREFIX(TraceInformation, "%!STDPREFIX! [%!FUNC!]");
// USEPREFIX(TraceVerbose, "%!STDPREFIX! [%!FUNC!]");
// USEPREFIX(FuncExitNoReturn, "%!STDPREFIX! [%!FUNC!] <--");
// end_wpp

// eof: Trace.h
//