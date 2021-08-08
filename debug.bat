diskpart /s attach.txt 
xcopy image\* V:\efi\boot\ /s /e /r /c /y /h 
diskpart /s detach.txt 
qemu-system-x86_64 -s -no-reboot -no-shutdown -d cpu_reset -m 256 -M pc -monitor stdio -drive if=pflash,format=raw,readonly,file="OVMF_X64.fd" -drive id=dummydisk,format=raw,file="C:\virtual\vdisk.vhd" -drive id=fat16,format=raw,file="C:\virtual\vdisk2.vhd"