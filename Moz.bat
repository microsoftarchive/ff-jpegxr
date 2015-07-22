@echo off

if ""=="%1" goto Error1

set destFolder=%1\image
if not exist %destFolder% goto Error2

set destFolder=%1\image\decoders
if not exist %destFolder% goto Error2

set destFolder=%1\image\build
if not exist %destFolder% goto Error2

set destFolder=%1\image\src
if not exist %destFolder% goto Error2

if not exist %1\image\jxrlib md %1\image\jxrlib
if not exist %1\image\jxrlib\include md %1\image\jxrlib\include
if not exist %1\image\jxrlib\image md %1\image\jxrlib\image
if not exist %1\image\jxrlib\image\decode md %1\image\jxrlib\image\decode
if not exist %1\image\jxrlib\image\sys md %1\image\jxrlib\image\sys
if not exist %1\image\jxrlib\image\x86 md %1\image\jxrlib\image\x86
if not exist %1\image\jxrlib\jxrglue md %1\image\jxrlib\jxrglue

copy include\guiddef.h %1\image\jxrlib\include
copy include\wmsal.h %1\image\jxrlib\include
copy include\wmspecstring.h %1\image\jxrlib\include
copy include\wmspecstrings_adt.h %1\image\jxrlib\include
copy include\wmspecstrings_strict.h %1\image\jxrlib\include
copy include\wmspecstrings_undef.h %1\image\jxrlib\include

copy image\x86\x86.h %1\image\jxrlib\image\x86
copy image\Decode\decode.c %1\image\jxrlib\image\decode
copy image\Decode\decode.h %1\image\jxrlib\image\decode
copy image\Decode\postprocess.c %1\image\jxrlib\image\decode
copy image\Decode\segdec.c %1\image\jxrlib\image\decode
copy image\Decode\strdec.c %1\image\jxrlib\image\decode
copy image\Decode\strInvTransform.c %1\image\jxrlib\image\decode
copy image\Decode\strPredQuantDec.c %1\image\jxrlib\image\decode

copy image\Sys\ansi.h %1\image\jxrlib\image\sys
copy image\Sys\common.h %1\image\jxrlib\image\sys
copy image\Sys\perfTimer.h %1\image\jxrlib\image\sys
copy image\Sys\strcodec.h %1\image\jxrlib\image\sys
copy image\Sys\strtransform.h %1\image\jxrlib\image\sys
copy image\Sys\windowsmediaphoto.h %1\image\jxrlib\image\sys
copy image\Sys\xplatform_image.h %1\image\jxrlib\image\sys

copy image\Sys\adapthuff.c %1\image\jxrlib\image\sys
copy image\Sys\image.c %1\image\jxrlib\image\sys
copy image\Sys\strcodec.c %1\image\jxrlib\image\sys
copy image\Sys\strPredQuant.c %1\image\jxrlib\image\sys
copy image\Sys\strTransform.c %1\image\jxrlib\image\sys

copy JXRGlueLib\JXRGlue.h %1\image\jxrlib\jxrglue
copy JXRGlueLib\JXRMeta.h %1\image\jxrlib\jxrglue

copy JXRGlueLib\JXRGlue.c %1\image\jxrlib\jxrglue
copy JXRGlueLib\JXRGlueDec.c %1\image\jxrlib\jxrglue
copy JXRGlueLib\JXRGluePFC.c %1\image\jxrlib\jxrglue
copy JXRGlueLib\JXRMeta.c %1\image\jxrlib\jxrglue

copy Mozilla\image\jxrlib\moz.build  %1\image\jxrlib


rem goto Exit

copy Mozilla\image\decoders\moz.build  %1\image\decoders
copy Mozilla\image\decoders\nsJPEGXRDecoder.h  %1\image\decoders
copy Mozilla\image\decoders\nsJPEGXRDecoder.cpp  %1\image\decoders

copy Mozilla\image\build\nsImageModule.cpp  %1\image\build
copy Mozilla\image\src\Image.h  %1\image\src
copy Mozilla\image\src\Image.cpp  %1\image\src
copy Mozilla\image\src\RasterImage.cpp  %1\image\src

copy Mozilla\image\moz.build  %1\image

copy Mozilla\netwerk\mime\nsMimeTypes.h %1\netwerk\mime
copy Mozilla\uriloader\exthandler\nsExternalHelperAppService.cpp %1\uriloader\exthandler


goto Exit

:Error1
echo Folder not specified!
goto Exit

:Error2
echo Destination folder %destFolder% does not exist!
goto Exit

:Error3
echo Could not create destination folder!
goto Exit

:Exit


