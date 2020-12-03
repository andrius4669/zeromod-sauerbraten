#!/bin/sh

set -ue

if [ ! -e .svn ]
then
	echo "no svn repo. ./svn-co.sh?" >&2
	exit 1
fi

svnrev=$(svn info --show-item last-changed-revision)
gitrev=$(cat svnrev.txt)

if [ $svnrev != $gitrev ]
then
	if [ $svnrev -lt $gitrev ]
	then
		svn up -r $gitrev --accept mf
	else
		echo "error: local repo is at higher revision than git one" >&2
		exit 1
	fi
fi

svn up
svn info --show-item last-changed-revision > svnrev.txt
