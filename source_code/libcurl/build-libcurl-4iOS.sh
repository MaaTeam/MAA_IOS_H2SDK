#!/bin/bash

export IPHONEOS_DEPLOYMENT_TARGET="6.0"
export CC="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"

ARCH="armv7"
export CFLAGS="-arch ${ARCH} -pipe -Os -gdwarf-2 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
export LDFLAGS="-arch ${ARCH} -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
make distclean
./configure --enable-shared --enable-static --host="${ARCH}-apple-darwin" --enable-threaded-resolver --with-darwinssl --with-nghttp2=/usr/share/nghttp2 || exit 1
make -j `sysctl -n hw.logicalcpu_max` || exit 1
cp include/curl/curlbuild.h ./libcurl-4iOS/curl/curlbuild32.h
cp lib/.libs/libcurl.a "./libcurl-4iOS/libcurl-${ARCH}.a"
cp lib/.libs/libcurl.dylib "./libcurl-4iOS/libcurl-${ARCH}.dylib"

echo "------------------------------------------------------------------------------------------------------------------"

ARCH="armv7s"
export CFLAGS="-arch ${ARCH} -pipe -Os -gdwarf-2 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
export LDFLAGS="-arch ${ARCH} -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
make distclean
./configure --enable-shared --enable-static --host="${ARCH}-apple-darwin" --enable-threaded-resolver --with-darwinssl --with-nghttp2=/usr/share/nghttp2 || exit 1
make -j `sysctl -n hw.logicalcpu_max` || exit 1
cp lib/.libs/libcurl.a "./libcurl-4iOS/libcurl-${ARCH}.a"
cp lib/.libs/libcurl.dylib "./libcurl-4iOS/libcurl-${ARCH}.dylib"

echo "------------------------------------------------------------------------------------------------------------------"

ARCH="arm64"
export IPHONEOS_DEPLOYMENT_TARGET="7.0"
export CFLAGS="-arch ${ARCH} -pipe -Os -gdwarf-2 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
export LDFLAGS="-arch ${ARCH} -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
make distclean
./configure --enable-shared --enable-static --host="aarch64-apple-darwin" --enable-threaded-resolver --with-darwinssl --with-nghttp2=/usr/share/nghttp2 || exit 1
make -j `sysctl -n hw.logicalcpu_max` || exit 1
cp include/curl/curlbuild.h ./libcurl-4iOS/curl/curlbuild64.h
cp lib/.libs/libcurl.a "./libcurl-4iOS/libcurl-${ARCH}.a"
cp lib/.libs/libcurl.dylib "./libcurl-4iOS/libcurl-${ARCH}.dylib"

echo "------------------------------------------------------------------------------------------------------------------"

ARCH="i386"
export IPHONEOS_DEPLOYMENT_TARGET="6.0"
export CFLAGS="-arch ${ARCH} -pipe -Os -gdwarf-2 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk"
export CPPFLAGS="-D__IPHONE_OS_VERSION_MIN_REQUIRED=${IPHONEOS_DEPLOYMENT_TARGET%%.*}0000"
export LDFLAGS="-arch ${ARCH} -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk"
make distclean
./configure --enable-shared --enable-static --host="${ARCH}-apple-darwin" --enable-threaded-resolver --with-darwinssl --with-nghttp2=/usr/share/nghttp2 || exit 1
make -j `sysctl -n hw.logicalcpu_max` || exit 1
cp lib/.libs/libcurl.a "./libcurl-4iOS/libcurl-${ARCH}.a"
cp lib/.libs/libcurl.dylib "./libcurl-4iOS/libcurl-${ARCH}.dylib"

echo "------------------------------------------------------------------------------------------------------------------"

ARCH="x86_64"
export IPHONEOS_DEPLOYMENT_TARGET="7.0"
export CFLAGS="-arch ${ARCH} -pipe -Os -gdwarf-2 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk"
export CPPFLAGS="-D__IPHONE_OS_VERSION_MIN_REQUIRED=${IPHONEOS_DEPLOYMENT_TARGET%%.*}0000"
export LDFLAGS="-arch ${ARCH} -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk"
make distclean
./configure --enable-shared --enable-static --host="${ARCH}-apple-darwin" --enable-threaded-resolver --with-darwinssl --with-nghttp2=/usr/share/nghttp2 || exit 1
make -j `sysctl -n hw.logicalcpu_max` || exit 1
cp lib/.libs/libcurl.a "./libcurl-4iOS/libcurl-${ARCH}.a"
cp lib/.libs/libcurl.dylib "./libcurl-4iOS/libcurl-${ARCH}.dylib"

echo "------------------------------------------------------------------------------------------------------------------"

cp include/curl/*.h ./libcurl-4iOS/curl/

lipo -create -output "./libcurl-4iOS/libcurl.a" "./libcurl-4iOS/libcurl-armv7.a" "./libcurl-4iOS/libcurl-armv7s.a" "./libcurl-4iOS/libcurl-arm64.a" "./libcurl-4iOS/libcurl-i386.a" "./libcurl-4iOS/libcurl-x86_64.a"
lipo -create -output "./libcurl-4iOS/libcurl.dylib" "./libcurl-4iOS/libcurl-armv7.dylib" "./libcurl-4iOS/libcurl-armv7s.dylib" "./libcurl-4iOS/libcurl-arm64.dylib" "./libcurl-4iOS/libcurl-i386.dylib" "./libcurl-4iOS/libcurl-x86_64.dylib"
