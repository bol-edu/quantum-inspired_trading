#!/bin/bash
EXEC_CMD="$*"
#echo "${EXEC_CMD}"
ip netns exec sf0 ${EXEC_CMD}
