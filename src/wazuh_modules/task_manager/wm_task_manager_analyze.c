/*
 * Wazuh Module for Task management.
 * Copyright (C) 2015-2020, Wazuh Inc.
 * July 13, 2020.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifdef WAZUH_UNIT_TESTING
// Remove static qualifier when unit testing
#define STATIC
#else
#define STATIC static
#endif

#ifndef WIN32

#include "../wmodules.h"
#include "wm_task_manager_db.h"
#include "wm_task_manager_parsing.h"

/**
 * Analyze a api task command.
 * @param command Command of the task to be analyzed.
 * @param error_code Variable to store an error code if something is wrong.
 * @param agent_id Agent id extracted from task_object.
 * @param task_id Task id extracted from task_object.
 * @return JSON object with the response for this task.
 * */
STATIC cJSON* wm_task_manager_analyze_task_api_module(char *command, int *error_code, int agent_id, int task_id) __attribute__((nonnull));

/**
 * Analyze a upgrade_module task by command. Update the tasks DB when necessary.
 * @param node Node that executed the command.
 * @param command Command of the task to be analyzed.
 * @param error_code Variable to store an error code if something is wrong.
 * @param agent_id Agent id extracted from task_object.
 * @param task_id Task id extracted from task_object.
 * @param status Status extracted from task_object.
 * @param status Error string extracted from task_object.
 * @return JSON object with the response for this task.
 * */
STATIC cJSON* wm_task_manager_analyze_task_upgrade_module(char *node, char *command, int *error_code, int agent_id, int task_id, char *status, char *error) __attribute__((nonnull(1, 2)));

cJSON* wm_task_manager_analyze_task(const cJSON *task_object, int *error_code) {
    cJSON *response = NULL;
    cJSON *tmp = NULL;

    char *node = NULL;
    char *module = NULL;
    char *command = NULL;
    int agent_id = OS_INVALID;
    int task_id = OS_INVALID;
    char *status = NULL;
    char *error = NULL;

    if (tmp = cJSON_GetObjectItem(task_object, task_manager_json_keys[WM_TASK_NODE]), tmp && tmp->type == cJSON_String) {
        os_strdup(tmp->valuestring, node);
    }
    if (tmp = cJSON_GetObjectItem(task_object, task_manager_json_keys[WM_TASK_MODULE]), tmp && tmp->type == cJSON_String) {
        os_strdup(tmp->valuestring, module);
    }
    if (tmp = cJSON_GetObjectItem(task_object, task_manager_json_keys[WM_TASK_COMMAND]), tmp && tmp->type == cJSON_String) {
        os_strdup(tmp->valuestring, command);
    }
    if (tmp = cJSON_GetObjectItem(task_object, task_manager_json_keys[WM_TASK_AGENT_ID]), tmp && tmp->type == cJSON_Number) {
        agent_id = tmp->valueint;
    }
    if (tmp = cJSON_GetObjectItem(task_object, task_manager_json_keys[WM_TASK_TASK_ID]), tmp && tmp->type == cJSON_Number) {
        task_id = tmp->valueint;
    }
    if (tmp = cJSON_GetObjectItem(task_object, task_manager_json_keys[WM_TASK_STATUS]), tmp && tmp->type == cJSON_String) {
        os_strdup(tmp->valuestring, status);
    }
    if (tmp = cJSON_GetObjectItem(task_object, task_manager_json_keys[WM_TASK_ERROR_MSG]), tmp && tmp->type == cJSON_String) {
        os_strdup(tmp->valuestring, error);
    }

    if (node && module && command) {
        if (!strcmp(task_manager_modules_list[WM_TASK_UPGRADE_MODULE], module)) {
            response = wm_task_manager_analyze_task_upgrade_module(node, command, error_code, agent_id, task_id, status, error);
        } else if (!strcmp(task_manager_modules_list[WM_TASK_API_MODULE], module)) {
            response = wm_task_manager_analyze_task_api_module(command, error_code, agent_id, task_id);
        } else {
            *error_code = WM_TASK_INVALID_MODULE;
            response = wm_task_manager_parse_data_response(WM_TASK_INVALID_MODULE, agent_id, task_id, status);
        }
    }

    os_free(node);
    os_free(module);
    os_free(command);
    os_free(status);
    os_free(error);

    return response;
}

STATIC cJSON* wm_task_manager_analyze_task_upgrade_module(char *node, char *command, int *error_code, int agent_id, int task_id, char *status, char *error) {
    cJSON *response = NULL;
    int result = 0;
    char *status_result = NULL;

    if (!strcmp(task_manager_commands_list[WM_TASK_UPGRADE], command) || !strcmp(task_manager_commands_list[WM_TASK_UPGRADE_CUSTOM], command)) {

        if (agent_id != OS_INVALID) {
            // Insert upgrade task into DB
            if (task_id = wm_task_manager_insert_task(agent_id, node, task_manager_modules_list[WM_TASK_UPGRADE_MODULE], command), task_id == OS_INVALID) {
                *error_code = WM_TASK_DATABASE_ERROR;
            } else {
                response = wm_task_manager_parse_data_response(WM_TASK_SUCCESS, agent_id, task_id, status);
            }
        } else {
            *error_code = WM_TASK_INVALID_AGENT_ID;
            response = wm_task_manager_parse_data_response(WM_TASK_INVALID_AGENT_ID, agent_id, task_id, status);
        }

    } else if (!strcmp(task_manager_commands_list[WM_TASK_UPGRADE_GET_STATUS], command)) {

        if (agent_id != OS_INVALID) {
            // Get upgrade task status
            if (result = wm_task_manager_get_upgrade_task_status(agent_id, &status_result), result == OS_INVALID) {
                *error_code = WM_TASK_DATABASE_ERROR;
            } else if (result) {
                *error_code = result;
                response = wm_task_manager_parse_data_response(result, agent_id, task_id, status);
            } else {
                response = wm_task_manager_parse_data_response(WM_TASK_SUCCESS, agent_id, task_id, status_result);
            }
        } else {
            *error_code = WM_TASK_INVALID_AGENT_ID;
            response = wm_task_manager_parse_data_response(WM_TASK_INVALID_AGENT_ID, agent_id, task_id, status);
        }

    } else if (!strcmp(task_manager_commands_list[WM_TASK_UPGRADE_UPDATE_STATUS], command)) {

        if (agent_id != OS_INVALID) {
            // Update upgrade task status
            if (result = wm_task_manager_update_upgrade_task_status(agent_id, status, error), result == OS_INVALID) {
                *error_code = WM_TASK_DATABASE_ERROR;
            } else if (result) {
                *error_code = result;
                response = wm_task_manager_parse_data_response(result, agent_id, task_id, status);
            } else {
                response = wm_task_manager_parse_data_response(WM_TASK_SUCCESS, agent_id, task_id, status);
            }
        } else {
            *error_code = WM_TASK_INVALID_AGENT_ID;
            response = wm_task_manager_parse_data_response(WM_TASK_INVALID_AGENT_ID, agent_id, task_id, status);
        }

    } else {
        *error_code = WM_TASK_INVALID_COMMAND;
        response = wm_task_manager_parse_data_response(WM_TASK_INVALID_COMMAND, agent_id, task_id, status);
    }

    os_free(status_result);

    return response;
}

STATIC cJSON* wm_task_manager_analyze_task_api_module(char *command, int *error_code, int agent_id, int task_id) {
    cJSON *response = NULL;
    int create_time = OS_INVALID;
    int last_update_time = OS_INVALID;
    char *node_result = NULL;
    char *module_result = NULL;
    char *command_result = NULL;
    char *status = NULL;
    char *error = NULL;

    if (!strcmp(task_manager_commands_list[WM_TASK_UPGRADE_RESULT], command)) {

        if (agent_id != OS_INVALID) {
            if (task_id = wm_task_manager_get_upgrade_task_by_agent_id(agent_id, &node_result, &module_result, &command_result, &status, &error, &create_time, &last_update_time), task_id == OS_INVALID) {
                *error_code = WM_TASK_DATABASE_ERROR;
                response = wm_task_manager_parse_data_response(WM_TASK_DATABASE_ERROR, agent_id, task_id, status);
            } else if (task_id == OS_NOTFOUND || task_id == 0) {
                *error_code = WM_TASK_DATABASE_NO_TASK;
                response = wm_task_manager_parse_data_response(WM_TASK_DATABASE_NO_TASK, agent_id, OS_INVALID, status);
            } else {
                response = wm_task_manager_parse_data_response(WM_TASK_SUCCESS, agent_id, task_id, NULL);
                wm_task_manager_parse_data_result(response, node_result, module_result, command_result, status, error, create_time, last_update_time, command);
            }
        } else {
            *error_code = WM_TASK_INVALID_AGENT_ID;
            response = wm_task_manager_parse_data_response(WM_TASK_INVALID_AGENT_ID, agent_id, task_id, status);
        }

    } else if (!strcmp(task_manager_commands_list[WM_TASK_TASK_RESULT], command)) {

        if (task_id != OS_INVALID) {
            if (agent_id = wm_task_manager_get_task_by_task_id(task_id, &node_result, &module_result, &command_result, &status, &error, &create_time, &last_update_time), agent_id == OS_INVALID) {
                *error_code = WM_TASK_DATABASE_ERROR;
                response = wm_task_manager_parse_data_response(WM_TASK_DATABASE_ERROR, agent_id, task_id, status);
            } else if (agent_id == OS_NOTFOUND || agent_id == 0) {
                *error_code = WM_TASK_DATABASE_NO_TASK;
                response = wm_task_manager_parse_data_response(WM_TASK_DATABASE_NO_TASK, OS_INVALID, task_id, status);
            } else {
                response = wm_task_manager_parse_data_response(WM_TASK_SUCCESS, agent_id, task_id, NULL);
                wm_task_manager_parse_data_result(response, node_result, module_result, command_result, status, error, create_time, last_update_time, command);
            }
        } else {
            *error_code = WM_TASK_INVALID_TASK_ID;
            response = wm_task_manager_parse_data_response(WM_TASK_INVALID_TASK_ID, agent_id, task_id, status);
        }

    } else {
        *error_code = WM_TASK_INVALID_COMMAND;
        response = wm_task_manager_parse_data_response(WM_TASK_INVALID_COMMAND, agent_id, task_id, status);
    }

    os_free(node_result);
    os_free(module_result);
    os_free(command_result);
    os_free(status);
    os_free(error);

    return response;
}

#endif
