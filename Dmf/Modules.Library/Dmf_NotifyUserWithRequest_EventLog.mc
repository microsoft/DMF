;/*++
;
;    Copyright (C) Microsoft. All rights reserved.
;
;Module Name:
;
;    Dmf_NotifyUserWithRequest_EventLog.h generated from Dmf_NotifyUserWithRequest_EventLog.mc using Visual Studio Message Compiler.
;
;Abstract:
;
;    Event log resources.
;
;Environment:
;
;    User/Kernel-mode drivers.
;
;--*/
;

;#pragma once
;

;//
;//  Status values are 32 bit values laid out as follows:
;//
;//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
;//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
;//  +---+-+-------------------------+-------------------------------+
;//  |Sev|C|       Facility          |               Code            |
;//  +---+-+-------------------------+-------------------------------+
;//
;//  where
;//
;//      Sev - is the severity code
;//
;//          00 - Success
;//          01 - Informational
;//          10 - Warning
;//          11 - Error
;//
;//      C - is the Customer code flag
;//
;//      Facility - is the facility code
;//
;//      Code - is the facility's status code
;//
;

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0:FACILITY_SYSTEM
               Runtime=0x2:FACILITY_RUNTIME
               Stubs=0x3:FACILITY_STUBS
               Io=0x4:FACILITY_IO_ERROR_CODE
              )

LanguageNames=(English=0x409:MSG00409)


; // The following are the message definitions.

MessageIdTypedef=DWORD

MessageId=0x300
Severity=Informational
Facility=Runtime
SymbolicName=MSG_USERMODE_EVENT_REQUEST_NOT_FOUND
Language=English
User-mode event request not found.
.

;// TODO: Add codes as needed. (Be sure to set the proper Severity for new codes.)
;//

;// eof: Dmf_NotifyUserWithRequest_EventLog.mc
;//
