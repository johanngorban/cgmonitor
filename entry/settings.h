#pragma once

int extract_settings(const char *path);

const char *get_log_path();

const char *get_db_path();

int get_server_port();

int get_miner_port();

int get_polling_interval();

int get_connection_retry_limit();

int get_debug_flag();