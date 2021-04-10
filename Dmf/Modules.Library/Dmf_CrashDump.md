## DMF_CrashDump

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Allows a Client to write data to the MEMORY.dmp (Crash Dump) file in case of a system crash. Also allows User-mode Callers to
write data that is stored in the Crash Dump file. Also allows User-mode Callers to read from the buffers that store data
that will be written to the Crash Dump file.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_CrashDump
````
typedef struct
{
  // The identifier of this component. It will be in the Bug Check data.
  // This is a required parameter.
  //
  UCHAR* ComponentName;
  // GUID for this driver's Ring Buffer data.
  //
  GUID RingBufferDataGuid;
  // GUID for this driver's additional data.
  //
  GUID AdditionalDataGuid;
  // Buffer Size for the RINGBUFFER_INDEX_SELF Ring Buffer. (This driver.)
  // NOTE: Use the absolute minimum necessary. Compress data if necessary.
  //
  ULONG BufferSize;
  // Number of buffers for RINGBUFFER_INDEX_SELF Ring Buffer. (This driver.)
  // NOTE: Use the absolute minimum necessary. Compress data if necessary.
  //
  ULONG BufferCount;
  // Maximum size of ring buffer to allow.
  //
  ULONG RingBufferMaximumSize;
  // Callbacks for the RINGBUFFER_INDEX_SELF Ring Buffer. (This driver.)
  //
  EVT_DMF_CrashDump_Query* EvtCrashDumpQuery;
  EVT_DMF_CrashDump_Write* EvtCrashDumpWrite;
  // Number of Data Sources for other clients.
  //
  ULONG DataSourceCount;

  // Number of triage dump data entries to allocate. This must be
  // set before using DMF_CrashDumpDataAdd.
  ULONG TriageDumpDataArraySize;
  // Callback for adding triage dump ranges during BugCheck processing.
  // This is optional, even if passing a TriageDumpDataArraySize since
  // buffers can be added prior to a BugCheck occurring.
  //
  EVT_DMF_CrashDump_StoreTriageDumpData* EvtCrashDumpStoreTriageDumpData;
} DMF_CONFIG_CrashDump;
````
Member | Description
----|----
ComponentName | A string that identifies the Client driver that is writing data to the crash dump file.
RingBufferDataGuid | GUID for DMF_RingBuffer data that is written to the crash dump file.
AdditionalDataGuid | GUID for the additional data that is written to the crash dump file.
BufferSize | Size in bytes of each item in the ring buffer that is written to the crash dump file.
BufferCount | Number of items in the ring buffer that is written to the crash dump file.
RingBufferMaximumSize | The maximum size the ring buffer allowed.
EvtCrashDumpQuery | Function that allows the crash dump writer to query the driver to determine how much data is needed.
EvtCrashDumpWrite | Function that the crash dump writer calls to allow this Module (and its Client) to write data to the crash dump file.
DataSourceCount | The maximum number of Data Sources (ring buffers) the instance of this Module allows (for other drivers and User-mode applications).
TriageDumpDataArraySize | Number of triage dump data entries to allocate. Allows marking data buffers for inclusion in kernel minidumps.
EvtCrashDumpStoreTriageDumpData | Function that the crash dump writer calls to allow the Client to mark buffers for inclusion in kernel minidumps.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_CrashDump_Query

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_CrashDump_Query(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** OutputBuffer,
    _Out_ ULONG* SizeNeededBytes
    );
````

Callback function for Client to inform OS how much space Client needs to write its data.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CrashDump Module handle.
Buffer | The address to where the address of the buffer that will be written to the crash dump file is set.
BufferLength | The address to where the length of the buffer that will be written to the crash dump file is set.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_CrashDump_Write
````
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_CrashDump_Write(
    _In_ DMFMODULE DmfModule,
    _Out_ VOID** OutputBuffer,
    _In_ ULONG* OutputBufferLength
    );
````

Callback function for Client to write its own data after the system is crashed. Note that this callback is only applicable
to the RINGBUFFER_INDEX_SELF instance. Other instances are used by User-mode and cannot use this callback.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CrashDump Module handle.
Buffer | The address to where the address of the buffer that will be written to the crash dump file is set.
BufferLength | The address to where the length of the buffer that will be written to the crash dump file is set.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_CrashDump_StoreTriageDumpData
````
_IRQL_requires_same_
VOID
EVT_DMF_CrashDump_StoreTriageDumpData(_In_ DMFMODULE DmfModule,
    _In_ ULONG BugCheckCode,
    _In_ ULONG_PTR BugCheckParameter1,
    _In_ ULONG_PTR BugCheckParameter2,
    _In_ ULONG_PTR BugCheckParameter3,
    _In_ ULONG_PTR BugCheckParameter4
    );
````

Callback for marking memory regions which should be included in the kernel minidump. This is called during BugCheck
at IRQL = HIGH_LEVEL so must be nonpaged and has restrictions on what it may do.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CrashDump Module handle.
BugCheckCode | Supplies the Bug Check code of the Bug Check in progress.
BugCheckParameter1 | Supplies Parameter 1 code of the Bug Check in progress.
BugCheckParameter2 | Supplies Parameter 2 code of the Bug Check in progress.
BugCheckParameter3 | Supplies Parameter 3 code of the Bug Check in progress.
BugCheckParameter4 | Supplies Parameter 4 code of the Bug Check in progress.

##### Remarks
The Bug Check code and parameters are provided so the callback may choose to only add data when certain Bug Checks occur.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------
##### CrashDump_DataSourceWriteSelf
````
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
CrashDump_DataSourceWriteSelf(
  _Inout_ DMFMODULE DmfModule,
  _In_ UCHAR* Buffer,
  _In_ ULONG BufferLength
  );
````

This Method writes a given buffer to its own ring buffer. That ring buffer is written to the crash dump file in the event
of a system crash.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CrashDump Module handle.
Buffer | The given buffer to write to the ring buffer.
BufferLength | The size of the given buffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_CrashDump_TriageDumpDataAdd
````
_IRQL_requires_same_
NTSTATUS
DMF_CrashDump_TriageDumpDataAdd(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR* Data,
    _In_ ULONG DataLength
    );
````

Add a Client Driver buffer to the Triage Dump Buffer list. This does not copy
the memory but adds the address of the buffer and length to the triage data array
so it will be marked for inclusion in a kernel minidump.  This could be called at
any IRQL, depending on if it was called during the BugCheck callback or earlier.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_CrashDump Module handle.
Data | The data to mark for inclusion in triage dump.
DataLength | The size of the data.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

##### IOCTL_DATA_SOURCE_CREATE
Create a Data Source.

Minimum Input Buffer Size: sizeof(DATA_SOURCE_CREATE)

Minimum Output Buffer Size: 0

-----------------------------------------------------------------------------------------------------------------------------------

##### IOCTL_DATA_SOURCE_DESTROY
Destroys a Data Source.

Minimum Input Buffer Size: 0

Minimum Output Buffer Size: 0

-----------------------------------------------------------------------------------------------------------------------------------

##### IOCTL_DATA_SOURCE_WRITE
Writes a buffer of data to the Data Source.

Minimum Input Buffer Size: Size indicated when Data Source is created.

Minimum Output Buffer Size: 0

-----------------------------------------------------------------------------------------------------------------------------------

##### IOCTL_DATA_SOURCE_READ
Reads a buffer of data from the Data Source.

Minimum Input Buffer Size: 0

Minimum Output Buffer Size: Size indicated when Data Source is created.

-----------------------------------------------------------------------------------------------------------------------------------

##### IOCTL_DATA_SOURCE_OPEN
Opens an existing Data Source so it can be read from or written to.

Minimum Input Buffer Size: sizeof(DATA_SOURCE_CREATE)

Minimum Output Buffer Size: 0

-----------------------------------------------------------------------------------------------------------------------------------

##### IOCTL_DATA_SOURCE_CAPTURE
Reads the entire Data Source data.

Minimum Input Buffer Size: Number of items * size of each item indicated in DATA_SOURCE during create.

Minimum Output Buffer Size: 0

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_RingBuffer

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* DMF_LiveKernelDump

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

