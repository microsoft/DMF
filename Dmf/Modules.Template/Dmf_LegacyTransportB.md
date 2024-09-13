## DMF_LegacyTransportB

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

This is a sample Transport Module which uses the Legacy Protocol-Transport Module feature in DMF.
This Module simply displays a string to the Traceview log to indicate which Transport Module is running.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

#### Remarks

* This Module has a single Method called a Transport Method which is called only by its corresponding Protocol Module.
* Although, it is possible that Transport Modules have other Methods, it is generally not the case.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* Although this Module has no Config and very few callbacks, that is only to make this sample simple. Transport Modules support all features of DMF.
* If you plan to use the Legacy Protocol-Transport DMF feature, use this Module as a template for a Transport Module.-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* LegacyProtocolTransport Sample Driver

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

* An updated Protocol Transport Feature that is more robust is planned for the near future.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Category

* Template/Samples

-----------------------------------------------------------------------------------------------------------------------------------

