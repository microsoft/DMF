## DMF_ToasterBus

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

DMF equivalent of Toaster sample in WDK.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_ToasterBus
````
typedef struct
{
  // Number of Toasters.
  //
  ULONG DefaultNumberOfToasters;
  // Bus Device Class Guid.
  //
  GUID ToasterBusDevClassGuid;
  // Bus Device Interface Guid.
  //
  GUID ToasterBusDevInterfaceGuid;
  // Hardware Id.
  //
  PWCHAR ToasterBusHardwareId;
  // Hardware Id Length.
  //
  ULONG ToasterBusHardwareIdLength;
  // Hardware Compatible Id.
  //
  PWCHAR ToasterBusHardwareCompatibleId;
  // Hardware compatible Id length.
  //
  ULONG ToasterBusHardwareCompatibleIdLength;
  // Description format for Bus Device.
  //
  PWCHAR ToasterBusDeviceDescriptionFormat;
  // Toaster Bus Number.
  //
  ULONG ToasterBusNumber;
  // Configuration for Wmi.
  //
  DMF_CONFIG_Wmi WmiConfig;
} DMF_CONFIG_ToasterBus;
````

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementations Details

* Detailed explanation about how this Module is implemented internally.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Template

-----------------------------------------------------------------------------------------------------------------------------------
