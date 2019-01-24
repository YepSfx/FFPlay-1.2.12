@echo off
SET THEFILE=stimultimedia
echo Assembling %THEFILE%
c:\codetyphon\fpc\fpc32\bin\i386-win32\arm-linux-as.exe -mfloat-abi=hard -meabi=5 -march=armv7-a -mfpu=vfpv3-d16 -o D:\Data\lazPlayer\lib\arm-linux\STImultimedia.o  D:\Data\lazPlayer\lib\arm-linux\STImultimedia.s
if errorlevel 1 goto asmend
Del D:\Data\lazPlayer\lib\arm-linux\STImultimedia.s
SET THEFILE=umain
echo Assembling %THEFILE%
c:\codetyphon\fpc\fpc32\bin\i386-win32\arm-linux-as.exe -mfloat-abi=hard -meabi=5 -march=armv7-a -mfpu=vfpv3-d16 -o D:\Data\lazPlayer\lib\arm-linux\umain.o  D:\Data\lazPlayer\lib\arm-linux\umain.s
if errorlevel 1 goto asmend
Del D:\Data\lazPlayer\lib\arm-linux\umain.s
SET THEFILE=rasplazplayer
echo Assembling %THEFILE%
c:\codetyphon\fpc\fpc32\bin\i386-win32\arm-linux-as.exe -mfloat-abi=hard -meabi=5 -march=armv7-a -mfpu=vfpv3-d16 -o D:\Data\lazPlayer\lib\arm-linux\rasplazPlayer.o  D:\Data\lazPlayer\lib\arm-linux\rasplazPlayer.s
if errorlevel 1 goto asmend
Del D:\Data\lazPlayer\lib\arm-linux\rasplazPlayer.s
SET THEFILE=rasplazPlayer
echo Linking %THEFILE%
c:\codetyphon\fpc\fpc32\bin\i386-win32\arm-linux-ld.exe   --dynamic-linker=/lib/ld-linux-armhf.so.3   -s -L. -o rasplazPlayer link.res
if errorlevel 1 goto linkend
goto end
:asmend
echo An error occured while assembling %THEFILE%
goto end
:linkend
echo An error occured while linking %THEFILE%
:end
