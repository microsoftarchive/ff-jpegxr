@echo off

if ""=="%1" goto Error1

echo Dest folder: %1

if ""=="%2" goto Error2

set bak_folder="%2"

echo %bak_folder%

set destFolder=%bak_folder%
if exist %destFolder% goto Error3

md %destFolder%

set destFolder=%bak_folder%\image
if not exist %destFolder% md %destFolder%

set destFolder=%bak_folder%\image\src
if not exist %destFolder% md %destFolder%

set destFolder=%bak_folder%\image\build
if not exist %destFolder% md %destFolder%

set destFolder=%bak_folder%\image\decoders
if not exist %destFolder% md %destFolder%

set destFolder=%bak_folder%\netwerk
if not exist %destFolder% md %destFolder%

set destFolder=%bak_folder%\netwerk\mime
if not exist %destFolder% md %destFolder%

set destFolder=%bak_folder%\uriloader
if not exist %destFolder% md %destFolder%

set destFolder=%bak_folder%\uriloader\exthandler
if not exist %destFolder% md %destFolder%

echo on

copy %1\image\build\nsImageModule.cpp %bak_folder%\image\build
copy %1\image\decoders\moz.build %bak_folder%\image\decoders
copy %1\image\src\Image.h %bak_folder%\image\src
copy %1\image\src\Image.cpp %bak_folder%\image\src
copy %1\image\src\RasterImage.cpp %bak_folder%\image\src

copy %1\image\moz.build %bak_folder%\image

copy %1\netwerk\mime\nsMimeTypes.h %bak_folder%\netwerk\mime
copy %1\uriloader\exthandler\nsExternalHelperAppService.cpp %bak_folder%\uriloader\exthandler

goto Exit

:Error1
echo Source folder is not specified!
goto Exit

:Error2
echo Dest folder is not specified!
goto Exit

:Error3
echo Backup folder already exists. Running this batch file would overwrite the initial backup!

:Exit