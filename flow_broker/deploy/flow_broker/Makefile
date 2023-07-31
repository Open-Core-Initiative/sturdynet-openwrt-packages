include $(TOPDIR)/rules.mk

PKG_NAME:=flow_broker
PKG_SOURCE_VERSION:=dfb565897aceae389dbbb06d82825892b4aea55b
PKG_SOURCE_DATE=2022-04-08
PKG_RELEASE:=v1.0
PKG_MAINTAINER:=Michael Foxworthy <mfoxworthy@ipsquared.com>

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/mfoxworthy/flow_broker.git
PKG_MIRROR_HASH:=skip

PKG_LICENSE:=GPLv3

include $(INCLUDE_DIR)/package.mk

define Package/flow_broker
  SECTION:=net
  CATEGORY:=Network
  DEPENDS:=+ulogd +ulogd-mod-extra +ulogd-mod-json +ulogd-mod-nfct +ulogd-mod-nflog +python3-netifaces +iptables-mod-nflog
  TITLE:=flow broker - Application to capture ulogd NFLOG and NFCT logs, format to JSON and send to a UDS.
  PKGARCH:=all
endef

define Package/flow_broker/description
  Flow broker takes all data form configured NFCT and NFLOG ulogd entries and sends them to a UDS in JSON format. This
  is useful for applications that need packet and flow level data.
endef


define Build/Compile
endef


define Package/flow_broker/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/deploy/openwrt/files/flow_broker.init  $(1)/etc/init.d/flow_broker

	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/deploy/openwrt/files/flow_broker.sh $(1)/usr/sbin

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/deploy/openwrt/files/flow_broker.config  $(1)/etc/config/flow_broker

	$(INSTALL_DIR) $(1)/usr/share/flow_broker
	$(CP) $(PKG_BUILD_DIR)/*.py $(1)/usr/share/flow_broker/
endef

define Package/flow_broker/postinst
#!/bin/sh
/etc/init.d/flow_broker enable
/etc/init.d/flow_broker start
exit 0
endef

$(eval $(call BuildPackage,flow_broker))
