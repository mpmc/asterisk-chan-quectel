#
# aterisk-chan-quectel
#
# OpenWRT package
#
include $(TOPDIR)/rules.mk
 
PKG_NAME:=asterisk-chan-quectel
 
PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/RoEdAl/asterisk-chan-quectel
PKG_SOURCE_DATE:=2023-05-10
PKG_RELEASE:=$(AUTORELEASE)
PKG_SOURCE_VERSION:=e47c8d8d093c97e1f0391defdae9d5652a0a88f4

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

define Package/asterisk-chan-quectel/postinst
#!/bin/sh
if [ -z "$${IPKG_INSTROOT}" ]; then
  echo
  echo 'o-------------------------------------------------------------------o'
  echo '| asterisk-chan-quectel note                                        |'
  echo 'o-------------------------------------------------------------------o'
  echo '| Adding the "asterisk" user to the "dialout" group might be        |'
  echo '| required for Asterisk to be able to access the module.            |'
  echo 'o-------------------------------------------------------------=^_^=-o'
  echo
fi
exit 0
endef
 
$(eval $(call BuildPackage,asterisk-chan-quectel))

