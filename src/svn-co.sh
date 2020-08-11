#!/bin/sh

set -uex

if [ -e .svn ]
then
	echo "svn stuff already checked out" >&2
	exit 1
fi

svn checkout https://svn.code.sf.net/p/sauerbraten/code/src src-svn
cd src-svn
svn up -r$(cat ../svnrev.txt)
cd ..
mv -f src-svn/.svn ./
rm -rf src-svn
