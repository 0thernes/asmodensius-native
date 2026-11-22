/* az_abi.h - Asmodensius-Native core ABI, version 4 (the wave-2 freeze).
 *
 * IF YOUR BUILD JUST BROKE on names like engram, memory, az_delta_ns or
 * AzConfig: the contract moved from version 3 to 4. Read
 * docs/agents/ABI-V4-NOTICE.md (what changed, who owns what, and every
 * contested decision). Version 2 to 3 is in ABI-V3-NOTICE.md.
 *
 * READ docs/ARCHITECTURE.md FIRST. This header is the contract every module
 * is written against. Only the project lead changes it.
 * WHO READS WHICH TRAIT and WHO HONOURS WHICH LESION is data: az_owners.def.
 *
 * Freestanding C23, integers only. No libc, no OS, no floating point in core/.
 */
#ifndef AZ_ABI_H
#define AZ_ABI_H

#include "az_types.h"

#define AZ_ABI_VERSION 4u

_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "little-endian target required (state hash, snapshots)");

/* ====================================================================== */
/* 1. Dimensions                                                          */
/* ====================================================================== */

#define AZ_NONE          0xFFFFFFFFu  /* "no slot", "no cell"                    */
#define AZ_SENS          64           /* sensory channels (u8, 0..127)           */
#define AZ_MOT           24           /* motor channels (i8, -127..127)          */
#define AZ_MODG          8            /* modulation lanes; hidden h -> h & 7     */
#define AZ_CORE_FAMS     80           /* gene families the development map reads; */
                                      /* a genome may hold any number of genes of */
                                      /* any family (az_genome.c)                 */
#define AZ_UNIT          64u          /* creature store granule: one cache line   */
#define AZ_TRAITS        112
#define AZ_PHEN          16
#define AZ_NSPECIES      8            /* field species                           */
#define AZ_NDIET         8            /* uptake channels                         */
#define AZ_NINV          4            /* inventory resources                     */
#define AZ_NMARK         4            /* stigmergy channels                      */
#define AZ_NSTAT         16
#define AZ_NGLOBAL       64           /* world-level module state words          */
#define AZ_MAX_WORKERS   64
#define AZ_MAX_PARTS     1024         /* most parts one parallel region is cut into */
/* per-worker scratch block (bytes; see az_ws_scan, az_ws_tick, az_scratch) */
#define AZ_WS_SCAN_BYTES   8192
#define AZ_WS_TICK_BYTES   8192
#define AZ_WS_MODULE_BYTES 16384
#define AZ_WS_BYTES        (AZ_WS_SCAN_BYTES + AZ_WS_TICK_BYTES + AZ_WS_MODULE_BYTES)
#define AZ_NEIGH_MAX     64           /* neighbours considered per entity        */
#define AZ_CHUNK         128          /* entity slots per cache block            */
/* No cap on a brood, a pool, an inventory or an energy store is compiled in:
 * what a body can hold follows from its parts and from the world parameter
 * CELL_HOLD (az_params.def); what a mother can bear, from her matter. */
#define AZ_VEL_MAX       16000        /* |velocity| per axis, 1/65536 cell per tick */
#define AZ_HOP_PATTERNS  4            /* 64-spin patterns in a creature's working memory (column engram, in the  */
                                      /* fixed record). Its long-term memory has any length: az_memory()         */
#define AZ_SUB_BITS      14           /* neighbour space: 1 cell edge = 1<<14    */
#define AZ_SUB_ONE       (1 << AZ_SUB_BITS)

_Static_assert(AZ_SENS % 8 == 0 && AZ_MOT % 8 == 0, "brain dims must fit the kernel layout");

/* ====================================================================== */
/* 2. Enumerations                                                        */
/* ====================================================================== */

/* entity flags (HOT column `flags`) */
enum {
    AZ_F_ALIVE   = 0x01,
    AZ_F_EGG     = 0x02,  /* dormant propagule: no brain, no motion, low cost  */
    AZ_F_JUV     = 0x04,  /* juvenile: cannot reproduce                        */
    AZ_F_SENESC  = 0x08,  /* senescent                                         */
    AZ_F_INIT    = 0x10,  /* initiator role this tick (else responder)         */
    AZ_F_COOP    = 0x20,  /* last resolved interaction was cooperative         */
    AZ_F_RECEPT  = 0x40,  /* mate-receptive                                    */
    AZ_F_DORMANT = 0x80   /* cyst: weather-proof, passive motion                    */
};

/* public snapshot flags: AzPub.rf low 4 bits; the high 4 bits are the size code */
enum { AZ_PF_RECEPT = 1, AZ_PF_COOP = 2, AZ_PF_PASSIVE = 4 /* egg or dormant */, AZ_PF_JUV = 8 };
#define AZ_PUB_SIZE(rf) ((u32)(rf) >> 4)   /* size code 0..15 of a public record */

/* Life stages (ABI 4). The stage of an entity is its flags plus the WARM
 * column birth_tick; there is no stage column and no HOT age byte (the fixed
 * record read every tick stays at 124 bytes):
 *   egg       ALIVE|EGG      no brain, no motion; hatches into a juvenile
 *   juvenile  ALIVE|JUV      cannot reproduce or be mate-receptive
 *   adult     ALIVE          RECEPT is set while it is ripe (see below)
 *   senescent ALIVE|SENESC   an adult past its prime: rising hazard
 *   dormant   ...|DORMANT    a cyst, at any stage after the egg; no brain
 * az_life.c is the only writer of ALIVE, EGG, JUV, SENESC, DORMANT and
 * RECEPT. It moves an entity between stages in a phased check (az_phase_due)
 * inside az_life_range, where it may read the WARM traits. RECEPT means
 * "ripe": an adult whose stores are above its own reproduction threshold; the
 * mate module reads it (flags and AZ_PF_RECEPT), az_life_brood clears it.
 * AZ_F_INIT belongs to the grid, AZ_F_COOP to az_social.c. */

/* lifecycle request bits (AzPair.req): a phase-3 hook sets them for its own
 * slot; az_life_range, the last hook of the block, applies them */
enum { AZ_RQ_DIE = 1, AZ_RQ_HATCH = 2 /* an egg or a cyst of this slot wakes now */,
       AZ_RQ_GROW = 4 /* one more body part (az_part_request; az_store_commit adds it) */,
       AZ_RQ_SHED = 8 /* give up a body part (az_part_shed_request); its matter must already be moved out */ };
/* bits 4..7 of req: the cause of a requested death, AZ_DIE_* (az_request_death) */

/* interaction intents (AzPair.act) */
enum {
    AZ_ACT_NONE = 0,
    AZ_ACT_SHARE,    /* give energy / matter to the target           (social) */
    AZ_ACT_GROOM,    /* remove toxin from target, build reputation   (social) */
    AZ_ACT_TEACH,    /* target copies part of my learned modulation  (social) */
    AZ_ACT_RECRUIT,  /* invite target into my colony                 (social) */
    AZ_ACT_ATTACK,   /* contest; winner takes matter                 (social) */
    AZ_ACT_STEAL,    /* take inventory; responder may retaliate      (social) */
    AZ_ACT_TRADE,    /* exchange inventory at a bargained price      (econ)   */
    AZ_ACT_MATE,     /* courtship; responder chooses                 (mate)   */
    AZ_ACT_COUNT
};

/* Sensory channels: all u8 in 0..127. A direction is encoded on four
 * tetrahedral receptor axes (az_tetra4), so every channel is non-negative.
 * "AHEAD" channels are the forward component of a direction in the body
 * frame, 0 when the thing is behind. */
enum {
    /* interoception and local environment */
    AZ_S_ENERGY = 0, AZ_S_HEALTH, AZ_S_PAIN, AZ_S_CROWD,
    AZ_S_LIGHT, AZ_S_HOT, AZ_S_COLD, AZ_S_MEDIUM,
    AZ_S_TOXIN, AZ_S_PRESS, AZ_S_AGE, AZ_S_CARRY,
    AZ_S_READY, AZ_S_WASTE, AZ_S_SIG_A, AZ_S_SIG_B,
    /* four directions, four tetrahedral receptors each */
    AZ_S_FOOD,   AZ_S_FOOD_1,   AZ_S_FOOD_2,   AZ_S_FOOD_3,     /* gradient of what I can eat   */
    AZ_S_THREAT, AZ_S_THREAT_1, AZ_S_THREAT_2, AZ_S_THREAT_3,   /* larger strangers             */
    AZ_S_PREY,   AZ_S_PREY_1,   AZ_S_PREY_2,   AZ_S_PREY_3,     /* smaller or passive strangers */
    AZ_S_KIN,    AZ_S_KIN_1,    AZ_S_KIN_2,    AZ_S_KIN_3,      /* kin and colony-mates         */
    /* specific others */
    AZ_S_MATE_NEAR, AZ_S_MATE_AHEAD, AZ_S_BOND_NEAR, AZ_S_BOND_AHEAD,
    AZ_S_FAMILY_NEAR, AZ_S_FAMILY_AHEAD, AZ_S_HOME, AZ_S_HOME_AHEAD,
    /* body and context */
    AZ_S_TOUCH, AZ_S_ALIGN, AZ_S_FLOW, AZ_S_UP,
    AZ_S_RANK, AZ_S_DAY, AZ_S_NOISE, AZ_S_AROUSAL,
    /* gradients and vector fields, as their forward component (AHEAD) or the
     * component to the body's left (SIDE): 64 = none, above = ahead / left */
    AZ_S_GRAD_T, AZ_S_LIGHT_AHEAD, AZ_S_GRAV_AHEAD, AZ_S_FLOW_AHEAD,
    AZ_S_SIGA_AHEAD, AZ_S_SIGA_SIDE, AZ_S_SIGB_AHEAD, AZ_S_SIGB_SIDE,
    /* the levels above and below the creature */
    AZ_S_COLONY,        /* the colony's shared signal channel 0 (AzColony.signal) */
    AZ_S_COLONY_NEED,   /* how much the colony wants my role                      */
    AZ_S_MASS,          /* my body matter, logarithmic                            */
    AZ_S_PARTS,         /* how many parts my body has, logarithmic                */
    /* the world's slow and wild variables, and the games */
    AZ_S_SEASON, AZ_S_STORM,
    AZ_S_PAYOFF,        /* what my last interaction gained or lost me: 64 = nothing */
    AZ_S_STAKE          /* what the neighbour I face has staked (its offer)       */
};
_Static_assert(AZ_S_STAKE == AZ_SENS - 1, "sensory enum must have AZ_SENS entries");

/* motor channels: all i8 in -127..127 */
enum {
    AZ_M_FWD = 0, AZ_M_LAT, AZ_M_VERT,        /* thrust in the body frame       */
    AZ_M_PITCH, AZ_M_YAW, AZ_M_ROLL,          /* turn rate (bivector)           */
    AZ_M_EAT, AZ_M_EXCRETE,                   /* ingest / deposit drives        */
    AZ_M_SIG_A, AZ_M_SIG_B,                   /* signal emission                */
    AZ_M_AFFIL, AZ_M_AGON, AZ_M_MATE,         /* social drives                  */
    AZ_M_REPRO, AZ_M_REST, AZ_M_GATE,         /* bud, dormancy, learning gate   */
    AZ_M_SIG_AIM,                             /* signal forward (+) or all round (-) */
    AZ_M_IMPROV,                              /* how much of its own noise the creature lets into what it does */
    AZ_M_STAKE,                               /* how much it puts at risk in a game: a continuous strategy */
    AZ_M_GIVE,                                /* give to (+) or take from (-) the colony's pools */
    AZ_M_GROW, AZ_M_SHED,                     /* grow a body part / resorb one  */
    AZ_M_COLONY_SIG,                          /* what it writes to the colony's shared channel */
    AZ_M_BUILD                                /* change the place it lives in (marks, deposits) */
};
_Static_assert(AZ_M_BUILD == AZ_MOT - 1, "motor enum must have AZ_MOT entries");

/* hot phenotype scalars (HOT column `phen`), derived from the genome at birth */
enum {
    AZ_P_METAB = 0,  /* basal metabolic rate                                   */
    AZ_P_THRUST,     /* thrust gain                                            */
    AZ_P_DRAG,       /* drag coefficient                                       */
    AZ_P_DENS,       /* body density (buoyancy against the medium)             */
    AZ_P_DIET_L,     /* affinity: light (phototrophy)                          */
    AZ_P_DIET_X,     /* affinity: reduced mineral NUT_X (chemotrophy)          */
    AZ_P_DIET_Y,     /* affinity: organic matter ORG_Y (heterotrophy)          */
    AZ_P_DIET_W,     /* affinity: waste WST_W (decomposition)                  */
    AZ_P_TOXRES,     /* toxin resistance                                       */
    AZ_P_TOPT,       /* optimal temperature                                    */
    AZ_P_TTOL,       /* temperature tolerance                                  */
    AZ_P_SENSE,      /* sensory range, 255 = one full cell                     */
    AZ_P_AGGR,       /* agonistic bias                                         */
    AZ_P_SOCIAL,     /* affiliative bias                                       */
    AZ_P_MATE,       /* low 3 bits mating mode AZ_MM_*, bit 3 sex, bits 4..7 choosiness */
    AZ_P_KIN         /* kin-recognition window on tag distance                 */
};

/* quantitative traits (WARM column `trait`), each u8. Every trait must be
 * read by simulation code and must change outcomes when knocked out.
 * az_owners.def names, for every trait, the module that reads it and makes it
 * matter, and how the value reaches that module:
 *   through a HOT phen scalar (AZ_P_*) or the tag, for every-tick use, or
 *   from the WARM row trait[], which rule D8 allows only on a pair path
 *   (score, resolve), at birth and death, in Tier 1, in a serial pass, or on a
 *   phased branch (az_phase_due with a period of at least 4 ticks).
 * tools/lint_core.py checks the table against the code; tests/test_traits.c
 * clamps each trait low and high and requires a different outcome. */
enum {
    /* morphology and physiology */
    AZ_T_SIZE = 0, AZ_T_DENSITY, AZ_T_DRAG, AZ_T_THRUST, AZ_T_ARMOR, AZ_T_WEAPON,
    AZ_T_SENSE_RANGE, AZ_T_SENSE_ACUITY, AZ_T_METAB, AZ_T_EFFIC, AZ_T_TOPT, AZ_T_TTOL,
    AZ_T_TOXRES, AZ_T_PRESS_TOL, AZ_T_RAD_TOL, AZ_T_LIFESPAN,
    /* diet and metabolism */
    AZ_T_DIET_LIGHT, AZ_T_DIET_X, AZ_T_DIET_Y, AZ_T_DIET_W, AZ_T_OXI_USE, AZ_T_PREDATION,
    AZ_T_STORE_CAP, AZ_T_TOX_SECRETE, AZ_T_WASTE_RATE, AZ_T_BUILD, AZ_T_GROWTH, AZ_T_DORMANCY,
    /* life history and reproduction */
    AZ_T_MATING_MODE, AZ_T_REPRO_THRESH, AZ_T_CLUTCH, AZ_T_PROVISION, AZ_T_EGG_TIME,
    AZ_T_MATURITY, AZ_T_DISPERSAL, AZ_T_CARE, AZ_T_SEX_BIAS, AZ_T_MUT_RATE, AZ_T_RECOMB,
    AZ_T_SELFING,
    /* social dispositions */
    AZ_T_AGGR, AZ_T_SOCIAL, AZ_T_ALTRUISM, AZ_T_KIN_BIAS, AZ_T_RECIPROCITY, AZ_T_FORGIVE,
    AZ_T_PUNISH, AZ_T_STEAL, AZ_T_TRUST, AZ_T_DOMINANCE, AZ_T_SUBMISSION, AZ_T_TERRITORIAL,
    AZ_T_COLONIALITY, AZ_T_CONFORMITY, AZ_T_BOLDNESS, AZ_T_MOODINESS,
    /* mate choice and signalling */
    AZ_T_PREF_SIGNAL, AZ_T_PREF_SIZE, AZ_T_PREF_SIMILAR, AZ_T_PREF_RANK, AZ_T_CHOOSY,
    AZ_T_ORNAMENT, AZ_T_SIG_A_GAIN, AZ_T_SIG_B_GAIN, AZ_T_SIG_HONESTY, AZ_T_BOND,
    AZ_T_FIDELITY, AZ_T_GUARD,
    /* economy */
    AZ_T_VAL_0, AZ_T_VAL_1, AZ_T_VAL_2, AZ_T_VAL_3, AZ_T_HOARD, AZ_T_TRADE_DRIVE,
    AZ_T_CARRY_CAP, AZ_T_STORE_SHARE,
    /* cognition and learning */
    AZ_T_LEARN_RATE, AZ_T_EXPLORE, AZ_T_MEMORY, AZ_T_PLASTICITY, AZ_T_CTX_GAIN, AZ_T_Q_DEPTH,
    AZ_T_Q_COUPLE, AZ_T_HOPFIELD, AZ_T_ATTENTION, AZ_T_AROUSAL_GAIN, AZ_T_AROUSAL_DECAY,
    AZ_T_IMITATE,
    /* appearance (causal through recognition, mate choice and detection) */
    AZ_T_HUE, AZ_T_SAT, AZ_T_PATTERN, AZ_T_GLOW, AZ_T_SHAPE_M, AZ_T_SHAPE_N, AZ_T_SPIKES,
    AZ_T_SHEEN,
    /* what the creature is built of and how it is built (body, genome, brain) */
    AZ_T_SEGMENT,      /* parts the body is born with, and how readily it grows more */
    AZ_T_ALLOMETRY,    /* how matter is shared among the parts as the body grows  */
    AZ_T_DEV_PLAST,    /* how far the place of birth bends development             */
    AZ_T_GENE_DUP, AZ_T_GENE_DEL, AZ_T_GENE_NEW,      /* rates of genome growth, loss and novelty */
    AZ_T_NEURO_MUT, AZ_T_NEURO_GROW, AZ_T_NEURO_PRUNE,/* rates of change of the brain */
    /* responses and strategies */
    AZ_T_TAXIS,        /* how strongly it turns along a gradient                   */
    AZ_T_CURVE,        /* shape of its responses: straight (low) to saturating (high) */
    AZ_T_IMPROV,       /* amplitude of its own noise in its actions                */
    AZ_T_STAKE,        /* how much it risks in a game before it has learned better */
    AZ_T_SIG_AIM,      /* how directed its signals are                             */
    AZ_T_CASTE,        /* which role it leans to inside a colony                   */
    AZ_T_COMMONS,      /* what share of its gains it gives to the colony's pools   */
    AZ_T_COUNT
};
_Static_assert(AZ_T_COUNT == AZ_TRAITS, "trait enum must have AZ_TRAITS entries");

/* mating modes (AZ_P_MATE low 3 bits) */
enum {
    AZ_MM_BUD = 0,      /* asexual budding                                      */
    AZ_MM_SELF,         /* selfing hermaphrodite (meiosis, both gametes own)    */
    AZ_MM_HERMA,        /* outcrossing hermaphrodite                            */
    AZ_MM_PROMISC,      /* two sexes, no bond                                   */
    AZ_MM_POLYGYN,      /* two sexes, rank-skewed mating                        */
    AZ_MM_MONOG,        /* two sexes, pair bond                                 */
    AZ_MM_LEK,          /* two sexes, display-based choice                      */
    AZ_MM_EUSOC         /* breeders reproduce, workers help                     */
};

/* roles (HOT column `role`) */
enum { AZ_ROLE_NONE = 0, AZ_ROLE_FORAGER, AZ_ROLE_SOLDIER, AZ_ROLE_NURSE, AZ_ROLE_BUILDER,
       AZ_ROLE_SCOUT, AZ_ROLE_BREEDER, AZ_ROLE_TRADER, AZ_ROLE_COUNT };

/* behaviour counters (WARM column `stat`, u32); they saturate at 2^32 - 1 */
enum { AZ_ST_MATES = 0, AZ_ST_PARTNERS, AZ_ST_ATTACKS, AZ_ST_WINS, AZ_ST_LOSSES, AZ_ST_SHARES,
       AZ_ST_RECEIVED, AZ_ST_TRADES, AZ_ST_OFFSPRING, AZ_ST_TRAVEL, AZ_ST_SIGNALS, AZ_ST_STOLEN,
       AZ_ST_EAT_L, AZ_ST_EAT_X, AZ_ST_EAT_Y, AZ_ST_EAT_W };

/* Measured behavioural classes, computed on demand by
 * az_classify from the stat counters as rates over the entity's age. They
 * are measurements of what an entity did, never labels it was given. */
enum {
    AZ_ARCH_NONE = 0, AZ_ARCH_PRODUCER, AZ_ARCH_CHEMOTROPH, AZ_ARCH_GRAZER, AZ_ARCH_DECOMPOSER,
    AZ_ARCH_PREDATOR, AZ_ARCH_THIEF, AZ_ARCH_ALTRUIST, AZ_ARCH_FREELOADER, AZ_ARCH_TRADER,
    AZ_ARCH_WARRIOR, AZ_ARCH_DOMINANT, AZ_ARCH_SUBORDINATE, AZ_ARCH_HERMIT, AZ_ARCH_NOMAD,
    AZ_ARCH_SIGNALLER, AZ_ARCH_MONOGAMIST, AZ_ARCH_POLYGAMIST, AZ_ARCH_BROODER, AZ_ARCH_CELIBATE,
    AZ_ARCH_TEACHER, AZ_ARCH_BUILDER, AZ_ARCH_SWITCHER, AZ_ARCH_COUNT
};
_Static_assert(AZ_ARCH_COUNT <= 32, "at most 32 archetypes");

/* Public commitments (EPH column `offer`, AZ_OFFER bytes), written in phase 1 from
 * the entity's own start-of-tick state. In phase 3 both sides of a pair
 * compute the outcome from the two offers (plus constants and az_rand keyed
 * by the two uids), so they agree exactly without reading each other's
 * changing state. Quantities are exact and capped by what the entity holds,
 * so a transfer can never overdraw.
 *   az_social_intent runs first: it zeroes the offer, then writes bytes
 *     0, 1, 2, 4, ENERGY, MATTER and the RETALIATE / SUBMIT bits.
 *   az_econ_intent owns GIVE_RES, WANT_RES, GIVE_QTY and PRICE.
 *   az_mate_intent owns byte 3 and the RECEPTIVE / SEX / BONDED / BREEDER bits.
 * ABI 4 rules:
 *   - Every entity writes its offer every tick, so the bytes come from HOT
 *     state only (rule D8). What a trait adds (weapon, armour, ornament,
 *     valuations ...) is read at score or resolve time from the trait row of
 *     either side: `trait` is constant in phases 2 and 3, the partner's too.
 *   - ENERGY and MATTER are one commitment that serves whichever pair the entity
 *     ends up in: the gift if it gives (share), the stake if it loses (attack,
 *     steal). They never exceed what the entity holds at the start of the
 *     tick; the pairs are resolved before any other phase-3 hook runs, so the
 *     amount is still there when the pair is applied.
 *   - The trade bytes are written only by an entity that is in the market
 *     this tick; everybody else has PRICE = AZ_PRICE_NONE and GIVE_QTY = 0.
 *   - DISP bits 6 and 7 are reserved (zero). A juvenile is recognised by
 *     AZ_PF_JUV of its public record, not by an offer bit. */
enum {
    AZ_O_STRENGTH = 0,  /* u8  fighting ability                                 */
    AZ_O_SIZE     = 1,  /* u8  size code                                        */
    AZ_O_WEALTH   = 2,  /* u8  log-scale energy + inventory                     */
    AZ_O_ORNAMENT = 3,  /* u8  display intensity                                */
    AZ_O_RANK     = 4,  /* u8  dominance, 128 = neutral                         */
    AZ_O_DISP     = 5,  /* u8  AZ_OD_* bits                                     */
    AZ_O_GIVE_RES = 6,  /* u8  inventory resource I hold most spare of          */
    AZ_O_WANT_RES = 7,  /* u8  inventory resource I want                        */
    AZ_O_GIVE_QTY = 8,  /* u32 exact units of GIVE_RES I hold                   */
    AZ_O_PRICE    = 12, /* u32 reservation price, Q16.16 units wanted per unit given; AZ_PRICE_NONE = not for sale */
    AZ_O_ENERGY   = 16, /* u32 energy (eu) I commit: gift, or stake if I lose   */
    AZ_O_MATTER   = 20  /* u32 pool_a matter (mu) at stake if I lose a contest  */
                        /* bytes 24..31: strategy bytes of wave 2 (zero until a module owns them) */
};
#define AZ_OFFER      32            /* bytes of one offer */
#define AZ_PRICE_NONE 0xFFFFFFFFu
enum { AZ_OD_RETALIATE = 1, AZ_OD_RECEPTIVE = 2, AZ_OD_SEX = 4, AZ_OD_BONDED = 8,
       AZ_OD_BREEDER = 16, AZ_OD_SUBMIT = 32 };
AZ_INLINE u32  az_offer_u32(const u8 *o, u32 at)
{
    return (u32)o[at] | ((u32)o[at + 1] << 8) | ((u32)o[at + 2] << 16) | ((u32)o[at + 3] << 24);
}
AZ_INLINE void az_offer_set_u32(u8 *o, u32 at, u32 v)
{
    o[at] = (u8)v; o[at + 1] = (u8)(v >> 8); o[at + 2] = (u8)(v >> 16); o[at + 3] = (u8)(v >> 24);
}

/* field species (cell column `sp`), matter units */
enum {
    AZ_SP_SOLV = 0,  /* solvent: the liquid medium                              */
    AZ_SP_GAS,       /* carrier gas: the gaseous medium                         */
    AZ_SP_OXI,       /* oxidiser                                                */
    AZ_SP_NUT_X,     /* reduced mineral from vents and lightning                */
    AZ_SP_ORG_Y,     /* organic matter: detritus, corpses                       */
    AZ_SP_TOX_Z,     /* radiotoxin: harms, mutates, decays                      */
    AZ_SP_WST_W,     /* metabolic waste                                         */
    AZ_SP_STR_S      /* structural solid: reefs and nests                       */
};

/* phases, 2 bits per species in cell column `phase` (species k in bits 2k, 2k+1) */
enum { AZ_PH_SOLID = 0, AZ_PH_LIQUID = 1, AZ_PH_GAS = 2, AZ_PH_PLASMA = 3 };
#define AZ_PHASE_OF(phase, species) (((u32)(phase) >> (2u * (species))) & 3u)

/* stigmergy channels (cell column `mark`, az_cell_mark). Channel 0 also
 * records its owner colony (mark_id). az_chem_cell decays every channel. */
enum { AZ_MK_TERRITORY = 0,  /* social: territory; the colony id is kept in mark_id */
       AZ_MK_TRAIL,          /* econ: laid by a loaded forager on its way           */
       AZ_MK_ALARM,          /* social: laid by the loser of a contest              */
       AZ_MK_NEST };         /* econ: laid where structure is built or stored       */
_Static_assert(AZ_MK_NEST == AZ_NMARK - 1, "mark enum must have AZ_NMARK entries");

/* uptake channels (EPH `eat`, AzCellNeed.dem and the cell grant column) */
enum { AZ_D_LIGHT = 0, AZ_D_NUT_X, AZ_D_ORG_Y, AZ_D_WST_W, AZ_D_OXI, AZ_D_STR_S, AZ_D_TOX_Z, AZ_D_SOLV };

/* RNG streams: the third word of the Philox counter. A stream belongs to one
 * module, which alone decides what its idx values mean (ABI 4): INIT world;
 * ROLE grid; NOISE, SENSE sense; CONTEST, SOCIAL social; MATE mate; MEIOSIS,
 * MUTATE, CODE genome; BIRTH, LIFE life; TRADE, ECON econ; PLASTIC, TIER1,
 * QSV, MEMORY tier1; TIER2, COLONY colony; CHEM, DECAY chem; WEATHER weather;
 * WORLDGEN worldgen; CLADE brain; PHYS physics; METAB metab. */
enum {
    AZ_RS_INIT = 1, AZ_RS_ROLE, AZ_RS_NOISE, AZ_RS_CONTEST, AZ_RS_MATE, AZ_RS_MEIOSIS,
    AZ_RS_MUTATE, AZ_RS_BIRTH, AZ_RS_TRADE, AZ_RS_PLASTIC, AZ_RS_TIER1, AZ_RS_TIER2,
    AZ_RS_CHEM, AZ_RS_WEATHER, AZ_RS_DECAY, AZ_RS_WORLDGEN, AZ_RS_CLADE, AZ_RS_SOCIAL,
    AZ_RS_LIFE, AZ_RS_PHYS, AZ_RS_METAB, AZ_RS_COLONY, AZ_RS_SENSE, AZ_RS_ECON,
    AZ_RS_CODE, AZ_RS_QSV, AZ_RS_MEMORY
};

/* lesion switches (AzConfig.lesion): each disables one mechanism so that a
 * test can show the mechanism matters. A lesion must change nothing else.
 * az_owners.def names the file or files that honour each bit; the owner's
 * test shows a measured outcome changing. Bits 21..31 are unassigned and
 * AzConfig.lesion_ext holds 64 more (ABI 4: room for the per-item switches of
 * the requirement suites; az_world_create refuses any bit that has no name). */
enum {
    AZ_LESION_BRAIN      = 1u << 0,  /* motor outputs forced to zero            */
    AZ_LESION_RECURRENCE = 1u << 1,  /* hidden state cleared every tick         */
    AZ_LESION_PLASTICITY = 1u << 2,  /* no lifetime learning                    */
    AZ_LESION_SOCIAL     = 1u << 3,  /* no pair interactions at all             */
    AZ_LESION_TRADE      = 1u << 4,  /* no trade pairs, no price update         */
    AZ_LESION_MATING     = 1u << 5,  /* no sexual reproduction                  */
    AZ_LESION_MUTATION   = 1u << 6,  /* offspring genomes carry no new mutation */
    AZ_LESION_WEATHER    = 1u << 7,  /* no weather step: suns, wind, heat flow, vents and storms stand still */
    AZ_LESION_CHEM       = 1u << 8,  /* no field chemistry: reactions, diffusion, phases and mark decay stop */
    AZ_LESION_SIGNALS    = 1u << 9,  /* nobody emits: the sig column stays 0    */
    AZ_LESION_COLONY     = 1u << 10, /* no colony is founded or joined, no Tier 2 pass */
    AZ_LESION_TIER1_Q    = 1u << 11, /* classical state-vector module off       */
    AZ_LESION_TIER1_SH   = 1u << 12, /* spherical-harmonic descriptors off      */
    AZ_LESION_MEMORY     = 1u << 13, /* associative memory off                  */
    AZ_LESION_GRAVITY    = 1u << 14, /* no weight: no buoyancy or sinking of bodies, no settling of matter */
    AZ_LESION_NOISE      = 1u << 15, /* intrinsic noise input off               */
    AZ_LESION_KIN        = 1u << 16, /* kin recognition off                     */
    AZ_LESION_TOXIN      = 1u << 17, /* the radiotoxin is harmless: no uptake, damage or extra mutation */
    AZ_LESION_METRIC     = 1u << 18, /* plasticity without its metric preconditioner */
    AZ_LESION_CARE       = 1u << 19, /* no parental care                        */
    AZ_LESION_THRUST     = 1u << 22, /* suppress powered translation; angular motors remain active */
    AZ_LESION_TAXIS      = 1u << 21, /* remove the gradient gain */
    AZ_LESION_SENSES     = 1u << 20  /* all exteroceptive inputs zero           */
};
#define AZ_LESION_ALL ((AZ_LESION_THRUST << 1) - 1u)   /* every AZ_LESION_* bit       */
#define AZ_CAP_MAX    0x7FFFFFC0u                      /* largest AzConfig.cap         */

/* ====================================================================== */
/* 3. Matter and energy accounting                                        */
/* ====================================================================== */

/* Chemical energy per matter unit (eu / mu) of every form matter can take.
 * Any conversion must book the difference: released energy goes to the
 * entity's `energy` and/or to cell heat; absorbed energy must be paid.
 * az_total_matter and az_total_energy are checked exactly after every tick. */
static const i32 az_eps_species[AZ_NSPECIES] = { 0, 0, 0, 24, 16, 4, 2, 0 };
#define AZ_EPS_BIO    16   /* pool_a, pool_m, pool_t                           */
#define AZ_EPS_WASTE   2   /* entity `waste`  (same as AZ_SP_WST_W)            */
#define AZ_EPS_TOXIN   4   /* entity `toxin`  (same as AZ_SP_TOX_Z)            */

/* inventory resource k is species az_inv_species[k] */
static const u8 az_inv_species[AZ_NINV] = { AZ_SP_NUT_X, AZ_SP_ORG_Y, AZ_SP_STR_S, AZ_SP_OXI };

/* light energy (eu) one unit of cell `light` offers per tick */
#define AZ_LIGHT_EU 4u

/* Temperature: the one shared definition (ABI 4). az_weather.c writes the
 * derived cell column `temp` with it; nobody computes a temperature another
 * way. A cell that holds 8 eu of heat per mu of matter is at 8192, the
 * optimum of a creature with phen TOPT = 128.
 *   AZ_TEMP_OF_TOPT(topt)   the temperature a TOPT value stands for
 *   AZ_TEMP_TOL(ttol)       half-width of the comfortable band of a TTOL value */
AZ_INLINE i32 az_temp_of(u32 heat, u64 matter_mu)
{
    u64 t = ((u64)heat << 10) / (matter_mu + 16u);
    return t > 32767u ? 32767 : (i32)t;
}
#define AZ_TEMP_OF_TOPT(topt) ((i32)(topt) << 6)
#define AZ_TEMP_TOL(ttol)     (256 + ((i32)(ttol) << 4))

/* ====================================================================== */
/* 4. Structures                                                          */
/* ====================================================================== */
/* Rule for every struct that is hashed, saved or sent to a GPU: fixed-width
 * integer members only, explicit pad members, no implicit padding, no
 * pointers, and a size assert. */

/* Public snapshot of one entity, in cell-sorted order: everything another
 * entity may read about it during a tick. Built by az_grid_build from the
 * state at the start of the tick, never written during the tick. It is two
 * records of 32 bytes each at the same sorted position:
 *   AzPub    where it is and what it looks like: read for every candidate of
 *            a neighbour scan, so it stays half a cache line;
 *   AzPubId  who it is (64-bit identities): read only for a neighbour that
 *            is inside the range, and by the pair code. */
typedef struct AzPub {
    u16 x, y, z;     /* sub-cell position, 0 .. AZ_SUB_ONE-1                    */
    i8  vx, vy, vz;  /* velocity >> 8                                           */
    u8  rf;          /* size code (bits 4..7) | AZ_PF_* (bits 0..3)             */
    u16 tag;
    u8  sig_a, sig_b;
    i8  face[3];     /* the body's forward axis in the world frame, -127..127   */
    u8  role;        /* AZ_ROLE_* (its caste, visible to others)                */
    u8  stake;       /* previous declared energy at risk, in receptor units */
    u8  pad;
    u32 colony;      /* colony id, 0 = solitary                                 */
    u32 mass;        /* body matter (mu), exact                                 */
    u32 clade;       /* clade (lineage) id                                      */
} AzPub;
_Static_assert(sizeof(AzPub) == 32, "AzPub must be 32 bytes");
typedef struct AzPubId {
    u64 uid;
    u64 parent_a, parent_b;
    u64 partner;     /* pair-bond partner uid, 0 = none                         */
} AzPubId;
_Static_assert(sizeof(AzPubId) == 32, "AzPubId must be 32 bytes");

/* Pair-matching scratch of one entity (EPH column `pair`, 16 bytes). An
 * entity is an initiator or a responder in a tick, never both, so one record
 * serves both roles. The tick resets it; phase 1 writes act, urge and peer;
 * phase 2 writes claim and the responder's peer. In phase 3 it is frozen:
 * nobody writes act, urge, peer or claim, not even of their own slot. The
 * outcome of a pair is fixed by the two offers; neither side can cancel it. */
typedef struct AzPair {
    u64 claim;   /* responder: best (score << 32 | ~sorted position) posted by   */
                 /* claimants: of equal scores the claimant that comes first in  */
                 /* the canonical order (cell, then uid) wins                    */
    u32 peer;    /* initiator: target slot; responder: accepted claimant; AZ_NONE */
    u8  act;     /* initiator: AZ_ACT_* proposed                                 */
    u8  urge;    /* strength of the proposal now held in act / peer              */
    u8  req;     /* AZ_RQ_* bits (the only member writable in phase 3)           */
    u8  pad;
} AzPair;
_Static_assert(sizeof(AzPair) == 16, "AzPair must be 16 bytes");

/* Reproduction request of one mother (EPH column `birth`, 24 bytes), written
 * through az_birth_request in phase 3, applied by az_life_commit. */
typedef struct AzBirth {
    u32 father;    /* father slot; AZ_NONE = budding; == own slot = selfing      */
    i32 energy;    /* total energy provision, already deducted from the mother  */
    u32 prov[3];   /* total pool_a, pool_m, pool_t provision, already deducted  */
    u32 n;         /* children requested this tick, 0 = none; any number: the   */
                   /* commit makes as many as find a slot and room, and gives   */
                   /* the rest of the provision back                            */
} AzBirth;
_Static_assert(sizeof(AzBirth) == 24, "AzBirth must be 24 bytes");

/* One u32 request per slot/channel before each grant. Even the maximum
 * supported world (cap < 2^31) sums to less than 2^63 in one cell. */
typedef struct AzCellNeed {
    u64 dem[AZ_NDIET];     /* atomic sums; granted fractions occupy c_grant */
} AzCellNeed;
_Static_assert(sizeof(AzCellNeed) == 64, "AzCellNeed must be one cache line");

/* Per-cell pending changes (EPH cell column `acc`, one cache line). Zero
 * except between a post and the next az_field_commit. A post made after the
 * tick's last field commit (life commit, Tier 1, Tier 2) stays here across
 * the tick boundary until the next tick's first commit; the audits count it,
 * the snapshot saves it and az_state_hash hashes it. */
typedef struct AzCellAcc {
    i32 delta[AZ_NSPECIES];  /* pending change of each species                   */
    i32 dheat;               /* low signed limb of the pending heat              */
    u32 dmark[AZ_NMARK];     /* pending mark deposits                            */
    u32 heat_carry;          /* one unit is 2^32 eu; formerly zero padding       */
    u64 dmark_id;            /* max-combine of (strength << 32 | colony id)      */
} AzCellAcc;
_Static_assert(sizeof(AzCellAcc) == 64, "AzCellAcc must be one cache line");

_Static_assert(offsetof(AzCellAcc, dheat) == 32 && offsetof(AzCellAcc, heat_carry) == 52,
               "snapshot heat limbs retain their format-5/6 offsets");

/* Quiescent access only: field commits and observers follow the producer
 * barrier. A zero carry preserves every old byte for all i32-sized deltas,
 * including negatives. The two limbs encode modulo 2^64, like an i64
 * accounting counter. Unsigned arithmetic also covers INT64_MIN. */
AZ_INLINE i64 az_pending_heat(const AzCellAcc *a)
{
    u64 bits = ((u64)a->heat_carry << 32) + (u64)(i64)a->dheat;
    return (i64)bits;
}

AZ_INLINE void az_set_pending_heat(AzCellAcc *a, i64 value)
{
    i32 low = (i32)(u32)(u64)value;
    a->dheat = low;
    a->heat_carry = (u32)(((u64)value - (u64)(i64)low) >> 32);
}

/* Result of the neighbour scan for one entity. Offsets are in neighbour
 * space (one cell edge = AZ_SUB_ONE). Slots are entity slots or AZ_NONE.
 * Classes, in this order of precedence:
 *   col    : same non-zero colony
 *   kin    : tag distance <= my kin window (AZ_P_KIN), not a colony-mate
 *   threat : any other neighbour whose size code is larger than mine and
 *            that is not passive
 *   prey   : every other neighbour (smaller or equal, or passive) */
typedef struct AzPercept {
    u16 n_used;         /* neighbours inside my sense range that were considered */
    u16 n_touch;        /* of those, overlapping my body                         */
    u16 n_kin, n_col, n_prey, n_threat;
    i32 kin[3];         /* offset sums per class                                 */
    i32 col[3];
    i32 prey[3];
    i32 threat[3];
    i32 vel[3];         /* summed pub velocity of kin and colony-mates           */
    i32 push[3];        /* soft contact repulsion                                */
    u32 sig_a, sig_b;   /* received signal intensity (distance-attenuated)       */
    /* nearest of each kind: slot, squared distance (0xFFFFFFFF if none), offset */
    u32 near_prey, near_threat, near_kin;   /* near_kin: nearest kin or colony-mate */
    u32 best_mate;      /* nearest receptive, active, adult neighbour            */
    u32 bond;           /* the neighbour whose uid is my partner                 */
    u32 parent;         /* a neighbour whose uid is my parent_a or parent_b      */
    u32 child;          /* nearest neighbour one of whose parents is me          */
    u32 d2_prey, d2_threat, d2_kin, d2_mate, d2_bond, d2_parent, d2_child;
    i32 mate_off[3], bond_off[3], parent_off[3], child_off[3];
    /* where the signals come from: sum over the neighbours of (offset >> 6) *
     * received intensity, per channel (a vector, not just a loudness) */
    i32 sigv_a[3], sigv_b[3];
    u32 faced_d2, faced_stake; /* nearest visible forward neighbor and its frozen stake */
} AzPercept;

/* What a cell offers the senses of everything inside it. Computed once per
 * cell and tick by az_sense_cell from the cell and its six face neighbours. */
typedef struct AzCellSense {
    i64 food[4][3];     /* signed differences of two u32 face stocks, up to +/-UINT32_MAX */
    u32 here[4];        /* their amount in this cell                             */
    u32 toxin;          /* ambient radiotoxin                                    */
    u32 pressure;       /* az_cell_pressure                                      */
    i32 temp;
    u32 light;
    u32 dens, visc;     /* the medium                                            */
    i32 wind[3];
    u32 mark[AZ_NMARK];
    u32 mark_id;
    i32 mark_grad[3];   /* gradient of mark channel 0                            */
    i32 temp_grad[3];   /* gradient of the temperature                           */
    i32 light_grad[3];  /* gradient of the light                                 */
    i32 grav[3];        /* the gravity vector here (cell column grav)            */
    i32 lvec[3];        /* where the light comes from here (cell column lvec)    */
} AzCellSense;

/* One lineage: a line of descent the engine keeps a record of (who founded
 * it, from which lineage it split, how many of it live) plus the look its
 * members share. A creature's neural program is its own (AzBrain, in its
 * block); a child whose brain differs in structure from its mother's founds
 * a new lineage (az_clade_assign). */
typedef struct AzClade {
    u8  body[16];      /* species look and body plan (AZ_B_*)                  */
    u32 refcount;      /* living members                                        */
    u32 species;       /* founder species index this lineage descends from      */
    u64 founder_uid;   /* uid of the individual that founded it: its stable id  */
    u64 parent_uid;    /* founder_uid of the parent lineage, 0 for a founder species */
    u64 born_tick;
    u8  alive;         /* 1 while the record is in use                          */
    u8  pad[15];
} AzClade;
_Static_assert(sizeof(AzClade) == 64, "AzClade size is part of the ABI");

/* A creature's brain: segment AZ_SEG_BRAIN of its block. A sparse int8
 * recurrent network of any size, stored the way the kernel walks it.
 *
 *   inputs    groups of 4 bytes: group g < AZ_SENS / 4 is sensors 4g..4g+3,
 *             group AZ_SENS / 4 + q is hidden units 4q..4q+3
 *   hidden    n_hb blocks of 8 units (n_hb may be 0: a creature without
 *             interneurons acts on its motor biases alone)
 *   motors    AZ_MOT / 8 blocks of 8
 *   tile      32 weights joining one input group to one output block:
 *             weight[k * 4 + j] is output k's weight for input j of the group
 *
 * The header is followed, at the offsets it gives (from the header's first
 * byte, every array on a multiple of 8), by
 *   i32 b1[8 * n_hb]      hidden biases
 *   i32 b2[AZ_MOT]        motor biases
 *   u16 c1[n_hb]          tiles of each hidden block
 *   u16 c2[AZ_MOT / 8]    tiles of each motor block
 *   u16 in1[n_t1]         input group of each layer-1 tile; the tiles of one
 *                         block are together, in block order, and inside a
 *                         block by ascending group, no group twice
 *   u16 in2[n_t2]         hidden group of each layer-2 tile, same order rule
 *   i8  w1[n_t1][32]
 *   i8  w2[n_t2][32]
 * Segment AZ_SEG_MIND holds what the brain keeps between ticks:
 *   u8 sens[AZ_SENS]      the senses of the last step
 *   u8 hid[8 * n_hb]      hidden state, 0..127
 *   u8 hid2[8 * n_hb]     the next hidden state while a step runs
 *   AzSenseMemory          developed receptor traits and preceding social readings
 * One step (az_brain_step_c is the definition; the twins are bit-identical):
 *   x        = sens ++ hid
 *   acc[h]   = b1[h] + sum over the tiles of h's block of w * x      (wraps mod 2^32)
 *   a        = (((acc[h] >> shift1) * gain[h & 7]) >> 6) + 2 * bias[h & 7]
 *   hid2[h]  = clamp(a, 0, 127)
 *   m[k]     = b2[k] + sum over the tiles of k's block of w * hid2
 *   motor[k] = clamp(m[k] >> shift2, -127, 127)
 * The caller then copies hid2 over hid. */
typedef struct AzBrain {
    u16 n_hb;          /* hidden blocks                                        */
    u16 n_mb;          /* motor blocks: AZ_MOT / 8                             */
    u32 n_t1;          /* tiles into hidden blocks                             */
    u32 n_t2;          /* tiles into motor blocks                              */
    u8  shift1, shift2;/* right shifts after each layer, 0..15                 */
    u8  n_sg;          /* sensor input groups: AZ_SENS / 4                     */
    u8  pad;
    u8  gain[AZ_MODG]; /* gain per hidden lane h & 7, 64 = 1.0. Innate from the */
    i8  bias[AZ_MODG]; /* genome (az_develop); Tier 1 and a teach pair adapt them */
    u32 off_b1, off_b2, off_c1, off_c2, off_in1, off_in2, off_w1, off_w2;
} AzBrain;
_Static_assert(sizeof(AzBrain) == 64, "the brain header is 64 bytes (the twins use its offsets)");

/* species look and body plan bytes in AzClade.body; AZ_B_HUE..AZ_B_SHEEN are
 * species baselines that the individual appearance traits shift */
enum { AZ_B_SHAPE_M = 0, AZ_B_SHAPE_N1, AZ_B_SHAPE_N2, AZ_B_SHAPE_N3, AZ_B_ELONG, AZ_B_SEGMENTS,
       AZ_B_SYMMETRY, AZ_B_SPIKES, AZ_B_HUE, AZ_B_SAT, AZ_B_PATTERN, AZ_B_GLOW, AZ_B_SHEEN,
       AZ_B_GLYPH_A, AZ_B_GLYPH_B, AZ_B_RESERVED };

/* A colony is an organism of its own, one level above the creature: it has
 * its own pools of matter and energy (inside the exact balance), a shared
 * signal channel its members' brains write and read, castes, a mood and a
 * memory. No field of a creature limits how many members it has. */
typedef struct AzColony {
    u64 founder_uid;
    u64 born_tick;
    i64 energy;             /* the colony's own energy pool (eu); part of the energy total */
    u32 members;            /* counted by Tier 2                                */
    u32 cx, cy, cz;         /* centroid (world position)                        */
    i16 vx, vy, vz;         /* mean velocity                                    */
    u16 tag;
    u32 store[AZ_NINV];     /* its own matter pools (mu); part of the matter total */
    u32 caste[AZ_ROLE_COUNT];/* members in each role, counted by Tier 2         */
    u8  need[AZ_ROLE_COUNT];/* stimulus per role (response-threshold model)     */
    u8  signal[4];          /* the shared channels: the mean of what the members */
                            /* wrote last tick (AZ_M_COLONY_SIG); their senses   */
                            /* read channel 0 as AZ_S_COLONY                     */
    u8  alarm;
    u8  mood;
    u8  culture[8];         /* shared context prototype                         */
    u8  memory[32];         /* four shared 64-bit patterns                      */
    u8  alive;
    u8  pad[9];
} AzColony;
_Static_assert(sizeof(AzColony) == 160, "AzColony size is part of the ABI");

/* per-tick demand on a colony store, same pattern as cells: members add
 * demand in phase 1, az_colony_grant turns it into fractions, members take
 * floor(demand * grant / 65535) in phase 3. dem and grant are zeroed at the
 * start of every tick. dep and taken stay pending until az_colony_commit
 * applies them. A funded surplus above CELL_HOLD is refunded to the colony's
 * cell in that commit. An unpaid withdrawal stays
 * pending (finding v40): the audits count it, az_state_hash hashes it, and a
 * snapshot refuses the world until it is applied. Colony 0 is "no colony":
 * nothing may deposit to it or take from it, and nothing commits it. */
typedef struct AzColonyEph {
    u64 dem[AZ_NINV];       /* at most cap u32 amounts per phase and resource */
    u64 dep[AZ_NINV];
    u64 dep_e;              /* energy and voice fields retain their original offsets */
    u64 taken_e;
    u64 dem_e;
    u32 grant_e;
    u32 pad2;
    u64 taken[AZ_NINV];
    u64 sig_sum[4];
    u32 sig_n[4];
    u16 grant[AZ_NINV];
    u32 pad4[2];
} AzColonyEph;
_Static_assert(sizeof(AzColonyEph) == 192, "AzColonyEph must be three cache lines");

/* ledger event (cold). Not part of the simulation state. */
typedef struct AzEvent {
    u64 tick;
    u64 a, b;      /* who: uids (64-bit), or what the table below says           */
    i64 v;
    u32 aux;
    u16 kind;
    u16 pad;       /* zero                                                       */
} AzEvent;
_Static_assert(sizeof(AzEvent) == 40, "AzEvent size is part of the ledger format");
#define AZ_EV_BLOCK 256u   /* events per block of the world's event pool        */

/* One step of the ledger chain, the only copy (finding v55: core/az_ledger.c
 * and the sink header each kept one that had to "stay identical"). */
AZ_INLINE u64 az_ledger_fold(u64 c, const AzEvent *e)
{
    const u64 k = 0x9E3779B97F4A7C15ull;   /* keeps 0 from being a fixed point */
    c = az_mix64((c ^ e->tick) + k);
    c = az_mix64((c ^ ((u64)e->kind | ((u64)e->aux << 32))) + k);
    c = az_mix64((c ^ e->a) + k);
    c = az_mix64((c ^ e->b) + k);
    c = az_mix64((c ^ (u64)e->v) + k);
    return c;
}
enum { AZ_EV_BIRTH = 1, AZ_EV_DEATH, AZ_EV_CLADE, AZ_EV_COLONY, AZ_EV_MATE, AZ_EV_ATTACK,
       AZ_EV_TRADE, AZ_EV_STORM, AZ_EV_SPECIATE, AZ_EV_TEACH, AZ_EV_BOND, AZ_EV_TICK,
       AZ_EV_REFUSED,   /* a serial pass refused an allocation: aux = AZ_REF_* (this number */
                        /* was UID_EXHAUSTED: uids are 64-bit now and cannot run out)       */
       AZ_EV_SHARE, AZ_EV_STEAL, AZ_EV_RECRUIT, AZ_EV_HATCH, AZ_EV_STAGE,
       AZ_EV_COUNT /* one past the last kind; kind 0 is unused */ };
/* Who emits what (ABI 4). The event pool holds 2 events per slot and tick, so
 * an entity emits at most ONE event from its phase-3 hooks (its pair act or
 * its stage change) plus its death; serial passes use the world reserve.
 *   kind       emitted by                      a            b            aux, v
 *   BIRTH      az_life_commit                  child uid    mother uid   aux 1 = laid as an egg
 *   DEATH      az_life_kill                    uid          0            aux = cause (AZ_DIE_*)
 *   HATCH      az_life.c                       uid          0
 *   STAGE      az_life.c                       uid          0            aux = the new AZ_F_* stage bits
 *   MATE       az_mate.c, the bearing side     mother uid   father uid   v = clutch size; aux 1 = she also bound herself to him
 *   BOND       az_mate.c, an initiator that sired: uid      partner uid  aux 1 = formed, 0 = broken
 *   ATTACK     az_social.c, the initiator      attacker uid target uid   aux 1 = the attacker won; v = matter moved
 *   STEAL      az_social.c, the initiator      thief uid    victim uid   aux 1 = caught; v = units taken
 *   SHARE      az_social.c, the initiator      giver uid    taker uid    v = energy given (groom: aux 1)
 *   TEACH      az_social.c, the initiator      teacher uid  learner uid
 *   RECRUIT    az_social.c, the initiator      recruiter    recruit uid  aux = colony id
 *   TRADE      az_econ.c, the initiator        uid          partner uid  aux = given << 8 | received resource; v = units given
 *   COLONY     az_colony.c (serial)            founder uid  0            aux = colony id; v = 1 founded, 0 dissolved
 *   CLADE, SPECIATE  az_brain.c / az_life_commit (serial)   child uid, parent clade founder uid
 *   STORM      az_weather_global (serial)      cell         0            aux = intensity
 *   TICK       the ledger (see below)
 *   REFUSED    a serial pass, the first time in a run that a table or store
 *              refuses: aux = AZ_REF_*, a = what was asked for */
enum { AZ_REF_SLOT = 1 /* no free entity slot */, AZ_REF_CLADE, AZ_REF_COLONY, AZ_REF_STORE, AZ_REF_ENTROPY, AZ_REF_TERRAIN };
enum { AZ_DIE_UNKNOWN = 0, AZ_DIE_STARVED, AZ_DIE_WOUNDS, AZ_DIE_AGE, AZ_DIE_POISON, AZ_DIE_COLD_HEAT,
       AZ_DIE_PRESSURE, AZ_DIE_KILLED, AZ_DIE_HAZARD };
/* AZ_EV_TICK is the summary az_ledger_flush appends as the last event of
 * every tick (modules never emit it):
 *   tick  the tick it closes
 *   aux   ledger events lost so far (totals.dropped_events plus drops not yet
 *         merged), saturated at 2^32 - 1: non-zero means the chain is lossy
 *   a     events of this tick, the summary not counted
 *   b     entities alive at the end of the tick, after its deaths and births
 *         (finding v11: it used to repeat the count of the tick's grid build)
 *   v     the chain before the summary was folded in */

/* Counters a worker accumulates privately during parallel stages. The tick
 * merges them into AzTotals after each stage (integer sums commute, so the
 * totals do not depend on the thread count, and no atomics are needed). */
typedef struct AzTally {
    i64 energy_in;          /* external energy absorbed (light, geothermal)     */
    i64 energy_out;         /* energy radiated away                             */
    i64 reservoir;          /* signed change of the buried matter reservoir     */
    u64 deaths;
    u64 interactions[AZ_ACT_COUNT];
    u64 trade_units[AZ_NINV][AZ_NINV];   /* units of resource i given for j     */
    u64 sat[8];             /* AZ_SAT_*                                         */
    u64 field_underflow;
} AzTally;
enum { AZ_SAT_POOL = 0, AZ_SAT_ENERGY, AZ_SAT_VELOCITY, AZ_SAT_FIELD, AZ_SAT_MARK,
       AZ_SAT_RANK, AZ_SAT_INVENTORY, AZ_SAT_OTHER };

/* Module code never indexes w->workers. `worker` is an opaque token handed
 * to az_tally, az_emit, az_cell_post and friends (it is 0 on a GPU). */
typedef struct AzWorker {
    AzTally  tally;
    /* this worker's open block of the world's event pool (NULL when none):
     * n_events used of cap_events; dropped_events counts what found no room */
    AzEvent *events;  u32 n_events, cap_events, dropped_events;
    u8  pad[AZ_CACHELINE];
} AZ_ALIGNED(64) AzWorker;

typedef struct AzTotals {
    u64 reservoir_mu;   /* buried matter waiting to be re-emitted by vents      */
    i64 energy_in;      /* cumulative external energy input                     */
    i64 energy_out;     /* cumulative radiative loss                            */
    u64 births, deaths;
    u64 store_denied;   /* blocks refused for want of room in the creature      */
                        /* store (a birth, or a body or brain that would grow)  */
    u64 clade_denied;   /* new lineages not recorded: the lineage table was full */
    u64 colony_denied;  /* colonies not founded: the colony table was full      */
    u64 cap_denied;     /* births refused for want of a slot: every slot was    */
                        /* alive at tick start or already reused this tick (a   */
                        /* slot that dies joins the free list at the next grid  */
                        /* build; finding v42)                                  */
    u64 interactions[AZ_ACT_COUNT];
    u64 trade_units[AZ_NINV][AZ_NINV];
    u64 sat[8];         /* sticky saturation counters: a clamp changed a value  */
    u64 field_underflow;  /* a cell amount would have gone negative: must stay 0 */
    u64 dropped_events;   /* ledger events lost to a full worker buffer: must stay 0 */
    u64 entropy_missed;   /* wild ticks for which the host had no fresh entropy: */
                          /* the tick ran on the last draw stirred with the tick, */
                          /* and the first such tick is a ledger event (AZ_REF_ENTROPY) */
} AzTotals;

/* The genetic code of this world: how a diploid genome becomes a phenotype.
 * Generated once from the world seed by az_genetic_code_init, then constant. */
typedef struct AzGeneticCode {
    u8 dev_idx[AZ_TRAITS][3];  /* the three loci each trait reads                  */
    i8 dev_w[AZ_TRAITS][3];    /* their signed weights, 16 = 1.0                   */
    u8 dev_base[AZ_TRAITS];    /* trait value when all its loci are neutral (128)  */
    u8 dom[AZ_CORE_FAMS];      /* dominance per core family: 0 additive, 1 high    */
                               /* value dominant, 2 low value dominant, 3 over-    */
                               /* dominant                                         */
    u8 mod_locus[AZ_MODG * 2]; /* families of the innate mod_gain[8], mod_bias[8]  */
    u8 marker_locus[4];        /* neutral marker families: [0] sex determination,  */
                               /* [1..3] kinship; no trait and no modulation       */
                               /* family reads them (finding v09)                  */
    u8 pad[4];
    u64 latent_key;            /* what a gene family outside the core does is a    */
                               /* hash of this key and the family number           */
} AzGeneticCode;

/* The two modes of a world.
 *   AZ_MODE_WILD (0, the default: what a zeroed AzConfig asks for) is the
 *   creatures' world. Its keys are fresh entropy from the host
 *   (AzHost.entropy) when it is made, and it draws 16 more bytes at the
 *   start of every tick, which go into every random draw of that tick. No
 *   seed determines it, cfg.seed is not read, and no two runs are the same.
 *   Matter and energy still balance exactly every tick: that is physics, not
 *   repeatability. A wild world cannot be made without a host that gives
 *   entropy. Given the same entropy the tick is still the same on any number
 *   of threads, which is what lets the tests cover the same code.
 *   AZ_MODE_LAB (1) is the test bench: the world is a function of cfg.seed
 *   and nothing else, for the test suite, lesion contrasts and reference
 *   hashes. */
enum { AZ_MODE_WILD = 0, AZ_MODE_LAB = 1 };

typedef struct AzConfig {
    u64 seed;            /* lab mode only: the world is a function of it        */
    u32 cap;             /* entity slot capacity (rounded up to a multiple of 64) */
    u32 n_initial;       /* founders placed by worldgen                         */
    u32 n_species;       /* founder species count                               */
    u32 max_clades;      /* capacity of the lineage table (0 = 4096): memory, set at run time */
    u32 max_colonies;    /* capacity of the colony table (0 = 4096): memory, set at run time  */
    u32 store_units;     /* creature store: AZ_UNIT units in each of its two    */
                         /* halves (0 = 64 per slot of cap): memory, set at run  */
                         /* time; a birth that finds no room is refused and      */
                         /* recorded (totals.store_denied, AZ_REF_STORE)         */
    u8  gx_bits, gy_bits, gz_bits;   /* grid = 2^bits cells per axis; x, y >= 2 */
    u8  reorder_period;  /* ticks between physical reorders, >= 1               */
    u8  tier1_period;    /* K1: an entity gets Tier 1 when (uid + tick) % K1 == 0 */
    u8  field_stride;    /* a cell is stepped when ((x+y+z+tick) & (stride-1)) == 0; a power of two, 2..128 */
    u16 tier2_period;    /* colony pass every K2 ticks                          */
    u32 lesion;          /* AZ_LESION_* bits                                    */
    u32 lesion_ext[2];   /* 64 more switches, none named yet (ABI 4): must be 0 */
    u32 kernels;         /* 0 = C reference, 1 = assembly where available. A    */
                         /* host instruction (finding v73): the core stores and */
                         /* saves it but never reads it; a host calls           */
                         /* az_kernels_install after az_world_create or         */
                         /* az_snapshot_load when it is set (az_run, az_snap,   */
                         /* az_bench do).                                       */
    u32 debug_split_seed;/* tests only: non-zero randomises every work split; results must not change */
    u16 store_period;    /* ticks between scheduled compactions of the creature */
                         /* store (0 = the reorder period); the layout of the    */
                         /* store is not state: results must not change with it  */
    u8  mode;            /* AZ_MODE_WILD (0, the default) or AZ_MODE_LAB (1)    */
    u8  pad;
} AzConfig;
_Static_assert(sizeof(AzConfig) == 64, "AzConfig is stored raw in snapshots: no implicit padding");

typedef struct AzArena {
    u8 *base;            /* must be aligned to 4096                             */
    u64 size;
    u64 used;
} AzArena;

struct AzWorld;
typedef void (*AzJobFn)(struct AzWorld *w, u32 begin, u32 end, u32 worker);

/* Services the platform layer provides. The core never calls an OS. With
 * host == NULL the core runs single-threaded.
 * Host obligations: the arena base is aligned to 4096; AVX state is enabled
 * on every processor that runs core code (or the core is built for
 * x86-64-v2); calls use the Microsoft x64 convention with a 16-byte aligned
 * stack; each worker has at least 64 KiB of stack; parallel_for returns only
 * when every part has run. */
typedef struct AzHost {
    void *ctx;
    u32   n_workers;     /* at most AZ_MAX_WORKERS: az_world_create refuses a   */
                         /* host with more (worker tokens index workers[], v44)  */
    u32   max_parts;     /* most parts parallel_for accepts; 0 means n_workers  */
    /* Run fn(w, splits[k], splits[k+1], t) exactly once for every k in
     * 0..n_parts-1, then return. t is the index of the executing worker
     * (< n_workers). A host with max_parts > n_workers hands parts out
     * dynamically, so a slow or preempted core does not hold the region up. */
    void (*parallel_for)(void *ctx, struct AzWorld *w, AzJobFn fn, const u32 *splits, u32 n_parts);
    u64  (*now_ns)(void *ctx);
    /* Ledger writer, may be NULL. It gets its own context (finding v75), so a
     * sink can be attached to any host without wrapping its parallel_for. */
    void (*sink)(void *sink_ctx, const AzEvent *ev, u32 n);
    void  *sink_ctx;
    /* Fresh entropy from the operating system or the hardware: fill buf with
     * `bytes` bytes that no seed determines; false = none could be had. A
     * wild world needs it (az_world_create returns NULL without); a lab
     * world never calls it. Called from serial code only. */
    bool (*entropy)(void *ctx, void *buf, u32 bytes);
    /* Entropy journal, may be NULL: called once per tick of a wild world with
     * the tick and the 16 bytes it drew. A record, so that a surprising
     * event can be looked into afterwards; nothing ever reads it back into a
     * run. */
    void (*entropy_log)(void *ctx, u64 tick, const u8 bytes[16]);
    /* Optional GPU brain-step batch offload (TR-04, hosted only; NULL = CPU).
     * When brain_batch != NULL and gpu_cap >= w->n_sorted, Phase 1 splits into
     * parallel sense -> GPU brain_batch -> parallel intent/demand/steer.
     * Returning false falls back to CPU brain_step with identical results. */
    bool (*brain_batch)(void *gpu_ctx, struct AzWorld *w, u32 n_sorted);
    void      *gpu_ctx;
    AzPercept *gpu_percepts;
    u8        *gpu_fin;
    u32        gpu_cap;
} AzHost;

/* kernel dispatch: C reference by default, assembly twins when enabled. A
 * twin must be bit-identical to its reference on every input.
 * philox_n is az_philox4x32_n (C) unless the AVX2 twin is installed: never
 * NULL (the owner asked for a C default; findings v35, v46, v74). It runs
 * live in job_perceive (AZ_RS_NOISE), az_life_range (AZ_RS_LIFE) and
 * az_tier1_range (AZ_RS_TIER1, AZ_RS_QSV) for batched counter draws with
 * key == w->tkey, with exact scalar fallback when uid >= 2^32 (TR-13). */
typedef struct AzKernels {
    void (*brain_step)(const AzBrain *b, u8 *mind, i8 *motor);
    void (*philox_n)(u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n);
    /* One wrapping axis (x or y) of the integrator for n entities of a block,
     * structure of arrays; az_kin_axis_c in az_kin_axis.c is the definition
     * (ABI 4). For i < n:
     *   v      = clamp(vel[i] + dv[i], -AZ_VEL_MAX, AZ_VEL_MAX)
     *   v      = trunc0(v * (256 - drag[i]) / 256)    drag 0..255
     *   vel[i] = v
     *   pos[i] = pos[i] + (u32)(v * (1 << (shift - 16)))       wraps mod 2^32 */
    void (*kin_axis)(u32 *pos, i16 *vel, const i16 *dv, const u8 *drag, u32 n, u32 shift);
    u32  asm_mask;   /* which twins are installed (bit per kernel)              */
} AzKernels;

/* Module code that runs in a parallel stage reads the columns, the tables
 * and the scalars above the line marked HOST below; it never touches the
 * members under it (host, k, arena, tmp, tmp_u32, splits, workers, prof_ns;
 * ws only through az_scratch).
 * Serial-stage code (grid, life commit, ledger) may use tmp, tmp_u32 and
 * host->parallel_for. Never keep a column pointer across az_tick calls: a
 * reorder may replace the columns. */
typedef struct AzWorld {
    AzConfig cfg;
    u32 key[2];          /* Philox key: the seed (lab) or the entropy the world */
                         /* was made from (wild): the record of where a run began */
    u32 tkey[2];         /* the key of this tick (az_tick_keys): key, with the  */
                         /* high half of the tick folded in                     */
    u64 role_key;        /* key of the initiator / responder hash               */
    u64 role_tkey;       /* role_key of this tick (az_tick_keys)                */
    u64 wild[2];         /* wild mode: the entropy this tick drew (not state:   */
                         /* not hashed, not saved; the next tick draws afresh)  */
    u64 tick;            /* 64-bit: never wraps                                 */
    u64 uid_next;        /* next uid to assign; uids start at 1 and are 64-bit: */
                         /* they never run out and are never reused             */
    u32 cap;             /* slot capacity                                       */
    u32 n_slots;         /* slots in use (alive or dead since last reorder)     */
    u32 n_live;          /* alive entities, counted by az_grid_build            */
    u32 rock_repaired_on_load;  /* restored live centers moved out of bedrock; local diagnostic */
    u32 n_sorted;        /* entries in order[] and pub[] (== n_live)            */
    u32 free_head;       /* dead slots chained through spos[]; AZ_NONE if none  */

    /* grid geometry */
    u32 gx, gy, gz, n_cells;
    u8  gx_bits, gy_bits, gz_bits;
    u8  shift_x, shift_y, shift_z;   /* pos >> shift = cell coordinate          */

    /* entity columns */
#define AZ_COL(name, type, per, tier) type *name;
#include "az_columns.def"
#undef AZ_COL

    /* cell-sorted snapshot, rebuilt every tick by az_grid_build */
    u32   *cell_start;   /* n_cells + 1 entries                                 */
    u32   *order;        /* sorted position -> slot; inside a cell sorted by uid */
    AzPub *pub;          /* sorted position -> public record                    */
    AzPubId *pub_id;     /* sorted position -> public identities                */

    /* cell columns */
#define AZ_CELL(name, type, per, tier) type *c_##name;
#include "az_cells.def"
#undef AZ_CELL

    AzClade     *clades;     u32 n_clades;     /* slots used (some may be free: alive == 0) */
    AzColony    *colonies;   u32 n_colonies;   /* index 0 is "no colony"        */
    AzColonyEph *colony_eph;                   /* cfg.max_colonies entries      */

    /* The creature store (az_store.c): everything about a creature that has
     * no fixed size lives in one block of AZ_UNIT units, found through the
     * handle column `blk`. Two halves of store_units units each; the active
     * one starts at unit store_base. Unit 0 of a half is never handed out and
     * stays zero, so a slot without a block (handle 0) reads as an empty one.
     * Blocks are allocated, released and moved only by serial code; where a
     * block lies is not state (handles and addresses are neither hashed nor
     * saved), what it holds is. */
    u8  *store;
    u32  store_units;    /* units per half                                      */
    u32  store_base;     /* first unit of the active half: 0 or store_units     */
    u32  store_top;      /* units handed out in the active half, unit 0 included */
    u32  store_live;     /* units of all blocks that slots hold (layout-free)   */

    u32  param[256];     /* the world parameters, AZ_WP_* (az_params.def and    */
                         /* the modules' own files in core/params/)             */

    AzTotals  totals;
    AzGeneticCode code;
    i32       gstate[AZ_NGLOBAL];   /* world-level module state (AZ_G_*): hashed and saved */
    u64       ledger_chain;   /* running digest of every event emitted so far (not state) */
    u64       ledger_count;

    /* worldgen constants, set by az_worldgen and read by the field modules;
     * part of the state hash */
    u32 sun_period[2];   /* ticks per day of each sun                           */
    u16 gravity;         /* downward acceleration, velocity units per tick      */
    u16 vent_count;
    u32 vent_cell[64];

    /* ---------------- HOST: not for parallel-stage module code -------------- */
    AzKernels k;
    AzHost   *host;
    AzArena  *arena;
    u8       *tmp;       /* cap * 128 bytes: scratch for reorders and commits   */
    u32      *tmp_u32;   /* max(cap, n_cells) + 1 entries                       */
    u32      *splits;    /* AZ_MAX_PARTS + 1 entries                            */
    u8       *ws;        /* AZ_MAX_WORKERS blocks of AZ_WS_BYTES: per-worker scratch */
    u32       n_workers;
    u32       split_counter;   /* region counter for debug_split_seed           */
    u64       prof_ns[12];     /* nanoseconds per pipeline stage (AZ_PROF_*)    */
    /* ledger events of the current tick: one pool sized by the slot capacity,
     * handed out in blocks of AZ_EV_BLOCK events (a worker takes a block with
     * an atomic add on ev_next and fills it through az_emit); the ledger reads
     * the used blocks and empties the pool at the end of every tick */
    AzEvent  *ev_pool;
    u32      *ev_fill;         /* events in each closed block                   */
    u32       ev_blocks;       /* blocks in the pool                            */
    u32       ev_next;         /* next block to hand out (atomic)               */
    u64       store_in_place_gen; /* bumped on compaction or dirty overflow (TR-04) */
    u32       store_dirty_h[1024]; /* block handles whose AZ_SEG_BRAIN moved in place */
    u32       store_dirty_u[1024]; /* block sizes (units) for dirty handles         */
    u32       store_dirty_n;      /* entries in store_dirty_h (0..1024)            */
    u32       store_sync_top;     /* GPU-synced unit watermark (0 when no GPU)     */
    u8        no_prefetch;     /* 0 = DDR5 streaming prefetch on (TR-01), 1 = off */
    AzWorker  workers[AZ_MAX_WORKERS];
} AzWorld;

/* world-level module state words in gstate[]: written only in serial passes
 * by the owner of the range, read by anybody. The named words are the ones
 * other modules read (ABI 4); the rest of a range is its owner's. */
enum { AZ_G_WEATHER = 0 /* 0..31 */, AZ_G_ECON = 32 /* 32..47 */, AZ_G_STATS = 48 /* 48..55 */,
       AZ_G_SPARE = 56 };
enum {
    /* weather (az_weather_global) */
    AZ_G_SUN0_PHASE = 0,   /* position in the day of sun 0, 0..65535; 0 = dawn  */
    AZ_G_SUN1_PHASE = 1,
    AZ_G_SUN0_LIGHT = 2,   /* light sun 0 gives the top layer now, 0..255       */
    AZ_G_SUN1_LIGHT = 3,
    AZ_G_STORM      = 4,   /* storm intensity 0..255, 0 = calm                  */
    AZ_G_STORM_CELL = 5,   /* cell under the storm centre                       */
    AZ_G_SEASON     = 6,   /* slow cycle of the two suns together, 0..65535     */
    /* economy (az_econ_global) */
    AZ_G_PRICE      = 32,  /* 32..35: reference value of inventory resource k, Q8.8 eu per mu */
    AZ_G_VOLUME     = 36   /* 36..39: smoothed units of resource k traded per tick, Q8.8 */
    /* AZ_G_STATS: no writer in ABI 4 (classification state is per entity) */
};

enum { AZ_PROF_REORDER = 0, AZ_PROF_GRID, AZ_PROF_PERCEIVE, AZ_PROF_MATCH, AZ_PROF_ACT,
       AZ_PROF_FIELD, AZ_PROF_LIFE, AZ_PROF_TIER1, AZ_PROF_TIER2, AZ_PROF_LEDGER, AZ_PROF_COUNT };

/* Per-worker scratch (finding v39). Core code has no heap and no stack frame
 * above 2 KB, so every worker token owns AZ_WS_BYTES of private, cache-line
 * aligned bytes in the arena. They are never hashed or saved and hold nothing
 * defined on entry. Three users can be active at once on one worker, and each
 * has its own part:
 *   az_ws_scan   the scan module (the pieces of a neighbourhood)
 *   az_ws_tick   the tick (the percepts of one batch)
 *   az_scratch   the module hook that is running: AZ_WS_MODULE_BYTES, valid
 *                until the hook returns. A hook may call az_scan_list or
 *                az_scan_range without losing it. Serial-stage code uses
 *                worker 0. Anything that must outlive the hook is state and
 *                belongs in a column or in gstate. */
AZ_INLINE void *az_ws_scan(const AzWorld *w, u32 worker)
{
    return w->ws + (size_t)worker * AZ_WS_BYTES;
}
AZ_INLINE void *az_ws_tick(const AzWorld *w, u32 worker)
{
    return w->ws + (size_t)worker * AZ_WS_BYTES + AZ_WS_SCAN_BYTES;
}
AZ_INLINE void *az_scratch(const AzWorld *w, u32 worker)
{
    return w->ws + (size_t)worker * AZ_WS_BYTES + AZ_WS_SCAN_BYTES + AZ_WS_TICK_BYTES;
}

/* ====================================================================== */
/* 5. Geometry helpers                                                    */
/* ====================================================================== */

AZ_INLINE u32 az_cell_xyz(const AzWorld *w, u32 cx, u32 cy, u32 cz)
{
    return (((cz << w->gy_bits) | cy) << w->gx_bits) | cx;
}
AZ_INLINE u32 az_cell_at(const AzWorld *w, u32 px, u32 py, u32 pz)
{
    return az_cell_xyz(w, px >> w->shift_x, py >> w->shift_y, pz >> w->shift_z);
}
AZ_INLINE u32 az_cell_x(const AzWorld *w, u32 c) { return c & (w->gx - 1); }
AZ_INLINE u32 az_cell_y(const AzWorld *w, u32 c) { return (c >> w->gx_bits) & (w->gy - 1); }
AZ_INLINE u32 az_cell_z(const AzWorld *w, u32 c) { return c >> (w->gx_bits + w->gy_bits); }

/* neighbour cell: x and y wrap, z does not (returns AZ_NONE outside) */
AZ_INLINE u32 az_cell_nb(const AzWorld *w, u32 c, i32 dx, i32 dy, i32 dz)
{
    i32 z = (i32)az_cell_z(w, c) + dz;
    if (z < 0 || z >= (i32)w->gz) return AZ_NONE;
    u32 x = (az_cell_x(w, c) + (u32)dx) & (w->gx - 1);
    u32 y = (az_cell_y(w, c) + (u32)dy) & (w->gy - 1);
    return az_cell_xyz(w, x, y, (u32)z);
}

/* sub-cell coordinate in neighbour space, 0 .. AZ_SUB_ONE-1 */
AZ_INLINE u32 az_sub(u32 pos, u32 shift) { return (pos >> (shift - AZ_SUB_BITS)) & (AZ_SUB_ONE - 1); }

/* position step: pos += vel, with vel in 1/65536 cell per tick */
AZ_INLINE u32 az_pos_step(u32 pos, i32 vel, u32 shift)
{
    return pos + (u32)(vel * (i32)(1u << (shift - 16)));
}

/* tag distance used for kin recognition and assortative mating: 0..510 */
AZ_INLINE u32 az_tag_dist(u16 a, u16 b)
{
    i32 h = (i32)(a >> 8) - (i32)(b >> 8);
    i32 l = (i32)(a & 255) - (i32)(b & 255);
    return (u32)(az_abs_i32(h) + az_abs_i32(l));
}

/* size code (4 bits) <-> body radius in neighbour space. code = 0..15,
 * radius = (code + 1) * (AZ_SUB_ONE / 32): 1/32 .. 1/2 of a cell. */
AZ_INLINE u32 az_radius_ns(u32 code) { return (code + 1u) * (AZ_SUB_ONE / 32); }
u32 az_radius_code(u32 mass);   /* az_fx.c: monotone map from body matter (mu) to 0..15 */

/* ====================================================================== */
/* 6. Random numbers                                                      */
/* ====================================================================== */

/* Philox4x32-10 (Salmon et al. 2011). Counter-based: no hidden state, so
 * draws do not depend on thread schedule or processing order. */
void az_philox4x32(u32 out[4], const u32 ctr[4], const u32 key[2]);          /* az_rng.c */
/* n independent blocks: out4n[4i..4i+3] = philox(ctr4n[4i..4i+3], key) */
void az_philox4x32_n(u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n);  /* az_rng.c */

/* The only way simulation code draws randomness. id = uid, cell index, clade
 * or colony index, or for a pair the smaller uid with the larger in idx. id
 * and idx are 64-bit: their low halves and the low half of the tick are the
 * counter, their high halves go into the key of the tick (w->tkey). */
AZ_INLINE void az_rand4(const AzWorld *w, u64 id, u32 stream, u64 idx, u32 out[4])
{
    u32 ctr[4] = { (u32)id, (u32)w->tick, stream, (u32)idx };
    u32 key[2] = { w->tkey[0] ^ (u32)(id >> 32), w->tkey[1] ^ (u32)(idx >> 32) };
    az_philox4x32(out, ctr, key);
}
AZ_INLINE u32 az_rand(const AzWorld *w, u64 id, u32 stream, u64 idx)
{
    u32 o[4];
    az_rand4(w, id, stream, idx, o);
    return o[0];
}
/* bounded draw in [0, n) by multiply-shift (bias below n / 2^32) */
AZ_INLINE u32 az_rand_below(const AzWorld *w, u64 id, u32 stream, u64 idx, u32 n)
{
    return (u32)(((u64)az_rand(w, id, stream, idx) * (u64)n) >> 32);
}

/* Role this tick: initiators propose interactions, responders receive them.
 * The one exception to rule D4: a counter-based hash of (uid, tick) under
 * role_key instead of a Philox call, because it runs for every entity every
 * tick. It guarantees an entity is in at most one pair interaction per tick.
 * An initiator should only propose to a responder: test the target's public
 * uid with this function. */
AZ_INLINE bool az_is_initiator(const AzWorld *w, u64 uid)
{
    u64 h = az_mix64((((u64)(u32)uid << 32) | (u64)(u32)w->tick) ^ w->role_tkey ^
                     ((uid >> 32) * 0x9E3779B97F4A7C15ull));
    return (h >> 23) & 1u;
}

/* Phased work (ABI 4): true on one tick in 2^log2_period for each entity, at
 * a phase fixed by its uid, so that in any tick the same fraction of the
 * population is due. `salt` keeps the phases of different mechanisms apart
 * (use your RNG stream number). A branch behind az_phase_due with
 * log2_period >= 2 may read WARM columns (rule D8). What it computes for the
 * ticks in between must live in a HOT column. */
AZ_INLINE bool az_phase_due(const AzWorld *w, u64 uid, u32 salt, u32 log2_period)
{
    return (((u32)uid + salt * 0x9E3779B1u + (u32)w->tick) & ((1u << log2_period) - 1u)) == 0;
}
/* the Tier 1 schedule: every tier1_period ticks per entity */
AZ_INLINE bool az_tier1_due(const AzWorld *w, u64 uid)
{
    return (uid + w->tick) % w->cfg.tier1_period == 0;
}

/* ====================================================================== */
/* 7. Cross-thread accumulators, private tallies, requests                */
/* ====================================================================== */

AZ_INLINE void az_atomic_add_u32(u32 *p, u32 v) { (void)__atomic_fetch_add(p, v, __ATOMIC_RELAXED); }
AZ_INLINE void az_atomic_add_i32(i32 *p, i32 v) { (void)__atomic_fetch_add(p, v, __ATOMIC_RELAXED); }
AZ_INLINE void az_atomic_add_u64(u64 *p, u64 v) { (void)__atomic_fetch_add(p, v, __ATOMIC_RELAXED); }
AZ_INLINE void az_atomic_max_u32(u32 *p, u32 v)
{
    u32 cur = __atomic_load_n(p, __ATOMIC_RELAXED);
    while (cur < v && !__atomic_compare_exchange_n(p, &cur, v, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) { }
}
AZ_INLINE void az_atomic_max_u64(u64 *p, u64 v)
{
    u64 cur = __atomic_load_n(p, __ATOMIC_RELAXED);
    while (cur < v && !__atomic_compare_exchange_n(p, &cur, v, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) { }
}

/* Entity -> world transfers. The field columns are read-only during the
 * entity phases; modules change the world only through these four calls and
 * never touch c_need or c_acc directly. az_field_commit applies the result. */
/* A low-limb signed crossing carries one unit of 2^32. Atomic fetch-add
 * assigns each crossing to one producer. The stages join all producers
 * before either limb is observed or reset. */
AZ_INLINE void az_add_pending(i32 *low, u32 *carry, i32 delta)
{
    i32 old = __atomic_fetch_add(low, delta, __ATOMIC_RELAXED);
    i64 sum = (i64)old + delta;
    if (sum > INT32_MAX) az_atomic_add_u32(carry, 1u);
    else if (sum < INT32_MIN) az_atomic_add_u32(carry, UINT32_MAX);
}

AZ_INLINE i64 az_pending_species(const AzWorld *w, u32 c, u32 species)
{
    u32 carry = w->c_species_carry[(size_t)c * AZ_NSPECIES + species];
    return (i64)(((u64)carry << 32) + (u64)(i64)w->c_acc[c].delta[species]);
}

AZ_INLINE void az_cell_post(AzWorld *w, u32 worker, u32 c, u32 species, i32 d_mu)
{
    (void)worker;
    az_add_pending(&w->c_acc[c].delta[species], &w->c_species_carry[(size_t)c * AZ_NSPECIES + species], d_mu);
}

/* Deposit an entire unsigned creature store into its original cell. The
 * journal accepts signed increments, so split only at its positive limit. */
AZ_INLINE void az_cell_deposit(AzWorld *w, u32 worker, u32 cell, u32 species, u32 amount)
{
    while (amount > INT32_MAX) {
        az_cell_post(w, worker, cell, species, INT32_MAX);
        amount -= INT32_MAX;
    }
    if (amount) az_cell_post(w, worker, cell, species, (i32)amount);
}

AZ_INLINE void az_cell_post_heat(AzWorld *w, u32 worker, u32 c, i32 d_eu)
{
    (void)worker;
    AzCellAcc *a = &w->c_acc[c];
    az_add_pending(&a->dheat, &a->heat_carry, d_eu);
}

/* Quiescent slow path. Fill the available storage and retain positive
 * surplus in the same cell. An excessive withdrawal preserves its debt
 * and the original field for a later deposit. */
AZ_INLINE void az_commit_species(AzWorld *w, u32 c, u32 species, AzTally *tally)
{
    size_t index = (size_t)c * AZ_NSPECIES + species;
    i64 pending = az_pending_species(w, c, species);
    if (!pending) return;
    u32 stored = w->c_sp[index];
    if (pending < -(i64)stored) {
        tally->field_underflow++;
        return;
    }
    i64 room = (i64)(UINT32_MAX - stored);
    if (pending > room) {
        u64 surplus = (u64)(pending - room);
        i32 low = (i32)(u32)surplus;
        w->c_sp[index] = UINT32_MAX;
        w->c_acc[c].delta[species] = low;
        w->c_species_carry[index] = (u32)((surplus - (u64)(i64)low) >> 32);
    } else {
        w->c_sp[index] = (u32)((i64)stored + pending);
        w->c_acc[c].delta[species] = 0;
        w->c_species_carry[index] = 0;
    }
}

/* Only the owner of a quiescent cell commits it. Positive excess remains
 * authoritative heat in the accumulator and is retried on the next commit.
 * An excessive withdrawal retains the previous field and the signed debt. */
AZ_INLINE void az_commit_heat(AzWorld *w, u32 c, AzTally *tally)
{
    AzCellAcc *a = &w->c_acc[c];
    i64 pending = az_pending_heat(a);
    if (pending == 0) return;
    u32 stored = w->c_heat[c];
    if (pending < -(i64)stored) {
        tally->field_underflow++;
        return;
    }
    i64 room = (i64)(UINT32_MAX - stored);
    if (pending > room) {
        w->c_heat[c] = UINT32_MAX;
        az_set_pending_heat(a, pending - room);
    } else {
        w->c_heat[c] = (u32)((i64)stored + pending);
        az_set_pending_heat(a, 0);
    }
}

/* These cell columns are scratch at tick boundaries. Pending changes in other
 * EPH columns remain part of the hash and snapshot. */
AZ_INLINE bool az_cell_scratch(const AzWorld *w, const void *col)
{
    return col == (const void *)w->c_need || col == (const void *)w->c_grant ||
           col == (const void *)w->c_pressure;
}

AZ_INLINE void az_cell_demand(AzWorld *w, u32 worker, u32 c, u32 diet, u32 n)
{
    (void)worker;
    az_atomic_add_u64(&w->c_need[c].dem[diet], n);
}
AZ_INLINE void az_cell_mark(AzWorld *w, u32 worker, u32 c, u32 channel, u32 strength, u32 colony)
{
    (void)worker;
    /* Commit clamps the standing cue to a byte. Keep an overflowing sum
     * distinguishable from zero until that per-channel commit is counted. */
    u32 *pending = &w->c_acc[c].dmark[channel];
    u32 current = __atomic_load_n(pending, __ATOMIC_RELAXED);
    while (current != UINT32_MAX && strength) {
        u32 total = strength > UINT32_MAX - current ? UINT32_MAX : current + strength;
        if (__atomic_compare_exchange_n(pending, &current, total, true,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED)) break;
    }
    if (channel == 0) az_atomic_max_u64(&w->c_acc[c].dmark_id, ((u64)strength << 32) | colony);
}
/* what a cell granted on a channel this tick: floor(n * grant / 65535) */
AZ_INLINE u32 az_cell_granted(const AzWorld *w, u32 c, u32 diet, u32 n)
{
    return (u32)(((u64)n * w->c_grant[(size_t)c * AZ_NDIET + diet]) / 65535u);
}

/* Whether the world's host runs parts in parallel: it must offer parallel_for
 * AND have more than one worker. A one-worker pool or a wrapper that forwards
 * parallel_for counts as serial, so a serial run keeps the seven-part test
 * split of cfg.debug_split_seed (finding v51). The tick and the grid share
 * this one rule (finding v55). */
AZ_INLINE bool az_host_parallel(const AzWorld *w)
{
    return w->host && w->host->parallel_for && w->n_workers > 1;
}

/* a worker's private tally (merged by the tick; never touch w->totals in a
 * parallel stage) */
AZ_INLINE AzTally *az_tally(AzWorld *w, u32 worker) { return &w->workers[worker].tally; }

/* Ask for the death of your own slot (phase 3). az_life_range, the last hook
 * of the block, carries it out through az_life_kill. The first cause given in
 * a tick stays: the hooks run in a fixed order, so the cause is deterministic. */
AZ_INLINE void az_request_death(AzWorld *w, u32 slot, u32 cause)
{
    u8 *rq = &w->pair[slot].req;
    if (!(*rq & AZ_RQ_DIE)) *rq = (u8)((*rq & 0x0Fu) | AZ_RQ_DIE | ((cause & 15u) << 4));
}

/* count one more of behaviour AZ_ST_* for your own slot (saturating; WARM:
 * call it only where rule D8 lets you touch WARM) */
AZ_INLINE void az_stat_bump(AzWorld *w, u32 slot, u32 which, u32 by)
{
    u32 *st = &w->stat[(size_t)slot * AZ_NSTAT + which];
    u64 v = (u64)*st + by;
    *st = v > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)v;
}

/* Ask for children. Call in phase 3 for your own slot only, at most once per
 * tick (a second call is refused). The provisions are the totals the mother
 * gives; when the call returns true, deduct exactly these amounts from her in
 * the same block. az_life_commit splits them among the n children and refunds
 * what no child could take. Returns false if refused (a second call or n out
 * of range); then nothing was recorded and you must not deduct. */
AZ_INLINE bool az_birth_request(AzWorld *w, u32 mother_slot, u32 father_slot, u32 n,
                                i32 prov_energy, u32 prov_a, u32 prov_m, u32 prov_t)
{
    AzBirth *b = &w->birth[mother_slot];
    if (b->n != 0 || n == 0) return false;
    b->father = father_slot;
    b->energy = prov_energy;
    b->prov[0] = prov_a; b->prov[1] = prov_m; b->prov[2] = prov_t;
    b->n = n;
    return true;
}

/* give a slot the cell of its position (serial code placing a newborn) */
AZ_INLINE void az_place(AzWorld *w, u32 slot)
{
    w->cell[slot] = az_cell_at(w, w->pos_x[slot], w->pos_y[slot], w->pos_z[slot]);
}

/* Keep a valid position or find the nearest open one. AZ_NONE leaves xyz untouched. */
u32 az_rock_place(const AzWorld *w, u32 *px, u32 *py, u32 *pz);
u32 az_rock_birth_place(const AzWorld *w, u32 mother, u32 *px, u32 *py, u32 *pz);

/* World parameters (core/az_params.def): numbers of the physics and of the
 * starting conditions that no gene carries. Set at run time before worldgen,
 * part of the state hash, saved in snapshots. */
enum {
#define AZ_PARAM(name, def, lo, hi, what) AZ_WP_##name,
#include "az_params.def"
#undef AZ_PARAM
    AZ_WP_COUNT
};
typedef struct AzParamDesc { const char *name; u32 def, lo, hi; const char *what; } AzParamDesc;
const AzParamDesc *az_param_desc(u32 id);            /* NULL when id >= AZ_WP_COUNT */
/* Serial, before any creature exists (after az_world_create, before
 * az_worldgen). False, and nothing changes, for an unknown id, a value out
 * of the parameter's range, or a world that has started. */
bool az_param_set(struct AzWorld *w, u32 id, u32 value);

/* ====================================================================== */
/* 8. Module contracts                                                    */
/* ====================================================================== */
/* Each module is one file core/az_<name>.c with exactly these entry points.
 * "cell range" = [begin, end) of cell indices; "slot range" = [begin, end)
 * of entity slots; `worker` is the opaque token of the executing worker.
 * The determinism rules D1..D8 are in docs/ARCHITECTURE.md section 4. */

/* column descriptors generated from az_columns.def / az_cells.def */
/* HND: a handle into the creature store. Moved with the entity like a HOT
 * column and counted in the fixed record, but neither hashed nor saved: the
 * block it points to is (az_state_hash, az_snapshot_save). */
enum { AZ_TIER_HOT = 0, AZ_TIER_WARM = 1, AZ_TIER_EPH = 2, AZ_TIER_FIELD = 3, AZ_TIER_HND = 4 };
typedef struct AzColDesc {
    void       *data;
    u32         elem;   /* bytes per element          */
    u32         per;    /* elements per entity / cell */
    u32         tier;
    const char *name;
} AzColDesc;
#define AZ_MAX_COLS 96
u32 az_n_columns(void);                                           /* entity columns   */
void az_column(const AzWorld *w, u32 k, AzColDesc *out);          /* k-th entity column */
u32 az_n_cell_columns(void);
void az_cell_column(const AzWorld *w, u32 k, AzColDesc *out);

/* ---- az_world.c (lead) ---- */
u64      az_world_bytes(const AzConfig *cfg);   /* arena size needed; 0 for an invalid config */
AzWorld *az_world_create(AzArena *arena, const AzConfig *cfg, AzHost *host);
/* Shift every address inside the world (moving it to another base or to a
 * device). host and arena become NULL and the kernel table goes back to the
 * C reference (finding v45); the caller installs its own host and twins. */
void     az_world_rebase(AzWorld *w, i64 delta);
void     az_tick(AzWorld *w);                                  /* az_tick.c        */
void     az_tally_merge(AzWorld *w);                           /* fold worker tallies into totals */
/* Serial, end of tick: close every worker's open event block. Afterwards the
 * first k blocks (k = the return value) hold the tick's events, block b with
 * ev_fill[b] events at ev_pool + b * AZ_EV_BLOCK, in no canonical order. */
u32      az_events_close(AzWorld *w);
void     az_events_reset(AzWorld *w);                          /* serial: empty the event pool */
u64      az_state_hash(const AzWorld *w);                      /* order-independent */
u64      az_total_matter(const AzWorld *w);                    /* invariant        */
i64      az_total_energy(const AzWorld *w);                    /* invariant        */
/* Serial only: a zeroed slot with a new uid, or AZ_NONE when every slot is
 * alive (counted in totals.cap_denied). The new slot has cell == AZ_NONE and
 * spos == AZ_NONE: set its position and call az_place. Until the next
 * az_grid_build it has no public record and is in no cell list. */
u32      az_spawn(AzWorld *w);
void    *az_arena_alloc(AzArena *a, u64 bytes, u64 align);
void     az_memset(void *dst, int v, size_t n);
void     az_memcpy(void *dst, const void *src, size_t n);
void     az_emit(AzWorld *w, u32 worker, u32 kind, u32 aux, u64 a, u64 b, i64 v);
/* Serial: recompute tkey and role_tkey from key, role_key and tick. az_tick
 * calls it whenever the tick changes; call it yourself after writing w->tick. */
void     az_tick_keys(AzWorld *w);

/* ---- az_snapshot.c : exact save and restore ---- */
u64      az_snapshot_bytes(const AzWorld *w);
u64      az_snapshot_save(const AzWorld *w, u8 *buf, u64 cap);          /* 0 if cap too small */
AzWorld *az_snapshot_load(AzArena *a, const u8 *buf, u64 n, AzHost *host);  /* NULL if invalid */
/* The world config a snapshot holds, after the same header and checksum checks
 * az_snapshot_load makes; false if buf is not a valid snapshot. A host sizes
 * the arena with az_world_bytes(out) before loading. */
bool     az_snapshot_config(const u8 *buf, u64 n, AzConfig *out);

/* ---- az_fx.c : fixed-point math and geometric algebra ---- */
u32  az_isqrt_u64(u64 x);                        /* floor(sqrt(x))                 */
i32  az_sin_q14(u32 angle);                      /* angle: 65536 = one turn        */
i32  az_cos_q14(u32 angle);
u32  az_log2_q8(u32 x);                          /* 256*log2(x), x >= 1; 0 for x=0 */
u32  az_exp2_q8(u32 e);                          /* 2^(e/256), saturating          */
u32  az_div_round(u64 num, u32 den);             /* round-half-up, den > 0         */
void az_rotor_norm(i16 r[4]);                    /* renormalise to Q1.14 unit      */
void az_rotor_mul(i16 out[4], const i16 a[4], const i16 b[4]);
void az_rotor_apply(i32 out[3], const i16 r[4], const i32 v[3]);      /* body -> world */
void az_rotor_apply_inv(i32 out[3], const i16 r[4], const i32 v[3]);  /* world -> body */
void az_rotor_step(i16 r[4], const i16 ang[3]);  /* R <- norm(R + (1/2) ang R)     */
/* Project v on the four tetrahedral axes (1,1,1), (1,-1,-1), (-1,1,-1),
 * (-1,-1,1); each result is max(0, dot) >> shift, clamped to 0..127. Four
 * non-negative receptor channels encode any 3D direction. */
void az_tetra4(const i32 v[3], u32 shift, u8 out[4]);

/* ---- az_grid.c : spatial binning ---- */
/* az_grid_build: for every slot below n_slots: an alive slot gets cell[] and
 * spos[], its AZ_F_INIT bit is set from az_is_initiator, and its public
 * record is written; a dead slot gets cell = AZ_NONE and joins the free
 * list. Outputs cell_start[], order[] (inside a cell by increasing uid),
 * pub[], n_live, n_sorted, free_head. The free list holds every dead slot,
 * chained through spos[] from the highest slot down:
 * free_head = highest dead slot, spos[s] = next lower dead slot or AZ_NONE.
 * It does not touch the pair or birth scratch (the tick resets those). */
void az_grid_build(AzWorld *w);
/* az_grid_reorder: move every alive entity to slot = its sorted position in
 * every HOT and WARM column, set n_slots = n_live and free_head = AZ_NONE. */
void az_grid_reorder(AzWorld *w);

/* ---- az_scan.c : neighbour scan ---- */
/* Fill out[k] for the entities at sorted positions p0 + k, k = 0..count-1.
 * The positions may span several cells; the cell of sorted position p is
 * w->cell[w->order[p]]. For each entity the neighbours are visited in the
 * canonical order (cells in increasing dz, dy, dx; inside a cell increasing
 * uid) and only the first AZ_NEIGH_MAX inside its sense range are used.
 * `worker` is the caller's worker token (0 in serial code): the scan keeps
 * its working buffer in that worker's scratch (az_ws_scan). */
void az_scan_range(const AzWorld *w, u32 p0, u32 count, AzPercept *out, u32 worker);
/* The same neighbours for one entity as a list (canonical order): offsets in
 * neighbour space and slots. Returns the count (<= max, <= AZ_NEIGH_MAX).
 * For Tier 1 modules that need individual directions. Returns 0 for an
 * entity without a sorted position (born this tick). */
u32  az_scan_list(const AzWorld *w, u32 slot, i32 (*off)[3], u32 *oslot, u32 max, u32 worker);

/* ---- az_sense.c ---- */
void az_sense_cell(const AzWorld *w, u32 c, AzCellSense *cs);
void az_sense(const AzWorld *w, u32 slot, const AzPercept *p, const AzCellSense *cs, u8 sens[AZ_SENS]);
u8 az_sense_and_fin(const AzWorld *w, u32 slot, const AzPercept *p, const AzCellSense *cs, u8 sens[AZ_SENS]); /* one traversal of the body in phase 1 */
u8 az_sense_and_fin_noise(const AzWorld *w, u32 slot, const AzPercept *p, const AzCellSense *cs, u8 sens[AZ_SENS], u8 noise); /* phase 1 with pre-drawn AZ_RS_NOISE byte */

/* ---- az_brain.c : the per-creature int8 recurrent network; lineages ---- */
/* One step: mind = sens ++ hid ++ hid2 (see AzBrain); writes hid2 and
 * motor[AZ_MOT]. Defined for every header that az_brain_ok accepts. */
void az_brain_step_mod_c(const AzBrain *b, const u8 gain[8], const i8 bias[8], u8 *mind, i8 *motor);
void az_brain_step_c(const AzBrain *b, u8 *mind, i8 *motor);
/* bytes of a brain segment and of its mind segment */
u64  az_brain_bytes(u32 n_hb, u32 n_t1, u32 n_t2);
u32  az_mind_bytes(u32 n_hb);
/* a brain segment of `bytes` bytes is well formed and its mind has room: the
 * kernel then stays inside both. Checked for every block a snapshot brings. */
bool az_brain_ok(const AzBrain *b, u32 bytes, u32 mind_bytes);
/* The founder brain of a species: sizes first (the caller makes the block),
 * then the fill. A pure function of the world seed, the species seed and the
 * BRAIN_* world parameters, the same at any tick. */
void az_brain_found_size(const AzWorld *w, u32 species_seed, u32 *brain_bytes, u32 *mind_bytes);
void az_brain_found(AzWorld *w, u32 slot, u32 species_seed);
/* A child's brain is its mother's, then neural mutation (weights and biases
 * shift; a tile of connections or a block of neurons is added or lost).
 * az_brain_child_size gives room that is always enough; az_brain_inherit
 * fills the child's segments and returns the bytes it used. */
void az_brain_child_size(const AzWorld *w, u32 mother, u32 *brain_bytes, u32 *mind_bytes);
void az_brain_inherit(AzWorld *w, u32 child, u32 mother, u32 *brain_bytes, u32 *mind_bytes);
u32  az_clade_found(AzWorld *w, u32 seed_id, u32 species);   /* new founder lineage (serial) */
/* SERIAL, life commit, after az_conceive: give the child its lineage and
 * count it. The mother's, unless the child's brain differs from hers in
 * structure (blocks or tiles): then a new lineage descending from hers, if
 * the table has room (cfg.max_clades; otherwise the mother's, and the first
 * refusal of a run is a ledger event). */
void az_clade_assign(AzWorld *w, u32 child, u32 mother);
void az_clade_release(AzWorld *w, u32 clade);    /* serial: mark a lineage with refcount 0 free */

/* ---- az_store.c (lead) : the creature store ---- */
/* A block: one header unit, then the segments in index order, each starting
 * on a multiple of 8 bytes; every byte that no segment uses is zero. The
 * block holds no address and no slot number, so it can be moved, hashed and
 * saved as plain bytes. */
/* The order is by use: what every tick reads is at the front of the block,
 * next to its header (the body parts first: phase 3 reads them without the
 * brain; then the mind and the brain, which the brain step reads as one
 * run); what birth and Tier 1 read (the genome, the memory) is behind. */
enum {
    AZ_SEG_BODY  = 0,   /* AzPart[]: the body parts                             */
    AZ_SEG_MIND  = 1,   /* state the brain keeps between ticks                  */
    AZ_SEG_BRAIN = 2,   /* the creature's own neural program (az_brain.c)       */
    AZ_SEG_GEN_A = 3,   /* AzGene[]: the haplotype from the bearing parent      */
    AZ_SEG_GEN_B = 4,   /* AzGene[]: the haplotype from the other parent (must  */
                        /* stay AZ_SEG_GEN_A + 1)                               */
    AZ_SEG_MEMORY = 5,  /* u64[]: what the creature has learned and keeps, of   */
                        /* any length (az_tier1.c); see az_memory below         */
    AZ_SEG_COUNT = 6
};
typedef struct AzBlk {
    u32 units;                 /* size of the block in AZ_UNIT units, header included */
    u32 pad0;
    u32 off[AZ_SEG_COUNT];     /* byte offset of each segment from the block start */
    u32 len[AZ_SEG_COUNT];     /* bytes in each segment                         */
    u32 pad1[2];
} AzBlk;
_Static_assert(sizeof(AzBlk) == AZ_UNIT, "the block header is one unit");

/* One body part, 8 bytes (segment AZ_SEG_BODY is an array of them). A body
 * has any number of parts; each holds its own matter, which is inside the
 * exact balance like the pools. Growing the matter of a part is its owner's
 * business in phase 3 (az_metab); a new part needs room in the block, so it
 * is asked for (az_part_request) and made by az_store_commit. */
enum { AZ_PART_CORE = 0, AZ_PART_GUT, AZ_PART_FIN, AZ_PART_ARMOR, AZ_PART_WEAPON, AZ_PART_STORE,
       AZ_PART_SENSOR, AZ_PART_GONAD, AZ_PART_KINDS };
typedef struct AzPart {
    u32 matter;        /* matter built into this part (mu)                     */
    u8  kind;          /* AZ_PART_*                                            */
    u8  health;        /* 0..255                                               */
    u16 aux;           /* the owner module's                                   */
} AzPart;
_Static_assert(sizeof(AzPart) == 8, "AzPart is 8 bytes");

/* One gene, 4 bytes. `fam` says what the gene is (its family); `allele` is
 * its sequence, 4 bases of 2 bits; `expr` is how strongly it is expressed,
 * 128 = 1.0. A haplotype may hold any number of genes, several of one family
 * (their effects add: dosage) or none (the family is absent). */
typedef struct AzGene { u16 fam; u8 allele; u8 expr; } AzGene;
_Static_assert(sizeof(AzGene) == 4, "AzGene is 4 bytes");

AZ_INLINE AzBlk *az_blk(const AzWorld *w, u32 slot)
{
    return (AzBlk *)(void *)(w->store + ((size_t)(w->store_base + w->blk[slot]) << 6));
}
/* segment `seg` of the slot's block and its size in bytes (0 if it has none) */
AZ_INLINE void *az_seg(const AzWorld *w, u32 slot, u32 seg, u32 *bytes)
{
    AzBlk *b = az_blk(w, slot);
    if (bytes) *bytes = b->len[seg];
    return (u8 *)(void *)b + b->off[seg];
}
/* Persisted after the two neural-state buffers. Birth starts prior outcomes at zero.
 * Genomic development owns the receptor traits; the completed previous interaction
 * and declared stake are updated in disjoint per-slot scheduler phases. */
typedef struct AzSenseMemory {
    u8 taxis, curve;
    i8 payoff;
    u8 stake;
    u8 innate_stake;  /* developed stake fraction used when publishing regular offers */
} AzSenseMemory;
_Static_assert(sizeof(AzSenseMemory) == 5, "sense memory storage contract");

AZ_INLINE AzSenseMemory *az_sense_memory(const AzWorld *w, u32 slot)
{
    u32 len;
    u8 *mind = (u8 *)az_seg(w, slot, AZ_SEG_MIND, &len);
    return len >= AZ_SENS + sizeof(AzSenseMemory) && (len - AZ_SENS - sizeof(AzSenseMemory)) % 16 == 0
        ? (AzSenseMemory *)(void *)(mind + len - sizeof(AzSenseMemory)) : NULL;
}
AZ_INLINE const AzGene *az_genes(const AzWorld *w, u32 slot, u32 hap, u32 *n)
{
    u32 bytes;
    const AzGene *g = (const AzGene *)az_seg(w, slot, AZ_SEG_GEN_A + hap, &bytes);
    *n = bytes / (u32)sizeof(AzGene);
    return g;
}
AZ_INLINE AzBrain *az_brain(const AzWorld *w, u32 slot, u32 *bytes)
{
    return (AzBrain *)az_seg(w, slot, AZ_SEG_BRAIN, bytes);
}
AZ_INLINE AzPart *az_parts(const AzWorld *w, u32 slot, u32 *n)
{
    u32 bytes;
    AzPart *p = (AzPart *)az_seg(w, slot, AZ_SEG_BODY, &bytes);
    *n = bytes / (u32)sizeof(AzPart);
    return p;
}
/* A hook that walks slots and reads their blocks (the body parts, say) asks
 * for the block of a slot a few steps ahead: every creature's block is its
 * own, so nothing else brings it into cache. Reads nothing, cannot fault. */
AZ_INLINE void az_blk_prefetch(const AzWorld *w, u32 slot)
{
    const u8 *b = (const u8 *)(const void *)az_blk(w, slot);
    __builtin_prefetch(b, 0, 3);
    __builtin_prefetch(b + 64, 0, 3);
}
/* matter in all parts of the slot's body (0 for a slot without a block) */
AZ_INLINE u64 az_body_matter(const AzWorld *w, u32 slot)
{
    u32 n;
    const AzPart *p = az_parts(w, slot, &n);
    u64 m = 0;
    for (u32 i = 0; i < n; i++) m += p[i].matter;
    return m;
}
/* SERIAL (az_life_commit). az_part_add: one more part of this kind, empty,
 * at the end of the body; false when the store has no room (recorded like
 * every refusal). az_part_drop: remove part `index`; its matter must be 0
 * (the owner moved it into a pool in phase 3), else nothing happens. */
bool az_part_add(AzWorld *w, u32 slot, u32 kind);
bool az_part_drop(AzWorld *w, u32 slot, u32 index);
/* A creature's long-term memory: segment AZ_SEG_MEMORY of its block, an
 * array of 64-bit words of any length (0 at birth). The engine gives it no
 * meaning: az_tier1.c owns the words (patterns for tk_hop beyond the four of
 * the working memory, or anything else it learns). Words can be read and
 * written from the owner's hook for its own slot; the length changes only
 * through az_memory_request. Memory is part of the block: hashed, saved,
 * moved with the creature, and not inherited (a child's block starts with
 * none; what a lineage hands down goes through genes, the brain's program or
 * teaching). */
AZ_INLINE u64 *az_memory(const AzWorld *w, u32 slot, u32 *words)
{
    u32 bytes;
    u64 *p = (u64 *)az_seg(w, slot, AZ_SEG_MEMORY, &bytes);
    *words = bytes >> 3;
    return p;
}
/* Own slot, phase 3 or the Tier 1 stage: ask the engine to change the
 * creature's block at the end of this tick (az_store_commit). A request that
 * the store has no room for is refused and recorded like every refusal; the
 * block is then unchanged.
 *   az_part_request       one more body part of this kind, empty
 *   az_part_shed_request  give up part `index` (its matter must be 0 by then)
 *   az_memory_request     make the memory `words` long: it keeps the words
 *                         that fit, new words are 0 */
AZ_INLINE void az_part_request(AzWorld *w, u32 slot, u32 kind)
{
    w->blk_req[(size_t)slot * 3] = kind;
    w->pair[slot].req |= AZ_RQ_GROW;
}
AZ_INLINE void az_part_shed_request(AzWorld *w, u32 slot, u32 index)
{
    w->blk_req[(size_t)slot * 3 + 1] = index;
    w->pair[slot].req |= AZ_RQ_SHED;
}
AZ_INLINE void az_memory_request(AzWorld *w, u32 slot, u32 words)
{
    w->blk_req[(size_t)slot * 3 + 2] = words == 0xFFFFFFFFu ? words : words + 1u;
}
/* SERIAL, the tick calls it after the Tier 1 stage: apply this tick's block
 * requests of the living (parts shed, parts grown, memory length), in uid
 * order, so a full store refuses the same requests whatever the slot order
 * and the thread count are. */
void az_store_commit(AzWorld *w);
/* units a block with these segment sizes takes */
u32  az_blk_units(const u32 len[AZ_SEG_COUNT]);
/* The four calls below are SERIAL (worldgen, the life commit, a snapshot
 * load) and may move every block of the store: fetch block and segment
 * pointers after them, never before.
 * az_blk_new: give the slot (which has none) a zero-filled block with these
 * segment sizes. False when the store has no room: the refusal is counted in
 * totals.store_denied, the first one is also a ledger event, and nothing
 * else changes. Room is judged by the units all blocks hold, never by where
 * they lie, so a refusal does not depend on the compaction schedule. */
bool az_blk_new(AzWorld *w, u32 slot, const u32 len[AZ_SEG_COUNT]);
/* shrink the segments to len[] (each at most what it is), close the gaps and
 * give the freed units back */
void az_blk_fit(AzWorld *w, u32 slot, const u32 len[AZ_SEG_COUNT]);
/* set the size of one segment, keeping what it holds (new bytes are zero);
 * false when a growing block finds no room (recorded as above) */
bool az_blk_resize(AzWorld *w, u32 slot, u32 seg, u32 bytes);
/* pack all blocks in slot order into the other half */
void az_store_compact(AzWorld *w);
/* release the block of a slot (az_grid_build does it for every dead slot) */
void az_blk_release(AzWorld *w, u32 slot);
/* structural check of a block that came from a file: sizes and offsets are
 * consistent and inside `units` */
bool az_blk_ok(const AzBlk *b, u32 units);

/* ---- az_genome.c : diploid heredity ---- */
void az_genetic_code_init(AzWorld *w);   /* fill w->code from the seed; call before any founder */
/* Move the slot's genome toward one whose developed traits approach target[k]
 * where weight[k] != 0, by a deterministic coordinate descent of at most iters
 * passes. It draws no random numbers and does not use the uid (findings v43,
 * v10): two slots with the same genome end with the same genome, so members
 * of a species keep only the variation they started with. The sex locus moves
 * only where the sex stays. The slot is left developed (trait, phen, tag, sex,
 * mod_gain and mod_bias rewritten). Returns the remaining weighted error.
 * Used by worldgen to seed founder niches. */
u32  az_genome_fit(AzWorld *w, u32 slot, const u8 *target, const u8 *weight, u32 iters);
/* SERIAL. Give a new slot its block. az_found builds a founder of the
 * species; az_conceive builds a child: with father == AZ_NONE a clonal bud of
 * the mother, otherwise one gamete of each parent (father == mother is
 * selfing), then mutation. Both return false when the creature store has no
 * room (see az_blk_new): the slot then has no block, and the caller leaves it
 * dead and gives back what it had set aside for it. On success the creature
 * is complete: genome, brain, mind, body parts, and it is developed (trait,
 * phen, tag, sex, the brain's innate gain and bias). Call az_develop yourself
 * only after you changed a genome (az_genome_fit does it). */
bool az_found(AzWorld *w, u32 slot, u32 species_seed);
bool az_conceive(AzWorld *w, u32 child, u32 mother, u32 father);
void az_develop(AzWorld *w, u32 slot);   /* genome -> trait[], phen[], tag, sex, the brain's innate gain and bias */

/* ---- az_social.c / az_econ.c / az_mate.c : interactions ---- */
/* Phase 1, every alive slot, called in this order. az_social_intent zeroes
 * the offer first. Each writes its own offer bytes. Each may propose an act
 * (initiators only, and only to a responder): it may replace pair.act and
 * pair.peer only if its urge is strictly greater than pair.urge, and then
 * stores its own urge. */
void az_social_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker);
void az_econ_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker);
void az_mate_intent(AzWorld *w, u32 slot, const AzPercept *p, u32 worker);
/* Phase 2: responder r scores claimant c that proposes `act`; < 0 = refuse.
 * For attack and steal the score is the responder's priority among
 * claimants, not consent. Nothing but pair.claim is written in phase 2, so a
 * score may read both offers, both public records, both trait rows and the
 * responder's own memory of others (rep_tag, rep_val, partner, bond, rank).
 * It runs on a worker other than the responder's, once per claimant: it must
 * be a pure function of those inputs (no az_tally, no az_emit, no writes). */
i32  az_social_score(const AzWorld *w, u32 r, u32 c, u32 act);
i32  az_mate_score(const AzWorld *w, u32 r, u32 c, u32 act);
i32  az_econ_score(const AzWorld *w, u32 r, u32 c, u32 act);
/* Phase 3: apply the outcome to `slot` only. The partner is being updated by
 * another worker at the same time, so from the partner you may read ONLY:
 * its frozen scratch (offer, pair, motor, eat), its start-of-tick public
 * record w->pub[w->spos[partner]], and the columns nobody writes in phase 3:
 * uid, genome, trait, phen, sex, parent_a, parent_b, clade, tag, and (of a
 * teacher) mod_gain, mod_bias. Never its pools, energy, inventory, rank,
 * stats, colony or flags. */
void az_social_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker);
void az_mate_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker);
void az_econ_resolve(AzWorld *w, u32 slot, u32 partner, u32 act, bool initiator, u32 worker);
/* Phase 3, every slot in the block, after the pairs: signals, marks, colony
 * store deposits and withdrawals, anything social that is not a pair. */
void az_social_range(AzWorld *w, u32 begin, u32 end, u32 worker);
void az_econ_range(AzWorld *w, u32 begin, u32 end, u32 worker);
/* Serial, once per tick after phase 3 and its tally merge (ABI 4; skipped
 * under AZ_LESION_TRADE): the market's memory. Updates gstate AZ_G_PRICE and
 * AZ_G_VOLUME from w->totals.trade_units, the only world-level state the
 * economy has. */
void az_econ_global(AzWorld *w);

/* ---- az_metab.c ---- */
void az_metab_demand(AzWorld *w, u32 slot, u32 worker);            /* phase 1: eat[] and az_cell_demand */
void az_metab_range(AzWorld *w, u32 begin, u32 end, u32 worker);   /* phase 3       */

/* ---- az_physics.c ---- */
void az_physics_steer_fin(AzWorld *w, u32 slot, const AzPercept *p, u8 fin, u32 worker);
void az_physics_steer(AzWorld *w, u32 slot, const AzPercept *p, u32 worker);   /* phase 1: acc[] */
void az_physics_range(AzWorld *w, u32 begin, u32 end, u32 worker); /* phase 3        */
/* the C definition of AzKernels.kin_axis (ABI 4); kernels/ holds its twin */
void az_kin_axis_c(u32 *pos, i16 *vel, const i16 *dv, const u8 *drag, u32 n, u32 shift);

/* ---- az_life.c ---- */
void az_life_range(AzWorld *w, u32 begin, u32 end, u32 worker);    /* phase 3        */
void az_life_commit(AzWorld *w);                 /* serial: births in (mother uid, child index) order */
/* Return all matter and energy of the slot to its cell and mark it dead. The
 * cause of the AZ_EV_DEATH event is the one in pair.req (az_request_death). */
void az_life_kill(AzWorld *w, u32 slot, u32 worker);
/* The one way a brood is requested (ABI 4). Phase 3, called for the bearing
 * parent's own slot: by az_life_range for budding and selfing (father ==
 * AZ_NONE or == mother) and by az_mate_resolve for a sexual pair (father =
 * the partner's slot). It decides clutch size and provision from the mother's
 * traits and stores, calls az_birth_request, deducts what it gave and clears
 * her AZ_F_RECEPT. Returns the clutch size, 0 when nothing was requested
 * (she is not ripe, or a request already stands this tick). No other module
 * calls az_birth_request. */
u32  az_life_brood(AzWorld *w, u32 mother, u32 father, u32 worker);

/* ---- az_field.c : field plumbing ---- */
void az_field_grant(AzWorld *w, u32 begin, u32 end, u32 worker);   /* dem -> grant, then dem = 0 */
void az_field_commit(AzWorld *w, u32 begin, u32 end, u32 worker);  /* apply and clear c_acc */
u32  az_field_avail(const AzWorld *w, u32 c, u32 diet);            /* what uptake channel `diet` offers in cell c */
void az_publish_pressure(AzWorld *w); /* serial field barrier, before local pressure can be observed */
u32  az_cell_pressure(const AzWorld *w, u32 c);  /* the one shared definition of pressure */

/* ---- az_chem.c / az_weather.c : field dynamics (stepped cells only) ---- */
/* A stepped cell is visited by chemistry first, then by weather. Both move
 * matter and heat only by posting (az_cell_post, az_cell_post_heat; a flux
 * to a neighbour is a negative post here and a positive one there), and each
 * writes its own derived columns of the stepped cell (ABI 4):
 *   az_chem.c     phase, med, and the decay of mark
 *   az_weather.c  temp, wind, charge, light
 * `solid` is written by az_worldgen only. The derive functions recompute the
 * derived columns of one cell from its amounts and post nothing; a step ends
 * with them and az_worldgen calls them for every cell, so the columns are
 * defined from tick 0 and simply stay as they are under the two lesions. */
void az_chem_cell(AzWorld *w, u32 c, u32 worker);
void az_chem_derive(AzWorld *w, u32 c);          /* phase, med of cell c          */
void az_weather_cell(AzWorld *w, u32 c, u32 worker);
void az_weather_derive(AzWorld *w, u32 c);       /* temp of cell c (az_temp_of)   */
void az_weather_global(AzWorld *w);              /* serial: suns, top-layer light, vents, storms; gstate AZ_G_WEATHER */

/* ---- az_colony.c ---- */
void az_colony_grant(AzWorld *w);                /* serial, after the cell grants: colony_eph dem -> grant */
void az_colony_commit(AzWorld *w);               /* serial, after phase 3: apply dep and taken to store */
/* Serial, every tier2_period ticks (skipped under AZ_LESION_COLONY): count
 * members, centroid and velocity, found colonies (the only caller of
 * az_colony_found), dissolve empty ones (the store goes back to the cell under
 * the centroid), set need[], alarm, mood, culture and memory, and give
 * members their role. A creature joins a colony in one of two ways only: at
 * birth (the mother's) or by accepting AZ_ACT_RECRUIT, where the recruit
 * writes its own `colony` from the recruiter's public record. */
void az_colony_tier2(AzWorld *w);
u32  az_colony_found(AzWorld *w, u32 founder_slot);   /* serial only; 0 = refused */

/* The colony's own energy pool and its shared signal channels. The engine
 * keeps these books itself (az_tick.c grants and commits them next to
 * az_colony_grant and az_colony_commit), so they hold under every colony
 * module. Colony 0 is "no colony": the calls do nothing for it.
 *   phase 1   az_colony_ask_energy    add to the demand on the pool
 *   phase 3   az_colony_take_energy   returns what the member gets of the eu
 *             it asked for, floor(eu * grant / 65535); it adds that to its
 *             own energy
 *             az_colony_give_energy   the member has already taken eu out of
 *             its own energy
 *             az_colony_say           one voice (0..255) on a channel; the
 *             colony's signal[] of the next tick is the mean of the voices,
 *             and a channel nobody spoke on fades by 1/8 per tick
 * Any phase: read w->colonies[c].signal[] and .energy. A colony that is
 * dissolved gives its energy to its cell as heat (the colony module). */
AZ_INLINE void az_colony_ask_energy(AzWorld *w, u32 colony, u32 eu)
{
    if (colony && colony < w->n_colonies) az_atomic_add_u64(&w->colony_eph[colony].dem_e, eu);
}
AZ_INLINE u32 az_colony_take_energy(AzWorld *w, u32 colony, u32 eu)
{
    if (!colony || colony >= w->n_colonies) return 0;
    AzColonyEph *e = &w->colony_eph[colony];
    const u32 got = (u32)(((u64)eu * e->grant_e) / 65535u);
    if (got) az_atomic_add_u64(&e->taken_e, got);
    return got;
}
AZ_INLINE void az_colony_give_energy(AzWorld *w, u32 colony, u32 eu)
{
    if (colony && colony < w->n_colonies) az_atomic_add_u64(&w->colony_eph[colony].dep_e, eu);
}
AZ_INLINE void az_colony_say(AzWorld *w, u32 colony, u32 channel, u32 v)
{
    if (!colony || colony >= w->n_colonies) return;
    az_atomic_add_u64(&w->colony_eph[colony].sig_sum[channel & 3u], v & 255u);
    az_atomic_add_u32(&w->colony_eph[colony].sig_n[channel & 3u], 1u);
}

/* ---- az_tier1.c : slow cognition, phased ---- */
/* Stage 6, every slot of the range; the module acts on a slot when
 * az_tier1_due says so. It owns arousal, mod_gain and mod_bias (shared with
 * the learner of a teach pair), elig, rbase, rprev, ctx, qstate, memory,
 * engram and hop_n. Classification is an on-demand observation. */
void az_tier1_range(AzWorld *w, u32 begin, u32 end, u32 worker);

/* ---- az_stats.c ---- */
enum {
    AZ_STATS_GENES, AZ_STATS_NEURONS, AZ_STATS_TILES, AZ_STATS_PARTS, AZ_STATS_BODY_MATTER,
    AZ_STATS_MEMORY, AZ_STATS_COLONY_SIZE, AZ_STATS_DEPTH, AZ_STATS_DISTRIBUTIONS
};
enum { AZ_STATS_QUANTILES = 7 };
typedef struct AzStats {
    u32 n_live, n_eggs, n_clades, n_colonies;
    u32 archetype[32];
    u32 mating_mode[8];
    u64 mean_energy_q8;
    u32 max_generation;
    u32 trait_mean[AZ_TRAITS];
    /* ABI 4 */
    u32 n_juv, n_senescent, n_dormant, n_bonded, n_colonial;
    u32 role[AZ_ROLE_COUNT];
    u32 diet[4];             /* entities whose largest lifetime uptake was LIGHT, NUT_X, ORG_Y, WST_W */
    u32 species_live[128];   /* living descendants of founder species k (lineage), k < 128 */
    u32 n_species_live;      /* founder species with at least one living descendant */
    u32 look[64];            /* living entities per look class (az_look_class)    */
    u32 n_looks;             /* look classes with at least one living entity      */
    u64 mean_age_q8, mean_mass_q8;
    u32 n_species_beyond, n_looks_beyond, n_diet_unknown, age_q8_saturated;
    u32 trait_var[AZ_TRAITS];
    u64 quantile[AZ_STATS_DISTRIBUTIONS][AZ_STATS_QUANTILES];
} AzStats;
void az_stats_collect(const AzWorld *w, AzStats *out);
/* Hosted observations use complete, attributable per-UID evidence. Amounts are
   counts of funding occasions or resolved actions. The legacy snapshot alone
   supplies no complete history for these fields. */
typedef struct {
    u64 intake_occasions[4], teaching_events, declined_recruit_events;
    /* Fourth-lane lower bound is intake_occasions[3]. A larger upper bound
       allows a different winner when source provenance is ambiguous. */
    u64 waste_possible_occasions;
    u64 predation_events, withheld_contribution_events, builder_events;
    u64 exclusive_bonded_matings, overlapping_bonded_mates;
    bool intake_complete, teaching_complete, declined_recruit_complete;
    bool predation_complete, commons_complete, builder_complete, partner_complete;
} AzClassObservations;
u32 az_classify_observed(const AzWorld *w, u32 slot, const AzClassObservations *observed);
u32 az_classify(const AzWorld *w, u32 slot);    /* observation, computed between ticks */
/* Look class 0..63 of an entity: a measurement of its shape, skin and glyph
 * (the appearance traits over the clade's body plan), for censuses and the
 * viewer. It feeds nothing back into the simulation. */
u32  az_look_class(const AzWorld *w, u32 slot);

/* ---- az_worldgen.c ---- */
/* fields, bedrock (`solid`), vents, suns, gravity, the genetic code, the
 * derived cell columns (through az_chem_derive and az_weather_derive) and the
 * founders of cfg.n_species species (1..100), each fitted to a niche with
 * az_genome_fit */
void az_worldgen(AzWorld *w);

/* ---- az_ledger.c ---- */
void az_ledger_flush(AzWorld *w);                /* sort and hand events to the host sink */
/* The ledger's canonical order, for tools and tests (finding v53): x before
 * y by (kind, a, b, aux, v as a signed number), then tick. A total order. */
bool az_event_less(const AzEvent *x, const AzEvent *y);

#endif /* AZ_ABI_H */
