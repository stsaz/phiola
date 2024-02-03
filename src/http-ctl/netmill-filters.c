/** phiola: HTTP server filters for netmill
2023, Simon Zolin */

#include <http-server/request-receive.h>
#include <http-server/request.h>
#include <http-server/virtspace.h>
#include <http-server/index.h>
#include <http-server/file.h>
#include <http-server/error.h>
#include <http-server/transfer.h>
#include <http-server/response.h>
#include <http-server/response-send.h>

const struct nml_filter* ah_filters[] = {
	&nml_filter_receive,
	&nml_filter_request,
	&nml_filter_virtspace,
	&nml_filter_index,
	&nml_filter_file,
	&nml_filter_error,
	&nml_filter_transfer,
	&nml_filter_response,
	&nml_filter_send,
	NULL
};
