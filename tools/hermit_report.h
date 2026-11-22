#ifndef AZ_HERMIT_REPORT_H
#define AZ_HERMIT_REPORT_H
#include "az_abi.h"
#include <stdio.h>

typedef bool (*AztRecordRefusal)(void *ctx, u64 uid);
u64 azt_hermit_bytes(u32 capacity);
bool azt_hermit_attach(const AzWorld *w, void *tail, u64 bytes, AztRecordRefusal record, void *ctx);
void azt_hermit_detach(void);
bool azt_hermit_window(const AzWorld *w);
void azt_hermit_json(FILE *out, const AzWorld *w, u32 covered, u32 partial, u32 positive, bool exact);
#endif
