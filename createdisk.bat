@ECHO OFF
ECHO Starting
ECHO Setting up a disk image

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
@ECHO qemu-system-x86_64 -no-shutdown -d cpu_reset -m 256 -M pc -monitor stdio -drive if=pflash,format=raw,readonly,file="OVMF_X64.fd" -drive id=dummydisk,format=raw,file="%IMAGE_DIRECTORY%vdisk.vhd" >> debug.bat

DISKPART /s create.txt

ECHO DONE
PAUSE

:END