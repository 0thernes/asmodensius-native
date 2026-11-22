/* Placement and refusal tests on page-locked, native worlds. */
#include "az_test.h"

static void birth_case(bool all_rock)
{
    AzConfig cfg = az_test_config(41, 8);
    cfg.cap = 64; cfg.gx_bits = 2; cfg.gy_bits = 2; cfg.gz_bits = 2;
    cfg.max_colonies = 16; cfg.max_clades = 32;
    AzArena arena = {0};
    AzWinInfo info = {0};
    CHECK(az_win_arena(&arena, az_world_bytes(&cfg), AZ_WIN_LOCK, &info) && info.arena_locked);
    if (!arena.base) return;
    AzWorld *w = az_world_create(&arena, &cfg, NULL);
    CHECK(w != NULL);
    if (w) {
        az_test_uniform(w);
        az_grid_build(w);
        for (u32 i = 0; i < w->n_cells; i++) w->c_solid[i] = 255;
        const u32 m = 0;
        w->pos_x[m] = 1u << 26; w->pos_y[m] = 1u << 26; w->pos_z[m] = 1u << 26;
        az_place(w, m);
        if (!all_rock) w->c_solid[az_cell_xyz(w, 3, 0, 1)] = 254;
        const u32 prior_slots = w->n_slots;
        const u64 prior_births = w->totals.births;
        const u64 prior_uid = w->uid_next;
        const u64 matter = az_total_matter(w);
        const i64 energy = az_total_energy(w);
        CHECK(az_birth_request(w, m, AZ_NONE, 1, 23, 7, 0, 0));
        w->energy[m] -= 23; w->pool_a[m] -= 7; w->mass[m] -= 7;
        if (all_rock) {
            CHECK(az_birth_request(w, 1, AZ_NONE, 3, 35, 8, 0, 0));
            w->energy[1] -= 35; w->pool_a[1] -= 8; w->mass[1] -= 8;
        }
        az_life_commit(w);
        CHECK_EQ(w->birth[1].n, 0);
        u32 denied = 0, born = 0;
        AzWorker *wk = &w->workers[0];
        for (u32 i = 0; i < wk->n_events; i++) {
            denied += wk->events[i].kind == AZ_EV_REFUSED && wk->events[i].aux == AZ_REF_TERRAIN;
            born += wk->events[i].kind == AZ_EV_BIRTH;
        }
        CHECK_EQ(denied, all_rock); CHECK_EQ(born, !all_rock);
        CHECK_EQ(w->birth[m].n, 0);
        CHECK_EQ(w->totals.births, prior_births + !all_rock);
        CHECK_EQ(w->n_slots, prior_slots + !all_rock);
        CHECK_EQ(w->uid_next, prior_uid + !all_rock);
        if (!all_rock && w->n_slots > prior_slots) {
            CHECK_EQ(w->c_solid[w->cell[prior_slots]], 254);
            CHECK(w->flags[prior_slots] & AZ_F_ALIVE);
        }
        az_field_commit(w, 0, w->n_cells, 0);
        az_tally_merge(w);
        CHECK_EQ(w->totals.field_underflow, 0); CHECK_EQ(w->totals.dropped_events, 0);
        CHECK_EQ(az_total_matter(w), matter);
        CHECK_EQ(az_total_energy(w), energy);
    }
    az_win_arena_free(&arena);
}
/* Independent tiled Manhattan oracle. Same-column vertical ties prefer the upper
 * cell; at positive horizontal distance, equal-depth ties prefer lowest index. */
static int tile_distance(int x, int y, int width)
{
    int d = abs(x-y);
    for (int k=-1; k<=1; k++) {
        int t = abs(x - (y + k*width));
        if (t < d) d = t;
    }
    return d;
}

static void geometry(AzWorld *w)
{
    u32 rng = 59171;
    for (u32 trial = 0; trial < 32; trial++) {
        for (u32 c = 0; c < w->n_cells; c++) {
            rng = 1664525u*rng + 1013904223u;
            w->c_solid[c] = trial && (trial == 1u || (rng >> 28u) < (trial & 7u)) ? (u8)(c % 255u) : 255u;
        }
        for (u32 src = 0; src < w->n_cells; src++) {
            const int x = az_cell_x(w,src), y = az_cell_y(w,src), z = az_cell_z(w,src);
            u32 best = AZ_NONE;
            int best_h = INT_MAX, best_v = INT_MAX, best_tie = INT_MAX;
            for (u32 c = 0; c < w->n_cells; c++) {
                if (w->c_solid[c] == 255) continue;
                int h = tile_distance(x,az_cell_x(w,c),w->gx) + tile_distance(y,az_cell_y(w,c),w->gy);
                int v = abs(z-(int)az_cell_z(w,c));
                int tie = h ? (int)c : -(int)az_cell_z(w,c);
                if (h < best_h || (h == best_h && (v < best_v || (v == best_v && tie < best_tie)))) {
                    best_h = h; best_v = v; best_tie = tie; best = c;
                }
            }
            u32 px = ((u32)x << w->shift_x) + 91u, py = ((u32)y << w->shift_y) + 321u;
            u32 pz = ((u32)z << w->shift_z) + 109u;
            const u32 ox=px, oy=py, oz=pz;
            CHECK_EQ(az_rock_place(w,&px,&py,&pz),best);
            if (best == AZ_NONE || best == src) {
                CHECK_EQ(px,ox); CHECK_EQ(py,oy); CHECK_EQ(pz,oz);
            } else {
                CHECK_EQ(az_cell_at(w,px,py,pz),best);
                const u32 bx=az_cell_x(w,best), by=az_cell_y(w,best), bz=az_cell_z(w,best);
                CHECK_EQ(px,best_h ? (2u*bx+1u)*(1u<<(w->shift_x-1)) : ox);
                CHECK_EQ(py,best_h ? (2u*by+1u)*(1u<<(w->shift_y-1)) : oy);
                CHECK_EQ(pz,(2u*bz+1u)*(1u<<(w->shift_z-1)));
            }
        }
    }
}

static void founders(bool sealed, u64 seed)
{
    AzConfig cfg = az_test_config(seed, 32);
    cfg.cap=128; cfg.max_colonies=32; cfg.max_clades=64;
    cfg.gx_bits=3; cfg.gy_bits=2; cfg.gz_bits=2;
    AzArena arena={0}; AzWinInfo info={0};
    CHECK(az_win_arena(&arena,az_world_bytes(&cfg),AZ_WIN_LOCK,&info) && info.arena_locked);
    if (!arena.base) return;
    AzWorld *w=az_world_create(&arena,&cfg,NULL); CHECK(w != NULL);
    if (w) {
        const u64 uid0=w->uid_next;
        for (u32 c=0; c<w->n_cells; c++) w->c_solid[c] = sealed ? 255u : 0u;
        az_worldgen(w);
        u32 open=0;
        for (u32 c=0; c<w->n_cells; c++) open += w->c_solid[c] != 255;
        CHECK_EQ(w->n_slots,open ? cfg.n_initial : 0);
        CHECK_EQ(w->uid_next,uid0 + w->n_slots);
        for (u32 s=0; s<w->n_slots; s++) {
            CHECK(w->flags[s] & AZ_F_ALIVE);
            CHECK_EQ(w->cell[s],az_cell_at(w,w->pos_x[s],w->pos_y[s],w->pos_z[s]));
            CHECK(w->c_solid[w->cell[s]] != 255);
        }
        if (!open) {
            u32 denied=0;
            for (u32 i=0; i<w->workers[0].n_events; i++) {
                const AzEvent *ev=&w->workers[0].events[i];
                denied += ev->kind==AZ_EV_REFUSED && ev->aux==AZ_REF_TERRAIN;
            }
            CHECK_EQ(denied,1);
        }
        printf("founders seed=%" PRIu64 " initial_sealed=%u open_cells=%u founders=%u\n",
               seed,(unsigned)sealed,open,w->n_slots);
        if (seed == 1 && !sealed) geometry(w);
    }
    az_win_arena_free(&arena);
}

#ifdef AZ_TEST_ROCK_SNAPSHOT
/* Current reference images can describe a covered live center. Each reader
 * has a defined behavior; clean images round-trip exactly in either build. */
static void current_snapshot(bool covered)
{
    AzConfig cfg = az_test_config(17, 2);
    cfg.cap=64; cfg.max_colonies=16; cfg.max_clades=32;
    cfg.gx_bits=cfg.gy_bits=cfg.gz_bits=2;
    const u64 cap=az_world_bytes(&cfg);
    AzArena a={0}, b={0}; AzWinInfo info={0};
    CHECK(az_win_arena(&a,cap,AZ_WIN_LOCK,&info) && info.arena_locked);
    if (!a.base) return;
    CHECK(az_win_arena(&b,cap,AZ_WIN_LOCK,&info) && info.arena_locked);
    if (!b.base) { az_win_arena_free(&a); return; }
    AzWorld *w=az_world_create(&a,&cfg,NULL); CHECK(w != NULL);
    if (w) {
        az_test_uniform(w);
        w->pos_x[0]=263u; w->pos_y[0]=689u; w->pos_z[0]=(1u<<30u)+77u;
        w->pos_x[1]=1u<<30u; w->pos_y[1]=213u; w->pos_z[1]=444u;
        w->vel_x[0]=7; w->vel_y[0]=8; w->vel_z[0]=9; w->vel_x[1]=-4;
        az_grid_build(w);
        memset(w->c_solid,0,w->n_cells);
        if (covered) w->c_solid[w->cell[0]]=255;
        u64 n=az_snapshot_bytes(w);
        u8 *buf=n ? malloc((size_t)n) : NULL;
        CHECK(buf != NULL);
        if (buf) {
            CHECK_EQ(az_snapshot_save(w,buf,n),n);
            AzWorld *v=az_snapshot_load(&b,buf,n,NULL); CHECK(v != NULL);
            if (v) {
                bool migrated=false;
#ifdef AZ_TEST_ROCK_AWARE_PHYSICS
                migrated=covered;
#endif
                CHECK_EQ(v->rock_repaired_on_load,(u32)migrated);
                CHECK_EQ(v->pos_x[0],w->pos_x[0]); CHECK_EQ(v->pos_y[0],w->pos_y[0]);
                CHECK_EQ(v->pos_z[0],migrated ? (5u<<29u) : w->pos_z[0]);
                CHECK_EQ(v->vel_x[0],migrated ? 0 : 7);
                CHECK_EQ(v->vel_y[0],migrated ? 0 : 8); CHECK_EQ(v->vel_z[0],migrated ? 0 : 9);
                CHECK_EQ(v->pos_x[1],w->pos_x[1]); CHECK_EQ(v->pos_y[1],w->pos_y[1]);
                CHECK_EQ(v->pos_z[1],w->pos_z[1]); CHECK_EQ(v->vel_x[1],-4);
                if (migrated) {
                    CHECK_EQ(v->n_sorted,2); CHECK_EQ(v->cell[0],az_cell_xyz(v,0,0,2));
                    CHECK(v->spos[0]<v->n_sorted && v->order[v->spos[0]]==0);
                } else {
                    CHECK_EQ(az_state_hash(v),az_state_hash(w));
                    u8 *saved=malloc((size_t)n); CHECK(saved != NULL);
                    if (saved) {
                        CHECK_EQ(az_snapshot_save(v,saved,n),n);
                        CHECK(memcmp(saved,buf,(size_t)n)==0); free(saved);
                    }
                }
            }
            free(buf);
        }
    }
    az_win_arena_free(&a); az_win_arena_free(&b);
}
#endif

int main(void)
{
#ifdef AZ_TEST_ROCK_SNAPSHOT
    current_snapshot(false);
    current_snapshot(true);
#endif
    birth_case(false);
    birth_case(true);
    for (u64 seed=1; seed<=4; seed++) {
        founders(false,seed); founders(true,seed);
    }
    return az_test_done();
}
