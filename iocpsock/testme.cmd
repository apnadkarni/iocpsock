@echo off
::Launch the test suite!

if not defined MSDevDir (
    call c:\dev\devstudio60\vc98\bin\vcvars32.bat
)

if not defined MSSdk (
    call c:\dev\platfo~1\SetEnv.bat /RETAIL
)

if not defined TCLDIR (
    set TCLDIR=d:\tcl_workspace\tcl_84_branch
)

nmake -nologo -f makefile.vc test
pause
