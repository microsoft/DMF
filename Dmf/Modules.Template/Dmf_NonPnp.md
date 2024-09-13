## DMF_NonPnp

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This Module supports the OSR FX2 board. The code is based on the original OSR sample driver. Most, but not all, of the
functionality exposed by the original driver is supported in this Module. This Module, along with the Client driver samples that
use this Module, show how to create a Module based on an existing driver. This Module demonstrates many important DMF concepts
  1. The difference between callbacks called by WDF and DMF.
  2. How to store a DMFMODULE handle in a WDFOBJECT and retrieve it.
  3. How to support IOCTLs inside a Module, how to call back into a Client.
  4. How to retrieve data from the Module Config.
  5. How to expose a Module Method.
  6. Several other important concepts.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

##### DMF_CONFIG_NonPnp
````
typedef struct
{
  // When interrupt pipe returns data, this callback is called.
  //
  EVT_DMF_NonPnp_InterruptPipeCallback* InterruptPipeCallback;
} DMF_CONFIG_NonPnp;
````
Member | Description
----|----
InterruptPipeCallback | Called by the Module when OSR FX2 board's switches are changed. This callback is also called when the device goes in and out of D0.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

##### NonPnp_Settings
Bit-mask that allows Client to determine how the device operates.

````
typedef enum
{
  NonPnp_Settings_NoDeviceInterface = 0x01,
  NonPnp_Settings_NoEnterIdle = 0x02,
  NonPnp_Settings_IdleIndication = 0x04,
} NonPnp_Settings;
````
Member | Description
----|----
NonPnp_Settings_NoDeviceInterface | Prevents applications or drivers from sending IOCTLs.
NonPnp_Settings_NoEnterIdle | Prevents the device from entering idle state after 10 seconds of idle time.
NonPnp_Settings_IdleIndication | Does turn on/off light bar when device is not idle/idle.

##### NonPnp_EventWriteMessage
These messages allow the Client to perform logging when specific events happen inside the Module. The logging mechanism is Client specific. It may just be tracing or it may write to event log.

````
typedef enum
{
  NonPnp_EventWriteMessage_Invalid,
  NonPnp_EventWriteMessage_ReadStart,
  NonPnp_EventWriteMessage_ReadFail,
  NonPnp_EventWriteMessage_ReadStop,
  NonPnp_EventWriteMessage_WriteStart,
  NonPnp_EventWriteMessage_WriteFail,
  NonPnp_EventWriteMessage_WriteStop,
  NonPnp_EventWriteMessage_SelectConfigFailure,
  NonPnp_EventWriteMessage_DeviceReenumerated,
} NonPnp_EventWriteMessage;
````
Member | Description
----|----
NonPnp_EventWriteMessage_ReadStart | Read from OSR FX2 board starts.
NonPnp_EventWriteMessage_ReadFail | Read from OSR FX2 board fails.
NonPnp_EventWriteMessage_ReadStop | Read from OSR FX2 board stops.
NonPnp_EventWriteMessage_WriteStart | Write from OSR FX2 board starts.
NonPnp_EventWriteMessage_WriteFail | Write from OSR FX2 board fails.
NonPnp_EventWriteMessage_WriteStop | Write from OSR FX2 board stops.
NonPnp_EventWriteMessage_SelectConfigFailure | USB Select configuration fails.
NonPnp_EventWriteMessage_DeviceReenumerated | The device is re-enumerated per User.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Structures

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

##### EVT_DMF_NonPnp_InterruptPipeCallback
````
_Function_class_(EVT_DMF_NonPnp_InterruptPipeCallback)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_NonPnp_InterruptPipeCallback(
    _In_ DMFMODULE DmfModule,
    _In_ UCHAR SwitchState,
    _In_ NTSTATUS NtStatus
    );
````

This callback is called when data is available from the OSR FX2 Interrupt Pipe.

##### Returns

NonPnp_EnumerationDispositionType

##### Parameters
Parameter | Description
----|----
DmfModule | The Dmf_NonPnp Module handle.
SwitchState | Current state of the switches (the new state after the last switch change).
NtStatus | Indicates if the Module detected an error on the interrupt pipe.

##### Remarks

* This callback is also called when the device goes in and out of D0.

##### EVT_DMF_NonPnp_EventWriteCallback
````
_Function_class_(EVT_DMF_NonPnp_EventWriteCallback)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
VOID
EVT_DMF_NonPnp_EventWriteCallback(
    _In_ DMFMODULE DmfModule,
    _In_ NonPnp_EventWriteMessage EventWriteMessage,
    _In_ ULONG_PTR Parameter1,
    _In_ ULONG_PTR Parameter2,
    _In_ ULONG_PTR Parameter3,
    _In_ ULONG_PTR Parameter4,
    _In_ ULONG_PTR Parameter5
    );
````

This callback is called when the Module determines a code path has occurrec that the Client may want to write to a logging output
device.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | The Dmf_NonPnp Module handle.
EventWriteMessage | Indicates the code path where logging occurs.
Parameter1 | Code path specific logging parameter.
Parameter2 | Code path specific logging parameter.
Parameter3 | Code path specific logging parameter.
Parameter4 | Code path specific logging parameter.
Parameter5 | Code path specific logging parameter.

##### Remarks

* See the Module for meanings of the code paths specific parameters.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

##### DMF_NonPnp_SwitchStateGet

````
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DMF_NonPnp_SwitchStateGet(
  _In_ DMFMODULE DmfModule,
  _Out_ UCHAR* SwitchState
  )
````

Allows the Client to retrieve the current state of the switches.

##### Returns

None

##### Parameters
Parameter | Description
----|----
DmfModule | An open DMF_NonPnp Module handle.
SwitchState | The current state of the switches is written to this buffer.

##### Remarks

* The primary reason for this Method is to demonstrate how to write a DMF Module Method and how a Client uses it.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

##### IOCTL_OSRUSBFX2_GET_CONFIG_DESCRIPTOR

Retrieve the OSR FX2 USB configuration descriptor information.

Minimum Input Buffer Size: 0

Minimum Output Buffer Size: Variable.

Output Data Buffer: USB_CONFIGURATION_DESCRIPTOR

##### Remarks

* See MSDN for information about the data returned.

##### IOCTL_OSRUSBFX2_RESET_DEVICE

Reset the OSR FX2 device.

Minimum Input Buffer Size: 0

Minimum Output Buffer Size: 0

##### Remarks

* Simply causes the OSR FX2 device to be reset.

##### IOCTL_OSRUSBFX2_REENUMERATE_DEVICE

Causes the OSR FX2 device to be re-enumerated.

Minimum Input Buffer Size: 0

Minimum Output Buffer Size: 0

##### Remarks

* The driver will unload and reload.

##### IOCTL_OSRUSBFX2_GET_BAR_GRAPH_DISPLAY

Returns the current state of the light bar.

Minimum Input Buffer Size: 0

Minimum Output Buffer Size: sizeof(BAR_GRAPH_STATE)

Output Data Buffer: BAR_GRAPH_STATE

##### Remarks

* The bits are not in an intuitive order. See OSR documentation.

##### IOCTL_OSRUSBFX2_SET_BAR_GRAPH_DISPLAY

Sets the current state of the light bar.

Minimum Input Buffer Size: sizeof(BAR_GRAPH_STATE)

Minimum Output Buffer Size: 0

Input Data Buffer: BAR_GRAPH_STATE

##### Remarks

* The bits are not in an intuitive order. See OSR documentation.

##### IOCTL_OSRUSBFX2_GET_7_SEGMENT_DISPLAY

Returns the current state of the 7-segment display.

Minimum Input Buffer Size: 0

Minimum Output Buffer Size: sizeof(UCHAR)

Output Data Buffer: UCHAR

##### Remarks

* It returns a bit mask corresponding to the LEDs that are lit, not the number displayed.

##### IOCTL_OSRUSBFX2_SET_7_SEGMENT_DISPLAY

Sets the current state of the 7-segment display.

Minimum Input Buffer Size: sizeof(UCHAR)

Minimum Output Buffer Size: 0

Input Data Buffer: UCHAR

##### Remarks

* The bits correspond to LED segments on the display, not the number displayed.

##### IOCTL_OSRUSBFX2_READ_SWITCHES

Reads the state of the 8-switches.

Minimum Input Buffer Size: 0

Minimum Output Buffer Size: sizeof(SWITCH_STATE)

Input Data Buffer: 0

Output Data Buffer: SWITCH_STATE

##### Remarks

* The bits are not in an intuitive order. See OSR documentation.

##### IOCTL_OSRUSBFX2_GET_INTERRUPT_MESSAGE

Allow the Client to receive asynchronous notification when switches change state.

Minimum Input Buffer Size: 0

Minimum Output Buffer Size: sizeof(UCHAR)

Input Data Buffer: 0

Output Data Buffer: UCHAR

##### Remarks

* The bits are not in an intuitive order. See OSR documentation.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* This code is based on the original OSR FX2 sample.
* The purpose of this Module is to show how DMF can be used in a driver.
* This sample also shows how to convert an existing driver into a Module.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

* This Module creates PASSIVE_LEVEL locks. This may need to be changed.

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

* Add a Module Config option that allows the Module to filter out interrupts when power transitions occur. (Good for demonstration purposes.)

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

Sample

-----------------------------------------------------------------------------------------------------------------------------------

