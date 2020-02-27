/**
 * @file idevicelaunch.c
 * @brief Launch an app on device using its bundle id.
 * \internal
 *
 * Copyright (c) 2020 Demiurge Studios Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <libimobiledevice/debugserver.h>
#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/libimobiledevice.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* number of arguments expected: idevicelaunch start BUNDLEID */
#define IDR_ARGC 3

/* convenience types. */
typedef int idl_bool;
#define IDR_FALSE 0
#define IDR_TRUE 1

/* for inclusion in error reporting. */
static char* idl_process_name = NULL;

/* utility for error logging. */
#define IDR_ERR(fmt, ...) fprintf(stderr, "%s: " fmt "\n", idl_process_name, ##__VA_ARGS__)

/* error reporting, convert debugserver client response enum into string. */
static char* idl_ds_err_to_string(debugserver_error_t res) {
#	define CASE(v) DEBUGSERVER_E_##v: return #v
	switch (res) {
	case CASE(SUCCESS);
	case CASE(INVALID_ARG);
	case CASE(MUX_ERROR);
	case CASE(SSL_ERROR);
	case CASE(RESPONSE_ERROR);
	case CASE(TIMEOUT);
	case CASE(UNKNOWN_ERROR);
#	undef CASE

	default:
		return "idl_ds_err_to_string(<invalid-case>)";
	};
}

/* error reporting, convert idevice response enum into string. */
static char* idl_idevice_err_to_string(idevice_error_t res) {
#	define CASE(v) IDEVICE_E_##v: return #v
	switch (res) {
	case CASE(SUCCESS);
	case CASE(INVALID_ARG);
	case CASE(UNKNOWN_ERROR);
	case CASE(NO_DEVICE);
	case CASE(NOT_ENOUGH_DATA);
	case CASE(SSL_ERROR);
	case CASE(TIMEOUT);
#	undef CASE

	default:
		return "idl_idevice_err_to_string(<invalid-case>)";
	};
}

/* error reporting, convert instproxy_client reponse enum to string. */
static char* idl_ip_err_to_string(instproxy_error_t res) {
#	define CASE(v) INSTPROXY_E_##v: return #v
	switch (res) {
	case CASE(SUCCESS);
	case CASE(INVALID_ARG);
	case CASE(PLIST_ERROR);
	case CASE(CONN_FAILED);
	case CASE(OP_IN_PROGRESS);
	case CASE(OP_FAILED);
	case CASE(RECEIVE_TIMEOUT);
	case CASE(ALREADY_ARCHIVED);
	case CASE(API_INTERNAL_ERROR);
	case CASE(APPLICATION_ALREADY_INSTALLED);
	case CASE(APPLICATION_MOVE_FAILED);
	case CASE(APPLICATION_SINF_CAPTURE_FAILED);
	case CASE(APPLICATION_SANDBOX_FAILED);
	case CASE(APPLICATION_VERIFICATION_FAILED);
	case CASE(ARCHIVE_DESTRUCTION_FAILED);
	case CASE(BUNDLE_VERIFICATION_FAILED);
	case CASE(CARRIER_BUNDLE_COPY_FAILED);
	case CASE(CARRIER_BUNDLE_DIRECTORY_CREATION_FAILED);
	case CASE(CARRIER_BUNDLE_MISSING_SUPPORTED_SIMS);
	case CASE(COMM_CENTER_NOTIFICATION_FAILED);
	case CASE(CONTAINER_CREATION_FAILED);
	case CASE(CONTAINER_P0WN_FAILED);
	case CASE(CONTAINER_REMOVAL_FAILED);
	case CASE(EMBEDDED_PROFILE_INSTALL_FAILED);
	case CASE(EXECUTABLE_TWIDDLE_FAILED);
	case CASE(EXISTENCE_CHECK_FAILED);
	case CASE(INSTALL_MAP_UPDATE_FAILED);
	case CASE(MANIFEST_CAPTURE_FAILED);
	case CASE(MAP_GENERATION_FAILED);
	case CASE(MISSING_BUNDLE_EXECUTABLE);
	case CASE(MISSING_BUNDLE_IDENTIFIER);
	case CASE(MISSING_BUNDLE_PATH);
	case CASE(MISSING_CONTAINER);
	case CASE(NOTIFICATION_FAILED);
	case CASE(PACKAGE_EXTRACTION_FAILED);
	case CASE(PACKAGE_INSPECTION_FAILED);
	case CASE(PACKAGE_MOVE_FAILED);
	case CASE(PATH_CONVERSION_FAILED);
	case CASE(RESTORE_CONTAINER_FAILED);
	case CASE(SEATBELT_PROFILE_REMOVAL_FAILED);
	case CASE(STAGE_CREATION_FAILED);
	case CASE(SYMLINK_FAILED);
	case CASE(UNKNOWN_COMMAND);
	case CASE(ITUNES_ARTWORK_CAPTURE_FAILED);
	case CASE(ITUNES_METADATA_CAPTURE_FAILED);
	case CASE(DEVICE_OS_VERSION_TOO_LOW);
	case CASE(DEVICE_FAMILY_NOT_SUPPORTED);
	case CASE(PACKAGE_PATCH_FAILED);
	case CASE(INCORRECT_ARCHITECTURE);
	case CASE(PLUGIN_COPY_FAILED);
	case CASE(BREADCRUMB_FAILED);
	case CASE(BREADCRUMB_UNLOCK_FAILED);
	case CASE(GEOJSON_CAPTURE_FAILED);
	case CASE(NEWSSTAND_ARTWORK_CAPTURE_FAILED);
	case CASE(MISSING_COMMAND);
	case CASE(NOT_ENTITLED);
	case CASE(MISSING_PACKAGE_PATH);
	case CASE(MISSING_CONTAINER_PATH);
	case CASE(MISSING_APPLICATION_IDENTIFIER);
	case CASE(MISSING_ATTRIBUTE_VALUE);
	case CASE(LOOKUP_FAILED);
	case CASE(DICT_CREATION_FAILED);
	case CASE(INSTALL_PROHIBITED);
	case CASE(UNINSTALL_PROHIBITED);
	case CASE(MISSING_BUNDLE_VERSION);
	case CASE(UNKNOWN_ERROR);
#	undef CASE

	default:
		return "idl_ip_err_to_string(<invalid-case>)";
	};
}

/* cached and shared, "OK" is sent a lot. */
static debugserver_command_t idl_ok_cmd = NULL;

/* send a command, optionally require a specific response
 * see also: https://opensource.apple.com/source/lldb/lldb-179.1/tools/debugserver/source/RNBRemote.cpp.auto.html */
static idl_bool idl_run_command_ex(debugserver_client_t ds, char* cmd_str, int argc, char** argv, char* expected_resp) {
	debugserver_command_t cmd = NULL;
	debugserver_error_t res = DEBUGSERVER_E_UNKNOWN_ERROR;
	char* resp = NULL;
	idl_bool ret = IDR_TRUE;

	/* execute the command. */
	debugserver_command_new(cmd_str, argc, argv, &cmd);
	res = debugserver_client_send_command(ds, cmd, &resp, 0);
	debugserver_command_free(cmd);
	cmd = NULL;

	/* check and cleanup response. */
	if (NULL != resp) {
		if (NULL != expected_resp) {
			if (0 != strncmp(resp, expected_resp, strlen(expected_resp))) {
				IDR_ERR("unexpected response to '%s': '%s'", cmd_str, resp);
				ret = IDR_FALSE;
			}
		}

		free(resp);
		resp = NULL;
	}

	/* report unexpected result. */
	if (DEBUGSERVER_E_SUCCESS != res) {
		IDR_ERR("unexpected result from '%s': %s", cmd_str, idl_ds_err_to_string(res));
		ret = IDR_FALSE;
	}

	/* done. */
	return ret;
}

/* convenience for the most common case, 0 args and a required "OK" response. */
static idl_bool idl_run_command(debugserver_client_t ds, char* cmd_str) {
	return idl_run_command_ex(ds, cmd_str, 0, NULL, "OK");
}

/* issue commands necessary to launch an app and verify that the launch was successful. */
static idl_bool idl_launch(debugserver_client_t ds, char* app_path, int argc, char** argv) {
	/* launch processing. */
	{
		/* app receives all arguments after the first IDR_ARGC - the first arg is the app path, so add +1 back. */
		int app_argc = (argc - IDR_ARGC + 1);
		char** app_argv = (char**)malloc(sizeof(char*) * (argc + 1)); /* +1 for NULL terminator. */
		/* populate. */
		{
			int i = 1;

			/* 0 is app path. */
			app_argv[0] = app_path;
			for (; i < app_argc; ++i) {
				app_argv[i] = argv[i+1];
			}
			/* NULL terminate. */
			app_argv[i] = NULL;
		}

		/* launch. */
		debugserver_client_set_argv(ds, app_argc, app_argv, NULL);
		free(app_argv);
		app_argv = NULL;
	}

	/* check for successful launch, then continue to allow process to execute. */
	if (!idl_run_command(ds, "qLaunchSuccess")) {
		return IDR_FALSE;
	}

	/* done. */
	return IDR_TRUE;
}

int main(int argc, char** argv) {
	int ret = 1;
	char* action = NULL;
	char* bundle_id = NULL;
	idevice_t idevice = NULL;
	char* app_path = NULL;
	char* app_pwd = NULL;
	debugserver_client_t ds = NULL;
	idl_bool running = IDR_FALSE;
	idl_bool leave_running = IDR_FALSE;

	/* process_name for reporting - do absolutely first,
	 * since this is implicitly used by the IDR_ERR reporting
	 * macro. */
	idl_process_name = strrchr(argv[0], '/');
	if (NULL == idl_process_name) {
		idl_process_name = argv[0];
	} else {
		idl_process_name++;
	}

	/* check arguments. */
	if (IDR_ARGC != argc || 0 != strcmp(argv[1], "start")) {
		fprintf(stderr, "Usage: %s start BUNDLEID [ARGS...]\n", idl_process_name);
		return ret;
	}

	/* convenience. */
	action = argv[1];
	bundle_id = argv[2];

	/* used throughout. */
	debugserver_command_new("OK", 0, NULL, &idl_ok_cmd);

	/* create the idevice. */
	{
		idevice_error_t idevice_res = idevice_new(&idevice, NULL);
		if (IDEVICE_E_SUCCESS != idevice_res) {
			IDR_ERR("Failed creating idevice: %s", idl_idevice_err_to_string(idevice_res));
			goto error;
		}
	}

	/* get path based on bundle id. */
	{
		instproxy_client_t instproxy_client = NULL;
		char* app_pwd_end = NULL;
		size_t size = 0;

		instproxy_error_t res = instproxy_client_start_service(idevice, &instproxy_client, "idevicelaunch");
		if (INSTPROXY_E_SUCCESS != res) {
			IDR_ERR("Failed creating instproxy_client: %s", idl_ip_err_to_string(res));
			goto error;
		}
		res = instproxy_client_get_path_for_bundle_identifier(instproxy_client, bundle_id, &app_path);
		instproxy_client_free(instproxy_client);
		instproxy_client = NULL;

		if (INSTPROXY_E_SUCCESS != res || NULL == app_path) {
			IDR_ERR("Failed getting app path for %s", bundle_id);
			goto error;
		}

		/* also determine and allocate the app's pwd. */
		app_pwd_end = strrchr(app_path, '/');
		if (NULL == app_pwd_end) {
			IDR_ERR("app_path is root, unexpected.");
			goto error;
		}
		size = (size_t)(app_pwd_end - app_path);
		app_pwd = (char*)malloc(size + 1);
		memcpy(app_pwd, app_path, size);
		app_pwd[size] = '\0';
	}

	/* create the debugserver for communication. */
	{
		debugserver_error_t res = debugserver_client_start_service(idevice, &ds, "idevicelaunch");
		if (DEBUGSERVER_E_SUCCESS != res) {
			IDR_ERR("Failed creating debugserver_client: %s", idl_ds_err_to_string(res));
			goto error;
		}
	}

	/* disable all logging. */
	if (!idl_run_command(ds, "QSetLogging:bitmask=LOG_RNB_NONE")) {
		goto error;
	}

	/* set working directory. */
	{
		char* app_args[2] = { app_pwd, NULL };
		if (!idl_run_command_ex(ds, "QSetWorkingDir:", 1, app_args, "OK")) {
			goto error;
		}
	}

	/* launch - always, this is how we form an association. */
	if (!idl_launch(ds, app_path, argc, argv)) {
		goto error;
	}

	/* now running. */
	running = IDR_TRUE;

	/* detach and leaving running. */

	/* "set the thread for subsequent actions" - see RNBRemote::HandlePacket_H
	 * Hc0 - H is thread set, c is "step/continue" ops, and -1 says "all threads". */
	if (!idl_run_command(ds, "Hc-1")) {
		goto error;
	}

	/* continue - no explicit response */
	if (!idl_run_command_ex(ds, "c", 0, NULL, NULL)) {
		goto error;
	}

	/* detach the debugger - responds with "OK", but we'll be seeing responses
	 * from the results of the continue, so we ignore. */
	if (!idl_run_command_ex(ds, "D", 0, NULL, NULL)) {
		goto error;
	}

	leave_running = IDR_TRUE;

	/* done. */
	ret = 0;
	/* fall-through. */

error:
	if (running && !leave_running) {
		if (NULL != ds) {
			/* kill command to deal with an unexpected error. */
			idl_run_command_ex(ds, "k", 0, NULL, NULL);
		}
	}

	/* cleanup */

	if (NULL != ds) {
		debugserver_client_free(ds);
		ds = NULL;
	}

	if (NULL != app_path) {
		free(app_path);
		app_path = NULL;
	}

	if (NULL != idevice) {
		idevice_free(idevice);
		idevice = NULL;
	}

	if (NULL != idl_ok_cmd) {
		debugserver_command_free(idl_ok_cmd);
		idl_ok_cmd = NULL;
	}

	return ret;
}
