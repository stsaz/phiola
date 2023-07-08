/** Disable system sleep timer on Linux via D-BUS.
2020, Simon Zolin
*/

/*
ffps_systimer_open ffps_systimer_close
ffps_systimer
*/

#include <FFOS/process.h>
#include <FFOS/error.h>
#include <dbus/dbus.h>

struct ffps_systimer {
	const char *appname;
	const char *reason;
	void *conn;
	void *reply;
};

#define FDSS_DEST  "org.freedesktop.ScreenSaver"
#define FDSS_PATH  "/ScreenSaver"
#define FDSS_INHIBIT  "Inhibit"
#define FDSS_UNINHIBIT  "UnInhibit"
#define SEND_TIMEOUT  (-1)

/** Open D-BUS connection. */
static inline int ffps_systimer_open(struct ffps_systimer *c)
{
	// DBusError err;
	// dbus_error_init(&err);
	c->conn = dbus_bus_get_private(DBUS_BUS_SESSION, NULL);
	if (c->conn == NULL) {
		// err.message
		// dbus_error_free(&err);
		return 1;
	}
	return 0;
}

/** Close D-BUS connection. */
static inline void ffps_systimer_close(struct ffps_systimer *c)
{
	if (c->reply != NULL) {
		dbus_pending_call_cancel(c->reply);
		dbus_pending_call_unref(c->reply);
		c->reply = NULL;
	}
	if (c->conn != NULL) {
		dbus_connection_close(c->conn);
		dbus_connection_unref(c->conn);
		c->conn = NULL;
	}
}

/**
flags: 1:inhibit screen saver;  0:uninhibit screen saver */
static inline int ffps_systimer(struct ffps_systimer *c, uint flags)
{
	int rc = -1;
	DBusMessage *m = NULL, *mreply = NULL;

	const char *method = (flags == 1) ? FDSS_INHIBIT : FDSS_UNINHIBIT;
	m = dbus_message_new_method_call(FDSS_DEST, FDSS_PATH, FDSS_DEST, method);
	if (m == NULL)
		goto end;

	if (c->conn == NULL) {
		if (0 != ffps_systimer_open(c))
			goto end;
	}

	if (flags == 0) {
		if (c->reply == NULL) {
			rc = 0;
			goto end;
		}
		dbus_pending_call_block(c->reply);
		mreply = dbus_pending_call_steal_reply(c->reply);
		dbus_pending_call_unref(c->reply);
		c->reply = NULL;
		if (mreply == NULL)
			goto end;

		uint cookie;
		if (!dbus_message_get_args(mreply, NULL, DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID))
			goto end;
		if (!dbus_message_append_args(m, DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID))
			goto end;
		if (!dbus_connection_send(c->conn, m, NULL))
			goto end;

		rc = 0;
		goto end;
	}

	if (c->reply != NULL) {
		rc = 0;
		goto end;
	}

	if (!dbus_message_append_args(m, DBUS_TYPE_STRING, &c->appname
		, DBUS_TYPE_STRING, &c->reason, DBUS_TYPE_INVALID))
		goto end;

	DBusPendingCall *reply = NULL;
	dbus_connection_send_with_reply(c->conn, m, &reply, SEND_TIMEOUT);
	if (reply == NULL)
		goto end;
	c->reply = reply;

	rc = 0;

end:
	if (m != NULL)
		dbus_message_unref(m);
	if (mreply != NULL)
		dbus_message_unref(mreply);
	return rc;
}
