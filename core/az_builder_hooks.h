#ifndef AZ_BUILDER_HOOKS_H
#define AZ_BUILDER_HOOKS_H
#include "az_abi.h"

void azt_builder_post(const AzWorld *w, u32 slot, u32 worker, u32 cell, u32 strength);
void azt_builder_before_commit(const AzWorld *w);
void azt_builder_after_commit(const AzWorld *w);
#endif
