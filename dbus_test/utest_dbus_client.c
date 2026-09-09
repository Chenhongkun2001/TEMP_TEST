/*
 * Embedded Linux library
 * Copyright (C) 2011-2016  Intel Corporation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>

#include <ell/ell.h>

#ifndef DBG_LOG_INFO 
	#define DBG_LOG_INFO(__fmt,...) printf("INFO:%s-L%d| "__fmt"\r\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

static void do_debug(const char *str, void *user_data)
{
	const char *prefix = user_data;

	l_info("%s%s", prefix, str);
}

static void signal_handler(uint32_t signo, void *user_data)
{
	DBG_LOG_INFO("signal %d received", signo);
	
	switch (signo) {
	case SIGINT:
	case SIGTERM:
		l_info("Terminate");
		DBG_LOG_INFO("Terminate");
		l_main_quit();
		break;
	}
}

static void ready_callback(void *user_data)
{
	l_info("ready");
	DBG_LOG_INFO("connection is ready");
}

static void disconnect_callback(void *user_data)
{
	DBG_LOG_INFO("D-BUS disconnect");
	l_main_quit();
}

static void iwd_service_appeared(struct l_dbus *dbus, void *user_data)
{
	l_info("Service appeared");
	DBG_LOG_INFO("Service appeared");
}

static void iwd_service_disappeared(struct l_dbus *dbus, void *user_data)
{
	l_info("Service disappeared");
	DBG_LOG_INFO("Service disappeared");
}

static void bluez_client_connected(struct l_dbus *dbus, void *user_data)
{
	l_info("client connected");
	DBG_LOG_INFO("Client connected");
}

static void bluez_client_disconnected(struct l_dbus *dbus, void *user_data)
{
	l_info("client disconnected");
	DBG_LOG_INFO("Client disconnected");
}

static void bluez_client_ready(struct l_dbus_client *client, void *user_data)
{
	l_info("client ready");
	DBG_LOG_INFO("Client Ready");
}

static void proxy_added(struct l_dbus_proxy *proxy, void *user_data)
{
	const char *interface = l_dbus_proxy_get_interface(proxy);
	const char *path = l_dbus_proxy_get_path(proxy);

	l_info("proxy added: %s %s", path, interface);
	DBG_LOG_INFO("proxy added %s ,%s", path, interface);
		
	if (!strcmp(interface, "org.test"))
	{
		char *str;

		if (!l_dbus_proxy_get_property(proxy, "String", "s", &str))
			return;

		l_info("  the string : %s", str);
		DBG_LOG_INFO(" the string : %s", str);
	}
}

static void proxy_removed(struct l_dbus_proxy *proxy, void *user_data)
{
	l_info("proxy removed: %s %s", l_dbus_proxy_get_path(proxy),
					l_dbus_proxy_get_interface(proxy));
	DBG_LOG_INFO("proxy removed: %s , %s", l_dbus_proxy_get_path(proxy),l_dbus_proxy_get_interface( proxy));
}

static void property_changed(struct l_dbus_proxy *proxy, const char *name,
				struct l_dbus_message *msg, void *user_data)
{
	l_info("property changed: %s (%s %s)", name,
					l_dbus_proxy_get_path(proxy),
					l_dbus_proxy_get_interface(proxy));

	DBG_LOG_INFO("property changed: %s (%s , %s)", name, l_dbus_proxy_get_path( proxy), l_dbus_proxy_get_interface(proxy));	

	if (!strcmp(name, "Address")) {
		char *str;

		if (!l_dbus_message_get_arguments(msg, "s", &str)) {
			return;
		}

		l_info("   Address: %s", str);
		DBG_LOG_INFO("Address : %s", str);
	}
	else if(!strcmp(name, "Integer"))
	{
		uint32_t tpu32 = 0;
		if(!l_dbus_message_get_arguments( msg, "u", &tpu32))
		{
			DBG_LOG_INFO("fail to get the argument");
			return;
		}
		DBG_LOG_INFO("the new property value is : %d", tpu32);
	}
}

int main(int argc, char *argv[])
{
	struct l_dbus_client *client;
	struct l_dbus *dbus;
	uint32_t iwd_watch_id;


	if (!l_main_init())
		return -1;

	#if(0)
		l_log_set_stderr();
	#else
		l_log_set_syslog();
	#endif
	

	dbus = l_dbus_new_default(L_DBUS_SESSION_BUS);
	l_dbus_set_debug(dbus, do_debug, "[DBUS-CLIENT] ", NULL);
	l_dbus_set_ready_handler(dbus, ready_callback, dbus, NULL);
	l_dbus_set_disconnect_handler(dbus, disconnect_callback, NULL, NULL);

	/* service basic watch */
	iwd_watch_id = l_dbus_add_service_watch(dbus, "net.connman.iwd",
						iwd_service_appeared,
						iwd_service_disappeared,
						NULL, NULL);

	/* proxy example */
	client = l_dbus_client_new(dbus, "org.test", "/test");

	l_dbus_client_set_connect_handler(client, bluez_client_connected, NULL,
									NULL);
	l_dbus_client_set_disconnect_handler(client, bluez_client_disconnected,
								NULL, NULL);

	l_dbus_client_set_proxy_handlers(client, proxy_added, proxy_removed,
							property_changed, NULL, NULL);

	l_dbus_client_set_ready_handler(client, bluez_client_ready, NULL, NULL);


	l_main_run_with_signal(signal_handler, NULL);

	l_dbus_remove_watch(dbus, iwd_watch_id);

	l_dbus_client_destroy(client);

	l_dbus_destroy(dbus);

	l_main_exit();

	return 0;
}









