# alib3 level-2 builder

export ALIB3 := $(dir $(firstword $(MAKEFILE_LIST)))
include config.mk
ROOT := ../..

LIBS := \
	ALAC \
	DynamicAudioNormalizer \
	fdk-aac \
	FLAC \
	mpg123 \
	opus \
	soxr \
	vorbis
ifneq "$(SYS)" "android"
	LIBS += \
		MAC \
		musepack \
		wavpack
endif

default: $(LIBS)

libALAC-phi.$(SO):
	$(MAKE) -f $(ALIB3)/ALAC/Makefile
ALAC: libALAC-phi.$(SO)

libDynamicAudioNormalizer-phi.$(SO):
	$(MAKE) -f $(ALIB3)/DynamicAudioNormalizer/Makefile
DynamicAudioNormalizer: libDynamicAudioNormalizer-phi.$(SO)

libfdk-aac-phi.$(SO):
	$(MAKE) -f $(ALIB3)/fdk-aac/Makefile
fdk-aac: libfdk-aac-phi.$(SO)

libFLAC-phi.$(SO):
	$(MAKE) -f $(ALIB3)/FLAC/Makefile
FLAC: libFLAC-phi.$(SO)

libMAC-phi.$(SO):
	$(MAKE) -f $(ALIB3)/MAC/Makefile
MAC: libMAC-phi.$(SO)

libmpg123-phi.$(SO):
	$(MAKE) -f $(ALIB3)/mpg123/Makefile
mpg123: libmpg123-phi.$(SO)

libmusepack-phi.$(SO):
	$(MAKE) -f $(ALIB3)/musepack/Makefile
musepack: libmusepack-phi.$(SO)

libopus-phi.$(SO):
	$(MAKE) -f $(ALIB3)/opus/Makefile
opus: libopus-phi.$(SO)

libsoxr-phi.$(SO):
	$(MAKE) -f $(ALIB3)/soxr/Makefile
soxr: libsoxr-phi.$(SO)

libvorbis-phi.$(SO):
	$(MAKE) -f $(ALIB3)/ogg/Makefile
	$(MAKE) -f $(ALIB3)/vorbis/Makefile
vorbis: libvorbis-phi.$(SO)

libwavpack-phi.$(SO):
	$(MAKE) -f $(ALIB3)/wavpack/Makefile
wavpack: libwavpack-phi.$(SO)
