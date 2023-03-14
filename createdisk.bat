@ECHO OFF
ECHO Starting
ECHO Setting up a disk image

ECHO *** By continuing to use the script or any other part of the NNXOS64 build tools or source, you confirm that you understand that being licensed under the GNU LGPL3, NNXOS64 is provided "'AS IS' WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU. SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION." ***
ECHO Use at your own risk!
ECHO *** WARNING!!! *** do not put whitespace in the file path
ECHO *** THE IMAGE IS MOUNTED AS THE V: DRIVE WHEN NNXOS64 IS BUILD ***
SET /P IMAGE_PATH=Please give image file path:
FOR %%A IN (%IMAGE_PATH%) DO SET "IMAGE_PATH=%%~fA"
FOR %%A IN (%IMAGE_PATH%) DO SET "IMAGE_DIRECTORY=%%~dpA"

ECHO Directory: %IMAGE_DIRECTORY%

IF IMAGE_DIRECTORY == "" (
	SET IMAGE_DIRECTORY = %CD%
) ELSE (
	REM
)

IF NOT EXIST %IMAGE_DIRECTORY% (
	MKDIR %IMAGE_DIRECTORY%
) ELSE (
	REM
)

IF EXIST %IMAGE_PATH% (
	CHOICE /M "File already exists, overrite?"
	IF ERRORLEVEL 2 GOTO :END
	DEL %IMAGE_PATH%
) ELSE (
	REM
)

@ECHO create vdisk file=%IMAGE_PATH% type=fixed maximum=100 > create.txt
@ECHO select vdisk file=%IMAGE_PATH% >> create.txt
@ECHO attach vdisk >> create.txt
@ECHO convert mbr >> create.txt
@ECHO create partition primary >> create.txt
@ECHO format fs=fat32 >> create.txt
@ECHO assign letter=V >> create.txt
@ECHO detach vdisk >> create.txt
@ECHO  

@ECHO select vdisk file=%IMAGE_PATH% > attach.txt
@ECHO attach vdisk >> attach.txt

@ECHO select vdisk file=%IMAGE_PATH% > detach.txt
@ECHO detach vdisk >> detach.txt

@ECHO diskpart /s attach.txt > debug.bat
@ECHO xcopy image\* V:\efi\boot\ /s /e /r /c /y /h >> debug.bat
@ECHO diskpart /s detach.txt >> debug.bat
@ECHO rem Due to an obscure bug, I assume in QEMU for Windows, the drive letter in an absolute path in quotes is treated as a protocol and this script is broken >> debug.bat
@ECHO rem However, if the path to the disk doesn't contain whitespace - the script work >> debug.bat
@ECHO qemu-system-x86_64 -no-shutdown -d cpu_reset -m 256 -M pc -monitor stdio -drive if=pflash,format=raw,readonly,file=OVMF_X64.fd -drive id=dummydisk,format=raw,file=%IMAGE_PATH% >> debug.bat

DISKPART /s create.txt

ECHO DONE
PAUSE

:END