/* Horizontal physics primitive, independently checked against signed magnitudes.
 * The byte-pointer declaration targets the native assembler directly and avoids
 * converting intentionally unaligned buffers to aligned C pointer types. */
#include "az_test.h"
#include "az_kernels.h"
#include <inttypes.h>
#include <windows.h>

#ifdef AZ_HAVE_KIN_AVX2
extern int az_test_kin_abi(void);
extern void axis_bytes(void *, void *, const void *, const void *, u32, u32)
    __asm__("az_kin_axis_avx2");
#endif

enum { PHYS_LANES = 128, PHYS_GUARD = 64, PHYS_BYTES = PHYS_GUARD * 2 + 31 + PHYS_LANES * 4 };

static i16 expected_speed(i16 velocity, i16 increment, u8 drag)
{
    const i64 sum = (i64)velocity + increment;
    u64 magnitude = (u64)(sum < 0 ? -sum : sum);
    if (magnitude > AZ_VEL_MAX) magnitude = AZ_VEL_MAX;
    const i32 speed = (i32)(magnitude * (256u - drag) / 256u);
    return (i16)(sum < 0 ? -speed : speed);
}

#ifdef AZ_HAVE_KIN_AVX2
static void check_tail_reads(void)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    u8 *page[4] = { NULL, NULL, NULL, NULL };
    const size_t width[] = {4, 2, 2, 1};
    bool valid = true;
    for (u32 k = 0; k < 4; k++) {
        page[k] = VirtualAlloc(NULL, 2u * info.dwPageSize, MEM_RESERVE, PAGE_NOACCESS);
        if (!page[k] || !VirtualAlloc(page[k], info.dwPageSize, MEM_COMMIT, PAGE_READWRITE)) {
            valid = false;
            break;
        }
    }
    CHECK(valid);
    if (valid) {
        for (u32 n = 0; n <= PHYS_LANES; n++) {
            u8 *args[4];
            for (u32 k = 0; k < 4; k++) args[k] = page[k] + info.dwPageSize - width[k]*n;
            axis_bytes(args[0], args[1], args[2], args[3], n, 18);
        }
    }
    for (u32 k = 0; k < 4; k++) if (page[k]) VirtualFree(page[k], 0, MEM_RELEASE);
}
#endif

static void check_axis(AzWorld *w, bool has_twin)
{
    static const u32 sizes[] = { 0, 1, 2, 7, 8, 9, 127, 128 };
    static const i16 extremes[] = { INT16_MIN, -AZ_VEL_MAX, -1, 0, 1, AZ_VEL_MAX, INT16_MAX };
    u64 state = 0x65b12147dULL;
    u64 lane_count = 0;
    u32 failed = 0;
    for (u32 block = 0; block < 1000000u; block++) {
        const u32 n = (block / (8u * 17u * 32u)) % 2u == 0u
            ? sizes[block % 8u] : (block / 32u) % (PHYS_LANES + 1u);
        const u32 shift = 16u + (block / 8u) % 17u;
        const u32 offset = (block / (8u * 17u)) % 32u;
        u8 bytes[4][PHYS_BYTES], expected[4][PHYS_BYTES];
        u32 pos[PHYS_LANES];
        i16 vel[PHYS_LANES], dv[PHYS_LANES];
        u8 drag[PHYS_LANES];
        size_t off[4];
        memset(bytes, 0xa5, sizeof(bytes));
        for (u32 k = 0; k < 4; k++) off[k] = PHYS_GUARD + ((offset * (2u*k + 1u) + k) % 32u);
        for (u32 i = 0; i < n; i++) {
            const u64 random = az_test_rand64(&state);
            pos[i] = (u32)random;
            vel[i] = (i16)(random >> 32);
            dv[i] = (i16)(random >> 48);
            drag[i] = (u8)az_test_rand64(&state);
            if ((block & 15u) < 8u && i == n-1u) {
                vel[i] = extremes[(block / 8u) % 7u];
                dv[i] = extremes[(block / 56u) % 7u];
                drag[i] = (block & 1u) ? 255u : 0u;
                pos[i] = (block & 2u) ? UINT32_MAX : 0u;
            }
            memcpy(bytes[0] + off[0] + 4*i, pos+i, 4);
            memcpy(bytes[1] + off[1] + 2*i, vel+i, 2);
            memcpy(bytes[2] + off[2] + 2*i, dv+i, 2);
            bytes[3][off[3]+i] = drag[i];
        }
        memcpy(expected, bytes, sizeof(bytes));
        for (u32 i = 0; i < n; i++) {
            i16 v = expected_speed(vel[i], dv[i], drag[i]);
            u32 x = (u32)((i64)pos[i] + (i64)v * ((i64)1 << (shift - 16)));
            memcpy(expected[0] + off[0] + 4*i, &x, 4);
            memcpy(expected[1] + off[1] + 2*i, &v, 2);
        }
        az_kin_axis_c(pos, vel, dv, drag, n, shift);
        bool matches = !memcmp(pos, expected[0] + off[0], 4*n) && !memcmp(vel, expected[1] + off[1], 2*n);
#ifdef AZ_HAVE_KIN_AVX2
        if (has_twin) {
            axis_bytes(bytes[0]+off[0], bytes[1]+off[1], bytes[2]+off[2], bytes[3]+off[3], n, shift);
            matches = matches && !memcmp(bytes, expected, sizeof(bytes));
        }
#else
        (void)has_twin;
#endif
        if (!matches && !failed)
            printf("axis mismatch at block=%u n=%u shift=%u offset=%u\n", block,n,shift,offset);
        failed += (u32)!matches;
        lane_count += n;
    }
    CHECK_EQ(failed,0);
    /* Half drag on the last signed unit must actually stop both directions. */
    u32 pos[2] = { 400, 400 };
    i16 vel[2] = { 1,-1 };
    const i16 dv[2] = {0,0};
    const u8 drag[2] = {128,128};
    w->k.kin_axis(pos,vel,dv,drag,2,20);
    CHECK_EQ(vel[0],0); CHECK_EQ(vel[1],0);
    CHECK_EQ(pos[0],400); CHECK_EQ(pos[1],400);
    printf("blocks=1000000 lanes=%" PRIu64 " lengths=0..128 shifts=17 byte_offsets=32 guard_bytes=64 discrepancies=%u twin=%u\n",
           lane_count,failed,(unsigned)has_twin);
}

#ifdef AZ_HAVE_KIN_AVX2
static void check_overlapping_inputs(void)
{
    struct Columns {
        i16 vel[224];
        i16 impulse[224];
        u8 drag[448];
        u32 pos[224];
    } ref, twin;
    const u32 lengths[] = {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 64, 128};
    u32 equal = 0, cases = 0;
    for (u32 mode = 0; mode < 3; ++mode) {
        for (i32 offset = -15; offset <= 15; ++offset) {
            for (u32 j = 0; j < AZ_LEN(lengths); ++j) {
                const u32 n = lengths[j];
                const u32 shift = 16u + (n + (u32)(offset + 15)) % 17u;
                for (u32 k = 0; k < 224; ++k) {
                    ref.vel[k] = (i16)((k * 719u + n) % 31469u - 15734);
                    ref.impulse[k] = (i16)((k * 123u + 59u) % 32117u - 16058);
                    ref.pos[k] = 0xfff10007u + k * 0x10293847u;
                    ref.drag[k] = ref.drag[k + 224] = (u8)(k * 11u);
                }
                twin = ref;
                const u8 *ref_drag = mode == 1 ? (const u8 *)(ref.vel + 48) + offset :
                                     mode == 2 ? (const u8 *)(ref.pos + 48) + offset : ref.drag;
                const u8 *twin_drag = mode == 1 ? (const u8 *)(twin.vel + 48) + offset :
                                      mode == 2 ? (const u8 *)(twin.pos + 48) + offset : twin.drag;
                az_kin_axis_c(ref.pos + 48, ref.vel + 48,
                              mode == 0 ? ref.vel + 48 + offset : ref.impulse, ref_drag, n, shift);
                axis_bytes(twin.pos + 48, twin.vel + 48,
                           mode == 0 ? twin.vel + 48 + offset : twin.impulse, twin_drag, n, shift);
                ++cases;
                equal += memcmp(&ref, &twin, sizeof(ref)) == 0;
            }
        }
    }
    printf("overlap: %u of %u forward-order cases matched\n", equal, cases);
    CHECK_EQ(equal, cases);
}
#endif

int main(void)
{
    AzWorld *w = az_test_shell();
    CHECK(w != NULL);
    if (!w) return az_test_done();
    CHECK_EQ(az_kernels_install(w,0),0); CHECK(w->k.kin_axis == az_kin_axis_c);
    bool active = false;
#ifdef AZ_HAVE_KIN_AVX2
    const bool supported = az_twin_ok_avx2(az_cpu_features());
    CHECK_EQ(az_kernels_install(w,AZ_K_KIN_AVX2), supported ? AZ_K_KIN_AVX2 : 0);
    active = w->k.kin_axis == az_kin_axis_avx2;
    CHECK(active == supported);
    if (active) {
        const int abi_status = az_test_kin_abi();
        CHECK_EQ(abi_status, 0);
        if (abi_status) {
            az_test_shell_free(w);
            return az_test_done();
        }
        check_tail_reads();
        check_overlapping_inputs();
    }
#endif
    check_axis(w,active);
    CHECK_EQ(az_kernels_install(w,0),0); CHECK(w->k.kin_axis == az_kin_axis_c);
    az_test_shell_free(w);
    return az_test_done();
}
