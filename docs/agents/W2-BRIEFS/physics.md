If your progress file `Z:\[Vibe Coded (AI)]\CLAUDECODE\Asmodensius-Native\build\work\w2\physics\PROGRESS.md` exists, read it first and continue from its next step; never redo a step it records as done and tested.

# Brief: az_physics.c and its NASM twin (motion in a 3D medium)

**Amended 2026-11-18 (ABI-V4-NOTICE section 5, README rules 13 to 20; they win over the text below).** Bodies have parts: thrust, drag and weight should follow what the body is (`AZ_PART_FIN`, the mass with its parts), and gravity is a vector field (`w->c_grav`).

You own `core/az_physics.c`, `kernels/az_kin_avx2.asm` and
`tests/test_physics.c`. Read `README.md` in this folder first. You write
`pos_*`, `vel_*`, `rot_*`, `ang_*` and the EPH column `acc`. The lead adds
the twin to `kernels/az_kernels.c` and `az_kernels.h` when it lands.

## az_physics_steer (phase 1)

`acc[3]` in the world frame: the brain's thrust (`AZ_M_FWD`, `AZ_M_LAT`,
`AZ_M_VERT`, body frame) turned by the body's rotor (`az_rotor_apply`),
times the gain of `AZ_P_THRUST`, plus the percept's contact push. Clamp to
i16. HOT state only.

## az_physics_range (phase 3), one block at a time

For the active slots of the block, gather into `az_scratch` arrays:
- `dv` per axis: `acc`, plus on z the weight: gravity times
  (`AZ_P_DENS` - `c_med[2c]`), so a body denser than its medium sinks and a
  lighter one rises (0 with `AZ_LESION_GRAVITY`), plus the pull of the wind
  (`c_wind`) scaled by the medium's density;
- `drag` 0..255 from `AZ_P_DRAG` and the medium's viscosity `c_med[2c + 1]`.

Then `w->k.kin_axis` for x and for y (they wrap), and z in C with the same
arithmetic plus reflection at the floor, the ceiling and bedrock: a body may
not enter a cell with `c_solid == 255` on any axis (reverse that velocity
component and keep the position). Write the results back.

Rotation: `ang_*` from `AZ_M_PITCH`, `AZ_M_YAW`, `AZ_M_ROLL` with damping,
`az_rotor_step`, renormalised.

Cost: thrust costs energy in proportion to `|motor thrust|` times
`AZ_P_THRUST`, paid as heat (`az_cell_post_heat`); a body with no energy
gets no thrust. Count velocity clamps in `sat[AZ_SAT_VELOCITY]`. Behind
`az_phase_due(w, uid, AZ_RS_PHYS, 4)` add the distance travelled since the
last phase to `AZ_ST_TRAVEL` (`az_stat_bump`).

Eggs and cysts do not thrust or turn; they still drift, sink and float.

## az_kin_axis_c and the twin

`az_kin_axis_c` is the definition (text in az_abi.h at `AzKernels`). The
NASM twin `az_kin_axis_avx2` (Win64 ABI, `%include "win64.inc"`) must give
the same `pos` and `vel` for every input, any `n` from 0 to 128, any
alignment.

## Contracts others rely on

- `|vel| <= AZ_VEL_MAX` per axis after every tick; z stays inside
  `1 << 16 .. 2^32 - 1 - (1 << 16)`; nobody ends a tick inside bedrock.
- You run after metab and before life in the block.
- The cell of an entity changes only at the next grid build.

## Traits and lesions you own

Through phen: DENSITY (`AZ_P_DENS`), DRAG, THRUST. Lesion:
`AZ_LESION_GRAVITY` (your part: weight of bodies).

## Tests (tests/test_physics.c)

The twin equals the C function on 1,000,000 random blocks including the
extremes of every input (and `n` = 0, 1, 127, 128). A still body in still
medium stays; a dense body sinks to the floor and rests; a light one rises;
with the lesion neither moves (measured: mean z after 200 ticks). Drag stops
a coasting body sooner in a viscous medium. Thrust moves a body along its
own heading and costs exactly the booked energy. Bedrock is never entered.
x and y wrap. Both invariants; equal hashes for 1, 3 and 8 workers, reorder
1 and 9, kernels 0 and 1.

References: semi-implicit (symplectic) Euler integration; Stokes drag for
small bodies in a viscous medium; Archimedes' principle for buoyancy.
