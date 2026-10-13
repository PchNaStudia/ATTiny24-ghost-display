#pragma once

typedef void (*ErrorCallback)(int const errorCode);

void setErrorCallback(ErrorCallback callback);
bool reportError(int const errorCode);
bool errorShouldTerminate(int const errorCode);