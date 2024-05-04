# alib3 level-2 builder

include config.mk
TGT_DIR := _$(SYS)-$(CPU)
ROOT := ../..

LIBS := \
	ALAC \
	DynamicAudioNormalizer \
	fdk-aac \
	FLAC \
	MAC \
	mpg123 \
	musepack \
	opus \
	soxr \
	vorbis \
	wavpack

default:
	mkdir -p $(TGT_DIR)
	$(MAKE) build

build: $(LIBS)

$(TGT_DIR)/libALAC-phi.$(SO):
	$(MAKE) -C ALAC -I..
	cp ALAC/*.$(SO) $(TGT_DIR)
ALAC: $(TGT_DIR)/libALAC-phi.$(SO)

$(TGT_DIR)/libDynamicAudioNormalizer-phi.$(SO):
	$(MAKE) -C DynamicAudioNormalizer -I..
	cp DynamicAudioNormalizer/*.$(SO) $(TGT_DIR)
DynamicAudioNormalizer: $(TGT_DIR)/libDynamicAudioNormalizer-phi.$(SO)

$(TGT_DIR)/libfdk-aac-phi.$(SO):
	$(MAKE) -C fdk-aac -I..
	cp fdk-aac/*.$(SO) $(TGT_DIR)
fdk-aac: $(TGT_DIR)/libfdk-aac-phi.$(SO)

$(TGT_DIR)/libFLAC-phi.$(SO):
	$(MAKE) -C FLAC -I..
	cp FLAC/*.$(SO) $(TGT_DIR)
FLAC: $(TGT_DIR)/libFLAC-phi.$(SO)

$(TGT_DIR)/libMAC-phi.$(SO):
	$(MAKE) -C MAC -I..
	cp MAC/*.$(SO) $(TGT_DIR)
MAC: $(TGT_DIR)/libMAC-phi.$(SO)

$(TGT_DIR)/libmpg123-phi.$(SO):
	$(MAKE) -C mpg123 -I..
	cp mpg123/*.$(SO) $(TGT_DIR)
mpg123: $(TGT_DIR)/libmpg123-phi.$(SO)

$(TGT_DIR)/libmusepack-phi.$(SO):
	$(MAKE) -C musepack -I..
	cp musepack/*.$(SO) $(TGT_DIR)
musepack: $(TGT_DIR)/libmusepack-phi.$(SO)

$(TGT_DIR)/libopus-phi.$(SO):
	$(MAKE) -C opus -I..
	cp opus/*.$(SO) $(TGT_DIR)
opus: $(TGT_DIR)/libopus-phi.$(SO)

$(TGT_DIR)/libsoxr-phi.$(SO):
	$(MAKE) -C soxr -I..
	cp soxr/*.$(SO) $(TGT_DIR)
soxr: $(TGT_DIR)/libsoxr-phi.$(SO)

ogg/libogg.a:
	$(MAKE) -C ogg -I..
$(TGT_DIR)/libvorbis-phi.$(SO):
	$(MAKE) ogg/libogg.a
	$(MAKE) -C vorbis -I..
	cp vorbis/*.$(SO) $(TGT_DIR)
vorbis: $(TGT_DIR)/libvorbis-phi.$(SO)

$(TGT_DIR)/libwavpack-phi.$(SO):
	$(MAKE) -C wavpack -I..
	cp wavpack/*.$(SO) $(TGT_DIR)
wavpack: $(TGT_DIR)/libwavpack-phi.$(SO)

clean:
	$(MAKE) clean -C ALAC -I..
	$(MAKE) clean -C DynamicAudioNormalizer -I..
	$(MAKE) clean -C fdk-aac -I..
	$(MAKE) clean -C FLAC -I..
	$(MAKE) clean -C MAC -I..
	$(MAKE) clean -C mpg123 -I..
	$(MAKE) clean -C musepack -I..
	$(MAKE) clean -C ogg -I..
	$(MAKE) clean -C opus -I..
	$(MAKE) clean -C soxr -I..
	$(MAKE) clean -C vorbis -I..
	$(MAKE) clean -C wavpack -I..

md5:
	md5sum -b \
		ALAC/master.zip \
		DynamicAudioNormalizer/2.10.tar.gz \
		fdk-aac/v0.1.6.tar.gz \
		FLAC/flac-1.4.2.tar.xz \
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
	md5sum -c packages.md5 --ignore-missing
