/** phiola: HTTP client filters */

#include <http-client/resolve.h>
#include <http-client/connect.h>
#include <http-client/request.h>
#include <http-client/request-send.h>
#include <http-client/response-receive.h>
#include <http-client/response.h>
#include <http-client/transfer.h>
#include <http-client/redirect.h>
#include <net/http-bridge.h>


static int phi_output_open(nml_http_client *c)
{
	struct phi_http_data d = {
		.code = c->response.code,
		.status = range16_tostr(&c->response.status, c->response.base),
		.ct = range16_tostr(&c->response.content_type, c->response.base),
	};

	ffstr h = range16_tostr(&c->response.headers, c->response.base), name = {}, val = {};
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

static const struct nml_filter nml_phi_bridge = {
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
	&nml_filter_redir,
	&nml_phi_bridge,
	NULL
};

#ifndef PHI_HTTP_NO_SSL
#include <http-client/ssl.h>
const struct nml_filter *hc_ssl_filters[] = {
	&nml_filter_resolve,
	&nml_filter_connect,
	&nml_filter_ssl_recv,
	&nml_filter_ssl_handshake,
	&nml_filter_ssl_send,
	&nml_filter_http_cl_request,
	&nml_filter_ssl_req,
	&nml_filter_ssl_send,
	&nml_filter_ssl_recv,
	&nml_filter_ssl_resp,
	&nml_filter_resp,
	&nml_filter_http_cl_transfer,
	&nml_filter_redir,
	&nml_phi_bridge,
	NULL
};
#endif
