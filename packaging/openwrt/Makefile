include $(TOPDIR)/rules.mk

PKG_NAME:=tr181-agent
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_LICENSE:=MIT

include $(INCLUDE_DIR)/package.mk

define Package/tr181-agent
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Minimal TR-181 data model CLI (tr181ctl)
endef

define Package/tr181-agent/description
  Exposes Device.DeviceInfo., Device.Ethernet.Interface.{i}. and
  Device.ManagementServer. from /proc and /sys, with USP-style JSON output.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ../../src ../../Makefile $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/tr181-agent/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tr181ctl $(1)/usr/bin/
endef

$(eval $(call BuildPackage,tr181-agent))
