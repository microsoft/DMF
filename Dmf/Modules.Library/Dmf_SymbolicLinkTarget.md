## DMF_SymbolicLinkTarget

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements a driver pattern that streams IOCTL requests to a WDFIOTARGET that dynamically appears/disappears. This Module
automatically creates buffers and WDFREQUESTS for both input and output data performs all the necessary operations to attach
those buffers to WDFREQUESTS.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_SymbolicLinkTarget
````
typedef struct
{
  // Symbolic link to open.
  //
  PWCHAR SymbolicLinkName;
  // Open in Read or Write mode.
  //
  ULONG OpenMode;
  // Share Access.
  //
  ULONG ShareAccess;
} DMF_CONFIG_SymbolicLinkTarget;
````
Member | Description
----|----
SymbolicLinkName | Name of symbolic link. Generally, this is known to the Client.
OpenMode | Indicates the Read/Write mode of the target. See MSDN.
ShareAccess | Indicates the Share Access of the target. See MSDN.

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

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This Module is provided to give access Symbolic Links in a similar manner as other targets using DMF. Generally speaking, Symbolic Links should not be used as it is not possible to receive arrival/removal notifications as well as the fact that names are hard coded. Use Device Interfaces and DMF_DeviceInterfaceTarget instead when possible.
* In some cases, such as to access a Filter Control Device, Symbolic Links are necessary.

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

Targets

-----------------------------------------------------------------------------------------------------------------------------------

