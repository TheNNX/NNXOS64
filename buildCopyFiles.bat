for %%I in ("%~dp0\.") do set ParentFolderName=%%~nxI
cd %ParentFolderName%
diskpart /s attach.txt
xcopy image\* V:\efi\boot\ /s /e /r /c /y /h
diskpart /s detach.txt
pause