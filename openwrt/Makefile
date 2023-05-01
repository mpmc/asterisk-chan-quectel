#
# aterisk-chan-quectel
#
# OpenWRT package
#
include $(TOPDIR)/rules.mk
 
PKG_NAME:=asterisk-chan-quectel
 
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/RoEdAl/asterisk-chan-quectel
PKG_SOURCE_DATE:=2023-05-01
PKG_RELEASE:=$(AUTORELEASE)
PKG_SOURCE_VERSION:=ddd73373f94df04045ac28ec1d9b8e0495b48ed7

PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=COPYRIGHT.txt LICENSE.txt
PKG_MAINTAINER:=Edmunt Pienkowsky <roed@onet.eu>

MODULES_DIR:=/usr/lib/asterisk/modules
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk
 
define Package/asterisk-chan-quectel
  SUBMENU:=Telephony
  SECTION:=net
  CATEGORY:=Utilities
  URL:=http://github.com/RoEdAl/asterisk-chan-quectel
  DEPENDS:=$(ICONV_DEPENDS) +kmod-usb-acm +kmod-usb-serial +kmod-usb-serial-option +libusb-1.0 +usb-modeswitch +alsa-lib +libsqlite3
  TITLE:=Asterisk Mobile Telephony Module
endef
 
define Package/asterisk-chan-quectel/description
Asterisk channel driver for Mobile Telephony.
endef

define Package/asterisk-chan-quectel/conffiles
/etc/asterisk/quectel.conf
endef

define Package/asterisk-chan-quectel/install
	$(INSTALL_DIR) $(1)/etc/asterisk
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/ipkg-install/etc/asterisk/quectel.conf $(1)/etc/asterisk
	$(INSTALL_DIR) $(1)$(MODULES_DIR)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ipkg-install/$(MODULES_DIR)/chan_quectel.so $(1)$(MODULES_DIR)
endef
 
$(eval $(call BuildPackage,asterisk-chan-quectel))

