## DMF_AcpiTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This Module gives the Client access to Acpi DSM (Device Specific Methods) functions.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_AcpiTarget

````
typedef struct
{
  // The DSM revision required by the Client.
  //
  ULONG DsmRevision;
  // The GUID that identifies the DSMs the Client will invoke.
  //
  GUID Guid;
} DMF_CONFIG_AcpiTarget;
````
Member | Description
----|----
DsmRevision | The DSM revision required by the Client.
Guid | The GUID that identifies the DSMs the Client will invoke.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_AcpiTarget_EvaluateMethod

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_AcpiTarget_EvaluateMethod(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG MethodName,
    _In_opt_ VOID* InputBuffer,
    _Out_opt_ WDFMEMORY* ReturnBufferMemory,
    __deref_opt_out_bcount_opt(*ReturnBufferSize) VOID** ReturnBuffer,
    _Out_opt_ ULONG* ReturnBufferSize,
    _In_ ULONG Tag
    );
````

Allows the Client to send and/or receive data to/from ACPI Control Method.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiTarget Module handle.
MethodName | Identifies the ACPI Control Method the Client wants to "evaluate".
InputBuffer | Data supplied by the Client to the ACPI Control Method.
ReturnBufferMemory | WDFMEMORY associated with allocated ReturnBuffer. The Client must call `WdfObjectDelete()` passing this object to free allocated memory.
ReturnBuffer | Address of a buffer allocated by this Module which contains data supplied by the ACPI Control Method. The Client must **not** free this buffer directly. Delete `ReturnBufferMemory` instead.
ReturnBufferSize | The size of the data returned by the ACPI Control Method.
Tag | Memory tag used by this Module when allocating memory on behalf of the Client.

##### Remarks

- This Method may allocate memory. If so, Client must delete it after the memory is no longer needed. See `ReturnBufferMemory` above.

##### DMF_AcpiTarget_InvokeDsm

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AcpiTarget_InvokeDsm(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG FunctionIndex,
  _In_ ULONG FunctionCustomArgument,
  _Out_opt_ VOID* ReturnBuffer,
  _Inout_opt_ ULONG* ReturnBufferSize
  );
````

Allows the Client to invoke a DSM. Data returned is validated prior to return to the Client.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiTarget Module handle.
FunctionIndex | Identifies the DSM the Client wants to "invoke".
FunctionCustomArgument | Data supplied by the Client to the DSM.
ReturnBuffer | Address of a buffer allocated by this Module which contains data supplied by the ACPI Control Method. The Client must free this buffer.
ReturnBufferSize | The size of the data returned by the ACPI Control Method.

##### Remarks

##### DMF_AcpiTarget_InvokeDsmRaw

````
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS
DMF_AcpiTarget_InvokeDsmRaw(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG FunctionIndex,
    _In_ ULONG FunctionCustomArgument,
    _Out_opt_ WDFMEMORY* ReturnBufferMemory,
    _Out_writes_opt_(*ReturnBufferSize) VOID** ReturnBuffer,
    _Out_opt_ ULONG* ReturnBufferSize,
    _In_ ULONG Tag
    );
````

Allows the Client to invoke a DSM and receive raw data in return.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiTarget Module handle.
FunctionIndex | Identifies the DSM the Client wants to "invoke".
FunctionCustomArgument | Data supplied by the Client to the DSM.
ReturnBufferMemory | WDFMEMORY associated with allocated ReturnBuffer. The Client must call `WdfObjectDelete()` passing this object to free allocated memory.
ReturnBuffer | Address of a buffer allocated by this Module which contains data supplied by the ACPI Control Method. The Client must **not** free this buffer directly. Delete `ReturnBufferMemory` instead.
ReturnBufferSize | The size of the data returned by the ACPI Control Method.
Tag | Memory tag used by this Module when allocating memory on behalf of the Client.

##### Remarks

- This Method may allocate memory. If so, Client must delete it after the memory is no longer needed. See `ReturnBufferMemory` above.

##### DMF_AcpiTarget_InvokeDsmWithCustomBuffer

````
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DMF_AcpiTarget_InvokeDsmWithCustomBuffer(
  _In_ DMFMODULE DmfModule,
  _In_ ULONG FunctionIndex,
  _In_reads_opt_(FunctionCustomArgumentsBufferSize) VOID* FunctionCustomArgumentsBuffer,
  _In_ ULONG FunctionCustomArgumentsBufferSize
  );
````

Allows the Client to invoke a DSM passing a buffer of arbitrary size (instead of ULONG).

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_AcpiTarget Module handle.
FunctionIndex | Identifies the DSM the Client wants to "invoke".
FunctionCustomArgumentsBuffer | Data supplied by the Client to the DSM.
FunctionCustomArgumentsBufferSize | The size in bytes of FunctionCustomArgumentsBuffer.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Targets

-----------------------------------------------------------------------------------------------------------------------------------

