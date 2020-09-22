EyeGaze IOCTL Sample
========================================================================
This sample shows how to create a driver that exposes an device interface that allows applications to
send GAZE_DATA to a virtual Eye Gaze device.

It is a basic Kernel-mode VHF function driver that creates a Virtual HID Eye Gaze device.

Kernel-mode VHF is selected because VHF drivers are not filter drivers and can expose device interfaces
and receive IOCTLs.

This sample is a good example of the layered approach in which DMF drivers are written with each layer
performing a specific task. Then, these layers can be used interchangably and changed as needed. For example,
the EyeGazeIoctl Module could be replaced by a Module that has a thread that reads data from a non-HID
Eye Gaze device and then sends it to the Virtual Eye Gaze HID device.

For more information about Eye Gaze device support in Windows see:

[MSR Gaze HID](https://github.com/MSREnable/GazeHid/)

Testing the driver
==================

1. Run the command "devcon install EyeGazeIoctl.inf root\EyeGazeIoctl"".
2. Install the "Windows Community Toolkit Sample Application" from the Windows Store.
3. Open the Gaze tab.
4. Write a program to send GAZE_DATA to the driver (not included in this sample).
5. You should see the point data appear in the application. 
