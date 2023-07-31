#!/bin/sh

PYTHON="$(which python3 2>/dev/null)"

if [ -z "$PYTHON" ]; then
    PYTHON="$(which python3.9 2>/dev/null)"
fi

if [ -z "$PYTHON" ]; then
    echo "Unable to locate Python3 interpreter."
    exit 1
fi

exec $PYTHON -Es /usr/share/flow_broker/flow_broker.py
