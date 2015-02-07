#!/bin/bash

die() {
    echo "$@"
    exit 1
}

if [ -z "$NETWORKID" ]; then
	NETWORKID=1
    echo "defaulting to network id: $NETWORKID"
fi

if [ -n "$NODEID" ]; then
    echo "default node id: $NODEID"
    NODEID="-DDEFAULT_NODEID=$NODEID"
else
    echo "no node id specified"
fi

echo "building..."
ino build -f "-DNETWORKID=$NETWORKID $NODEID \
              -ffunction-sections -fdata-sections -g -Os -w" || die

if [ "$1" != "-n" ]; then
    echo "uploading..."
    ino upload || die
fi

echo "done."
