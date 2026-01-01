#include <kernel/error.h>
#include <kernel/kernel.h>

static int last_error = E_SUCCESS;

void error_set_last(int error_code) {
    last_error = error_code;
}

int error_get_last(void) {
    return last_error;
}

const char *error_to_string(int error_code) {
    switch (error_code) {
        case E_SUCCESS:  return "Success";
        case E_GENERIC:  return "Generic error";
        case E_NOMEM:    return "Out of memory";
        case E_INVAL:    return "Invalid argument";
        case E_NOENT:    return "No such entry";
        case E_PERM:     return "Permission denied";
        case E_BUSY:     return "Resource busy";
        case E_EXIST:    return "Already exists";
        case E_NOTDIR:   return "Not a directory";
        case E_ISDIR:    return "Is a directory";
        case E_NOSPACE:  return "No space left";
        case E_RANGE:    return "Out of range";
        case E_IO:       return "I/O error";
        case E_BADF:     return "Bad file descriptor";
        case E_CHILD:    return "No child processes";
        case E_AGAIN:    return "Try again";
        case E_NODEV:    return "No such device";
        case E_NOTIMPL:  return "Not implemented";
        case E_OVERFLOW: return "Value overflow";
        case E_DEADLK:   return "Deadlock";
        case E_CORRUPT:  return "Data corruption";
        default:         return "Unknown error";
    }
}

void error_report(const char *subsystem, int error_code, const char *message) {
    if (!subsystem) subsystem = "KERNEL";
    if (!message) message = "";
    
    kprintf("ERROR [%s]: %s - %s\n", 
            subsystem, error_to_string(error_code), message);
    error_set_last(error_code);
}
