#!/bin/bash
#
# $Id$
#
#
# This file is part of Kosmos File System (KFS).
#
# Licensed under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#
# Script to copy the metaserver checkpoint files to a remote node.
# The same script can also be used to restore the checkpoint files
# from remote path to local.
# 

# process any command-line arguments
TEMP=`getopt -o d:r:p:R:h -l dir:,remote:,path:,recover:help -n metabkup.sh -- "$@"`
eval set -- "$TEMP"

$recover=0
while true
do
	case "$1" in
	-d|--dir) cpdir=$2; shift 2;;
	-r|--remote) remote=$2; shift 2;;
	-p|--path) path=$2; shift 2;;
	-R|--recover) recover=1; shift;;
	-h|--help) echo "usage: $0 [-d cpdir] [-r remote] [-p path] {-recover}"; exit ;;
	--) shift; break ;;
	esac
done

if [ ! -d $cpdir ];
    then
    echo "$cpdir is non-existent"
    exit -1
fi

if [ $recover -ne 0];
    then
    rsync -avz --delete $cpdir $remote:$path
else
    # Restore the checkpoint files from remote node
    rsync -avz $remote:$path $cpdir
fi    
