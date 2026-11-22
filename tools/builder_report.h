#ifndef AZ_BUILDER_REPORT_H
#define AZ_BUILDER_REPORT_H
#include "az_abi.h"
#include <stdio.h>

u64 azt_builder_bytes(u32 capacity);
bool azt_builder_attach(const AzWorld *w, void *tail, u64 bytes);
void azt_builder_detach(void);
bool azt_builder_window(const AzWorld *w);
void azt_builder_json(FILE *out, const AzWorld *w, u32 covered, u32 partial, u32 positive, bool exact);
#endif
