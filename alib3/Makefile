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
	$(SUBMAKE) md5check

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

md5:
	cd $(ALIB3) && md5sum -b \
		ALAC/master.zip \
		DynamicAudioNormalizer/2.10.tar.gz \
		fdk-aac/v0.1.6.tar.gz \
		FLAC/1.4.3.zip \
		MAC/MAC_SDK_433.zip \
		mpg123/mpg123-1.32.3.tar.bz2 \
		musepack/musepack_src_r475.tar.gz \
		ogg/libogg-1.3.3.tar.xz \
		opus/opus-1.5.2.zip \
		soxr/soxr-0.1.3-Source.tar.xz \
		vorbis/libvorbis-1.3.7.tar.xz \
		wavpack/wavpack-4.75.0.tar.bz2 \
		>packages.md5

md5check:
	cd $(ALIB3) && md5sum -c packages.md5 --ignore-missing
