# libopus

include ../config.mk

VER := 1.3.1
URL := https://archive.mozilla.org/pub/opus/opus-$(VER).tar.gz
PKG := $(notdir $(URL))
DIR := opus-$(VER)
LIB := libopus-phi.$(SO)

ifeq "$(CPU)" "arm64"
	ifeq "$(SYS)" "android"
		CONFIGURE_FLAGS := --host=aarch64
	else
		CONFIGURE_FLAGS := --host=aarch64-linux-gnu
	endif
else ifeq "$(CPU)" "arm"
	CONFIGURE_FLAGS := --host=armv7
endif
ifeq "$(SYS)" "android"
	CONFIGURE_ENV := CC="$(C_DIR)/clang" CFLAGS="$(A_CFLAGS)"
endif
default: $(DIR)
	cd $(DIR) && $(CONFIGURE_ENV) ./configure $(CONFIGURE_FLAGS)
	cat $(DIR)/config.h
	$(MAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	$(UNTAR_GZ) $(PKG)
	touch $(DIR)

# build
CFLAGS += \
	-DHAVE_CONFIG_H -DOPUS_EXPORT= \
	-I$(DIR) -I$(DIR)/include -I$(DIR)/celt -I$(DIR)/silk -I$(DIR)/silk/float -I$(DIR)/silk/fixed
CFLAGS_SSE :=

CELT_SOURCES := \
	$(DIR)/celt/bands.c \
	$(DIR)/celt/celt.c \
	$(DIR)/celt/celt_encoder.c \
	$(DIR)/celt/celt_decoder.c \
	$(DIR)/celt/cwrs.c \
	$(DIR)/celt/entcode.c \
	$(DIR)/celt/entdec.c \
	$(DIR)/celt/entenc.c \
	$(DIR)/celt/kiss_fft.c \
	$(DIR)/celt/laplace.c \
	$(DIR)/celt/mathops.c \
	$(DIR)/celt/mdct.c \
	$(DIR)/celt/modes.c \
	$(DIR)/celt/pitch.c \
	$(DIR)/celt/celt_lpc.c \
	$(DIR)/celt/quant_bands.c \
	$(DIR)/celt/rate.c \
	$(DIR)/celt/vq.c

SILK_SOURCES := \
	$(DIR)/silk/CNG.c \
	$(DIR)/silk/code_signs.c \
	$(DIR)/silk/init_decoder.c \
	$(DIR)/silk/decode_core.c \
	$(DIR)/silk/decode_frame.c \
	$(DIR)/silk/decode_parameters.c \
	$(DIR)/silk/decode_indices.c \
	$(DIR)/silk/decode_pulses.c \
	$(DIR)/silk/decoder_set_fs.c \
	$(DIR)/silk/dec_API.c \
	$(DIR)/silk/enc_API.c \
	$(DIR)/silk/encode_indices.c \
	$(DIR)/silk/encode_pulses.c \
	$(DIR)/silk/gain_quant.c \
	$(DIR)/silk/interpolate.c \
	$(DIR)/silk/LP_variable_cutoff.c \
	$(DIR)/silk/NLSF_decode.c \
	$(DIR)/silk/NSQ.c \
	$(DIR)/silk/NSQ_del_dec.c \
	$(DIR)/silk/PLC.c \
	$(DIR)/silk/shell_coder.c \
	$(DIR)/silk/tables_gain.c \
	$(DIR)/silk/tables_LTP.c \
	$(DIR)/silk/tables_NLSF_CB_NB_MB.c \
	$(DIR)/silk/tables_NLSF_CB_WB.c \
	$(DIR)/silk/tables_other.c \
	$(DIR)/silk/tables_pitch_lag.c \
	$(DIR)/silk/tables_pulses_per_block.c \
	$(DIR)/silk/VAD.c \
	$(DIR)/silk/control_audio_bandwidth.c \
	$(DIR)/silk/quant_LTP_gains.c \
	$(DIR)/silk/VQ_WMat_EC.c \
	$(DIR)/silk/HP_variable_cutoff.c \
	$(DIR)/silk/NLSF_encode.c \
	$(DIR)/silk/NLSF_VQ.c \
	$(DIR)/silk/NLSF_unpack.c \
	$(DIR)/silk/NLSF_del_dec_quant.c \
	$(DIR)/silk/process_NLSFs.c \
	$(DIR)/silk/stereo_LR_to_MS.c \
	$(DIR)/silk/stereo_MS_to_LR.c \
	$(DIR)/silk/check_control_input.c \
	$(DIR)/silk/control_SNR.c \
	$(DIR)/silk/init_encoder.c \
	$(DIR)/silk/control_codec.c \
	$(DIR)/silk/A2NLSF.c \
	$(DIR)/silk/ana_filt_bank_1.c \
	$(DIR)/silk/biquad_alt.c \
	$(DIR)/silk/bwexpander_32.c \
	$(DIR)/silk/bwexpander.c \
	$(DIR)/silk/debug.c \
	$(DIR)/silk/decode_pitch.c \
	$(DIR)/silk/inner_prod_aligned.c \
	$(DIR)/silk/lin2log.c \
	$(DIR)/silk/log2lin.c \
	$(DIR)/silk/LPC_analysis_filter.c \
	$(DIR)/silk/LPC_inv_pred_gain.c \
	$(DIR)/silk/table_LSF_cos.c \
	$(DIR)/silk/NLSF2A.c \
	$(DIR)/silk/NLSF_stabilize.c \
	$(DIR)/silk/NLSF_VQ_weights_laroia.c \
	$(DIR)/silk/pitch_est_tables.c \
	$(DIR)/silk/resampler.c \
	$(DIR)/silk/resampler_down2_3.c \
	$(DIR)/silk/resampler_down2.c \
	$(DIR)/silk/resampler_private_AR2.c \
	$(DIR)/silk/resampler_private_down_FIR.c \
	$(DIR)/silk/resampler_private_IIR_FIR.c \
	$(DIR)/silk/resampler_private_up2_HQ.c \
	$(DIR)/silk/resampler_rom.c \
	$(DIR)/silk/sigm_Q15.c \
	$(DIR)/silk/sort.c \
	$(DIR)/silk/sum_sqr_shift.c \
	$(DIR)/silk/stereo_decode_pred.c \
	$(DIR)/silk/stereo_encode_pred.c \
	$(DIR)/silk/stereo_find_predictor.c \
	$(DIR)/silk/stereo_quant_pred.c \
	$(DIR)/silk/LPC_fit.c

SILK_SOURCES_FLOAT := \
	$(DIR)/silk/float/apply_sine_window_FLP.c \
	$(DIR)/silk/float/corrMatrix_FLP.c \
	$(DIR)/silk/float/encode_frame_FLP.c \
	$(DIR)/silk/float/find_LPC_FLP.c \
	$(DIR)/silk/float/find_LTP_FLP.c \
	$(DIR)/silk/float/find_pitch_lags_FLP.c \
	$(DIR)/silk/float/find_pred_coefs_FLP.c \
	$(DIR)/silk/float/LPC_analysis_filter_FLP.c \
	$(DIR)/silk/float/LTP_analysis_filter_FLP.c \
	$(DIR)/silk/float/LTP_scale_ctrl_FLP.c \
	$(DIR)/silk/float/noise_shape_analysis_FLP.c \
	$(DIR)/silk/float/process_gains_FLP.c \
	$(DIR)/silk/float/regularize_correlations_FLP.c \
	$(DIR)/silk/float/residual_energy_FLP.c \
	$(DIR)/silk/float/warped_autocorrelation_FLP.c \
	$(DIR)/silk/float/wrappers_FLP.c \
	$(DIR)/silk/float/autocorrelation_FLP.c \
	$(DIR)/silk/float/burg_modified_FLP.c \
	$(DIR)/silk/float/bwexpander_FLP.c \
	$(DIR)/silk/float/energy_FLP.c \
	$(DIR)/silk/float/inner_product_FLP.c \
	$(DIR)/silk/float/k2a_FLP.c \
	$(DIR)/silk/float/LPC_inv_pred_gain_FLP.c \
	$(DIR)/silk/float/pitch_analysis_core_FLP.c \
	$(DIR)/silk/float/scale_copy_vector_FLP.c \
	$(DIR)/silk/float/scale_vector_FLP.c \
	$(DIR)/silk/float/schur_FLP.c \
	$(DIR)/silk/float/sort_FLP.c

OPUS_SOURCES := \
	$(DIR)/src/opus.c \
	$(DIR)/src/opus_decoder.c \
	$(DIR)/src/opus_encoder.c \
	$(DIR)/src/opus_multistream.c \
	$(DIR)/src/opus_multistream_encoder.c \
	$(DIR)/src/opus_multistream_decoder.c \
	$(DIR)/src/repacketizer.c \
	$(DIR)/src/opus_projection_encoder.c \
	$(DIR)/src/opus_projection_decoder.c \
	$(DIR)/src/mapping_matrix.c

OPUS_SOURCES_FLOAT := \
	$(DIR)/src/analysis.c \
	$(DIR)/src/mlp.c \
	$(DIR)/src/mlp_data.c

CPUFAM := x86
ifeq "$(CPU)" "arm64"
	CPUFAM := arm
else ifeq "$(CPU)" "arm"
	CPUFAM := arm
endif

ifeq "$(CPUFAM)" "x86"

	CFLAGS_SSE := -msse -msse2 -msse4.1

	CELT_SOURCES_SSE := \
		$(DIR)/celt/x86/x86cpu.c \
		$(DIR)/celt/x86/x86_celt_map.c \
		$(DIR)/celt/x86/pitch_sse.c

	CELT_SOURCES_SSE2 := \
		$(DIR)/celt/x86/pitch_sse2.c \
		$(DIR)/celt/x86/vq_sse2.c

	CELT_SOURCES_SSE4_1 := \
		$(DIR)/celt/x86/celt_lpc_sse4_1.c \
		$(DIR)/celt/x86/pitch_sse4_1.c

	SILK_SOURCES_SSE4_1 :=  \
		$(DIR)/silk/x86/NSQ_sse4_1.c \
		$(DIR)/silk/x86/NSQ_del_dec_sse4_1.c \
		$(DIR)/silk/x86/x86_silk_map.c \
		$(DIR)/silk/x86/VAD_sse4_1.c \
		$(DIR)/silk/x86/VQ_WMat_EC_sse4_1.c

	CELT_SOURCES += $(CELT_SOURCES_SSE) $(CELT_SOURCES_SSE2) $(CELT_SOURCES_SSE4_1)
	SILK_SOURCES += $(SILK_SOURCES_SSE4_1)

else ifeq "$(CPUFAM)" "arm"

	CELT_SOURCES_ARM := \
		$(DIR)/celt/arm/armcpu.c \
		$(DIR)/celt/arm/arm_celt_map.c

	CELT_SOURCES_ARM_NEON_INTR := \
		$(DIR)/celt/arm/celt_neon_intr.c \
		$(DIR)/celt/arm/pitch_neon_intr.c

	SILK_SOURCES_ARM_NEON_INTR := \
		$(DIR)/silk/arm/arm_silk_map.c \
		$(DIR)/silk/arm/biquad_alt_neon_intr.c \
		$(DIR)/silk/arm/LPC_inv_pred_gain_neon_intr.c \
		$(DIR)/silk/arm/NSQ_del_dec_neon_intr.c \
		$(DIR)/silk/arm/NSQ_neon.c

	CELT_SOURCES += $(CELT_SOURCES_ARM) $(CELT_SOURCES_ARM_NEON_INTR)
	SILK_SOURCES += $(SILK_SOURCES_ARM_NEON_INTR)

endif

SILK_SOURCES += $(SILK_SOURCES_FLOAT)
OPUS_SOURCES += $(OPUS_SOURCES_FLOAT)

SRC := $(CELT_SOURCES) $(SILK_SOURCES) $(OPUS_SOURCES)
OBJ := opus-phi.o $(SRC:.c=.o)

%.o: %.c
	$(C) $(CFLAGS) $(CFLAGS_SSE) $< -o $@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

clean:
	$(RM) $(OBJ) $(DIR)
