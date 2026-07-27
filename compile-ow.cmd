@echo off
rem compile-ow.cmd -- Build CLIPUNI.exe with OpenWatcom 2.0 on OS/2

if not exist bin-ow md bin-ow

wmake -f makefile-ow 2>&1 | tee compile-ow.log
