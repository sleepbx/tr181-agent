# tr181-agent

A small C99 tool (`tr181ctl`) that exposes an embedded Linux device as a
**TR-181 (Device:2) data model**. It reads `/proc` and `/sys/class/net`, answers
`ubus-cli`-style get/set queries, and prints **USP (TR-369)-style GetResp JSON**.
Ships with an **OpenWrt package Makefile** and a **Yocto recipe**.

Live playground: `docs/index.html` (enable GitHub Pages on the `docs/` folder).

## Data model

| Object | Access | Source |
|---|---|---|
| `Device.DeviceInfo.` (Manufacturer, ModelName, SoftwareVersion, UpTime, MemoryStatus.Total) | R | `uname`, `/proc/uptime`, `/proc/meminfo` |
| `Device.Ethernet.Interface.{i}.` (Name, Status, MACAddress, MaxMTUSize) | R | `/sys/class/net/<if>/` |
| `Device.Ethernet.Interface.{i}.Stats.` (Bytes/Packets/Errors Sent/Received) | R | `/sys/class/net/<if>/statistics/` |
| `Device.ManagementServer.` (URL, PeriodicInformEnable, PeriodicInformInterval) | RW | in memory, persisted to `$TR181_STATE` |

## Usage

```sh
make && make test

./tr181ctl get Device.DeviceInfo.                 # partial path -> whole object
./tr181ctl get Device.Ethernet.Interface.1.Status
./tr181ctl set Device.ManagementServer.URL https://acs.example.net
./tr181ctl json Device.DeviceInfo.                # USP-style GetResp
./tr181ctl schema                                 # every param, type, access
./tr181ctl shell                                  # interactive prompt
```

Example:

```
$ ./tr181ctl json Device.DeviceInfo.
{
  "get_resp": {
    "requested_path": "Device.DeviceInfo.",
    "err_code": 0,
    "result_params": {
        "Device.DeviceInfo.Manufacturer": "OpenSource CPE",
        "Device.DeviceInfo.UpTime": 145,
        ...
    }
  }
}
```

Errors follow the data model rules: unknown paths fail, read-only parameters
reject `set`, and values are type-checked (`unsignedInt`, `boolean`, `string`).
An invalid path in `json` returns USP error code `7026`.

## Design

- `src/datamodel.c` – every parameter is produced by a getter and passed to a
  visitor callback, so `get`, `json`, `schema` and state saving share one walk.
- Linux `operstate` is mapped to the TR-181 `Status` enum (`Up`, `Down`, `Dormant`, ...).
- `src/main.c` – CLI, JSON formatter, state persistence, interactive shell.

## Building for a target

**OpenWrt** – copy `packaging/openwrt/Makefile` to `package/tr181-agent/` in your
OpenWrt tree, then `make package/tr181-agent/compile`.

**Yocto** – add `packaging/yocto/tr181-agent_1.0.0.bb` to your layer (point
`SRC_URI` at this repo) and run `bitbake tr181-agent`.

**Cross-compile by hand** – `make CC=aarch64-linux-gnu-gcc`.

## Ideas for next steps

- Register the tree on `ubus` (libubus) so `ubus call` can query it.
- Publish parameter changes over MQTT as a USP Notify.
- Add `Device.IP.Interface.{i}.IPv4Address.` from netlink.
