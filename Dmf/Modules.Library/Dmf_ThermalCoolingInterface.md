## DMF_ThermalCoolingInterface

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

Implements the Thermal Cooling Interface and provides Passive and Active cooling callbacks for clients to act upon.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_ThermalCoolingInterface
````
typedef struct
{
  // Reference string for the device interface
  //
  UNICODE_STRING ReferenceString;
  // ActiveCooling callback routine.
  //
  EVT_DMF_ThermalCoolingInterface_ActiveCooling* CallbackActiveCooling;
  // PassiveCooling callback routine.
  //
  EVT_DMF_ThermalCoolingInterface_PassiveCooling* CallbackPassiveCooling;
} DMF_CONFIG_ThermalCoolingInterface;
````
Member | Description
----|----
ReferenceString | ReferenceString to differentiate between multiple instances of device interface.
CallbackActiveCooling | The Client's callback that will be executed when ActiveCooling interface is invoked.
CallbackPassiveCooling | The Client's callback that will be executed when PassiveCooling interface is invoked.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_ThermalCoolingInterface_ActiveCooling
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ThermalCoolingInterface_ActiveCooling(
    _In_ DMFMODULE DmfModule,
    _In_ BOOLEAN Engaged
    );
````

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_ThermalCoolingInterface Module handle.
Engaged | If TRUE, the driver must engage active cooling (for example, by turning the fan on). If FALSE, the driver must disengage active cooling (for example, by turning the fan off).

##### Remarks

----------------------------------------------------------------------------------------------------------------------------------
##### EVT_DMF_ThermalCoolingInterface_PassiveCooling
````
_IRQL_requires_max_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_ThermalCoolingInterface_PassiveCooling(
    _In_ DMFMODULE DmfModule,
    _In_ ULONG Percentage
    );
````

##### Parameters
Parameter | Description
----|----
DmfModule | DMF_ThermalCoolingInterface Module handle.
Percentage | The percentage of full performance at which the device is permitted to operate.

##### Remarks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

* None

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

Hardware

-----------------------------------------------------------------------------------------------------------------------------------

