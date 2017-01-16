#
# Samsung Common file.
#
obj-y += ss_dsi_panel_common.o
obj-y += ss_dsi_mdnie_lite_common.o
obj-y += ss_dsi_smart_dimming_common.o
obj-$(CONFIG_SAMSUNG_DEBUG_DISPLAY) += dlog.o
#
# panel directory make file
#

# Lucky panel
obj-$(CONFIG_PANEL_S6E3HA2_AMS509ES01_WQHD) += S6E3HA2_AMS509ES01/

# J7x panel
obj-$(CONFIG_PANEL_S6E3FA3_AMS549JR01_FHD) += S6E3FA3_AMS549JR01/

# A9 panel
obj-$(CONFIG_PANEL_S6E3FA3_AMS598KH01_FHD) += S6E3FA3_AMS598KH01/

# TAB S2 T715 panel
obj-$(CONFIG_PANEL_S6E3HA2_AMS801HK01_QXGA) += S6E3HA2_AMS801HK01/

# TAB S2 T815 panel
obj-$(CONFIG_PANEL_ANA38401_AMS968HH01_QXGA) += ANA38401_AMS968HH01/

# A5x panel
obj-$(CONFIG_PANEL_S6E3FA3_AMS520KG01_FHD) += S6E3FA3_AMS520KG01/

# C5Pro panel
obj-$(CONFIG_PANEL_S6E3FA3_AMS520KG09_FHD) += S6E3FA3_AMS520KG09/


#
# To build ALL panel files(For build error checking)
#
ifeq ($(CONFIG_DISPLAY_BUILD_ALL),y)
obj-y += S6E3HA2_AMS509ES01/
obj-y += S6E3FA3_AMS549JR01/
obj-y += S6E3FA3_AMS598KH01/
obj-y += S6E3HA2_AMS801HK01/
obj-y += ANA38401_AMS968HH01/
obj-y += S6E3FA3_AMS520KG01/
obj-y += S6E3FA3_AMS520KG09/
endif
