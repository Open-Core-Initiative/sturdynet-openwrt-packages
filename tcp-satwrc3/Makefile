#
# Copyright (C) 2006-2009 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=tcp-satwrc3
PKG_RELEASE:=0.1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/tcp-satwrc3
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Support
  TITLE:=TCP SatWRC3 Congestion Control Algorithm
  FILES:=$(PKG_BUILD_DIR)/satwrc3.ko
  AUTOLOAD:=$(call AutoLoad,99,satwrc3)
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(KERNEL_MAKE_FLAGS) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(BUILDFLAGS)" \
		modules
endef

$(eval $(call KernelPackage,tcp-satwrc3))
