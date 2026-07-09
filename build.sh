#!/bin/sh

VERSION=(`cat version`)

cmake --build build

macdeployqt build/AudioFlow3.app -verbose=2

codesign --force --deep --sign - build/AudioFlow3.app

ditto -c -k --sequesterRsrc --keepParent build/AudioFlow3.app "AudioFlow3-${VERSION}-macos-x86_64.zip"
