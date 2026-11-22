#ifndef AZ_HERMIT_HOOKS_H
#define AZ_HERMIT_HOOKS_H
#include "az_abi.h"

/* Any negative score still refuses the proposal. Only this branch expresses
   the recipient's willingness decision on an otherwise eligible invitation. */
enum { AZ_RECRUIT_UNWILLING = -2 };
void azt_hermit_proposal(const AzWorld *w, u32 sender, u32 recipient, u32 worker, u32 act, i32 score);
void azt_hermit_barrier(const AzWorld *w);
#endif
