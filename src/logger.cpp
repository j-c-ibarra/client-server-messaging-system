#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "../include/global.h"
#include "../include/logger.h"

char LOGFILE[FILEPATH_LEN];

void cse4589_init_log(char* port)
{
    FILE* fp;
    fp = popen("hostname | tr '.' '\n' | sed -n 1p", "r");
    if (fp == NULL) {
        printf("Oops! Failed to get hostname. Contact the course staff!\n");
        exit(1);
    }

    char* hostname = (char*) malloc(HOSTNAME_LEN * sizeof(char));
    bzero(hostname, HOSTNAME_LEN);
    fscanf(fp, "%s", hostname);

    char* exec_path = (char*) malloc(PATH_LEN * sizeof(char));
    bzero(exec_path, PATH_LEN);

#ifdef __APPLE__
    uint32_t size = PATH_LEN;
    if (_NSGetExecutablePath(exec_path, &size) != 0) {
        printf("Oops! Failed to get executable path. Contact the course staff!\n");
        exit(1);
    }
#else
    if (readlink("/proc/self/exe", exec_path, PATH_LEN) == -1) {
        printf("Oops! Failed to get executable path. Contact the course staff!\n");
        exit(1);
    }
#endif

    char* last_slash = strrchr(exec_path, '/');
    if (last_slash == NULL) {
        printf("Oops! Failed to determine executable directory. Contact the course staff!\n");
        exit(1);
    }
    *last_slash = '\0';

    bzero(LOGFILE, FILEPATH_LEN);
    snprintf(LOGFILE, FILEPATH_LEN, "%s/logs/assignment_log_%s_%s",
             exec_path, hostname, port);

    free(exec_path);
    free(hostname);
    fclose(fp);
}

int ret_print, ret_log;

void cse4589_print_and_log(const char* format, ...)
{
    va_list args_pointer;

    va_start(args_pointer, format);
    ret_print = vprintf(format, args_pointer);
    va_end(args_pointer);

    FILE* fp;
    if ((fp = fopen(LOGFILE, "a")) == NULL) {
        ret_log = -100;
        return;
    }

    va_start(args_pointer, format);
    ret_log = vfprintf(fp, format, args_pointer);
    va_end(args_pointer);

    fclose(fp);
}