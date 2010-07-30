/*
 * idbus.c
 * interface to dbus
 *
 * Copyright (c) 2010 Jan Seiffert
 *
 * This file is part of g2cd.
 *
 * g2cd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * g2cd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with g2cd.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id: $
 */

#include "idbus.h"
#define G2CD_DBUS_SERVICE "org.g2cd"
#define G2CD_DBUS_PATH "/org/g2cd"

#ifndef HAVE_DBUS
# include "lib/log_facility.h"
bool idbus_init(void)
{
	/* nop */
	logg(LOGF_NOTICE, "Warning: you wanted dbus, but it's not compiled in\n");
	return true;
}
#else
# include <stdlib.h>
# include <dbus/dbus.h>
# include "G2MainServer.h"
# include "G2ConRegistry.h"
# include "timeout.h"
# include "lib/other.h"
# include "lib/my_epoll.h"
# include "lib/log_facility.h"
# include "lib/my_bitopsm.h"

static DBusConnection *idbus_connection;

static void handle_watch(struct sock_com *s, short revents)
{
	struct DBusWatch *w = s->data;
	unsigned int flags = 0;

	if(!w)
		return;
	if(!dbus_watch_get_enabled(w)) {
		s->enabled = false;
		return;
	}

	flags |= revents & POLLIN  ? DBUS_WATCH_READABLE : 0;
	flags |= revents & POLLOUT ? DBUS_WATCH_WRITABLE : 0;
	flags |= revents & POLLERR ? DBUS_WATCH_ERROR    : 0;
	flags |= revents & POLLHUP ? DBUS_WATCH_HANGUP   : 0;
	if(flags)
		dbus_watch_handle(w, flags);
	if(idbus_connection)
	{
		dbus_connection_ref(idbus_connection);
		while(DBUS_DISPATCH_DATA_REMAINS == dbus_connection_dispatch(idbus_connection)) {
			/* nop */;
		}
		dbus_connection_unref(idbus_connection);
	}
}

static dbus_bool_t add_watch(DBusWatch *watch, void *data GCC_ATTR_UNUSED_PARAM)
{
	struct sock_com *s;
	unsigned int flags;
	int fd;
	short events = 0;

	fd = dbus_watch_get_unix_fd(watch);
	flags = dbus_watch_get_flags(watch);

	events |= flags & DBUS_WATCH_READABLE ? POLLIN  : 0;
	events |= flags & DBUS_WATCH_WRITABLE ? POLLOUT : 0;

	logg_develd("adding watch: %p %#x %s\n", watch, flags, dbus_watch_get_enabled(watch) ? "enabled" : "disabled");
	s = sock_com_add_fd(handle_watch, watch, fd, events, dbus_watch_get_enabled(watch));
	if(s) {
		dbus_watch_set_data(watch, s, free);
		return TRUE;
	} else
		return FALSE;
}

static void toggle_watch(DBusWatch *watch, void *data GCC_ATTR_UNUSED_PARAM)
{
	struct sock_com *s;

	logg_develd("want to toggle watch: %p %i %s\n", watch,
	            dbus_watch_get_unix_fd(watch),
	            dbus_watch_get_enabled(watch) ? "enabled" : "disabled");
	s = dbus_watch_get_data(watch);
	if(!s)
		return;

	logg_develd("toggling watch: %p %#x %s\n", watch,
	            dbus_watch_get_flags(watch),
	            dbus_watch_get_enabled(watch) ? "enabled" : "disabled");
	s->enabled = dbus_watch_get_enabled(watch);
}

static void remove_watch(DBusWatch *watch, void *data GCC_ATTR_UNUSED_PARAM)
{
	struct sock_com *s;

	logg_develd("want to remove watch: %p\n", watch);
	s = dbus_watch_get_data(watch);
	if(!s)
		return;

	logg_develd("removing watch: %p %p\n", watch, s);
	sock_com_delete(s);
}

static int handle_timeout(void *data)
{
	DBusTimeout *timeout = data;
	int ret_val = 0;

	if(!data)
		return ret_val;

	if(!dbus_timeout_get_enabled(timeout))
		return 0;

	dbus_timeout_handle(timeout);
	if(idbus_connection)
	{
		dbus_connection_ref(idbus_connection);
		while(DBUS_DISPATCH_DATA_REMAINS == dbus_connection_dispatch(idbus_connection)) {
			/* nop */;
		}
		dbus_connection_unref(idbus_connection);
	}

	if(!dbus_timeout_get_enabled(timeout))
		return 0;

	ret_val = dbus_timeout_get_interval(timeout);
	ret_val = DIV_ROUNDUP(ret_val, 100);
	return ret_val;
}

static dbus_bool_t add_timeout(DBusTimeout *timeout, void *data GCC_ATTR_UNUSED_PARAM)
{
	struct timeout *t = malloc(sizeof(*t));

	if(!t)
		return FALSE;

	INIT_TIMEOUT(t);
	t->data = timeout;
	t->fun = handle_timeout;
	dbus_timeout_set_data(timeout, t, free);

	if(dbus_timeout_get_enabled(timeout)) {
		int val = dbus_timeout_get_interval(timeout);
		int nval = DIV_ROUNDUP(val, 100);
		logg_develd_old("adding a timeout: %p %s %i %i\n", timeout,
		                dbus_timeout_get_enabled(timeout) ? "enabled" : "disabled",
		                val, nval);
		return timeout_add(t, nval);
	}
	logg_develd_old("adding a timeout: %p %s\n", timeout,
	                dbus_timeout_get_enabled(timeout) ? "enabled" : "disabled");
	return TRUE;
}

static void toggle_timeout(DBusTimeout *timeout, void *data GCC_ATTR_UNUSED_PARAM)
{
	struct timeout *t = dbus_timeout_get_data(timeout);
	int val, nval;

	if(!t)
		return;

	if(!dbus_timeout_get_enabled(timeout)) {
		logg_develd("toggling a timeout: %p %s\n", timeout,
		            dbus_timeout_get_enabled(timeout) ? "enabled" : "disabled");
		timeout_cancel(t);
		return;
	}
	val = dbus_timeout_get_interval(timeout);
	nval = DIV_ROUNDUP(val, 100);
	logg_develd("adding a timeout: %p %s %i %i\n", timeout,
	           dbus_timeout_get_enabled(timeout) ? "enabled" : "disabled",
	           val, nval);
	if(!timeout_advance(t, nval))
		timeout_add(t, nval);
}

static void remove_timeout(DBusTimeout *timeout, void *data GCC_ATTR_UNUSED_PARAM)
{
	struct timeout *t = dbus_timeout_get_data(timeout);

	if(!t)
		return;
	timeout_cancel(t);
}

static const char *introspection_xml =
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node name=\"" G2CD_DBUS_PATH "\">\n"
"	<interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"		<method name=\"Introspect\">\n"
"			<arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
"		</method>\n"
"	</interface>\n"
"	<interface name=\"" G2CD_DBUS_SERVICE "\">\n"
"		<method name=\"version\">\n"
"			<arg name=\"version\" direction=\"out\" type=\"" DBUS_TYPE_STRING_AS_STRING "\"/>\n"
"		</method>\n"
"		<method name=\"get_num_connections\">\n"
"			<arg name=\"get_num_connections\" direction=\"out\" type=\"" DBUS_TYPE_INT32_AS_STRING "\"/>\n"
"		</method>\n"
"		<method name=\"get_num_hubs\">\n"
"			<arg name=\"get_num_hubs\" direction=\"out\" type=\"" DBUS_TYPE_INT32_AS_STRING "\"/>\n"
"		</method>\n"
"		<method name=\"list_hubs\">\n"
"			<arg name=\"list_hubs\" direction=\"out\" type=\"" DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_STRING_AS_STRING "\"/>\n"
"		</method>\n"
"		<method name=\"list_cons\">\n"
"			<arg name=\"list_cons\" direction=\"out\" type=\"" DBUS_TYPE_ARRAY_AS_STRING DBUS_TYPE_STRING_AS_STRING "\"/>\n"
"		</method>\n"
"	</interface>\n"
"</node>\n";

// TODO: use one of our other tls buffers
 /*
  * The log buffer is not exported (better so?),
  * but would "fit", problem then: no logging while busy...
  * The zlib scratchpads are open for grab, but the
  * threads ending here (main, timeout) normally do not
  * have one (would be +-0, they gain a zpad for no
  * other use).
  */
static char tfmt_buff[4096];

static intptr_t dump_a_con(g2_connection_t *con, void *carg)
{
	DBusMessageIter *it = carg;
	const char *x_ptr = tfmt_buff;

	my_snprintf(tfmt_buff, sizeof(tfmt_buff), "%p#I\t-> %p#G %s d:%c l:%u\t\"%s\"",
	            &con->remote_host, con->guid, con->vendor_code, con->flags.dismissed ? 't' : 'f',
	            con->u.handler.leaf_count, con->uagent);
	dbus_message_iter_append_basic(it, DBUS_TYPE_STRING, &x_ptr);
	return 0;
}

static DBusHandlerResult message_handler(DBusConnection *con, DBusMessage *msg, void *udata GCC_ATTR_UNUSED_PARAM)
{
	if(dbus_message_is_method_call(msg, DBUS_INTERFACE_INTROSPECTABLE, "Introspect"))
	{
		DBusMessage *reply = dbus_message_new_method_return(msg);
		dbus_message_append_args(reply, DBUS_TYPE_STRING, &introspection_xml, DBUS_TYPE_INVALID);
		dbus_connection_send(con, reply, NULL);
		dbus_message_unref(reply);
	}
	else
	{
		const char *method = dbus_message_get_member(msg);
		if(strcmp(method, "version") == 0)
		{
			const char *v = OUR_VERSION;
			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_message_append_args(reply, DBUS_TYPE_STRING, &v, DBUS_TYPE_INVALID);
			dbus_connection_send(con, reply, NULL);
			dbus_message_unref(reply);
		}
		else if(strcmp(method, "get_num_connections") == 0)
		{
			int32_t cs = atomic_read(&server.status.act_connection_sum);
			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_message_append_args(reply, DBUS_TYPE_INT32, &cs, DBUS_TYPE_INVALID);
			dbus_connection_send(con, reply, NULL);
			dbus_message_unref(reply);
		}
		else if(strcmp(method, "get_num_hubs") == 0)
		{
			int32_t cs = atomic_read(&server.status.act_hub_sum);
			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_message_append_args(reply, DBUS_TYPE_INT32, &cs, DBUS_TYPE_INVALID);
			dbus_connection_send(con, reply, NULL);
			dbus_message_unref(reply);
		}
		else if(strcmp(method, "list_hubs") == 0)
		{
			DBusMessageIter it;
			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_message_iter_init_append(reply, &it);
			g2_conreg_all_hub(NULL, dump_a_con, &it);
			dbus_connection_send(con, reply, NULL);
			dbus_message_unref(reply);
		}
		else if(strcmp(method, "list_cons") == 0)
		{
			DBusMessageIter it;
			DBusMessage *reply = dbus_message_new_method_return(msg);
			dbus_message_iter_init_append(reply, &it);
			g2_conreg_all_con(dump_a_con, &it);
			dbus_connection_send(con, reply, NULL);
			dbus_message_unref(reply);
		}
		else {
			logg_develd("unknown dbus method called: \"%s\"\n", method);
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}

bool idbus_init(void)
{
	static const struct DBusObjectPathVTable bp_vtable = {.message_function = message_handler};
	DBusError dbus_error;

	if(!dbus_threads_init_default()) {
		logg(LOGF_NOTICE, "Warning, couldn't init dbus thread foo, but will continue\n");
		return true;
	}

	dbus_error_init(&dbus_error);
	idbus_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error);
	if(!idbus_connection) {
		logg(LOGF_NOTICE, "Warning, couldn't connect to dbus, but will continue: %s\n", dbus_error.message);
		return true;
	}

	dbus_connection_set_exit_on_disconnect(idbus_connection, FALSE);
	dbus_connection_set_watch_functions(idbus_connection, add_watch, remove_watch, toggle_watch,
	                                    NULL /* data */, NULL /* data free */);
	dbus_connection_set_timeout_functions(idbus_connection, add_timeout, remove_timeout, toggle_timeout,
	                                    NULL /* data */, NULL /* data free */);

	dbus_error_init(&dbus_error);
	dbus_bus_request_name(idbus_connection, G2CD_DBUS_SERVICE, 0, &dbus_error);
	if(dbus_error_is_set(&dbus_error)) {
		logg(LOGF_ERR, "Couldn't request our g2cd dbus service \""G2CD_DBUS_SERVICE"\": %s\n",
		     dbus_error.message);
		return false;
	}

	dbus_error_init(&dbus_error);
	if(!dbus_connection_try_register_object_path(idbus_connection, G2CD_DBUS_PATH, &bp_vtable,
	                                             NULL /* user data */, &dbus_error)) {
		logg(LOGF_ERR, "Couldn't register our g2cd dbus path \""G2CD_DBUS_PATH"\": %s\n", dbus_error.message);
		return false;
	}

	return true;
}
#endif

/*@unused@*/
static char const rcsid_idbus[] GCC_ATTR_USED_VAR = "$Id: $";
/* EOF */
