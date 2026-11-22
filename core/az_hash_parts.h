/* Additive terms of az_state_hash. The world must stay immutable until all
 * readers finish. Each slot and cell belongs to exactly one half-open range.
 * The tail is added exactly once; all arithmetic is modulo 2^64. */
#ifndef AZ_HASH_PARTS_H
#define AZ_HASH_PARTS_H
#include "az_abi.h"
u64 az_hash_entities(const AzWorld *world, u32 begin, u32 end);
u64 az_hash_entities_batched(const AzWorld *world, u32 begin, u32 end);
u64 az_hash_cells(const AzWorld *world, u32 begin, u32 end);
u64 az_hash_tail(const AzWorld *world);
#endif
