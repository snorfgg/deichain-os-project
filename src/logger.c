/*
    Diogo André de Freitas Alves - 2020214197
    Bruno Miguel Santos Marques - 2018278019
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include "logger.h"

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOG_FILENAME "DEIChain_log.log"
#define MAX_LOG_LINES 50000

static int log_line_count = 0;

int init_logger() {
    log_file = fopen(LOG_FILENAME, "w");  //abre novo
    if (!log_file) {
        perror("[Logger] fopen log file");
        return -1;
    }
    log_line_count = 0;
    return 0;
}

void close_logger() {
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}

void log_message(const char *format, ...) {
    pthread_mutex_lock(&log_mutex);

    // Generate timestamp
    time_t now = time(NULL);
    char time_str[26];
    ctime_r(&now, time_str);
    time_str[strcspn(time_str, "\n")] = 0;

    // Log to stdout
    fprintf(stdout, "[%s] ", time_str);
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    fprintf(stdout, "\n");

    // Log to file if available
    if (log_file) {
        fprintf(log_file, "[%s] ", time_str);
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        fprintf(log_file, "\n");
        fflush(log_file);

        log_line_count++;

        if (log_line_count >= MAX_LOG_LINES) {
            fclose(log_file);
            log_file = fopen(LOG_FILENAME, "w");
            if (!log_file) {
                perror("[Logger] Failed to reopen log file for truncation");
            } else {
                log_message("[Logger] Log truncated after %d lines.", MAX_LOG_LINES);
                log_line_count = 0;
            }
        }
    }

    pthread_mutex_unlock(&log_mutex);
}

