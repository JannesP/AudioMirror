This driver isn't finished and has a few quirks. If you want a build and ship ready solution this is sadly not what you were looking for.
### You've been warned.

An audio driver for Windows 10 (only tested on x64) that works as a virtual audio cable. Adds a virtual speaker that routes all played audio into a virtual microphone device.

As for documentation ... there's not much out there, at least I couldn't find it. 
1. it's derived from the [sysvad](https://github.com/microsoft/Windows-driver-samples/tree/master/audio/sysvad) sample drivers with quite a bit of refactoring
2. as for actual documentation that's not examples - the only good source seemed to be the [Microsoft Docs](https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/) which are actually really good with explaining the most important concepts

For development I mostly used a virtual machine to see if the driver worked or crashed and to debug the code.
Here's the basic installation process I used to install the driver during development (basically just devcon):
![alt text](https://user-images.githubusercontent.com/5788115/85946963-47b43e00-b948-11ea-9266-4466db063168.png "basic installation process")
