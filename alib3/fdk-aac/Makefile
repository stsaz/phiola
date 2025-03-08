# libfdk-aac

include ../config.mk

VER := 2.0.3
URL := https://github.com/mstorsjo/fdk-aac/archive/refs/tags/v$(VER).tar.gz
MD5SUM := fbaf688a6fd6ddf63c68566909a178b4
PKG := $(ALIB3)/fdk-aac/$(notdir $(URL))
DIR := fdk-aac-$(VER)
LIB := libfdk-aac-phi.$(SO)

default: $(DIR)
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	echo "$(MD5SUM) *$(PKG)" | md5sum -c -
	$(UNTAR_GZ) $(PKG)
	cat $(ALIB3)/fdk-aac/*.patch | patch -d $(DIR) -p1

# build
CF += -fno-exceptions -fno-rtti \
	-I$(DIR)/libAACdec/include \
	-I$(DIR)/libMpegTPDec/include \
	-I$(DIR)/libSBRdec/include \
	-I$(DIR)/libSYS/include \
	-I$(DIR)/libFDK/include \
	-I$(DIR)/libPCMutils/include \
	-I$(DIR)/libArithCoding/include \
	-I$(DIR)/libDRCdec/include \
	-I$(DIR)/libSACdec/include
CF += \
	-I$(DIR)/libAACenc/include \
	-I$(DIR)/libMpegTPEnc/include \
	-I$(DIR)/libSBRenc/include \
	-I$(DIR)/libSACenc/include

CFLAGS += -Wno-narrowing
# $(DIR)/libAACenc/src/aacEnc_rom.cpp:661:1: error: narrowing conversion of ‘2180108801u’ from ‘unsigned int’ to ‘FIXP_DBL {aka long int}’ inside { } [-Wnarrowing]

LINKXXFLAGS += -static-libstdc++

AACDEC_SRC := \
	$(DIR)/libAACdec/src/FDK_delay.cpp \
	$(DIR)/libAACdec/src/aac_ram.cpp \
	$(DIR)/libAACdec/src/aac_rom.cpp \
	$(DIR)/libAACdec/src/aacdec_drc.cpp \
	$(DIR)/libAACdec/src/aacdec_hcr.cpp \
	$(DIR)/libAACdec/src/aacdec_hcr_bit.cpp \
	$(DIR)/libAACdec/src/aacdec_hcrs.cpp \
	$(DIR)/libAACdec/src/aacdec_pns.cpp \
	$(DIR)/libAACdec/src/aacdec_tns.cpp \
	$(DIR)/libAACdec/src/aacdecoder.cpp \
	$(DIR)/libAACdec/src/aacdecoder_lib.cpp \
	$(DIR)/libAACdec/src/block.cpp \
	$(DIR)/libAACdec/src/channel.cpp \
	$(DIR)/libAACdec/src/channelinfo.cpp \
	$(DIR)/libAACdec/src/conceal.cpp \
	$(DIR)/libAACdec/src/ldfiltbank.cpp \
	$(DIR)/libAACdec/src/pulsedata.cpp \
	$(DIR)/libAACdec/src/rvlc.cpp \
	$(DIR)/libAACdec/src/rvlcbit.cpp \
	$(DIR)/libAACdec/src/rvlcconceal.cpp \
	$(DIR)/libAACdec/src/stereo.cpp \
	$(DIR)/libAACdec/src/usacdec_ace_d4t64.cpp \
	$(DIR)/libAACdec/src/usacdec_ace_ltp.cpp \
	$(DIR)/libAACdec/src/usacdec_acelp.cpp \
	$(DIR)/libAACdec/src/usacdec_fac.cpp \
	$(DIR)/libAACdec/src/usacdec_lpc.cpp \
	$(DIR)/libAACdec/src/usacdec_lpd.cpp \
	$(DIR)/libAACdec/src/usacdec_rom.cpp

AACENC_SRC := \
	$(DIR)/libAACenc/src/aacenc.cpp \
	$(DIR)/libAACenc/src/aacenc_lib.cpp \
	$(DIR)/libAACenc/src/aacenc_pns.cpp \
	$(DIR)/libAACenc/src/aacEnc_ram.cpp \
	$(DIR)/libAACenc/src/aacEnc_rom.cpp \
	$(DIR)/libAACenc/src/aacenc_tns.cpp \
	$(DIR)/libAACenc/src/adj_thr.cpp \
	$(DIR)/libAACenc/src/band_nrg.cpp \
	$(DIR)/libAACenc/src/bandwidth.cpp \
	$(DIR)/libAACenc/src/bit_cnt.cpp \
	$(DIR)/libAACenc/src/bitenc.cpp \
	$(DIR)/libAACenc/src/block_switch.cpp \
	$(DIR)/libAACenc/src/channel_map.cpp \
	$(DIR)/libAACenc/src/chaosmeasure.cpp \
	$(DIR)/libAACenc/src/dyn_bits.cpp \
	$(DIR)/libAACenc/src/grp_data.cpp \
	$(DIR)/libAACenc/src/intensity.cpp \
	$(DIR)/libAACenc/src/line_pe.cpp \
	$(DIR)/libAACenc/src/metadata_compressor.cpp \
	$(DIR)/libAACenc/src/metadata_main.cpp \
	$(DIR)/libAACenc/src/mps_main.cpp \
	$(DIR)/libAACenc/src/ms_stereo.cpp \
	$(DIR)/libAACenc/src/noisedet.cpp \
	$(DIR)/libAACenc/src/pnsparam.cpp \
	$(DIR)/libAACenc/src/pre_echo_control.cpp \
	$(DIR)/libAACenc/src/psy_configuration.cpp \
	$(DIR)/libAACenc/src/psy_main.cpp \
	$(DIR)/libAACenc/src/qc_main.cpp \
	$(DIR)/libAACenc/src/quantize.cpp \
	$(DIR)/libAACenc/src/sf_estim.cpp \
	$(DIR)/libAACenc/src/spreading.cpp \
	$(DIR)/libAACenc/src/tonality.cpp \
	$(DIR)/libAACenc/src/transform.cpp

ARITHCODING_SRC := \
	$(DIR)/libArithCoding/src/ac_arith_coder.cpp

DRCDEC_SRC := \
	$(DIR)/libDRCdec/src/FDK_drcDecLib.cpp \
	$(DIR)/libDRCdec/src/drcDec_gainDecoder.cpp \
	$(DIR)/libDRCdec/src/drcDec_reader.cpp \
	$(DIR)/libDRCdec/src/drcDec_rom.cpp \
	$(DIR)/libDRCdec/src/drcDec_selectionProcess.cpp \
	$(DIR)/libDRCdec/src/drcDec_tools.cpp \
	$(DIR)/libDRCdec/src/drcGainDec_init.cpp \
	$(DIR)/libDRCdec/src/drcGainDec_preprocess.cpp \
	$(DIR)/libDRCdec/src/drcGainDec_process.cpp

FDK_SRC := \
	$(DIR)/libFDK/src/FDK_bitbuffer.cpp \
	$(DIR)/libFDK/src/FDK_core.cpp \
	$(DIR)/libFDK/src/FDK_crc.cpp \
	$(DIR)/libFDK/src/FDK_decorrelate.cpp \
	$(DIR)/libFDK/src/FDK_hybrid.cpp \
	$(DIR)/libFDK/src/FDK_lpc.cpp \
	$(DIR)/libFDK/src/FDK_matrixCalloc.cpp \
	$(DIR)/libFDK/src/FDK_qmf_domain.cpp \
	$(DIR)/libFDK/src/FDK_tools_rom.cpp \
	$(DIR)/libFDK/src/FDK_trigFcts.cpp \
	$(DIR)/libFDK/src/autocorr2nd.cpp \
	$(DIR)/libFDK/src/dct.cpp \
	$(DIR)/libFDK/src/fft.cpp \
	$(DIR)/libFDK/src/fft_rad2.cpp \
	$(DIR)/libFDK/src/fixpoint_math.cpp \
	$(DIR)/libFDK/src/huff_nodes.cpp \
	$(DIR)/libFDK/src/mdct.cpp \
	$(DIR)/libFDK/src/nlc_dec.cpp \
	$(DIR)/libFDK/src/qmf.cpp \
	$(DIR)/libFDK/src/scale.cpp

MPEGTPDEC_SRC := \
	$(DIR)/libMpegTPDec/src/tpdec_adif.cpp \
	$(DIR)/libMpegTPDec/src/tpdec_adts.cpp \
	$(DIR)/libMpegTPDec/src/tpdec_asc.cpp \
	$(DIR)/libMpegTPDec/src/tpdec_drm.cpp \
	$(DIR)/libMpegTPDec/src/tpdec_latm.cpp \
	$(DIR)/libMpegTPDec/src/tpdec_lib.cpp

MPEGTPENC_SRC := \
	$(DIR)/libMpegTPEnc/src/tpenc_adif.cpp \
	$(DIR)/libMpegTPEnc/src/tpenc_adts.cpp \
	$(DIR)/libMpegTPEnc/src/tpenc_asc.cpp \
	$(DIR)/libMpegTPEnc/src/tpenc_latm.cpp \
	$(DIR)/libMpegTPEnc/src/tpenc_lib.cpp

SACDEC_SRC := \
	$(DIR)/libSACdec/src/sac_bitdec.cpp \
	$(DIR)/libSACdec/src/sac_calcM1andM2.cpp \
	$(DIR)/libSACdec/src/sac_dec.cpp \
	$(DIR)/libSACdec/src/sac_dec_conceal.cpp \
	$(DIR)/libSACdec/src/sac_dec_lib.cpp \
	$(DIR)/libSACdec/src/sac_process.cpp \
	$(DIR)/libSACdec/src/sac_qmf.cpp \
	$(DIR)/libSACdec/src/sac_reshapeBBEnv.cpp \
	$(DIR)/libSACdec/src/sac_rom.cpp \
	$(DIR)/libSACdec/src/sac_smoothing.cpp \
	$(DIR)/libSACdec/src/sac_stp.cpp \
	$(DIR)/libSACdec/src/sac_tsd.cpp

SACENC_SRC := \
	$(DIR)/libSACenc/src/sacenc_bitstream.cpp \
	$(DIR)/libSACenc/src/sacenc_delay.cpp \
	$(DIR)/libSACenc/src/sacenc_dmx_tdom_enh.cpp \
	$(DIR)/libSACenc/src/sacenc_filter.cpp \
	$(DIR)/libSACenc/src/sacenc_framewindowing.cpp \
	$(DIR)/libSACenc/src/sacenc_huff_tab.cpp \
	$(DIR)/libSACenc/src/sacenc_lib.cpp \
	$(DIR)/libSACenc/src/sacenc_nlc_enc.cpp \
	$(DIR)/libSACenc/src/sacenc_onsetdetect.cpp \
	$(DIR)/libSACenc/src/sacenc_paramextract.cpp \
	$(DIR)/libSACenc/src/sacenc_staticgain.cpp \
	$(DIR)/libSACenc/src/sacenc_tree.cpp \
	$(DIR)/libSACenc/src/sacenc_vectorfunctions.cpp

SBRENC_SRC := \
	$(DIR)/libSBRenc/src/bit_sbr.cpp \
	$(DIR)/libSBRenc/src/code_env.cpp \
	$(DIR)/libSBRenc/src/env_bit.cpp \
	$(DIR)/libSBRenc/src/env_est.cpp \
	$(DIR)/libSBRenc/src/fram_gen.cpp \
	$(DIR)/libSBRenc/src/invf_est.cpp \
	$(DIR)/libSBRenc/src/mh_det.cpp \
	$(DIR)/libSBRenc/src/nf_est.cpp \
	$(DIR)/libSBRenc/src/ps_bitenc.cpp \
	$(DIR)/libSBRenc/src/ps_encode.cpp \
	$(DIR)/libSBRenc/src/ps_main.cpp \
	$(DIR)/libSBRenc/src/resampler.cpp \
	$(DIR)/libSBRenc/src/sbr_encoder.cpp \
	$(DIR)/libSBRenc/src/sbr_misc.cpp \
	$(DIR)/libSBRenc/src/sbrenc_freq_sca.cpp \
	$(DIR)/libSBRenc/src/sbrenc_ram.cpp \
	$(DIR)/libSBRenc/src/sbrenc_rom.cpp \
	$(DIR)/libSBRenc/src/ton_corr.cpp \
	$(DIR)/libSBRenc/src/tran_det.cpp

SBRDEC_SRC := \
	$(DIR)/libSBRdec/src/HFgen_preFlat.cpp \
	$(DIR)/libSBRdec/src/env_calc.cpp \
	$(DIR)/libSBRdec/src/env_dec.cpp \
	$(DIR)/libSBRdec/src/env_extr.cpp \
	$(DIR)/libSBRdec/src/hbe.cpp \
	$(DIR)/libSBRdec/src/huff_dec.cpp \
	$(DIR)/libSBRdec/src/lpp_tran.cpp \
	$(DIR)/libSBRdec/src/psbitdec.cpp \
	$(DIR)/libSBRdec/src/psdec.cpp \
	$(DIR)/libSBRdec/src/psdec_drm.cpp \
	$(DIR)/libSBRdec/src/psdecrom_drm.cpp \
	$(DIR)/libSBRdec/src/pvc_dec.cpp \
	$(DIR)/libSBRdec/src/sbr_deb.cpp \
	$(DIR)/libSBRdec/src/sbr_dec.cpp \
	$(DIR)/libSBRdec/src/sbr_ram.cpp \
	$(DIR)/libSBRdec/src/sbr_rom.cpp \
	$(DIR)/libSBRdec/src/sbrdec_drc.cpp \
	$(DIR)/libSBRdec/src/sbrdec_freq_sca.cpp \
	$(DIR)/libSBRdec/src/sbrdecoder.cpp

PCMUTILS_SRC := \
	$(DIR)/libPCMutils/src/limiter.cpp \
	$(DIR)/libPCMutils/src/pcm_utils.cpp \
	$(DIR)/libPCMutils/src/pcmdmx_lib.cpp

SYS_SRC := \
	$(DIR)/libSYS/src/genericStds.cpp \
	$(DIR)/libSYS/src/syslib_channelMapDescr.cpp

SRC := $(AACDEC_SRC) \
	$(ARITHCODING_SRC) \
	$(DRCDEC_SRC) \
	$(MPEGTPDEC_SRC) \
	$(SACDEC_SRC) \
	$(SBRDEC_SRC) \
	$(PCMUTILS_SRC) \
	$(FDK_SRC) \
	$(SYS_SRC)
SRC += $(AACENC_SRC) \
	$(MPEGTPENC_SRC) \
	$(SACENC_SRC) \
	$(SBRENC_SRC)
OBJ := fdk-aac-phi.o $(SRC:.cpp=.o)

%.o: $(ALIB3)/fdk-aac/%.c $(ALIB3)/fdk-aac/fdk-aac-phi.h
	$(C) $(CFLAGS) $(CF) $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CF) $< -o $@

$(LIB): $(OBJ)
	$(LINKXX) $(LINKXXFLAGS) -shared $+ -o $@

clean:
	$(RM) $(OBJ) $(DIR)
