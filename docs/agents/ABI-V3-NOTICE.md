> History. Z: only since 2026-11-18: read N:\ as `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native`; the build is `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\z`.

# ABI version 3 notice (2026-11-17)

If your build suddenly fails with errors such as `'AzWorld' has no member
named 'c_dem'`, `'act'`, `'tgt'`, `'acc_by'`, `conflicting types for
'az_clade_found'`, `undefined reference to 'az_scan_range'`, or
`implicit declaration of 'az_columns'`: the contract moved from version 2 to
version 3 while you were working. Nothing is wrong with your tools.

Do this:

1. Re-read `N:\core\az_abi.h`, the two `.def` files, and the stub of your
   module in `N:\core\stubs\` (all updated).
2. Read the "What changed from version 2 to 3" list at the top of
   `N:\docs\ARCHITECTURE.md`.
3. Adapt your file. The renames, one line each:

| Version 2 | Version 3 |
|---|---|
| `w->act[s]`, `w->urge[s]`, `w->req[s]` | `w->pair[s].act`, `.urge`, `.req` |
| `w->tgt[s]` (initiator), `w->acc_by[s]` (responder) | `w->pair[s].peer` (both) |
| `w->claim[s]` | `w->pair[s].claim` |
| `w->c_dem[c*N+d]`, `w->c_grant[c*N+d]` | `w->c_need[c].dem[d]`, `.grant[d]`; use `az_cell_demand`, `az_cell_granted` |
| `w->c_delta[c*N+k]`, `w->c_dheat[c]`, `w->c_dmark...` | `w->c_acc[c].delta[k]`, `.dheat`, `.dmark[k]`, `.dmark_id`; use `az_cell_post`, `az_cell_post_heat`, `az_cell_mark` |
| `az_cell_post(w, c, sp, n)` | `az_cell_post(w, worker, c, sp, n)` (same for `_heat`) |
| `az_field_begin` | removed; `az_field_grant` clears the demand it has used |
| `az_scan_cell(w, c, first, count, out)` | `az_scan_range(w, p0, count, out)` over sorted positions |
| per-worker birth queue (`AzBirthReq`, `workers[k].births`) | `az_birth_request(w, mother_slot, father_slot, n, energy, a, m, t)`; commit reads `w->birth[s]` |
| `az_columns(w, array)` | `az_n_columns()`, `az_column(w, k, &desc)`; same for `_cell_` |
| `az_clade_found(w, seed_id)` | `az_clade_found(w, seed_id, species)` |
| `AzClade.parent` (index) | `AzClade.parent_uid` (founder uid of the parent clade), plus `alive`, `species` |
| grid build resets the pair scratch | the tick resets it; the grid builds the free list of dead slots instead |
| `az_social_intent(w, slot, p)` etc. | trailing `worker` argument on the three intent hooks, `az_metab_demand`, `az_physics_steer` |

The reasons are in `N:\docs\research\review\` (four contract reviews). The
short version: births and the state hash must not depend on the thread count
or on how often dead slots are compacted; per-cell plumbing must not sweep
every cell every tick; the hot loop must not do an atomic add on a shared
line per creature.
