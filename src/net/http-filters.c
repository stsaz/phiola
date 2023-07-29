/** phiola: HTTP client filters */

#include <http-client/resolve.h>
#include <http-client/connect.h>
#include <http-client/io.h>
#include <http-client/request.h>
#include <http-client/request-send.h>
#include <http-client/response-receive.h>
#include <http-client/response.h>
#include <http-client/transfer.h>
#include <net/http-bridge.h>


static int phi_output_open(nml_http_client *c)
{
	struct phi_http_data d = {
		.code = c->response.code,
		.status = range16_tostr(&c->response.status, c->recv.buf.ptr),
		.ct = range16_tostr(&c->response.content_type, c->recv.buf.ptr),
	};

	ffstr h = range16_tostr(&c->response.headers, c->recv.buf.ptr), name = {}, val = {};
	for (;;) {
		int r = http_hdr_parse(h, &name, &val);
		if (r <= 2)
			break;
		ffstr_shift(&h, r);

		if (ffstr_ieqz(&name, "icy-metaint")) {
			(void)ffstr_to_uint32(&val, &d.icy_meta_interval);
		}
	}

	return phi_hc_resp(c->conf->opaque, &d);
}

static int phi_output_process(nml_http_client *c)
{
	return phi_hc_data(c->conf->opaque, c->input, c->resp_complete);
}

const struct nml_filter nml_phi_bridge = {
	(void*)phi_output_open, NULL, (void*)phi_output_process,
	"phiola-output"
};


const struct nml_filter *hc_filters[] = {
	&nml_filter_resolve,
	&nml_filter_connect,
	&nml_filter_http_cl_request,
	&nml_filter_http_cl_send,
	&nml_filter_recv,
	&nml_filter_resp,
	&nml_filter_http_cl_transfer,
	&nml_phi_bridge,
	NULL
};
