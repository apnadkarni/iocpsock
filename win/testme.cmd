@echo off
:: Launch the test suite!

if not defined MSDevDir (
    ::call "C:\Program Files\Microsoft Developer Studio\vc\bin\vcvars32.bat"
    ::call "C:\Program Files\Microsoft Developer Studio\vc98\bin\vcvars32.bat"
    call c:\dev\devstudio60\vc98\bin\vcvars32.bat
)

if not defined MSSdk (
    ::call c:\dev\platsdk\SetEnv.bat /RETAIL
    call "C:\Program Files\Microsoft SDK\SetEnv.bat" /RETAIL
)

if not defined TCLDIR (
    set TCLDIR=d:\tcl_workspace\tcl_84_branch
)

nmake -nologo -csf makefile.vc test
pause
