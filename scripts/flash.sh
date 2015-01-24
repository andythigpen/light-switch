#!/bin/bash

die() {
    echo "$@"
    exit 1
}

if [ -z "$NETWORKID" ]; then
	NETWORKID=1
    echo "defaulting to network id: $NETWORKID"
fi

echo "building..."
ino build -f "-DNETWORKID=$NETWORKID \
              -ffunction-sections -fdata-sections -g -Os -w" || die

if [ "$1" != "-n" ]; then
    echo "uploading..."
    ino upload || die
fi

echo "done."
