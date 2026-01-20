## FFPlay‑1.2.12 ##
This repository provides a wrapper library (FFPlayLib.dll) for FFmpeg‑1.2.12, along with example projects demonstrating how to use the wrapper.
FFPlayLib.dll is a dynamic library generated from ffplay.c in the FFmpeg‑1.2.12 source tree. It exposes the functionality of ffplay.c, allowing applications to play any media that the original ffplay implementation supports.


## Prerequisites: ##
Before building or using this library, you must first compile FFmpeg‑1.2.12. Make sure the compiled FFmpeg binaries and libraries are available in your system path.
Platform Support


## FFPlayLib currently supports: ##
Windows (Win32)

Linux (Ubuntu)

Raspbian (Raspberry Pi)


## Repository Structure ##
The repository contains the following directories:

FFPlayLib — Contains the wrapper library derived from ffplay.c.

Applications — Contains example applications demonstrating how to use FFPlayLib (C/C++, Delphi, Lazarus/Codetyphon, C#, and more).



## Additional Information ##
For detailed explanations of audio/video synchronization, refer to:

http://dranger.com/ffmpeg/tutorial01.html


## A similar project based on FFmpeg 3.4.x is available here: ##

https://github.com/YepSfx/FFPlay-3.4.8

---
If you have any questions, please e-mail me @ cj.github.proj@gmail.com
