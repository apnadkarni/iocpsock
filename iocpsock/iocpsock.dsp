# Microsoft Developer Studio Project File - Name="iocpsock" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=iocpsock - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "iocpsock.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "iocpsock.mak" CFG="iocpsock - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "iocpsock - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "iocpsock - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "iocpsock - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IOCPSOCK_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "\tcl_workspace\tcl_84_branch\generic" /D "WIN32" /D "NDEBUG" /D "TCL_THREADS" /D "USE_TCL_STUBS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /i "\tcl_workspace\tcl_84_branch\generic" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 /nologo /dll /machine:I386 /out:"Release/iocpsock20.dll" /libpath:"\tcl_workspace\tcl_84_branch\win\Release" /opt:nowin98 /opt:icf,6
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "iocpsock - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "IOCPSOCK_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W4 /Gm /GX /ZI /Od /I "\tcl_workspace\tcl_84_branch\generic" /D "WIN32" /D "_DEBUG" /D "TCL_THREADS" /D "USE_TCL_STUBS" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /i "\tcl_workspace\tcl_84_branch\generic" /d "DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /dll /debug /machine:I386 /out:"Debug/iocpsock20g.dll" /pdbtype:sept /libpath:"\tcl_workspace\tcl_84_branch\win\Release"

!ENDIF 

# Begin Target

# Name "iocpsock - Win32 Release"
# Name "iocpsock - Win32 Debug"
# Begin Group "tests"

# PROP Default_Filter "test"
# Begin Source File

SOURCE=.\tests\all.tcl
# End Source File
# Begin Source File

SOURCE=.\tests\socket2.test
# End Source File
# Begin Source File

SOURCE=.\tests\test_client.tcl
# End Source File
# Begin Source File

SOURCE=.\tests\test_server.tcl
# End Source File
# End Group
# Begin Source File

SOURCE=.\dllmain.c
# End Source File
# Begin Source File

SOURCE=.\iocpsock.h
# End Source File
# Begin Source File

SOURCE=.\iocpsock.rc

!IF  "$(CFG)" == "iocpsock - Win32 Release"

# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
# SUBTRACT RSC /d "NDEBUG"

!ELSEIF  "$(CFG)" == "iocpsock - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\iocpsock_hilevel.c
# End Source File
# Begin Source File

SOURCE=.\iocpsock_lolevel.c
# End Source File
# Begin Source File

SOURCE=.\linkedlist.c
# End Source File
# Begin Source File

SOURCE=.\ws2apltalk.c
# End Source File
# Begin Source File

SOURCE=.\ws2atm.c
# End Source File
# Begin Source File

SOURCE=.\ws2decnet.c
# End Source File
# Begin Source File

SOURCE=.\ws2ipx.c
# End Source File
# Begin Source File

SOURCE=.\ws2irda.c
# End Source File
# Begin Source File

SOURCE=.\ws2isotp4.c
# End Source File
# Begin Source File

SOURCE=.\ws2netbios.c
# End Source File
# Begin Source File

SOURCE=.\ws2tcp.c
# End Source File
# Begin Source File

SOURCE=.\ws2udp.c
# End Source File
# Begin Source File

SOURCE=.\ws2vines.c
# End Source File
# End Target
# End Project
