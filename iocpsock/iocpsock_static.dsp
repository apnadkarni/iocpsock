# Microsoft Developer Studio Project File - Name="iocpsock_static" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=iocpsock_static - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "iocpsock_static.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "iocpsock_static.mak" CFG="iocpsock_static - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "iocpsock_static - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "iocpsock_static - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "iocpsock_static - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "iocpsock_static___Win32_Release"
# PROP BASE Intermediate_Dir "iocpsock_static___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_Static"
# PROP Intermediate_Dir "Release_Static"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\tcl\generic" /D "WIN32" /D "STATIC_BUILD" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Release_Static\iocpsocks.lib"

!ELSEIF  "$(CFG)" == "iocpsock_static - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "iocpsock_static___Win32_Debug"
# PROP BASE Intermediate_Dir "iocpsock_static___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug_Static"
# PROP Intermediate_Dir ".\Debug_Static"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /ZI /Od /I "..\tcl\generic" /D "WIN32" /D "_DEBUG" /D "STATIC_BUILD" /D "SHOWDBG" /YX"iocpsock.h" /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\tcl\generic" /d "DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug_Static\iocpsocksd.lib"

!ENDIF 

# Begin Target

# Name "iocpsock_static - Win32 Release"
# Name "iocpsock_static - Win32 Debug"
# Begin Source File

SOURCE=.\dllmain.c
# End Source File
# Begin Source File

SOURCE=.\iocpsock.h
# End Source File
# Begin Source File

SOURCE=.\iocpsock.rc
# PROP Exclude_From_Build 1
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
