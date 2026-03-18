#include "error.h"

void warning_impl(const char* file, int line, const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "%s:%d warning: ", file, line);

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void error_exit_impl(const char* file, int line, ERROR_CODES err_code, const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "%s:%d error: ", file, line);

    if (fmt == NULL || fmt[0] == '\0'){
        switch (err_code) {
            case ERR_MALLOC:
                fprintf(stderr, "Malloc failed.\n");
                break;
            case ERR_NULL_POINTER:
                fprintf(stderr, "Trying to dereference null pointer.\n");
                break;
            default:
                fprintf(stderr, "Something went wrong.\n");
                break;
        }
    }

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    exit(err_code);
}
