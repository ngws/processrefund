# Process Refund

An attempt to implement Process Doppelgänging
## Getting Started

Just clone the repo and open the .sln with Visual Studio 2015.

### Prerequisites

Works on 32-bit and 64-bit.
To use you need a dummy exe like svchost.exe and your malicous exe.
read below - you need to be able to write over the file.

WARNING DONT USE ON WIN10 YOU WILL GET A BSOD.

exmaple:

	processrefund.exe svchost.exe MalExe.exe
 ![alt text](https://raw.githubusercontent.com/spajed/processrefund/master/example.png)
 ![alt text](https://raw.githubusercontent.com/spajed/processrefund/master/modules.png)
 ![alt text](https://raw.githubusercontent.com/spajed/processrefund/master/memory.png)

### Problems with Process Doppelgänging
* You can not replace any file. If you try to replace  C:\windows\system32\svchost.exe you will get "Access Denied".
  Yet in the black hat slide show they show replacing "svchost.exe", but after viewing the DEMO image in the slide show
  it is clear they didn't replace svchost.exe in the demo but Vmmap.exe from sysinternals.
 ![alt text](https://raw.githubusercontent.com/spajed/processrefund/master/cheating.png) ![alt text](https://raw.githubusercontent.com/spajed/processrefund/master/cheating2.png)
* This techinque will not bypass all AntiViruses because of the use of NtCreateThreadEx, which is equal to CreateRemoteThread.
   An AntiVirus may monitor the creation of remote thread (via PsSetCreateThreadNotifyRoutine) thus detecting our Doppelgänging. Also an AntiVirus may compare the memory with the image of the created process and will be aware of our malicous process. This techinque may be good to avoid file signatures and loading executables without wiritng them to disk("filesless") but it will not avoid everything.
* Seems to be somewhat unreliable, sometimes it works and sometimes it doesn't. I will look into it.
## Acknowledgments
* https://www.blackhat.com/docs/eu-17/materials/eu-17-Liberman-Lost-In-Transaction-Process-Doppelganging.pdf
* Spajed
