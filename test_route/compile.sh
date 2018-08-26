#!/bin/bash
#
gcc BasicValidator_Route.c ssp_helpers.c linux.c libitlssp.a  libhredis.a -o BasicValidator_Route -lpthread
