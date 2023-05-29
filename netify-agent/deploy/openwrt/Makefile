#
# Copyright (C) 2016-2022 eGloo Incorporated
#
# This is free software, licensed under the GNU General Public License v2.

include $(TOPDIR)/rules.mk

PKG_NAME:=netifyd
PKG_RELEASE:=1
PKG_MAINTAINER:=Darryl Sokoloski <darryl@egloo.ca>
PKG_LICENSE:=GPL-3.0-or-later

PKG_BUILD_PARALLEL:=1
PKG_FIXUP:=autoreconf
PKG_INSTALL:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://gitlab.com/netify.ai/public/netify-agent.git
PKG_SOURCE_DATE:=2023-05-16
PKG_SOURCE_VERSION:=e8db613cc3507aa1624de51a2bba4e874bf882d9
#PKG_SOURCE_VERSION:=@GIT_LAST_COMMIT_HASH@
PKG_MIRROR_HASH:=whatever

include $(INCLUDE_DIR)/package.mk

define Package/netifyd
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Netify Agent
  URL:=http://www.netify.ai/
  DEPENDS:=+ca-bundle +libcurl +libmnl +libnetfilter-conntrack +libpcap +zlib +libpthread @!USE_UCLIBC
  # Explicitly depend on libstdcpp rather than $(CXX_DEPENDS).  At the moment
  # std::unordered_map is only available via libstdcpp which is required for
  # performance reasons.
  DEPENDS+=+libstdcpp
endef

define Package/netifyd/description
The Netify Agent is a deep-packet inspection server which detects network
protocols and applications.  These detections can be saved locally, served over
a UNIX or TCP socket, and/or "pushed" (via HTTP POSTs) to a remote third-party
server.  Flow metadata, network statistics, and detection classifications are
JSON encoded for easy consumption by third-party applications.
endef

define Package/netifyd/conffiles
/etc/netifyd.conf
/etc/config/netifyd
/etc/netify.d/agent.uuid
/etc/netify.d/serial.uuid
/etc/netify.d/site.uuid
endef

TARGET_CFLAGS+=-ffunction-sections -fdata-sections -Wno-psabi
TARGET_CXXFLAGS+=-std=c++11 -ffunction-sections -fdata-sections -Wno-psabi
TARGET_LDFLAGS+=-Wl,--gc-sections

CONFIGURE_ARGS+= \
	--sharedstatedir=/var/run \
	# --enable-lean-and-mean
	--disable-libtcmalloc \
	--disable-jemalloc \
	--without-systemdsystemunitdir \
	--without-tmpfilesdir

ifneq ($(CONFIG_LIBCURL_ZLIB),y)
CONFIGURE_ARGS+= \
	--without-libcurl-zlib
endif

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/netifyd
	$(CP) $(PKG_INSTALL_DIR)/usr/include/netifyd/*.h $(1)/usr/include/netifyd
	$(INSTALL_DIR) $(1)/usr/include/netifyd/pcap-compat
	$(CP) $(PKG_INSTALL_DIR)/usr/include/netifyd/pcap-compat/*.h $(1)/usr/include/netifyd/pcap-compat
	$(INSTALL_DIR) $(1)/usr/include/netifyd/nlohmann
	$(CP) $(PKG_INSTALL_DIR)/usr/include/netifyd/nlohmann/*.hpp $(1)/usr/include/netifyd/nlohmann
	$(INSTALL_DIR) $(1)/usr/include/netifyd/radix
	$(CP) $(PKG_INSTALL_DIR)/usr/include/netifyd/radix/*.hpp $(1)/usr/include/netifyd/radix
	$(INSTALL_DIR) $(1)/usr/include/ndpi
	$(CP) $(PKG_INSTALL_DIR)/usr/include/ndpi/*.h $(1)/usr/include/ndpi
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libnetifyd.{a,so*} $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/lib/pkgconfig
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/lib/pkgconfig/libnetifyd.pc $(1)/usr/lib/pkgconfig
endef

define Package/netifyd/install
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/deploy/netifyd.conf $(1)/etc
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/netifyd.config $(1)/etc/config/netifyd
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/netifyd.init $(1)/etc/init.d/netifyd
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/netifyd $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libnetifyd.so.* $(1)/usr/lib
	$(INSTALL_DIR) $(1)/etc/netify.d
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/deploy/netify-apps.conf $(1)/etc/netify.d
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/deploy/netify-categories.json $(1)/etc/netify.d
	$(INSTALL_DIR) $(1)/usr/share/netifyd
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/deploy/functions.sh $(1)/usr/share/netifyd
endef

$(eval $(call BuildPackage,netifyd))