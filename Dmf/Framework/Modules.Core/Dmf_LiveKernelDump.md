## DMF_LiveKernelDump

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Allows DMF / DMF Modules / Client Driver to initiate a Live Kernel Memory Dump containing the complete DMF state of all
driver components captured when the live dump is created.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_LiveKernelDump
````
typedef struct
{
  // Callback function to initialize Live Kernel Dump feature.
  //
  EVT_DMF_LiveKernelDump_Initialize* LiveKernelDumpFeatureInitialize;
  // Guid used by applications to find this Module and talk to it.
  //
  GUID GuidDeviceInterface;
  // Null terminated wide character string used to identify the set of minidumps generated from this driver.
  //
  WCHAR ReportType[16];
  // Guid used to locate the secondary data associated with the minidumps generated from this driver.
  //
  GUID GuidSecondaryData;
} DMF_CONFIG_LiveKernelDump;
````

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Macros (Used by Client)

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_LIVEKERNELDUMP_POINTER_STORE
````
DMF_LIVEKERNELDUMP_POINTER_STORE(
    DmfModule,
    Buffer,
    BufferLength)
````

This macro can be used by the Client Driver to store the address of a Buffer that it wants to include when a Live Dump is
generated.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
Buffer | Address of the buffer being added.
BufferLength | Length of the buffer being added.

##### Remarks

* This API can be called at IRQL <= DISPATCH_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_LIVEKERNELDUMP_POINTER_REMOVE
````
DMF_LIVEKERNELDUMP_POINTER_REMOVE(DmfModule,
                 Buffer,
                 BufferLength)
````
This macro can be used by the Client Driver to remove a Buffer from the list of Buffers that will be included when a Live Dump
is generated.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
Buffer | Address of the buffer being added.
BufferLength | Length of the buffer being added.

##### Remarks

* This API can be called at IRQL <= DISPATCH_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_LIVEKERNELDUMP_CREATE
````
DMF_LIVEKERNELDUMP_CREATE(DmfModule,
             BugCheckCode,
             BugCheckParameter,
             ExcludeDmfData,
             NumberOfClientStructures,
             ArrayOfClientStructures,
             SizeOfSecondaryData,
             SecondaryDataBuffer)
````
This macro can be used by the Client Driver to generate a Live Kernel Dump.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
BugCheckCode | This value is the bugcheck code.
BugCheckParameter | This parameter defines the bugcheck parameter value and is defined per component.
ExcludeDmfData | When set to true, the mini dump generated does not contain DMF structures.
NumberOfClientStructures | Indicates the number of client data structures that the client wants to include to the mini dump.
ArrayOfClientStructures | An array containing the client data structures that will be included in the mini dump.
SizeOfSecondaryData | Size of secondary data that needs to be part of the mini dump.
SecondaryDataBuffer | Pointer to a buffer containing the secondary data.

##### Remarks

* This API should be called only at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Macros (Used by Client)

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_MODULE_LIVEKERNELDUMP_POINTER_STORE
````
DMF_MODULE_LIVEKERNELDUMP_POINTER_STORE(DmfModule,
                    Buffer,
                    BufferLength)
````
This macro can be used by the DMF Module or DMF Framework to store the address of a Buffer that it wants to include when a Live Dump is generated.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
Buffer | Address of the buffer being added.
BufferLength | Length of the buffer being added.

##### Remarks

* This API can be called at IRQL <= DISPATCH_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_MODULE_LIVEKERNELDUMP_POINTER_REMOVE
````
DMF_MODULE_LIVEKERNELDUMP_POINTER_REMOVE(DmfModule,
                     Buffer,
                     BufferLength)
````
This macro can be used by the DMF Module or DMF Framework to remove a Buffer from the list of Buffers that will be included when a Live Dump is generated.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
Buffer | Address of the buffer being added.
BufferLength | Length of the buffer being added.

##### Remarks

* This API can be called at IRQL <= DISPATCH_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_MODULE_LIVEKERNELDUMP_CREATE
````
DMF_MODULE_LIVEKERNELDUMP_CREATE(DmfModule,
                 BugCheckCode,
                 BugCheckParameter,
                 NumberOfModuleStructures,
                 ArrayOfModuleStructures,
                 SizeOfSecondaryData,
                 SecondaryDataBuffer)
````
This macro can be used by the DMF Module or DMF Framework to generate a Live Kernel Dump.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
BugCheckCode | This value is the bugcheck code.
BugCheckParameter | This parameter defines the bugcheck parameter value and is defined per component.
ExcludeDmfData | When set to true, the mini dump generated does not contain DMF structures.
NumberOfModuleStructures | Indicates the number of Module data structures that the Module wants to include to the mini dump.
ArrayOfModuleStructures | An array containing the Module data structures that will be included in the mini dump.
SizeOfSecondaryData | Size of secondary data that needs to be part of the mini dump.
SecondaryDataBuffer | Pointer to a buffer containing the secondary data.

##### Remarks

* This API should be called only at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_MODULE_LIVEKERNELDUMP_DMFCOLLECTION_AS_BUGCHECK_PARAMETER_STORE
````
DMF_MODULE_LIVEKERNELDUMP_DMFCOLLECTION_AS_BUGCHECK_PARAMETER_STORE(DmfModule,
                                  DmfCollection)
````
This macro can be used by DMF to store the DmfCollection as a Bugcheck Parameter.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
DmfCollection | This DmfCollection handle.

##### Remarks

* This API should be called only at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_LiveKernelDump_Initialize
````
VOID
EVT_DMF_LiveKernelDump_Initialize(
    _In_ DMFMODULE DmfModule
    );
````

This callback allows the Client to store the Feature Module.
It is defined if the driver is using this Module as a DMF Feature.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.

##### Remarks

* This API should be called only at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_LiveKernelDump_DataBufferSourceAdd

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_LiveKernelDump_DataBufferSourceAdd(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* Buffer,
  _In_ ULONG BufferLength
  );
````

This method adds a data buffer to the Live Kernel Dump Module.

##### Returns

NTSTATUS. Fails if the data buffer could not be added to the Live Kernel Dump Module.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
Buffer | Address of the buffer being added.
BufferLength | Length of the buffer being added.

##### Remarks

* This API can be called at IRQL <= DISPATCH_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_LiveKernelDump_DataBufferSourceRemove

````
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
VOID
DMF_LiveKernelDump_DataBufferSourceRemove(
  _In_ DMFMODULE DmfModule,
  _In_ VOID* Buffer,
  _In_ ULONG BufferLength
  );
````

This method removes a data buffer from the Live Kernel Dump Module.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
Buffer | Address of the buffer being removed.
BufferLength | Length of the buffer being removed.

##### Remarks

* This API should be used to remove buffers from the Live Kernel Dump Module before they are destroyed.
* This API can be called at IRQL <= DISPATCH_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_LiveKernelDump_LiveKernelMemoryDumpCreate

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_LiveKernelDump_LiveKernelMemoryDumpCreate(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG BugCheckCode,
  _In_ ULONG_PTR BugCheckParameter,
  _In_ BOOLEAN ExcludeDmfData,
  _In_ ULONG NumberOfClientStructures,
  _In_opt_ PLIVEKERNELDUMP_CLIENT_STRUCTURE ArrayOfClientStructures,
  _In_ ULONG SizeOfSecondaryData,
  _In_opt_ VOID* SecondaryDataBuffer
  );
````

This Method creates a Live Kernel Memory Dump. It includes all the data buffers stored by the
Client Driver and DMF Modules in the live kernel memory dump.

##### Returns

NTSTATUS. Fails if the Live Kernel Memory Dump could not be created.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
BugCheckCode | This value is the bugcheck code.
BugCheckParameter | This parameter defines the bugcheck parameter value and is defined per component. This shows up as the second parameter in the Live Kernel Dump.
ExcludeDmfData | When set to true, the mini dump generated does not contain DMF structures.
NumberOfClientStructures | Indicates the number of client data structures that the client wants to include to the mini dump.
ArrayOfClientStructures | An array containing the client data structures that will be included in the mini dump.
SizeOfSecondaryData | Size of secondary data that needs to be part of the mini dump.
SecondaryDataBuffer | Pointer to a buffer containing the secondary data.

##### Remarks

* This API should be called only at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_LiveKernelDump_StoreDmfCollectionAsBugcheckParameter

````
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DMF_LiveKernelDump_StoreDmfCollectionAsBugcheckParameter(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG_PTR DmfCollection
  );
````

This method is used by DMF to store the DmfCollection as Bugcheck Parameter.

##### Returns

None.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_LiveKernelDump Module handle.
DmfCollection | This DmfCollection handle.

##### Remarks

* This API should be called only at PASSIVE_LEVEL.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

##### IOCTL_LIVEKERNELDUMP_CREATE

This IOCTL is used to generate a Live Kernel Dump from an application.
````
Input Buffer:

typedef struct {
  // A bugcheck code used to identify this Live Dump.
  //
  ULONG BugCheckCode;
  // Bugcheck parameter.
  //
  ULONG_PTR BugCheckParameter;
  // Flag to indicate if DMF data should be included in the mini dump.
  //
  BOOLEAN ExcludeDmfData;
  // Number of client data structures.
  //
  ULONG NumberOfClientStructures;
  // Array of client data structures.
  //
  PLIVEKERNELDUMP_CLIENT_STRUCTURE ArrayOfClientStructures;
  // Size of Secondary data.
  //
  ULONG SizeOfSecondaryData;
  // Pointer to Secondary data.
  //
  VOID* SecondaryDataBuffer;
} LIVEKERNELDUMP_INPUT_BUFFER;
````

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* DMF_RingBuffer

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Features

-----------------------------------------------------------------------------------------------------------------------------------

