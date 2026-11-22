#ifndef AZ_PARTNER_REPORT_H
#define AZ_PARTNER_REPORT_H
#include "az_abi.h"
#include <stdio.h>
u64 azt_partner_bytes(u32 cap);
bool azt_partner_attach(const AzWorld *w, bool genesis, void *tail, u64 bytes);
void azt_partner_detach(void);
void azt_partner_prepare(const AzWorld *w);
void azt_partner_fill(const AzWorld *w, u32 s, AzClassObservations *e);
void azt_partner_json(FILE *f, const AzWorld *w);
#endif
