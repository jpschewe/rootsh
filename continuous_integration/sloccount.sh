#!/bin/sh

debug() { ! "${log_debug-false}" || log "DEBUG: $*" >&2; }
log() { printf '%s\n' "$*"; }
warn() { log "WARNING: $*" >&2; }
error() { log "ERROR: $*" >&2; }
fatal() { error "$*"; exit 1; }

mydir=$(cd "$(dirname "$0")" && pwd -L) || fatal "Unable to determine script directory"

if [ -z "`type -p sloccount`" ]; then
    log "No sloccount executable found"
    touch sloccount.sc
else
    sloccount --wide --details ../src ../test > sloccount.sc
fi
