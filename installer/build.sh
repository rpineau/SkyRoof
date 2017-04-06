#!/bin/bash

mkdir -p ROOT/tmp/SkyRoof_X2/
cp "../SkyRoof.ui" ROOT/tmp/SkyRoof_X2/
cp "../SkyRoof.png" ROOT/tmp/SkyRoof_X2/
cp "../domelist SkyRoof.txt" ROOT/tmp/SkyRoof_X2/
cp "../build/Release/libSkyRoof.dylib" ROOT/tmp/SkyRoof_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.SkyRoof_X2 --sign "$installer_signature" --scripts Scritps --version 1.0 SkyRoof_X2.pkg
pkgutil --check-signature ./SkyRoof_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.SkyRoof_X2 --scripts Scritps --version 1.0 SkyRoof_X2.pkg
fi

rm -rf ROOT
