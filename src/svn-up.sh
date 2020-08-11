#!/bin/sh

set -ue

if [ ! -e .svn ]
then
	echo "no svn repo. ./svn-co.sh?" >&2
	exit 1
fi

svn up "$@"
svn info --show-item revision > svnrev.txt
