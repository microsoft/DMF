## DMF_SmbiosWmi

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This Module allows a Client access to the SMBIOS table that resides on the Host.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_SmbiosWmi_Table01Get

````
_Must_inspect_result_
NTSTATUS
DMF_SmbiosWmi_Table01Get(
    _In_ DMFMODULE DmfModule,
    _Out_ SmbiosWmi_Table01* SmbiosTable01Buffer,
    _Inout_ size_t* SmbiosTable01BufferSize
    );
````
Copies SMBIOS Table 01 data into the caller's buffer.

##### Returns

    STATUS_SUCCESS - SMBIOS Table 01 has been copied to target buffer.
    STATUS_UNSUCCESSFUL - SMBIOS Table 01 is not present.
    STATUS_BUFFER_TOO_SMALL - Target buffer is not large enough.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SmbiosWmi Module handle.
SmbiosTable01Buffer | The given Client buffer where the table is copied.
SmbiosTable01BufferSize | The size of TargetBuffer passed by Client. If the buffer is too small, the needed buffer size is written to this parameter.

##### Remarks

* Pointers in SmbiosTable01Buffer point to data in the Module Context. Only read from those pointers.

##### DMF_SmbiosWmi_TableCopy

````
_Check_return_
NTSTATUS
DMF_SmbiosWmi_TableCopy(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(TargetBufferSize) UCHAR* TargetBuffer,
    _In_ ULONG TargetBufferSize
  );
````

Copies the SMBIOS table that the Module read during initialization and has stored in its local context to a given Client
buffer. **This Method is included for legacy applications only. Use `DMF_SmbiosWmi_TableCopyEx()` instead.**

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SmbiosWmi Module handle.
TargetBuffer | The given Client buffer where the table is copied.
TargetBufferSize | The size of TargetBuffer.

##### Remarks

* The buffer returned by this Method has a WMI header.
* **This Method is included for legacy applications only. Use `DMF_SmbiosWmi_TableCopyEx()` instead.**

##### DMF_SmbiosWmi_TableCopyEx

````
_Check_return_
NTSTATUS
DMF_SmbiosWmi_TableCopyEx(
    _In_ DMFMODULE DmfModule,
    _Out_writes_(*TargetBufferSize) UCHAR* TargetBuffer,
    _Inout_ size_t* TargetBufferSize
  );
````

Copies the SMBIOS table that the Module read during initialization and has stored in its local context to a given Client
buffer.

##### Returns

    STATUS_SUCCESS - All SMBIOS data has been copied to target buffer.
    STATUS_BUFFER_TOO_SMALL - Target buffer is not large enough.

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SmbiosWmi Module handle.
TargetBuffer | The given Client buffer where the table is copied.
TargetBufferSize | The size of TargetBuffer passed by Client. If the buffer is too small, the needed buffer size is written to this parameter.

##### Remarks

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
**This Method is included for legacy applications only. Use `DMF_SmbiosWmi_TableInformationGetEx()` instead.**

##### Returns

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_SmbiosWmi Module handle.
TargetBuffer | The given Client buffer where the address of the SMBIOS table is written.
TargetBufferSize | The size of the SMBIOS table stored by this Module.

##### Remarks

* Use this Method with caution as it returns the address where the SMBIOS is stored by the Module.
* The buffer returned by this Method has a WMI header.
* **This Method is included for legacy applications only. Use `DMF_SmbiosWmi_TableInformationGetEx()` instead.**
##### DMF_SmbiosWmi_TableInformationGetEx

````
VOID
DMF_SmbiosWmi_TableInformationGetEx(
  _In_ DMFMODULE DmfModule,
  _Out_ UCHAR** TargetBuffer,
  _Out_ ULONG* TargetBufferSize
  );
````

Gives the Client the address and size of the SMBIOS table stored by this Module.

##### Returns

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

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* Remove legacy support.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

