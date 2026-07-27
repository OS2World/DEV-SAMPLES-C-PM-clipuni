@echo off
rem compile-gcc.cmd -- Build CLIPUNI.exe with GCC/kLIBC on OS/2

set EMXOMFLD_TYPE=WLINK
set EMXOMFLD_LINKER=wl.exe
set EMXOMFLD_PRELINK=0

if not exist bin-gcc md bin-gcc

make -f makefile-gcc 2>&1 | tee compile-gcc.log
