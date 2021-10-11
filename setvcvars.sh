# set MKS Korn shell environment variables for MS Visual Studio 2015
#
# !!! modified version !!!
# Copyright (C) 2016, 2020 Free Software Foundation, Inc

# Microsoft Visual Studio requires that several environment variables
# include many directories if a build is to be done from the command
# line.  These variables are normally set by selecting 'Developer
# Command Prompt' on the Windows Start Menu; the shortcut runs a batch
# file that calls several other batch files to set the variables before
# launching an instance of the Windows command interpreter.  This
# program calls the Windows command interpreter to run the batch file;
# the resulting values of the variables are echoed and read into the
# shell to set the variables in the shell.  For the values to persist,
# this program must be run in the current environment, i.e.,
#
#   source setvcvars
#
#       or
#
#   . setvcvars
#
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY--without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-
# 1301 USA
#
# 
# This program was written by Jeff Conrad (jeff_conrad@msn.com), and
# tested with the MKS Toolkit version 10.0 and Microsoft Visual Studio
# 2015 on Windows 10 Professional.

# variables needed for Visual Studio 2015
envvars='PATH=|INCLUDE=|LIB=|PATH='

# batch file: this value is installation and version dependent--adjust
# as needed. It should be the shortcut on the Start Menu for 'Developer
# Command Prompt'

case $1 in
x86|32|32bit|32-bit)
    vsbatfile="C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\VsDevCmd.bat"
    vsarch=
    ;;
*)
    vsbatfile="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
    vsarch=amd64
    ;;
esac

# don't set the variables twice, because new values are added to previous values
if test "$VCVARSSET" != ""
then
    echo "VC variables already set: $VCVARSSET bit"
elif test ! -e "$vsbatfile"
then
    echo "cannot find Command Prompt batch file '$vsbatfile'"
elif test -z "$ComSpec"
then
    echo "no path to command interpreter: ComSpec not set"
elif test ! -e "$ComSpec"
then
    print -r "cannot find command interpreter '$ComSpec'"
elif test ! -x "$ComSpec"	# this should never happen ...
then
    print -r "cannot run command interpreter '$ComSpec'"
else
    OPATH="$PATH"

    # keep UTF-8 signature from getting prepended with MKS Toolkit
    export TK_HEREDOC_FORMAT=ascii
    # turn off echoing with /q
    eval `$ComSpec /q  <<END | egrep "$envvars"
rem set the prompt to a space
prompt \\$s
call "$vsbatfile" $vsarch
echo PATH="%PATH%"; export PATH
echo INCLUDE="%INCLUDE%"; export INCLUDE
echo LIB="%LIB%"; export LIB
echo LIBPATH="%LIBPATH%"; export LIBPATH
END
`
    # variable initialization probably failed
    if test "$PATH" = "$OPATH"
    then
	echo "could not set VC variables"
    else # prevent setting the variables more than once
	case "$vsarch" in
	amd64)
	    VCVARSSET=64 ;;
	*)
	    VCVARSSET=32 ;;
	esac
	export VCVARSSET
    fi
fi
unset OPATH vsbatfile vsarch
