## DMF_SmbiosWmi

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

This Module allows a Client access to the SMBIOS table that resides on the Host.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_SmbiosWmi_TableCopy

````
_Check_return_
NTSTATUS
DMF_SmbiosWmi_TableCopy(
  _In_ DMFMODULE DmfModule,
  _In_ UCHAR* TargetBuffer,
  _In_ ULONG TargetBufferSize
  );
````

Copies the SMBIOS table that the Module read during initialization and has stored in its local context to a given Client
buffer.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SmbiosWmi Module handle.
TargetBuffer | The given Client buffer where the table is copied.
TargetBufferSize | The size of TargetBuffer.

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_SmbiosWmi_TableInformationGet

````
VOID
DMF_SmbiosWmi_TableInformationGet(
  _In_ DMFMODULE DmfModule,
  _Out_ UCHAR** TargetBuffer,
  _Out_ ULONG* TargetBufferSize
  );
````

Gives the Client the address and size of the SMBIOS table stored by this Module.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SmbiosWmi Module handle.
TargetBuffer | The given Client buffer where the address of the SMBIOS table is written.
TargetBufferSize | The size of the SMBIOS table stored by this Module.

##### Remarks

* Use this Method with caution as it returns the address where the SMBIOS is stored by the Module.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

