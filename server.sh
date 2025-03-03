#!/bin/bash
redis-server $NESTDAQ/etc/redis.conf --loadmodule $NESTDAQ/lib/redistimeseries.so
daq-webctl >& /dev/null &