/** phiola: audio format helper functions
2024, Simon Zolin */

static const char phi_af_names[][8] = {
	"float32",
	"float64",
	"int16",
	"int24",
	"int24-4",
	"int32",
	"int8",
	"uint8",
};
static const ushort phi_af_values[] = {
	PHI_PCM_FLOAT32,
	PHI_PCM_FLOAT64,
	PHI_PCM_16,
	PHI_PCM_24,
	PHI_PCM_24_4,
	PHI_PCM_32,
	PHI_PCM_8,
	PHI_PCM_U8,
};

/** Convert audio format numeric value to string */
static inline const char* phi_af_name(uint fmt)
{
	int r = ffarrint16_find(phi_af_values, FF_COUNT(phi_af_values), fmt);
	if (r < 0)
		return "";
	return phi_af_names[r];
}

/** Convert audio format string to numeric value */
static inline int phi_af_val(ffstr s)
{
	int r = ffcharr_findsorted(phi_af_names, FF_COUNT(phi_af_names), sizeof(phi_af_names[0]), s.ptr, s.len);
	if (r < 0)
		return -1;
	return phi_af_values[r];
}
