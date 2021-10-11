@rem create Windows installation director hierarchy for units,
@rem   a program for units conversion
@rem
@rem   for Microsoft Windows(R) without Unix-style utilities
@rem   tested with Windows XP SP3
@rem
@rem version 1.0 12 March 2014 by Jeff Conrad (jeff_conrad@msn.com)
@rem
@rem Copyright (C) 2014
@rem Free Software Foundation, Inc
@rem
@rem This program is free software; you can redistribute it and/or modify
@rem it under the terms of the GNU General Public License as published by
@rem the Free Software Foundation; either version 3 of the License, or
@rem (at your option) any later version.
@rem
@rem This program is distributed in the hope that it will be useful,
@rem but WITHOUT ANY WARRANTY; without even the implied warranty of
@rem MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
@rem GNU General Public License for more details.
@rem
@rem You should have received a copy of the GNU General Public License
@rem along with this program; if not, write to the Free Software
@rem Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
@rem    
@rem
@rem The units program was written by Adrian Mariano (adrianm@gnu.org).

@echo off

SetLocal EnableDelayedExpansion EnableExtensions
path %SystemRoot%\System32;%SystemRoot%;%SystemRoot%\System32\Wbem

@rem basename of this script
set ProgName=%~n0
set Errors=0

if "%cmdextversion%"=="" (
  echo %0: you must enable command extensions to run this batch file
  goto :eof
)

set args=%*
if not defined args (
  echo %ProgName%: missing directory
  echo Usage: %ProgName% ^<directory1^> [^<directory2^> ... ]
  goto end
)

@rem protect parens, e.g., 'Program Files (x86)'
set args=%args:(=^(%
set args=%args:)=^)%

@rem separate directory pathnames into components
for %%i in (%args%
) do (
    @rem strip quotes
    call :mkdirs %%~i
)
goto end

:mkdirs
  set installdir=%*
  if not defined installdir goto end_mkdirs
  if "%installdir:~0,1%"==":" (
    echo %ProgName%: pathname '%*': cannot begin with ':'
    goto end_mkdirs
  )
  @rem tag leading '\' so it can be preserved
  if "%installdir:~0,1%"=="\" set installdir=:#%installdir%
  @rem tag spaces so we can restore them later
  set installdir=%installdir: =:@:%
  @rem split pathname into components
  set installdir=%installdir:\= %
  @rem restore leading '\'
  set installdir=%installdir::#=\%
  call :mkpathcomp %installdir%
:end_mkdirs
goto :eof

:mkpathcomp
  set drive=%1
  set drive=%drive:~0,2%
  @rem protect silly stuff like '()'
  set drive=%drive:(=^^(%
  set drive=%drive:)=^^)%
  if "%drive:~1,1%"==":" (
    if not exist "%drive%" (
      echo %ProgName%: drive '%drive%' does not exist
      set Errors=1
      goto end_mkpathcomp
    )
  )
  set pathcomp=
  set pathargs=%*
  set pathargs=%pathargs:(=^^(%
  set pathargs=%pathargs:)=^^)%
  for %%j in (%pathargs%) do (
    set pathcomp=!pathcomp!%%j
    @rem restore spaces
    set pathcomp=!pathcomp::@:= !
    if "!pathcomp!"=="\" set pathcomp=

    if not exist !pathcomp!\ (
      mkdir "!pathcomp!"
      if errorlevel 1  (
        echo %ProgName%: cannot create directory '!pathcomp!'
        set Errors=1
        goto end_mkpathcomp
      ) else (
        echo %ProgName%: created directory '!pathcomp!'
      )
    )
    set pathcomp=!pathcomp!\
  )
:end_mkpathcomp
goto :eof

:end
if %Errors% NEQ 0 (
    echo.
    echo %ProgName%: one or more directories could not be created
)
@rem needed to handle invocation with command extension disabled
:eof
EndLocal
