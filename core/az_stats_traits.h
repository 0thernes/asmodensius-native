#ifndef AZ_STATS_TRAITS_H
#define AZ_STATS_TRAITS_H
/* Shared production arithmetic for a group of byte-valued traits. */
AZ_INLINE void add_trait_values(u64 *sum, u64 *squares, const u8 *p, u32 width)
{
    for (u32 j = 0; j < width; ++j) {
        sum[j] += p[j];
        squares[j] += (u32)p[j] * p[j];
    }
}

/* Exact mean and population variance for byte-valued traits. Both sums fit
   even for UINT32_MAX samples. The fractional mean is retained in variance. */
AZ_INLINE void trait_moments(AzStats *out, u32 k, u32 n, u64 sum, u64 squares)
{
    const u64 mean = sum / n, rem = sum % n;
    const u64 centered = squares + (u64)n * mean * mean - 2u * mean * sum;
    out->trait_mean[k] = (u32)mean;
    out->trait_var[k] = (u32)(centered / n - ((centered % n) * n < rem * rem));
}

#endif
