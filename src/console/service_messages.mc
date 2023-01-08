MessageIdTypedef=DWORD

SeverityNames=(
    Success=0x0:STATUS_SEVERITY_SUCCESS
    Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
    Warning=0x2:STATUS_SEVERITY_WARNING
    Error=0x3:STATUS_SEVERITY_ERROR
)

FacilityNames=(
    System=0x0:FACILITY_SYSTEM
    Runtime=0x2:FACILITY_RUNTIME
    Stubs=0x3:FACILITY_STUBS
    Io=0x4:FACILITY_IO_ERROR_CODE
)

LanguageNames=(English=0x409:MSG00409)

;// Message definitions

MessageId=0x1
Severity=Informational
Facility=Runtime
SymbolicName=WINPX_STARTED
Language=English
WinPX proxy server (%1) has started.
.

MessageId=0x2
Severity=Informational
Facility=Runtime
SymbolicName=WINPX_STOPPED
Language=English
WinPX proxy server (%1) has stopped.
.

MessageId=0x3
Severity=Informational
Facility=Runtime
SymbolicName=WINPX_PROXY_INFO
Language=English
WinPX proxy server (%1) configuration:%n%2
.

MessageId=0x4
Severity=Error
Facility=Runtime
SymbolicName=WINPX_ERROR_START_FAILURE
Language=English
Failed to start WinPX proxy server (%1): %2
.
