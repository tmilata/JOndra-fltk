# Microsoft Developer Studio Project File - Name="Jondra" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=Jondra - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Jondra.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Jondra.mak" CFG="Jondra - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Jondra - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Jondra - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Jondra - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\fltk-1.1.10\\" /I "..\fltk-1.1.10\zlib" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /FD /Zm200 /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x405 /d "NDEBUG"
# ADD RSC /l 0x405 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib comctl32.lib ..\fltk-1.1.10\lib\fltk.lib ..\fltk-1.1.10\lib\fltkforms.lib ..\fltk-1.1.10\lib\fltkgl.lib ..\fltk-1.1.10\lib\fltkimages.lib ..\fltk-1.1.10\lib\fltkjpeg.lib ..\fltk-1.1.10\lib\fltkpng.lib ..\fltk-1.1.10\lib\fltkz.lib /nologo /subsystem:windows /machine:I386 /nodefaultlib:"LIBCMT.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=if not exist "dist\Win98-Win11" md "dist\Win98-Win11"	copy /Y "Release\Jondra.exe" "dist\Win98-Win11\Jondra.exe"
# End Special Build Tool

!ELSEIF  "$(CFG)" == "Jondra - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\fltk-1.1.10\\" /I "..\fltk-1.1.10\zlib" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FD /GZ /Zm200 /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x405 /d "_DEBUG"
# ADD RSC /l 0x405 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib comctl32.lib ..\fltk-1.1.10\lib\fltk.lib ..\fltk-1.1.10\lib\fltkforms.lib ..\fltk-1.1.10\lib\fltkgl.lib ..\fltk-1.1.10\lib\fltkimages.lib ..\fltk-1.1.10\lib\fltkjpeg.lib ..\fltk-1.1.10\lib\fltkpng.lib ..\fltk-1.1.10\lib\fltkz.lib /nologo /subsystem:windows /debug /machine:I386 /nodefaultlib:"LIBCD.lib" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Jondra - Win32 Release"
# Name "Jondra - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\BinOpen.cpp
# End Source File
# Begin Source File

SOURCE=.\Clock.cpp
# End Source File
# Begin Source File

SOURCE=.\Config.cpp
# End Source File
# Begin Source File

SOURCE=.\cpuintf.cpp
# End Source File
# Begin Source File

SOURCE=.\Debug.cpp
# End Source File
# Begin Source File

SOURCE=.\DebuggerWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\DirtyTiles.cpp
# End Source File
# Begin Source File

SOURCE=.\EmbeddedResources.cpp
# End Source File
# Begin Source File

SOURCE=.\EmbeddedResourcesData.cpp
# End Source File
# Begin Source File

SOURCE=.\FlatButton.cpp
# End Source File
# Begin Source File

SOURCE=.\HexInput.cpp
# End Source File
# Begin Source File

SOURCE=.\Jondra.cpp
# End Source File
# Begin Source File

SOURCE=.\Keyboard.cpp
# End Source File
# Begin Source File

SOURCE=.\Melodik.cpp
# End Source File
# Begin Source File

SOURCE=.\Memory.cpp
# End Source File
# Begin Source File

SOURCE=.\MemoryTimeline.cpp
# End Source File
# Begin Source File

SOURCE=.\MTimer.cpp
# End Source File
# Begin Source File

SOURCE=.\Ondra.cpp
# End Source File
# Begin Source File

SOURCE=.\Screen.cpp
# End Source File
# Begin Source File

SOURCE=.\Settings.cpp
# End Source File
# Begin Source File

SOURCE=.\Sound.cpp
# End Source File
# Begin Source File

SOURCE=.\SoundBuffer.cpp
# End Source File
# Begin Source File

SOURCE=.\SoundSample.cpp
# End Source File
# Begin Source File

SOURCE=.\Tape.cpp
# End Source File
# Begin Source File

SOURCE=.\TapeSignalProc.cpp
# End Source File
# Begin Source File

SOURCE=.\TapFile.cpp
# End Source File
# Begin Source File

SOURCE=.\z80disassembler.cpp
# End Source File
# Begin Source File

SOURCE=.\z80emu.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\BinOpen.h
# End Source File
# Begin Source File

SOURCE=.\Clock.h
# End Source File
# Begin Source File

SOURCE=.\Config.h
# End Source File
# Begin Source File

SOURCE=.\cpuintf.h
# End Source File
# Begin Source File

SOURCE=.\CustomMenuBar.h
# End Source File
# Begin Source File

SOURCE=.\Debug.h
# End Source File
# Begin Source File

SOURCE=.\DebuggerWindow.h
# End Source File
# Begin Source File

SOURCE=.\DirtyTiles.h
# End Source File
# Begin Source File

SOURCE=.\EmbeddedResources.h
# End Source File
# Begin Source File

SOURCE=.\flatbutton.h
# End Source File
# Begin Source File

SOURCE=.\HexInput.h
# End Source File
# Begin Source File

SOURCE=.\instructions.h
# End Source File
# Begin Source File

SOURCE=.\Jondra.h
# End Source File
# Begin Source File

SOURCE=.\Keyboard.h
# End Source File
# Begin Source File

SOURCE=.\macros.h
# End Source File
# Begin Source File

SOURCE=.\Melodik.h
# End Source File
# Begin Source File

SOURCE=.\Memory.h
# End Source File
# Begin Source File

SOURCE=.\MemoryTimeline.h
# End Source File
# Begin Source File

SOURCE=.\MTimer.h
# End Source File
# Begin Source File

SOURCE=.\Ondra.h
# End Source File
# Begin Source File

SOURCE=.\Screen.h
# End Source File
# Begin Source File

SOURCE=.\Settings.h
# End Source File
# Begin Source File

SOURCE=.\Sound.h
# End Source File
# Begin Source File

SOURCE=.\SoundBuffer.h
# End Source File
# Begin Source File

SOURCE=.\SoundSample.h
# End Source File
# Begin Source File

SOURCE=.\tables.h
# End Source File
# Begin Source File

SOURCE=.\Tape.h
# End Source File
# Begin Source File

SOURCE=.\TapeSignalProc.h
# End Source File
# Begin Source File

SOURCE=.\TapFile.h
# End Source File
# Begin Source File

SOURCE=.\z80disassembler.h
# End Source File
# Begin Source File

SOURCE=.\z80emu.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# Begin Source File

SOURCE=.\Resources.dep

!IF  "$(CFG)" == "Jondra - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Generating embedded resources
InputPath=.\Resources.dep

".\EmbeddedResourcesData.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ResPacker\Release\ResPacker.exe Release EmbeddedResourcesData.cpp

# End Custom Build

!ELSEIF  "$(CFG)" == "Jondra - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Generating embedded resources
InputPath=.\Resources.dep

".\EmbeddedResourcesData.cpp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ResPacker\Debug\ResPacker.exe Debug EmbeddedResourcesData.cpp

# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
