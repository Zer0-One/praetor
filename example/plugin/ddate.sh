#!/bin/sh

DATE=""

while sleep 1m; do
    if [ `date +%D` -ne $DATE ] && [ `date +%H` -eq "00" ]; then
        DATE=`date +%D`
        cat <<EOF
{
    "type" : "privmsg",
    "network" : "*",
    "target" : "*",
    "msg" : "HEAR YE, HEAR YE: `ddate`"
}
EOF
    fi
done
