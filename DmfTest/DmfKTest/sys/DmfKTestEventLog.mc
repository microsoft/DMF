;/*++
;
;    Copyright (C) Microsoft. All rights reserved.
;
;Module Name:
;
;    EventLog.h generated from EventLog.mc using Visual Studio Message Compiler.
;
;Abstract:
;
;    Event log resources.
;
;Environment:
;
;    User/Kernel mode drivers.
;
;--*/
;

;#pragma once
;

;//
;//  Status values are 32 bit values layed out as follows:
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

MessageIdTypedef=NTSTATUS

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0
               Driver=0x1:FACILITY_DRIVER_ERROR_CODE
              )

;// Copy of Dmf_DataTransferViaAcpi_EventLog.mc messages
;//
MessageId=0x4400
Facility=Driver
Severity=Error
SymbolicName=EVENTLOG_MESSAGE_ACPI_MESSAGE_ERROR
Language=English
%2
.

MessageId=0x4401
Facility=Driver
Severity=Warning
SymbolicName=EVENTLOG_MESSAGE_ACPI_MESSAGE_WARNING
Language=English
%2
.

MessageId=0x4402
Facility=Driver
Severity=Informational
SymbolicName=EVENTLOG_MESSAGE_ACPI_MESSAGE_INFORMATIONAL
Language=English
%2
.

;// Copy of Dmf_SamNotificationViaSsh_EventLog.mc messages
;//
MessageId=0x4500
Facility=Driver
Severity=Informational
SymbolicName=EVENTLOG_MESSAGE_EVENT_RECEIVED
Language=English
SamNotificationViaSsh received event. Notified OS with status: %2. %3, %4, %5, %6.
.

;// Copy of Dmf_AcpiNotificationViaSsh_EventLog.mc messages
;//
MessageId=0x4600
Facility=Driver
Severity=Informational
SymbolicName=EVENTLOG_MESSAGE_THERMAL_CMDID_SET_DPTF_TRIP_POINT
Language=English
AcpiNotificationViaSsh received thermal set trip point event. %2, %3, %4.
.

;// TODO: Add codes as needed. (Be sure to set the proper Severity for new codes.)
;//

;// eof: EventLog.mc
;//
