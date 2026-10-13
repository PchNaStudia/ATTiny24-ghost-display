#include "Errors.h"

static ErrorCallback g_errorCallback = nullptr;

void setErrorCallback(ErrorCallback callback) {
    g_errorCallback = callback;
}

bool reportError(int const errorCode) {
    if (g_errorCallback) {
        g_errorCallback(errorCode);
    }

    return errorShouldTerminate(errorCode);
}

bool errorShouldTerminate(int const errorCode) {
    // if there are errors that we don't want to terminate program on, they should be checked there.
    switch (errorCode) {
        case 0:
            return false; // no error
        default:
            return true; // default behavior is to terminate on error
    }
}