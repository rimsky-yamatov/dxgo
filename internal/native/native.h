#ifndef DXGO_NATIVE_H
#define DXGO_NATIVE_H

#include <stdint.h>

int dxgo_native_init(const char* title, int width, int height);
int dxgo_native_open(void);
void dxgo_native_clear(float r, float g, float b, float a);
void dxgo_native_update(void);
void dxgo_native_close(void);

#endif