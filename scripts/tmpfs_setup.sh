#!/bin/bash
TMPFSDIR=/mnt/tmpfs
TMPFSUID=root
TMPFSGID=root

set -e

echo -n "Creating base cache directory ${TMPFSDIR} ... "
mkdir -p "${TMPFSDIR}" >/dev/null
echo "done"
echo -n "Mounting mem-backed filesystem on top of ${TMPFSDIR} (uid=${TMPFSUID}, gid=${TMPFSGID})... "
mount -t tmpfs -o uid="${TMPFSUID}",gid="${TMPFSGID}" none "${TMPFSDIR}" > /dev/null
echo "done"
