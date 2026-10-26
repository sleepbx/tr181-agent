SUMMARY = "Minimal TR-181 data model CLI (tr181ctl)"
DESCRIPTION = "Reads Device.DeviceInfo., Device.Ethernet.Interface.{i}. from /proc and /sys \
and prints them as TR-181 parameters or USP-style JSON."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# Point this at your fork, e.g. git://github.com/<user>/tr181-agent.git;protocol=https;branch=main
SRC_URI = "file://src/ file://Makefile"
S = "${WORKDIR}"

do_compile() {
    oe_runmake CC="${CC}" CFLAGS="${CFLAGS}" LDFLAGS="${LDFLAGS}"
}

do_install() {
    oe_runmake install DESTDIR="${D}" PREFIX="${prefix}"
}
