#!/bin/bash
EXEC_CMD="$*"
#echo "${EXEC_CMD}"
ip netns exec sf1 ${EXEC_CMD}