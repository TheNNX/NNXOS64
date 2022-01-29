@echo on
for /f "tokens=*" %%a in ( 
'type .\NNXOSKRN\nnxver.h ^| find "#define NNX_BUILD"' 
) do ( 
set val=%%a 
) 

set MAJOR=0
set MINOR=1
set PATCH=0
set OSNAME="NNXOS64"

set val=%val:#DEFINE NNX_BUILD= %
set val=%val: =% 
set /A val=%val% + 1

echo /* Do not change any values in this file, change them in genverh.bat instead */ > .\NNXOSKRN\nnxver.h
echo #ifndef NNX_VER_HEADER>>.\NNXOSKRN\nnxver.h
echo #define NNX_VER_HEADER>>.\NNXOSKRN\nnxver.h
echo #define NNX_OSNAME %OSNAME% >> .\NNXOSKRN\nnxver.h
echo #define NNX_MAJOR %MAJOR% >> .\NNXOSKRN\nnxver.h
echo #define NNX_MINOR %MINOR% >> .\NNXOSKRN\nnxver.h
echo #define NNX_PATCH %PATCH% >> .\NNXOSKRN\nnxver.h
echo #define NNX_BUILD %val% >> .\NNXOSKRN\nnxver.h
echo #endif>>.\NNXOSKRN\nnxver.h

echo %val%