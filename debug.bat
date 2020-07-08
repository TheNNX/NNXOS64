for %%I in ("%~dp0\.") do set ParentFolderName=%%~nxI
cd %ParentFolderName%
diskpart /s attach.txt
xcopy image\* V:\efi\boot\ /s /e /r /c /y /h
diskpart /s detach.txt
qemu-system-x86_64 -no-reboot -no-shutdown -d cpu_reset -m 256 -M pc -monitor stdio -drive if=pflash,format=raw,readonly,file="OVMF_X64.fd" -drive id=dummydisk,format=raw,file="C:\virtual\vdisk.vhd" -drive format=raw,file="C:\virtual\vdisk3.vhd" -drive file=fat:rw:fat16
