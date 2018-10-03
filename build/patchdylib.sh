#!/bin/sh

# Patch the dylib dependencies for all executables in the out directory
# Redirect from libimobiledevice.6.dylib in the out folder to libimobiledevice.dylib
# in the folder where the executable is located (this will be the setup in our target
# environment)
for f in $INSTALLDIR/bin/* $INSTALLDIR/lib/*.dylib; do
   for l in libplist libusbmuxd libimobiledevice; do
     chmod +w $f

     # Skip the first line of the otool output, this is just the header
     dylibs=`otool -L $f | tail -n +2 | grep "$l" | awk -F' ' '{ print $1 }'`

     for dylib in $dylibs; do
       echo Patching $dylib in $f

       # https://www.mikeash.com/pyblog/friday-qa-2009-11-06-linking-and-install-names.html
       install_name_tool -change $dylib @loader_path/$l.dylib $f
     done
   done

   otool -L $f
done

libcrypto=`otool -L $INSTALLDIR/lib/libssl.1.0.0.dylib | tail -n +2 | grep "libcrypto" | awk -F' ' '{ print $1 }'`
install_name_tool -change $libcrypto @loader_path/libcrypto.1.0.0.dylib $HOME/out/lib/libssl.1.0.0.dylib

install_name_tool -change /usr/local/opt/openssl/lib/libssl.1.0.0.dylib @loader_path/libssl.1.0.0.dylib $INSTALLDIR/lib/libimobiledevice.6.dylib
install_name_tool -change /usr/local/opt/openssl/lib/libcrypto.1.0.0.dylib @loader_path/libcrypto.1.0.0.dylib $INSTALLDIR/lib/libimobiledevice.6.dylib
