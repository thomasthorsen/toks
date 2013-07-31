#! /bin/sh
#
# Builds the release zip for Windows using mingw
# On ubuntu, you'll need to install the mingw32 package.
#

# grab the version - there is probably an easier way...
VERSION=`grep '#define VERSION ' src/config.h | \
         sed -e "s/#define VERSION//" -e 's/\"//g' -e 's/ //g'`
VERDIR=toks-$VERSION-win32
RELDIR=.
THEZIP=$RELDIR/$VERDIR.zip

echo "Building version $VERSION for Win32"

./configure --host=i586-mingw32msvc

make clean
make

if [ -e $VERDIR ] ; then
  rm -rf $VERDIR
fi
mkdir $VERDIR

cp src/toks.exe $VERDIR/

strip $VERDIR/toks.exe

if ! [ -e $RELDIR ] ; then
  mkdir $RELDIR
fi
[ -e $THEZIP ] && rm -f $THEZIP

zip -r9 $THEZIP $VERDIR

echo "Stored in $THEZIP"
