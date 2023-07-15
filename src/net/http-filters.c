/** phiola: HTTP client filters */

#include <http-client/resolve.h>
#include <http-client/connect.h>
#include <http-client/io.h>
#include <http-client/request.h>
#include <http-client/request-send.h>
#include <http-client/response-receive.h>
#include <http-client/response.h>
#include <http-client/transfer.h>

extern int phi_hc_resp(void *ctx, uint code, ffstr status, ffstr ct);
extern int phi_hc_data(void *ctx, ffstr data, uint flags);

static int phi_output_open(nml_http_client *c)
{
	ffstr status = range16_tostr(&c->response.status, c->recv.buf.ptr);
	ffstr ct = range16_tostr(&c->response.content_type, c->recv.buf.ptr);
	return phi_hc_resp(c->conf->opaque, c->response.code, status, ct);
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
