@echo off
:: Launch the test suite!

set platsdk="C:\Program Files\Microsoft SDK\SetEnv.bat"
if "%MSSdk%" == "" call %platsdk% /RETAIL
nmake -nologo -csf makefile.vc TCLDIR=..\tcl test
pause

