# alib3 level-2 builder

include config.mk
TGT_DIR := _$(SYS)-$(CPU)
ROOT := ../..

LIBS := \
	ALAC \
	fdk-aac \
	FLAC \
	MAC \
	mpg123 \
	musepack \
	ogg \
	opus \
	soxr \
	vorbis \
	wavpack

default:
	mkdir -p $(TGT_DIR)
	$(MAKE) build

build: $(addprefix $(TGT_DIR)/lib,$(addsuffix -phi.$(SO),$(LIBS)))

$(TGT_DIR)/lib%-phi.$(SO): %
	$(MAKE) -I.. -C $<
	mv $</*.$(SO) $(TGT_DIR)

$(TGT_DIR)/libvorbis-phi.$(SO): vorbis $(TGT_DIR)/libogg-phi.$(SO)
	mv $(TGT_DIR)/libogg-phi.$(SO) $<
	$(MAKE) -I.. -C $<
	mv $</*.$(SO) $(TGT_DIR)

clean:
	$(MAKE) -I.. -C ALAC clean
	$(MAKE) -I.. -C fdk-aac clean
	$(MAKE) -I.. -C FLAC clean
	$(MAKE) -I.. -C MAC clean
	$(MAKE) -I.. -C mpg123 clean
	$(MAKE) -I.. -C musepack clean
	$(MAKE) -I.. -C ogg clean
	$(MAKE) -I.. -C opus clean
	$(MAKE) -I.. -C soxr clean
	$(MAKE) -I.. -C vorbis clean
	$(MAKE) -I.. -C wavpack clean

md5:
	md5sum -b \
		ALAC/master.zip \
		fdk-aac/v0.1.6.tar.gz \
		FLAC/flac-1.3.3.tar.xz \
		MAC/MAC_SDK_433.zip \
		mpg123/mpg123-1.25.10.tar.bz2 \
		musepack/musepack_src_r475.tar.gz \
		ogg/libogg-1.3.3.tar.xz \
		opus/opus-1.3.1.tar.gz \
		soxr/soxr-0.1.3-Source.tar.xz \
		vorbis/libvorbis-1.3.7.tar.xz \
		wavpack/wavpack-4.75.0.tar.bz2 \
		>packages.md5

md5check:
	md5sum -c packages.md5 --ignore-missing
