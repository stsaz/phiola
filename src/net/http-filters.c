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

static const nml_http_cl_component nml_http_cl_phi_bridge = {
	phi_output_open, NULL, phi_output_process,
	"phiola-output"
};


const nml_http_cl_component *hc_chain[] = {
	&nml_http_cl_resolve,
	&nml_http_cl_connect,
	&nml_http_cl_request,
	&nml_http_cl_send,
	&nml_http_cl_recv,
	&nml_http_cl_response,
	&nml_http_cl_transfer,
	&nml_http_cl_redir,
	&nml_http_cl_phi_bridge,
	NULL
};

#ifndef PHI_HTTP_NO_SSL
#include <ssl/ssl.h>
#include <ssl/send.h>
#include <ssl/receive.h>
#include <ssl/handshake.h>
#include <ssl/request.h>
#include <ssl/response.h>
const nml_http_cl_component *hc_ssl_chain[] = {
	&nml_http_cl_resolve,
	&nml_http_cl_connect,
	&nml_http_cl_ssl_recv,
	&nml_http_cl_ssl_handshake,
	&nml_http_cl_ssl_send,
	&nml_http_cl_request,
	&nml_http_cl_ssl_req,
	&nml_http_cl_ssl_send,
	&nml_http_cl_ssl_recv,
	&nml_http_cl_ssl_resp,
	&nml_http_cl_response,
	&nml_http_cl_transfer,
	&nml_http_cl_redir,
	&nml_http_cl_phi_bridge,
	NULL
};
#endif
