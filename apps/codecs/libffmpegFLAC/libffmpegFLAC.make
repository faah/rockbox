#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
# $Id$
#

# libffmpegFLAC
FFMPEGFLACLIB := $(CODECDIR)/libffmpegFLAC.a
FFMPEGFLACLIB_SRC := $(call preprocess, $(APPSDIR)/codecs/libffmpegFLAC/SOURCES)
FFMPEGFLACLIB_OBJ := $(call c2obj, $(FFMPEGFLACLIB_SRC))
OTHER_SRC += $(FFMPEGFLACLIB_SRC)

# libffmpegFLAC is faster on ARM-targets with -O2 than -O1
FFMPEGFLACFLAGS = -I$(APPSDIR)/codecs/libffmpegFLAC $(filter-out -O%,$(CODECFLAGS))
FFMPEGFLACFLAGS += -O2

$(FFMPEGFLACLIB): $(FFMPEGFLACLIB_OBJ)
	$(SILENT)$(shell rm -f $@)
	$(call PRINTS,AR $(@F))$(AR) rcs $@ $^ >/dev/null

$(CODECDIR)/libffmpegFLAC/%.o: $(ROOTDIR)/apps/codecs/libffmpegFLAC/%.c
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(FFMPEGFLACFLAGS) -c $< -o $@
