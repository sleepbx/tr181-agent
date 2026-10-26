#!/bin/sh
# Smoke tests for tr181ctl. Usage: tests/run.sh ./tr181ctl
BIN=${1:-./tr181ctl}
export TR181_STATE=$(mktemp)
fail=0

check() {
    desc=$1; shift
    if "$@" >/dev/null 2>&1; then echo "PASS  $desc"; else echo "FAIL  $desc"; fail=1; fi
}

check "get DeviceInfo object"        $BIN get Device.DeviceInfo.
check "get single parameter"         $BIN get Device.DeviceInfo.UpTime
check "loopback interface present"   sh -c "$BIN get Device.Ethernet. | grep -q '\"lo\"'"
check "unknown path fails"           sh -c "! $BIN get Device.Nope."
check "set writable param"           $BIN set Device.ManagementServer.URL http://acs.example.com
check "value persisted"              sh -c "$BIN get Device.ManagementServer.URL | grep -q acs.example.com"
check "read-only rejected"           sh -c "! $BIN set Device.DeviceInfo.UpTime 5"
check "bad uint rejected"            sh -c "! $BIN set Device.ManagementServer.PeriodicInformInterval abc"
check "bool normalised"              sh -c "$BIN set Device.ManagementServer.PeriodicInformEnable 1 && $BIN get Device.ManagementServer.PeriodicInformEnable | grep -q true"
check "json output valid"            sh -c "$BIN json Device.DeviceInfo. | python3 -m json.tool"

rm -f "$TR181_STATE"
exit $fail
