## DMF_File

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Support for working with files in drivers.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

* None

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_File_DriverFileRead

````
_Must_inspect_result_
NTSTATUS
DMF_File_DriverFileRead(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* FileName, 
    _Out_ WDFMEMORY* FileContentMemory,
    _Out_opt_ UCHAR** Buffer,
    _Out_opt_ size_t* BufferLength
    );
````

Reads the contents of a file, located in the same location where the driver's file is installed, into
a buffer that is allocated for the Client. Client is responsible for freeing the allocated memory.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_File Module handle.
FileName  | Name of the file.
FileContentMemory | Buffer handle where read file contents are copied. The caller owns this memory.
Buffer | Optional pointer to the buffer that contains the read data.
BufferLength | Optional pointer to the length of the read data.

##### Remarks

* This Method is driver specific, not device specific. A new Method, `DMF_File_DeviceFileRead()`, will be added
in an upcoming update.

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DMF_File_Read

````
_Must_inspect_result_
NTSTATUS
DMF_File_Read(
    _In_ DMFMODULE DmfModule,
    _In_ WDFSTRING FileName, 
    _Out_ WDFMEMORY* FileContentMemory
    );
````

Reads the contents of a file into a buffer that is allocated for the Client. Client is responsible
for freeing the allocated memory.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_File Module handle.
FileName  | Name of the file.
FileContentMemory | Buffer handle where read file contents are copied. The caller owns this memory.

##### Remarks

*

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DMF_File_ReadEx

````
_Must_inspect_result_
NTSTATUS
DMF_File_ReadEx(
    _In_ DMFMODULE DmfModule,
    _In_ WCHAR* FileName, 
    _Out_ WDFMEMORY* FileContentMemory
    _Out_opt_ UCHAR** Buffer,
    _Out_opt_ size_t* BufferLength
    );
````

Reads the contents of a file into a buffer that is allocated for the Client. Client is responsible
for freeing the allocated memory.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_File Module handle.
FileName  | Name of the file.
FileContentMemory | Buffer handle where read file contents are copied. The caller owns this memory.
Buffer | Optional pointer to the buffer that contains the read data.
BufferLength | Optional pointer to the length of the read data.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

##### DMF_DMF_File_Write

````
_Must_inspect_result_
NTSTATUS
DMF_File_Write(
    _In_ DMFMODULE DmfModule,
    _In_ WDFSTRING FileName,
    _In_ WDFMEMORY FileContentMemory
    );
````

    Writes the contents of a wdf memory to a file.
    This function will try to create the file if it doesn't exists and overwrite current file.

##### Returns

NTSTATUS

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_File Module handle.
FileName  | Name of the file.
FileContentMemory | Buffer handle of buffer holding data to write.

##### Remarks

*

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* 

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* Add support for Kernel-mode.

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Driver Patterns

-----------------------------------------------------------------------------------------------------------------------------------

