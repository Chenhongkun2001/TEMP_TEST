/*
 * Embedded Linux library
 * Copyright (C) 2011-2014  Intel Corporation
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


#if(1)
//typedef void (*l_dbus_destroy_func_t) (void *user_data);
void dbus_debug_destroy(void*user_data)
{
	DBG_LOG_INFO("debug destroy is called.");
		l_info("%s hello there.", __FUNCTION__);
}
#endif



static void do_debug(const char *str, void *user_data)
{
	const char *prefix = user_data;

	l_info("%s%s", prefix, str);
}

static void signal_handler(uint32_t signo, void *user_data)
{
	switch (signo) {
	case SIGINT:
	case SIGTERM:
		l_info("Terminate");
		l_main_quit();
		break;
	}
}

static void request_name_callback(struct l_dbus *dbus, bool success,
					bool queued, void *user_data)
{
	l_info("request name result=%s",
		success ? (queued ? "queued" : "success") : "failed");
	#if(1)
		if(user_data != NULL)
		{
			DBG_LOG_INFO("the user_data is %d",*((int*)user_data));
		}
	#endif
}

static void ready_callback(void *user_data)
{
	l_info("ready");
}

static void disconnect_callback(void *user_data)
{
	l_main_quit();
}


#if(1)
	static void disconnect_callback_destroy(void *user_data)
	{
		DBG_LOG_INFO("This is a test");
	}
#endif

struct test_data {
	char *string;
	uint32_t integer;
	double fval;
};

static void test_data_destroy(void *data)
{
	struct test_data *test = data;

	l_free(test->string);
	l_free(test);
}

static struct l_dbus_message *test_method_call(struct l_dbus *dbus,
						struct l_dbus_message *message,
						void *user_data)
{
	struct l_dbus_message *reply;

	l_info("Method Call");

	reply = l_dbus_message_new_method_return(message);
	l_dbus_message_set_arguments(reply, "");

	return reply;
}

static bool test_string_getter(struct l_dbus *dbus,
				struct l_dbus_message *message,
				struct l_dbus_message_builder *builder,
				void *user_data)
{
	struct test_data *test = user_data;

	l_dbus_message_builder_append_basic(builder, 's', test->string);

	return true;
}

static struct l_dbus_message *test_string_setter(struct l_dbus *dbus,
					struct l_dbus_message *message,
					struct l_dbus_message_iter *new_value,
					l_dbus_property_complete_cb_t complete,
					void *user_data)
{
	const char *strvalue;
	struct test_data *test = user_data;

	if (!l_dbus_message_iter_get_variant(new_value, "s", &strvalue))
		return l_dbus_message_new_error(message,
						"org.test.InvalidArguments",
						"String value expected");

	l_info("New String value: %s", strvalue);
	l_free(test->string);
	test->string = l_strdup(strvalue);

	complete(dbus, message, NULL);

	return NULL;
}

static bool test_int_getter(struct l_dbus *dbus,
				struct l_dbus_message *message,
				struct l_dbus_message_builder *builder,
				void *user_data)
{
	struct test_data *test = user_data;

	l_dbus_message_builder_append_basic(builder, 'u', &test->integer);

	#if(1)
		test->integer++;
		//l_dbus_property_changed(struct l_dbus * dbus, const char * path, const char * interface, const char * property)
		l_dbus_property_changed( dbus, "/test", "org.test", "Integer");
	#endif
	
	return true;
}

static struct l_dbus_message *test_int_setter(struct l_dbus *dbus,
				struct l_dbus_message *message,
				struct l_dbus_message_iter *new_value,
				l_dbus_property_complete_cb_t complete,
				void *user_data)
{
	uint32_t u;
	struct test_data *test = user_data;

	if (!l_dbus_message_iter_get_variant(new_value, "u", &u))
		return l_dbus_message_new_error(message,
						"org.test.InvalidArguments",
						"Integer value expected");

	l_info("New Integer value: %u", u);
	test->integer = u;

	complete(dbus, message, NULL);

	return NULL;
}

#if(1)
	//typedef bool (*p) (struct l_dbus *,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data);
	bool test_property_double_getter(struct l_dbus *ptbus,struct l_dbus_message *message,struct l_dbus_message_builder *builder,void *user_data)
	{
		struct test_data *test = user_data;
		l_dbus_message_builder_append_basic(builder, 'd', &test->fval);
		return true;
	}


	static struct l_dbus_message *test_property_double_setter(struct l_dbus *dbus,
				struct l_dbus_message *message,
				struct l_dbus_message_iter *new_value,
				l_dbus_property_complete_cb_t complete,
				void *user_data)
	{
		double dval;
		struct test_data *test = user_data;

		if (!l_dbus_message_iter_get_variant(new_value, "d", &dval))
			return l_dbus_message_new_error(message,
						"org.test.InvalidArguments",
						"Integer value expected");

		l_info("New Integer value: %f", dval);
		DBG_LOG_INFO("new double value %f", dval);

		test->fval = dval;

		complete(dbus, message, NULL);

		return NULL;
	}
	
#endif

static void setup_test_interface(struct l_dbus_interface *interface)
{
	l_dbus_interface_method(interface, "MethodCall", 0,
				test_method_call, "", "");

	l_dbus_interface_property(interface, "String", 0, "s",
					test_string_getter, test_string_setter);
	l_dbus_interface_property(interface, "Integer", 0, "u",
					test_int_getter, test_int_setter);
	#if(1)
		l_dbus_interface_property( interface, "Double", 0, "d", test_property_double_getter, test_property_double_setter);
	#endif
}

int main(int argc, char *argv[])
{
	struct l_dbus *dbus;
	struct test_data *test;

	if (!l_main_init())
		return -1;

	#if(0)
		l_log_set_stderr();
	#else
		l_log_set_syslog();
	#endif

	dbus = l_dbus_new_default(L_DBUS_SESSION_BUS);
	#if(0)
		l_dbus_set_debug(dbus, do_debug, "[DBUS] ", NULL);
	#else		
		l_dbus_set_debug(dbus, do_debug, "[DBUS-S] ", dbus_debug_destroy);
	#endif
	l_dbus_set_ready_handler(dbus, ready_callback, dbus, NULL);

	#if(0)
		l_dbus_set_disconnect_handler(dbus, disconnect_callback, NULL, NULL);
	#else
		l_dbus_set_disconnect_handler(dbus, disconnect_callback, NULL, disconnect_callback_destroy);				
	#endif

	#if(0)
	l_dbus_name_acquire(dbus, "org.test", false, false, false,
				request_name_callback, NULL);
	#else
		int tpval = 666;
	l_dbus_name_acquire(dbus, "org.test", false, false, false,
				request_name_callback, &tpval);	
	#endif

	if (!l_dbus_object_manager_enable(dbus, "/")) {
		l_info("Unable to enable Object Manager");
		goto cleanup;
	}

	test = l_new(struct test_data, 1);
	test->string = l_strdup("Default");
	test->integer = 42;
	test->fval = 666.666;
	

	if (!l_dbus_register_interface(dbus, "org.test", setup_test_interface,
					test_data_destroy, true)) {
		l_info("Unable to register interface");
		test_data_destroy(test);
		goto cleanup;
	}

	if (!l_dbus_object_add_interface(dbus, "/test", "org.test", test)) {
		l_info("Unable to instantiate interface");
		test_data_destroy(test);
		goto cleanup;
	}

	if (!l_dbus_object_add_interface(dbus, "/test",
					L_DBUS_INTERFACE_PROPERTIES, NULL)) {
		l_info("Unable to instantiate the properties interface");
		test_data_destroy(test);
		goto cleanup;
	}

	l_main_run_with_signal(signal_handler, NULL);

	l_dbus_unregister_object(dbus, "/test");

cleanup:
	l_dbus_destroy(dbus);

	l_main_exit();

	DBG_LOG_INFO("the end of app");

	return 0;
}






