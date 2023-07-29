/** netmill--phiola.http bridge interface */

#include <ffbase/string.h>

struct phi_http_data {
	uint code;
	ffstr status;
	ffstr ct;
	uint icy_meta_interval;
};

extern int phi_hc_resp(void *ctx, struct phi_http_data *d);
extern int phi_hc_data(void *ctx, ffstr data, uint flags);
