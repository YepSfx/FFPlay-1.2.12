## FFPlay-1.2.12 ##


This repository provides a wrapper library (FFPlayLib.dll) to use FFMPEG-1.2.12
and exmaple projects to use the wrapper library. 

The FFPlayLib.dll is a dynamic library converted from ffplay.c in ffplay-1.2.12.
It helps to use functions in ffplay.c so that it can play any files that 
ffplay.c plays.

Before use or build this library, the ffmpeg-1.2.12 library must be built and 
the outputs must be shown within your path.

The FFPlayLib is supporting Win32, Linux and Raspbian (Raspberry Pi).

This repository contains following two folders.

* FFPlayLib: A folder to contain ffplay.c wrapper library
* Applications: A folder to contain example applications to use the FFPlayLib
               [C/C++, Delphi, Lazarus(Codetyphon) and so on]
               
               
