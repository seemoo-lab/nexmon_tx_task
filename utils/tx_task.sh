#!/bin/bash

#
#  ###########   ###########   ##########    ##########
# ############  ############  ############  ############
# ##            ##            ##   ##   ##  ##        ##
# ##            ##            ##   ##   ##  ##        ##
# ###########   ####  ######  ##   ##   ##  ##    ######
#  ###########  ####  #       ##   ##   ##  ##    #    #
#           ##  ##    ######  ##   ##   ##  ##    #    #
#           ##  ##    #       ##   ##   ##  ##    #    #
# ############  ##### ######  ##   ##   ##  ##### ######
# ###########    ###########  ##   ##   ##   ##########
#
#    S E C U R E   M O B I L E   N E T W O R K I N G
#
# ISC License (ISC)
#
# Copyright 2023 Jakob Link <jlink@seemoo.de>
#
# Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#


usage_str="usage: $0 <interface> <cmd>\n
<interface>:\t the wifi interface, e.g. eth6\n
<cmd>:\t\t init | start | stop | deinit\n
\t\t init:\t create a new tx task context and apply parameters to it
\t\t start:\t trigger start of currently configured tx task
\t\t stop:\t trigger stop of currently configured tx task
\t\t deinit: stop currently configured tx task and free context\n
\t\t an initialized tx task can be stopped and (re)started\n\n"

IFACE=$1
CMD=$2

NEXUTIL="/jffs/nexutil"
PRINTF="/usr/bin/printf"
SED="/bin/sed"
HEAD="/usr/bin/head"
OPENSSL="/usr/sbin/openssl"
TR="/usr/bin/tr"

# user settings
spatial_mode=0    # 0: OFF, 1: ON, 255: AUTO, might enable STBC -> leave OFF if unwanted
periodic=1        # 0: single transmission, 1: repetitive transmissions
tx_delay=500      # delay in milliseconds between repetitive transmissions and before the first transmission
repetitions=-1    # number of repetitive transmissions, -1 means infinite
bandwidth=1       # bandwidth (1, 2, 3 : 20, 40 or 80 MHz)
mcs=0             # VHT Modulation Coding Scheme
spatial_streams=1 # number of spatial strams (1 - 4)

# Data frame, To DS: 0 From DS: 1, Receiver/Destination address: 00:11:22:33:44:55, Transmitter address/BSS Id: 00:11:22:33:44:66, Source address: 00:11:22:33:44:77, no payload
frame_length=24
frame_bytes='\x08\x02\x00\x00\x00\x11\x22\x33\x44\x55\x00\x11\x22\x33\x44\x66\x00\x11\x22\x33\x44\x77\x00\x00'

if [ "$#" -lt 2 ]; then
    ${PRINTF} "${usage_str}"
    exit
fi

if [ ! -f ${NEXUTIL} ]; then
    ${PRINTF} "${NEXUTIL} not found\n"
    exit
fi
if [ ! -x ${NEXUTIL} ]; then
    ${PRINTF} "${NEXUTIL} not executable\n"
    exit
fi

if [ ${CMD} == "init" ]; then
    sm=$(${PRINTF} "\\\x%02x" ${spatial_mode})
    pe=$(${PRINTF} "\\\x%02x" ${periodic})
    dl=$(${PRINTF} "%08x" ${tx_delay} | ${SED} -r 's/(..)(..)(..)(..)/\\\x\4\\\x\3\\\x\2\\\x\1/')
    rp=$(${PRINTF} "%08x" ${repetitions} | ${SED} -r 's/(..)(..)(..)(..)/\\\x\4\\\x\3\\\x\2\\\x\1/' | ${HEAD} -c16)
    rt=$(${PRINTF} "\\\x%1x%1x\\\x00\\\x%02x\\\xc2" ${spatial_streams} ${mcs} ${bandwidth}) # see nexmon/patches/include/rates.h for more options
    ${NEXUTIL} -I${IFACE} -s429 -l$((14 + ${frame_length})) -b -v $(${PRINTF} "${sm}${pe}${dl}${rp}${rt}${frame_bytes}" | ${OPENSSL} enc -base64 | ${TR} -d "\n")
    ${PRINTF} "done\n"
    exit
fi

if [ ${CMD} == "start" ]; then
    ${NEXUTIL} -I${IFACE} -s430
    ${PRINTF} "done\n"
    exit
fi

if [ ${CMD} == "stop" ]; then
    ${NEXUTIL} -I${IFACE} -s431
    ${PRINTF} "done\n"
    exit
fi

if [ ${CMD} == "deinit" ]; then
    ${NEXUTIL} -I${IFACE} -s432
    ${PRINTF} "done\n"
    exit
fi

${PRINTF} "unknown cmd: ${CMD}\n"
${PRINTF} "${usage_str}"
exit
