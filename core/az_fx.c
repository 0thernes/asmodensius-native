/* az_fx.c - fixed-point math and rotor (unit quaternion) algebra.
 *
 * Freestanding, integers only, no mutable state: every function here is a
 * pure function of its arguments, so it is safe in any parallel stage.
 *
 * Conventions
 *   angle      u32, 65536 = one turn (only the low 16 bits are used)
 *   Q14        16384 = 1.0  (sine, cosine, rotor components)
 *   Q8 log     az_log2_q8(x) = 256 * log2(x)
 *   rotor      unit quaternion (w, a, b, c), Q1.14; az_rotor_apply maps the
 *              body frame to the world frame: v' = q v q*
 *
 * Accuracy (checked by tests/test_fx.c against double precision)
 *   az_isqrt_u64   exact floor(sqrt(x)) for every u64
 *   az_sin_q14     within 1.1 units of 16384 * sin; exact at the quarter
 *                  turns; sin(a + half turn) = -sin(a) and sin(-a) = -sin(a)
 *                  hold exactly
 *   az_log2_q8     within 1 unit (1/256 bit); az_log2_q8(x) >> 8 is always
 *                  floor(log2(x)); exact at powers of two; monotone
 *   az_exp2_q8     correctly rounded 2^(e/256) to within 1 unit in the last
 *                  place of a 2^31-scaled table; monotone; saturates at
 *                  e >= 32 * 256
 *   rotors         norm, mul and step round every component of the exactly
 *                  normalised result to nearest; apply divides by the
 *                  rotor's own squared norm, so it is an exact rotation
 *                  rounded to nearest and apply_inv(apply(v)) is within one
 *                  unit of v
 *
 * The tables are produced by tools/gen/gen_fx_tables.py (60-digit decimal
 * arithmetic); `python tools/gen/gen_fx_tables.py --check core/az_fx.c`
 * verifies that the block below is the generated one. */
#include "az_abi.h"

/* BEGIN GENERATED TABLES (tools/gen/gen_fx_tables.py) - do not edit by hand */
/* round(16384 * sin(pi/2 * i/256)), i = 0..256 */
static const i16 AZ_SIN_Q14[257] = {
    0, 101, 201, 302, 402, 503, 603, 704, 804, 904, 1005, 1105,
    1205, 1306, 1406, 1506, 1606, 1706, 1806, 1906, 2006, 2105, 2205, 2305,
    2404, 2503, 2603, 2702, 2801, 2900, 2999, 3098, 3196, 3295, 3393, 3492,
    3590, 3688, 3786, 3883, 3981, 4078, 4176, 4273, 4370, 4467, 4563, 4660,
    4756, 4852, 4948, 5044, 5139, 5235, 5330, 5425, 5520, 5614, 5708, 5803,
    5897, 5990, 6084, 6177, 6270, 6363, 6455, 6547, 6639, 6731, 6823, 6914,
    7005, 7096, 7186, 7276, 7366, 7456, 7545, 7635, 7723, 7812, 7900, 7988,
    8076, 8163, 8250, 8337, 8423, 8509, 8595, 8680, 8765, 8850, 8935, 9019,
    9102, 9186, 9269, 9352, 9434, 9516, 9598, 9679, 9760, 9841, 9921, 10001,
    10080, 10159, 10238, 10316, 10394, 10471, 10549, 10625, 10702, 10778, 10853, 10928,
    11003, 11077, 11151, 11224, 11297, 11370, 11442, 11514, 11585, 11656, 11727, 11797,
    11866, 11935, 12004, 12072, 12140, 12207, 12274, 12340, 12406, 12472, 12537, 12601,
    12665, 12729, 12792, 12854, 12916, 12978, 13039, 13100, 13160, 13219, 13279, 13337,
    13395, 13453, 13510, 13567, 13623, 13678, 13733, 13788, 13842, 13896, 13949, 14001,
    14053, 14104, 14155, 14206, 14256, 14305, 14354, 14402, 14449, 14497, 14543, 14589,
    14635, 14680, 14724, 14768, 14811, 14854, 14896, 14937, 14978, 15019, 15059, 15098,
    15137, 15175, 15213, 15250, 15286, 15322, 15357, 15392, 15426, 15460, 15493, 15525,
    15557, 15588, 15619, 15649, 15679, 15707, 15736, 15763, 15791, 15817, 15843, 15868,
    15893, 15917, 15941, 15964, 15986, 16008, 16029, 16049, 16069, 16088, 16107, 16125,
    16143, 16160, 16176, 16192, 16207, 16221, 16235, 16248, 16261, 16273, 16284, 16295,
    16305, 16315, 16324, 16332, 16340, 16347, 16353, 16359, 16364, 16369, 16373, 16376,
    16379, 16381, 16383, 16384, 16384
};
/* round(32768 * log2(1 + i/256)), i = 0..256 */
static const u16 AZ_LOG2_Q15[257] = {
    0, 184, 368, 551, 733, 914, 1095, 1275, 1455, 1633, 1811, 1989,
    2166, 2342, 2517, 2692, 2866, 3039, 3212, 3385, 3556, 3727, 3897, 4067,
    4236, 4405, 4573, 4740, 4907, 5073, 5239, 5404, 5568, 5732, 5895, 6058,
    6220, 6382, 6543, 6703, 6863, 7023, 7182, 7340, 7498, 7655, 7812, 7968,
    8124, 8279, 8434, 8588, 8742, 8895, 9048, 9200, 9352, 9503, 9654, 9804,
    9954, 10104, 10253, 10401, 10549, 10696, 10843, 10990, 11136, 11282, 11427, 11572,
    11716, 11860, 12004, 12147, 12289, 12431, 12573, 12715, 12855, 12996, 13136, 13276,
    13415, 13554, 13692, 13830, 13968, 14105, 14242, 14378, 14514, 14650, 14785, 14920,
    15055, 15189, 15322, 15456, 15589, 15721, 15854, 15986, 16117, 16248, 16379, 16509,
    16639, 16769, 16898, 17027, 17156, 17284, 17412, 17540, 17667, 17794, 17921, 18047,
    18173, 18298, 18424, 18548, 18673, 18797, 18921, 19045, 19168, 19291, 19414, 19536,
    19658, 19780, 19901, 20022, 20143, 20263, 20383, 20503, 20623, 20742, 20861, 20980,
    21098, 21216, 21334, 21451, 21568, 21685, 21802, 21918, 22034, 22150, 22265, 22380,
    22495, 22610, 22724, 22838, 22952, 23066, 23179, 23292, 23404, 23517, 23629, 23741,
    23852, 23964, 24075, 24186, 24296, 24407, 24517, 24627, 24736, 24845, 24955, 25063,
    25172, 25280, 25388, 25496, 25604, 25711, 25818, 25925, 26031, 26138, 26244, 26350,
    26455, 26561, 26666, 26771, 26876, 26980, 27084, 27188, 27292, 27396, 27499, 27602,
    27705, 27808, 27910, 28012, 28114, 28216, 28318, 28419, 28520, 28621, 28722, 28822,
    28922, 29022, 29122, 29222, 29321, 29421, 29520, 29618, 29717, 29815, 29914, 30012,
    30109, 30207, 30304, 30401, 30498, 30595, 30692, 30788, 30884, 30980, 31076, 31172,
    31267, 31362, 31457, 31552, 31647, 31741, 31836, 31930, 32024, 32117, 32211, 32304,
    32397, 32490, 32583, 32676, 32768
};
/* round(2^31 * 2^(i/256)), i = 0..255 */
static const u32 AZ_EXP2_Q31[256] = {
    2147483648u, 2153306067u, 2159144272u, 2164998306u, 2170868212u, 2176754033u,
    2182655811u, 2188573592u, 2194507417u, 2200457330u, 2206423375u, 2212405596u,
    2218404036u, 2224418739u, 2230449750u, 2236497113u, 2242560872u, 2248641071u,
    2254737756u, 2260850970u, 2266980759u, 2273127167u, 2279290240u, 2285470023u,
    2291666561u, 2297879899u, 2304110083u, 2310357160u, 2316621173u, 2322902170u,
    2329200197u, 2335515300u, 2341847524u, 2348196917u, 2354563524u, 2360947394u,
    2367348571u, 2373767105u, 2380203040u, 2386656425u, 2393127307u, 2399615734u,
    2406121752u, 2412645410u, 2419186755u, 2425745836u, 2432322700u, 2438917396u,
    2445529972u, 2452160476u, 2458808958u, 2465475465u, 2472160047u, 2478862753u,
    2485583632u, 2492322733u, 2499080105u, 2505855799u, 2512649863u, 2519462348u,
    2526293303u, 2533142779u, 2540010826u, 2546897494u, 2553802834u, 2560726895u,
    2567669730u, 2574631389u, 2581611923u, 2588611383u, 2595629821u, 2602667287u,
    2609723834u, 2616799513u, 2623894376u, 2631008476u, 2638141863u, 2645294591u,
    2652466713u, 2659658279u, 2666869345u, 2674099961u, 2681350181u, 2688620059u,
    2695909648u, 2703219000u, 2710548171u, 2717897212u, 2725266179u, 2732655125u,
    2740064105u, 2747493172u, 2754942382u, 2762411789u, 2769901447u, 2777411411u,
    2784941738u, 2792492481u, 2800063696u, 2807655439u, 2815267765u, 2822900730u,
    2830554390u, 2838228802u, 2845924021u, 2853640104u, 2861377107u, 2869135087u,
    2876914102u, 2884714207u, 2892535461u, 2900377920u, 2908241642u, 2916126685u,
    2924033107u, 2931960965u, 2939910317u, 2947881223u, 2955873740u, 2963887926u,
    2971923842u, 2979981545u, 2988061094u, 2996162550u, 3004285971u, 3012431416u,
    3020598946u, 3028788621u, 3037000500u, 3045234644u, 3053491112u, 3061769967u,
    3070071267u, 3078395075u, 3086741451u, 3095110456u, 3103502151u, 3111916599u,
    3120353861u, 3128813999u, 3137297074u, 3145803149u, 3154332287u, 3162884549u,
    3171459999u, 3180058700u, 3188680714u, 3197326104u, 3205994934u, 3214687269u,
    3223403170u, 3232142703u, 3240905930u, 3249692918u, 3258503729u, 3267338429u,
    3276197082u, 3285079754u, 3293986508u, 3302917412u, 3311872529u, 3320851927u,
    3329855670u, 3338883824u, 3347936457u, 3357013633u, 3366115421u, 3375241885u,
    3384393094u, 3393569115u, 3402770014u, 3411995860u, 3421246719u, 3430522660u,
    3439823750u, 3449150059u, 3458501653u, 3467878602u, 3477280975u, 3486708840u,
    3496162267u, 3505641324u, 3515146082u, 3524676610u, 3534232978u, 3543815256u,
    3553423514u, 3563057822u, 3572718252u, 3582404874u, 3592117759u, 3601856978u,
    3611622603u, 3621414705u, 3631233356u, 3641078629u, 3650950594u, 3660849326u,
    3670774895u, 3680727376u, 3690706840u, 3700713361u, 3710747013u, 3720807869u,
    3730896002u, 3741011488u, 3751154399u, 3761324810u, 3771522796u, 3781748432u,
    3792001792u, 3802282952u, 3812591987u, 3822928972u, 3833293984u, 3843687099u,
    3854108391u, 3864557939u, 3875035819u, 3885542107u, 3896076880u, 3906640216u,
    3917232192u, 3927852886u, 3938502376u, 3949180739u, 3959888054u, 3970624400u,
    3981389855u, 3992184498u, 4003008408u, 4013861665u, 4024744348u, 4035656537u,
    4046598312u, 4057569753u, 4068570940u, 4079601955u, 4090662878u, 4101753790u,
    4112874773u, 4124025908u, 4135207276u, 4146418961u, 4157661043u, 4168933606u,
    4180236731u, 4191570503u, 4202935003u, 4214330316u, 4225756525u, 4237213713u,
    4248701965u, 4260221365u, 4271771996u, 4283353945u
};
/* round(4096 * sqrt(i + 0.5)), i = 64..255 */
static const u16 AZ_SQRT_SEED[192] = {
    32896, 33150, 33402, 33652, 33900, 34147, 34392, 34635, 34876, 35116, 35354, 35590,
    35825, 36059, 36291, 36521, 36750, 36978, 37204, 37429, 37652, 37874, 38095, 38315,
    38533, 38750, 38966, 39181, 39394, 39606, 39818, 40028, 40237, 40445, 40652, 40857,
    41062, 41266, 41469, 41671, 41871, 42071, 42270, 42468, 42665, 42861, 43057, 43251,
    43445, 43637, 43829, 44020, 44210, 44400, 44588, 44776, 44963, 45149, 45334, 45519,
    45703, 45886, 46069, 46250, 46431, 46612, 46791, 46970, 47149, 47326, 47503, 47679,
    47855, 48030, 48204, 48378, 48551, 48723, 48895, 49067, 49237, 49407, 49577, 49746,
    49914, 50082, 50249, 50416, 50582, 50747, 50912, 51077, 51241, 51404, 51567, 51730,
    51892, 52053, 52214, 52374, 52534, 52694, 52853, 53011, 53169, 53327, 53484, 53640,
    53797, 53952, 54108, 54262, 54417, 54571, 54724, 54877, 55030, 55182, 55334, 55485,
    55636, 55787, 55937, 56087, 56236, 56385, 56534, 56682, 56830, 56977, 57124, 57271,
    57417, 57563, 57709, 57854, 57999, 58143, 58287, 58431, 58574, 58717, 58860, 59002,
    59144, 59286, 59427, 59568, 59709, 59849, 59989, 60129, 60268, 60407, 60546, 60684,
    60822, 60960, 61098, 61235, 61372, 61508, 61644, 61780, 61916, 62051, 62186, 62321,
    62456, 62590, 62724, 62857, 62991, 63124, 63256, 63389, 63521, 63653, 63785, 63916,
    64047, 64178, 64309, 64439, 64569, 64699, 64828, 64957, 65086, 65215, 65344, 65472
};
/* END GENERATED TABLES */

/* ---------------------------------------------------------------------- */
/* scalar helpers                                                          */
/* ---------------------------------------------------------------------- */

/* floor(sqrt(x)), exact.
 * x is shifted left by an even count so that 2^62 <= m < 2^64; the top
 * eight bits of m select a seed with about 8 correct bits; two Newton steps
 * give 32. A Newton step from any positive start never lands below
 * floor(sqrt(m)), so only a downward correction is needed. */
u32 az_isqrt_u64(u64 x)
{
    if (x == 0) return 0;
    u32 lz = (u32)__builtin_clzll(x) & ~1u;
    u64 m = x << lz;
    u64 r = (u64)AZ_SQRT_SEED[(m >> 56) - 64u] << 16;
    r = (r + m / r) >> 1;
    r = (r + m / r) >> 1;
    if (r > 0xFFFFFFFFull) r = 0xFFFFFFFFull;
    while (r * r > m) r--;
    return (u32)(r >> (lz >> 1));
}

/* 16384 * sin(angle), angle in 1/65536 turn. Quarter-wave table with
 * linear interpolation (64 angle units per table step). */
i32 az_sin_q14(u32 angle)
{
    u32 a = angle & 0xFFFFu;
    u32 neg = a >> 15;                       /* second half turn */
    a &= 0x7FFFu;
    if (a > 16384u) a = 32768u - a;          /* mirror about the quarter turn */
    u32 i = a >> 6, f = a & 63u;
    i32 s = AZ_SIN_Q14[i];
    if (f) s += ((AZ_SIN_Q14[i + 1] - s) * (i32)f + 32) >> 6;
    return neg ? -s : s;
}

i32 az_cos_q14(u32 angle) { return az_sin_q14(angle + 16384u); }

/* 256 * log2(x), rounded to nearest; 0 for x = 0. The fraction is capped at
 * 255 so that the integer part is always floor(log2(x)). */
u32 az_log2_q8(u32 x)
{
    if (x == 0) return 0;
    u32 lz = (u32)__builtin_clz(x);
    u32 m = x << lz;                         /* bit 31 set */
    u32 i = (m >> 23) & 0xFFu;               /* 8 bits below the leading one */
    u32 f = (m >> 7) & 0xFFFFu;              /* the next 16 bits */
    u32 lo = AZ_LOG2_Q15[i], hi = AZ_LOG2_Q15[i + 1];
    u32 q15 = lo + (((hi - lo) * f + 32768u) >> 16);
    u32 frac = (q15 + 64u) >> 7;
    if (frac > 255u) frac = 255u;
    return ((31u - lz) << 8) + frac;
}

/* 2^(e/256), rounded to nearest; 0xFFFFFFFF once the true value needs more
 * than 32 bits (e >= 8192). */
u32 az_exp2_q8(u32 e)
{
    u32 ip = e >> 8;
    if (ip >= 32u) return 0xFFFFFFFFu;
    u32 m = AZ_EXP2_Q31[e & 255u];           /* 2^31 * 2^frac */
    u32 sh = 31u - ip;
    if (sh == 0) return m;
    return (m >> sh) + ((m >> (sh - 1u)) & 1u);
}

/* num / den rounded half up, saturating at 0xFFFFFFFF. No intermediate
 * overflow for any num. den = 0 is outside the contract; it saturates
 * (0 for 0/0) instead of trapping. */
u32 az_div_round(u64 num, u32 den)
{
    if (den == 0) return num ? 0xFFFFFFFFu : 0u;
    u64 q = num / den, rem = num % den;
    if (rem >= den - rem) q++;
    return q > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)q;
}

/* floor(log2(mass + 1)) capped at 15 across the full u32 mass range. */
u32 az_radius_code(u32 mass)
{
    if (mass >= (1u << 15) - 1u) return 15u;
    return 31u - (u32)__builtin_clz(mass + 1u);
}

/* Four tetrahedral receptor channels; same results as the reference stub.
 * A shift of 63 or more gives 0 on every channel. */
void az_tetra4(const i32 v[3], u32 shift, u8 out[4])
{
    i64 x = v[0], y = v[1], z = v[2];
    i64 d[4] = { x + y + z, x - y - z, -x + y - z, -x - y + z };
    u32 s = shift > 63u ? 63u : shift;
    for (u32 k = 0; k < 4; k++) {
        i64 r = d[k] > 0 ? d[k] >> s : 0;
        out[k] = (u8)(r > 127 ? 127 : r);
    }
}

/* ---------------------------------------------------------------------- */
/* rotors                                                                   */
/* ---------------------------------------------------------------------- */

AZ_INLINE u64 fx_mag(i64 v) { return (u64)(v < 0 ? -v : v); }

/* One component of fx_unit4: floor((mag * 2^up + s/2) / s) with the sign
 * of q, by one multiplication with inv = floor(2^48 / s) and a remainder
 * check. x < (s + 1) * (2^14 + 1/2) and inv <= 2^48 / s, so x * inv < 2^63,
 * and the estimate is the exact quotient or one less (x / 2^48 < 1/2). */
AZ_INLINE i16 fx_unit1(u64 mag, i64 q, u32 up, u64 s, u64 inv)
{
    u64 x = (mag << up) + (s >> 1);
    u64 v = (x * inv) >> 48;
    if (x - v * s >= s) v++;
    if (v > 16384u) v = 16384u;
    return (i16)(q < 0 ? -(i32)v : (i32)v);
}

/* out = 16384 * q / |q|, every component rounded to nearest (ties away from
 * zero). Needs |q[k]| <= 2^32. A zero q gives the identity rotor.
 * Components of 2^30 and more (not reached from unit rotors with angular
 * velocities below one radian per tick) are first divided by 8, rounded,
 * which keeps 27 bits. The squared norm is then shifted left by an even
 * count into [2^62, 2^64), so its integer square root s has 32 significant
 * bits: s = floor(|q| * 2^h), and mag * 2^h <= s for every component.
 * Written component by component on purpose: everything stays in general
 * registers (a loop over small arrays makes gcc build 64-bit vector
 * multiplies and reload four scalar stores as one vector, which stalls). */
AZ_INLINE void fx_unit4(i16 out[4], i64 q0, i64 q1, i64 q2, i64 q3)
{
    u64 m0 = fx_mag(q0), m1 = fx_mag(q1), m2 = fx_mag(q2), m3 = fx_mag(q3);
    u64 any = m0 | m1 | m2 | m3;
    if (any == 0) { out[0] = 16384; out[1] = 0; out[2] = 0; out[3] = 0; return; }
    if (any >> 30) {
        m0 = (m0 + 4u) >> 3; m1 = (m1 + 4u) >> 3; m2 = (m2 + 4u) >> 3; m3 = (m3 + 4u) >> 3;
    }
    u64 n2 = m0 * m0 + m1 * m1 + m2 * m2 + m3 * m3;          /* < 2^62 */
    u32 sh = (u32)__builtin_clzll(n2) & ~1u;
    u64 s = az_isqrt_u64(n2 << sh);                          /* 2^31 <= s < 2^32 */
    u64 inv = ((u64)1 << 48) / s;
    u32 up = 14u + (sh >> 1);
    i16 o0 = fx_unit1(m0, q0, up, s, inv), o1 = fx_unit1(m1, q1, up, s, inv);
    i16 o2 = fx_unit1(m2, q2, up, s, inv), o3 = fx_unit1(m3, q3, up, s, inv);
    out[0] = o0; out[1] = o1; out[2] = o2; out[3] = o3;
}

/* Renormalise to a Q1.14 unit rotor; the identity for a zero input. */
void az_rotor_norm(i16 r[4])
{
    fx_unit4(r, r[0], r[1], r[2], r[3]);
}

/* out = normalise(a * b) (Hamilton product), so that
 * az_rotor_apply(out, v) = az_rotor_apply(a, az_rotor_apply(b, v)).
 * The product is formed exactly and normalised before rounding. out may
 * alias a or b. */
void az_rotor_mul(i16 out[4], const i16 a[4], const i16 b[4])
{
    i64 a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];
    i64 b0 = b[0], b1 = b[1], b2 = b[2], b3 = b[3];
    fx_unit4(out,
             a0 * b0 - a1 * b1 - a2 * b2 - a3 * b3,
             a0 * b1 + a1 * b0 + a2 * b3 - a3 * b2,
             a0 * b2 - a1 * b3 + a2 * b0 + a3 * b1,
             a0 * b3 + a1 * b2 - a2 * b1 + a3 * b0);
}

/* One component of fx_rotate: acc / n2 rounded to nearest (ties away from
 * zero), saturated to an i32. (A division-free variant for near-unit rotors,
 * 2^88 / n2 by a three-term series and a 128-bit product, was measured and
 * was not faster than the plain division on the target machine.) */
AZ_INLINE i32 fx_rot1(u64 acc, u64 n2)
{
    i64 s = (i64)acc;
    u64 qv = ((s < 0 ? 0u - acc : acc) + (n2 >> 1)) / n2;
    if (qv > 0x7FFFFFFFull) qv = 0x7FFFFFFFull;
    return s < 0 ? -(i32)qv : (i32)qv;
}

/* out = M v / |q|^2, where M = |q|^2 * R(q) is the exact integer rotation
 * matrix of the rotor (w, a, b, c). Dividing by the rotor's own squared
 * norm makes the map an exact rotation for any rotor length, so only the
 * final rounding (nearest, ties away from zero) is lost. Results that do
 * not fit an i32 saturate. A zero rotor acts as the identity. out may
 * alias v. Exact for every i32 vector while |q|^2 <= 2^31 (twice the unit
 * length and more); beyond that the sums wrap (defined, not meaningful). */
static void fx_rotate(i32 out[3], i64 w, i64 a, i64 b, i64 c, const i32 v[3])
{
    i64 ww = w * w, aa = a * a, bb = b * b, cc = c * c;
    u64 n2 = (u64)(ww + aa + bb + cc);
    u64 x = (u64)(i64)v[0], y = (u64)(i64)v[1], z = (u64)(i64)v[2];
    if (n2 == 0) { out[0] = v[0]; out[1] = v[1]; out[2] = v[2]; return; }
    i64 ab = a * b, ac = a * c, bc = b * c, wa = w * a, wb = w * b, wc = w * c;
    u64 acc0 = (u64)(ww + aa - bb - cc) * x + (u64)(2 * (ab - wc)) * y + (u64)(2 * (ac + wb)) * z;
    u64 acc1 = (u64)(2 * (ab + wc)) * x + (u64)(ww - aa + bb - cc) * y + (u64)(2 * (bc - wa)) * z;
    u64 acc2 = (u64)(2 * (ac - wb)) * x + (u64)(2 * (bc + wa)) * y + (u64)(ww - aa - bb + cc) * z;
    i32 o0 = fx_rot1(acc0, n2), o1 = fx_rot1(acc1, n2), o2 = fx_rot1(acc2, n2);
    out[0] = o0; out[1] = o1; out[2] = o2;
}

/* body frame -> world frame: v' = q v q* */
void az_rotor_apply(i32 out[3], const i16 r[4], const i32 v[3])
{
    fx_rotate(out, r[0], r[1], r[2], r[3], v);
}

/* world frame -> body frame: v' = q* v q */
void az_rotor_apply_inv(i32 out[3], const i16 r[4], const i32 v[3])
{
    fx_rotate(out, r[0], -(i64)r[1], -(i64)r[2], -(i64)r[3], v);
}

/* One tick of a world-frame angular velocity ang (Q1.14 radians per tick):
 * R <- normalise(R + (1/2) (0, ang) R). The sum is formed exactly at scale
 * 2^29 and normalised before rounding. The rotation per tick is
 * 2 atan(|ang| / 2) about ang: |ang| to second order. */
void az_rotor_step(i16 r[4], const i16 ang[3])
{
    i64 w = r[0], a = r[1], b = r[2], c = r[3];
    i64 x = ang[0], y = ang[1], z = ang[2];
    fx_unit4(r,
             w * 32768 - x * a - y * b - z * c,
             a * 32768 + x * w + y * c - z * b,
             b * 32768 - x * c + y * w + z * a,
             c * 32768 + x * b - y * a + z * w);
}
