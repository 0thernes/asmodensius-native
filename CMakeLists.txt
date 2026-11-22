cmake_minimum_required(VERSION 3.20)
project(asmodensius_native C)

# ---------------------------------------------------------------------------
# Asmodensius-Native build.
#
# Z: only (owner, 2026-11-18): no drive letters. With A = "Z:/[Vibe Coded (AI)]/CLAUDECODE/Asmodensius-Native"
# (always quoted: the path has spaces, [ ] and ( )):
# Integration build (everything that exists):
#   cmake -S "A" -B "A/build/z" -G Ninja
#   cmake --build "A/build/z"
#   ctest --test-dir "A/build/z" --output-on-failure
#
# Owner build (only your own pieces, so nobody's half-written file breaks you):
#   cmake -S "A" -B "A/build/owners/<key>" -G Ninja    (everything builds and runs on Z:)
#         -DAZ_ONLY="<modules taken from core/, others from core/stubs>"
#         -DAZ_TESTS="core;<your tests>"      names without the test_ prefix
#         -DAZ_TOOLS="run;<your tools>"       names without the az_ prefix
#         -DAZ_TWINS="<your asm twins>"       file names without az_ and .asm; NONE for none
#         -DAZ_COMPONENTS="<view;gpu>"        optional parts; NONE for none
# Every one of these defaults to ALL.
# ---------------------------------------------------------------------------

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The source path has [ ] in it (Z:/[Vibe Coded (AI)]/...), and file(GLOB) reads [...] as a set of
# characters, so a glob on "${CMAKE_SOURCE_DIR}/..." finds nothing there. AZ_GLOB_ROOT is the source
# path with every [ written as [[] and every ] as []] (a set of one character), which matches the real
# folder names. Use it for every glob; use CMAKE_SOURCE_DIR everywhere else.
string(REPLACE "[" "<az-lb>" AZ_GLOB_ROOT "${CMAKE_SOURCE_DIR}")
string(REPLACE "]" "[]]" AZ_GLOB_ROOT "${AZ_GLOB_ROOT}")
string(REPLACE "<az-lb>" "[[]" AZ_GLOB_ROOT "${AZ_GLOB_ROOT}")

set(AZ_ONLY "ALL" CACHE STRING "modules taken from core/ (others from core/stubs); ALL = every module that exists")
set(AZ_TESTS "ALL" CACHE STRING "tests to build (names without test_); ALL or NONE or a list")
set(AZ_TOOLS "ALL" CACHE STRING "tools to build (names without az_); ALL or NONE or a list")
set(AZ_TWINS "ALL" CACHE STRING "assembly twins to build (names without az_ and .asm); ALL or NONE or a list")
set(AZ_COMPONENTS "ALL" CACHE STRING "optional components: view;gpu; ALL or NONE or a list")
set(AZ_MARCH "x86-64-v3" CACHE STRING "-march value")
# Windows enters a function with the stack aligned to 16 bytes, and GCC for MinGW can emit an aligned
# 256-bit move to a stack slot without realigning it (GCC bug 54412; found 2026-11-18 in a test that
# passed a struct by value: an access violation in about every second run). Tests and tools let the
# assembler encode aligned vector moves as unaligned ones; ctest avx_stack_gate checks every program
# and library of the build, the core included, with tools/check_avx_stack.py.
set(AZ_UNALIGNED_MOVES "")
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND WIN32)
  set(AZ_UNALIGNED_MOVES -Wa,-muse-unaligned-vector-move)
endif()
# A second build with -DAZ_UBSAN=ON (its own folder, for example "A/build/ubsan") compiles the
# core, the hosts and the tests with the undefined-behaviour sanitizer in trap mode: no runtime
# library is needed, a violation executes an illegal instruction and the test program dies.
# -fwrapv is left out there, so signed overflow, which the code must not rely on, traps too.
option(AZ_UBSAN "undefined-behaviour sanitizer, trap mode (a separate build folder)" OFF)

function(az_selected out list_var name)
  if("${${list_var}}" STREQUAL "ALL")
    set(${out} TRUE PARENT_SCOPE)
  elseif("${${list_var}}" STREQUAL "NONE" OR "${${list_var}}" STREQUAL "")
    set(${out} FALSE PARENT_SCOPE)
  else()
    list(FIND ${list_var} "${name}" found)
    if(found EQUAL -1)
      set(${out} FALSE PARENT_SCOPE)
    else()
      set(${out} TRUE PARENT_SCOPE)
    endif()
  endif()
endfunction()

set(AZ_MODULES rng fx grid scan sense brain genome social mate econ metab physics
               life field chem weather colony tier1 stats worldgen ledger snapshot)

set(AZ_SRCS core/az_world.c core/az_tick.c core/az_store.c core/az_kin_axis.c core/az_rock_place.c)
set(AZ_REAL "")
set(AZ_STUB "")
foreach(m ${AZ_MODULES})
  set(real "${CMAKE_SOURCE_DIR}/core/az_${m}.c")
  set(stub "${CMAKE_SOURCE_DIR}/core/stubs/az_${m}.c")
  az_selected(sel AZ_ONLY "${m}")
  if(sel AND EXISTS "${real}")
    list(APPEND AZ_SRCS "${real}")
    list(APPEND AZ_REAL "${m}")
  else()
    list(APPEND AZ_SRCS "${stub}")
    list(APPEND AZ_STUB "${m}")
  endif()
endforeach()
# Tsotchke-derived pure kernels: core/tk_<name>.c, always built when present
file(GLOB AZ_TK_SRCS CONFIGURE_DEPENDS "${AZ_GLOB_ROOT}/core/tk_*.c")
if("${AZ_ONLY}" STREQUAL "ALL")
  list(APPEND AZ_SRCS ${AZ_TK_SRCS})
else()
  foreach(src ${AZ_TK_SRCS})
    get_filename_component(n "${src}" NAME_WE)
    az_selected(sel AZ_ONLY "${n}")
    if(sel)
      list(APPEND AZ_SRCS "${src}")
    endif()
  endforeach()
endif()
message(STATUS "az modules (real): ${AZ_REAL}")
message(STATUS "az modules (stub): ${AZ_STUB}")

set(AZ_WARN -Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes
            -Wno-unused-parameter -Werror=implicit-function-declaration
            -Werror=incompatible-pointer-types -Werror=return-type)

if(AZ_UBSAN)
  set(AZ_SAN -fsanitize=undefined -fsanitize-trap=undefined -fno-sanitize-recover=undefined)
  set(AZ_WRAPV "")
  message(STATUS "az: UBSan trap build (no -fwrapv)")
else()
  set(AZ_SAN "")
  set(AZ_WRAPV -fwrapv)
endif()

# --- the freestanding core --------------------------------------------------
# no core stack frame above 2048 bytes (findings v39, v79): a larger frame also pulls in
# ___chkstk_ms, which ctest core_obj_gate refuses. The sanitizer's checks enlarge frames, so
# the UBSan build does not enforce the limit (the normal build does).
set(AZ_CORE_FLAGS ${AZ_WARN} -ffreestanding -fno-strict-aliasing ${AZ_WRAPV} -march=${AZ_MARCH} -O2 ${AZ_SAN})
if(NOT AZ_UBSAN)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    list(APPEND AZ_CORE_FLAGS -Werror=stack-usage=2048)
  else()   # clang's form of the same rule (the second-compiler check, build/work/scripts/clang_check.ps1)
    list(APPEND AZ_CORE_FLAGS -Wframe-larger-than=2048 -Werror=frame-larger-than)
  endif()
endif()
# A current snapshot from reference physics may contain a body in bedrock.
# The collision-aware build repairs that input before publishing its live grid.
if("physics" IN_LIST AZ_REAL)
  set_property(SOURCE core/az_snapshot.c APPEND PROPERTY COMPILE_DEFINITIONS AZ_ROCK_AWARE_PHYSICS=1)
endif()
add_library(az_core STATIC ${AZ_SRCS})
target_include_directories(az_core PUBLIC core)
target_compile_options(az_core PRIVATE ${AZ_CORE_FLAGS})

# --- the protocol set: the core with TEST-ONLY interacting modules ----------------
# tests/protocol/az_<module>.c replace the social, econ, mate and life modules in a second
# core library. With the reference stubs no pair ever forms and nobody dies; with this set the
# pair-matching protocol, deaths, slot reuse and both kinds of birth run under the determinism
# and conservation checks (findings s1, v31, v38, v71). It is linked only into test_protocol
# and az_run_proto (az_run on this set, for the command-line determinism matrix), never into
# the engine. The files obey the core's rules and are compiled with the core's flags.
set(AZ_PROTO_MODULES social econ mate life)
set(AZ_HAVE_PROTO FALSE)
az_selected(sel AZ_TESTS "protocol")
if(sel AND EXISTS "${CMAKE_SOURCE_DIR}/tests/test_protocol.c")
  set(AZ_PROTO_SRCS ${AZ_SRCS})
  foreach(m ${AZ_PROTO_MODULES})
    list(FILTER AZ_PROTO_SRCS EXCLUDE REGEX "/az_${m}\\.c$")
    list(APPEND AZ_PROTO_SRCS "${CMAKE_SOURCE_DIR}/tests/protocol/az_${m}.c")
  endforeach()
  add_library(az_core_proto STATIC ${AZ_PROTO_SRCS})
  target_include_directories(az_core_proto PUBLIC core)
  target_compile_options(az_core_proto PRIVATE ${AZ_CORE_FLAGS})
  set(AZ_HAVE_PROTO TRUE)
endif()

# --- kernel dispatch + NASM twins --------------------------------------------
set(AZ_TWIN_NAMES brain_vnni dense_vnni philox_avx2 scan_avx2 physics_avx2 kin_avx2)
set(AZ_TWIN_SRCS "")
set(AZ_KERNEL_DEFS "")
foreach(t ${AZ_TWIN_NAMES})
  set(f "${CMAKE_SOURCE_DIR}/kernels/az_${t}.asm")
  az_selected(sel AZ_TWINS "${t}")
  if(sel AND EXISTS "${f}")
    list(APPEND AZ_TWIN_SRCS "${f}")
    string(TOUPPER "${t}" T)
    list(APPEND AZ_KERNEL_DEFS "AZ_HAVE_${T}=1")
  endif()
endforeach()
if(AZ_TWIN_SRCS)
  enable_language(ASM_NASM)
  set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
endif()
add_library(az_kernels STATIC kernels/az_kernels.c kernels/az_dense.c ${AZ_TWIN_SRCS})
target_include_directories(az_kernels PUBLIC core kernels)
target_compile_definitions(az_kernels PUBLIC ${AZ_KERNEL_DEFS})
target_compile_options(az_kernels PRIVATE $<$<COMPILE_LANGUAGE:C>:${AZ_WARN} -march=${AZ_MARCH} -O2 ${AZ_SAN}>)
# the twins' %include folder as ONE argument: the Z: path has spaces, which a flags string would split
target_compile_options(az_kernels PRIVATE "$<$<COMPILE_LANGUAGE:ASM_NASM>:-I${CMAKE_SOURCE_DIR}/kernels/>")
message(STATUS "az assembly twins: ${AZ_KERNEL_DEFS}")

# --- Windows host -----------------------------------------------------------
file(GLOB AZ_HOST_SRCS CONFIGURE_DEPENDS "${AZ_GLOB_ROOT}/platform/win/*.c")
add_library(az_host STATIC ${AZ_HOST_SRCS})
target_include_directories(az_host PUBLIC core platform/win)
target_compile_options(az_host PRIVATE ${AZ_WARN} -O2 -march=${AZ_MARCH} ${AZ_SAN})
# WaitOnAddress / WakeByAddress* (the worker pool's sleep and wake-up)
target_link_libraries(az_host PUBLIC synchronization bcrypt)

# --- optional component: view (software renderer; hosted C, floats allowed) --
set(AZ_HAVE_VIEW FALSE)
az_selected(sel AZ_COMPONENTS "view")
file(GLOB AZ_VIEW_SRCS CONFIGURE_DEPENDS "${AZ_GLOB_ROOT}/view/*.c")
if(sel AND AZ_VIEW_SRCS)
  add_library(az_view STATIC ${AZ_VIEW_SRCS})
  target_include_directories(az_view PUBLIC core view)
  target_compile_options(az_view PRIVATE ${AZ_WARN} -O2 -march=${AZ_MARCH})
  set(AZ_HAVE_VIEW TRUE)
endif()

# az_link(target [core library]): the second argument replaces az_core (the protocol set)
# A nonempty directory enables the per-page Windows residency check in this
# qualification build. CTest passes it to children, including the UBSan build.
set(AZ_RESIDENCY_RECEIPTS "$ENV{AZ_QUAL_LOCK_RECEIPTS}" CACHE PATH "Working-set readback receipts (empty for ordinary builds)")
if(AZ_RESIDENCY_RECEIPTS)
  file(MAKE_DIRECTORY "${AZ_RESIDENCY_RECEIPTS}")
  add_library(az_suite_guard OBJECT tests/suite_lock_guard.c)
  target_include_directories(az_suite_guard PRIVATE core)
endif()

function(az_link target)
  if(AZ_RESIDENCY_RECEIPTS)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:az_suite_guard>)
    target_link_options(${target} PRIVATE -Wl,--wrap,az_world_create)
  endif()
  set(core az_core)
  if(ARGC GREATER 1)
    set(core ${ARGV1})
  endif()
  target_link_libraries(${target} PRIVATE az_kernels ${core} az_host az_kernels ${core})
  if(AZ_HAVE_VIEW)
    target_link_libraries(${target} PRIVATE az_view ${core})
    target_compile_definitions(${target} PRIVATE AZ_HAVE_VIEW=1)
  endif()
  target_include_directories(${target} PRIVATE "${CMAKE_SOURCE_DIR}" tools tests)
  target_compile_options(${target} PRIVATE ${AZ_WARN} -O2 -march=${AZ_MARCH} ${AZ_SAN} ${AZ_UNALIGNED_MOVES})
  target_link_options(${target} PRIVATE -static)
  target_link_libraries(${target} PRIVATE gdi32 user32 psapi)
endfunction()

# tools and tests whose names start with one of these need the view component
set(AZ_VIEW_PREFIXES shot view snap$ live$)
function(az_needs_view out name)
  set(${out} FALSE PARENT_SCOPE)
  foreach(p ${AZ_VIEW_PREFIXES})
    if("${name}" MATCHES "^${p}")
      set(${out} TRUE PARENT_SCOPE)
    endif()
  endforeach()
endfunction()

# --- tools: tools/az_<name>.c -------------------------------------------------
file(GLOB AZ_TOOL_SRCS CONFIGURE_DEPENDS "${AZ_GLOB_ROOT}/tools/az_*.c")
foreach(src ${AZ_TOOL_SRCS})
  get_filename_component(exe "${src}" NAME_WE)
  string(REGEX REPLACE "^az_" "" name "${exe}")
  az_selected(sel AZ_TOOLS "${name}")
  az_needs_view(nv "${name}")
  if(sel AND (NOT nv OR AZ_HAVE_VIEW))
    add_executable(${exe} "${src}")
    az_link(${exe})
  endif()
endforeach()
# The hosted reporters and their causal witnesses must use the same instrumented
# core objects, callback implementations and preprocessor definitions.
if("metab" IN_LIST AZ_REAL AND "grid" IN_LIST AZ_REAL AND "stats" IN_LIST AZ_REAL AND (TARGET az_run OR TARGET az_live))
  set(AZ_OBSERVER_CORE core/az_metab.c core/az_grid.c)
  set(AZ_OBSERVER_REPORTS tools/intake_report.c)
  set(AZ_OBSERVER_DEFINITIONS AZ_INTAKE_OBSERVER=1)
  if("econ" IN_LIST AZ_REAL)
    list(APPEND AZ_OBSERVER_CORE core/az_econ.c)
    list(APPEND AZ_OBSERVER_DEFINITIONS AZ_COMMONS_OBSERVER=1)
  endif()
  if("social" IN_LIST AZ_REAL)
    list(APPEND AZ_OBSERVER_CORE core/az_tick.c core/az_social.c)
    list(APPEND AZ_OBSERVER_REPORTS tools/hermit_report.c)
    list(APPEND AZ_OBSERVER_DEFINITIONS AZ_HERMIT_OBSERVER=1 AZ_PREDATION_OBSERVER=1)
    if("mate" IN_LIST AZ_REAL)
      list(APPEND AZ_OBSERVER_CORE core/az_mate.c)
      list(APPEND AZ_OBSERVER_REPORTS tools/partner_report.c)
      list(APPEND AZ_OBSERVER_DEFINITIONS AZ_PARTNER_OBSERVER=1)
    endif()
    if("econ" IN_LIST AZ_REAL AND "field" IN_LIST AZ_REAL)
      list(APPEND AZ_OBSERVER_REPORTS tools/builder_report.c)
      list(APPEND AZ_OBSERVER_DEFINITIONS AZ_BUILDER_OBSERVER=1)
    endif()
  endif()
  add_library(az_intake_core OBJECT ${AZ_OBSERVER_CORE})
  target_include_directories(az_intake_core PRIVATE core)
  target_compile_definitions(az_intake_core PRIVATE ${AZ_OBSERVER_DEFINITIONS})
  target_compile_options(az_intake_core PRIVATE ${AZ_CORE_FLAGS})

  function(az_attach_observers target)
    target_sources(${target} PRIVATE ${AZ_OBSERVER_REPORTS} $<TARGET_OBJECTS:az_intake_core>)
    target_compile_definitions(${target} PRIVATE ${AZ_OBSERVER_DEFINITIONS})
  endfunction()

  foreach(tool az_run az_live)
    if(TARGET ${tool})
      az_attach_observers(${tool})
    endif()
  endforeach()
endif()

# az_run on the protocol set: same program, same options, the interacting modules
az_selected(sel AZ_TOOLS "run")
if(sel AND AZ_HAVE_PROTO)
  add_executable(az_run_proto "${CMAKE_SOURCE_DIR}/tools/az_run.c")
  az_link(az_run_proto az_core_proto)
endif()

# --- tests: tests/test_<name>.c, one executable each ---------------------------
enable_testing()
file(GLOB AZ_TEST_SRCS CONFIGURE_DEPENDS "${AZ_GLOB_ROOT}/tests/test_*.c")
foreach(src ${AZ_TEST_SRCS})
  get_filename_component(exe "${src}" NAME_WE)
  string(REGEX REPLACE "^test_" "" name "${exe}")
  az_selected(sel AZ_TESTS "${name}")
  # The body acceptance fixture executes generated worlds, snapshots and the public tick.
  if(name STREQUAL "physics_body_controls")
    foreach(required IN ITEMS physics grid worldgen genome brain sense metab field ledger snapshot)
      if(NOT "${required}" IN_LIST AZ_REAL)
        set(sel FALSE)
      endif()
    endforeach()
  endif()
  az_needs_view(nv "${name}")
  if(sel AND (NOT nv OR AZ_HAVE_VIEW))
    add_executable(${exe} "${src}")
    if(name STREQUAL "physics" AND "AZ_HAVE_KIN_AVX2=1" IN_LIST AZ_KERNEL_DEFS)
      add_library(physics_abi_probe OBJECT tests/physics_abi.asm)
      target_compile_options(physics_abi_probe PRIVATE "-I${CMAKE_SOURCE_DIR}/kernels/")
      target_sources(${exe} PRIVATE $<TARGET_OBJECTS:physics_abi_probe>)
    endif()
    # A standalone kernel test still exercises the real kernel when the engine uses stubs.
    set(test_kernel "${CMAKE_SOURCE_DIR}/core/${name}.c")
    if("${test_kernel}" IN_LIST AZ_TK_SRCS AND NOT "${test_kernel}" IN_LIST AZ_SRCS)
      add_library(${exe}_kernel OBJECT "${test_kernel}")
      target_compile_options(${exe}_kernel PRIVATE ${AZ_CORE_FLAGS})
      target_sources(${exe} PRIVATE $<TARGET_OBJECTS:${exe}_kernel>)
    endif()
    if("${name}" STREQUAL "rock_placement" AND "snapshot" IN_LIST AZ_REAL)
      target_compile_definitions(${exe} PRIVATE AZ_TEST_ROCK_SNAPSHOT=1)
      if("physics" IN_LIST AZ_REAL)
        target_compile_definitions(${exe} PRIVATE AZ_TEST_ROCK_AWARE_PHYSICS=1)
      endif()
    endif()
    if(name STREQUAL "wide_lifecycle" AND "life" IN_LIST AZ_REAL)
      target_compile_definitions(${exe} PRIVATE AZ_TEST_REAL_BROOD=1)
    endif()
    if("${name}" STREQUAL "life" AND "${AZ_ONLY}" STREQUAL "ALL" AND "sense" IN_LIST AZ_REAL AND "genome" IN_LIST AZ_REAL
       AND "brain" IN_LIST AZ_REAL AND "field" IN_LIST AZ_REAL)
      target_compile_definitions(${exe} PRIVATE AZ_TEST_DEVELOPED_SENSE_TRAITS=1)
      if("physics" IN_LIST AZ_REAL)
        target_compile_definitions(${exe} PRIVATE AZ_TEST_DEVELOPED_PHYSICS_TRAITS=1)
      endif()
    endif()
    if("${name}" STREQUAL "field" AND "${AZ_ONLY}" STREQUAL "ALL" AND "metab" IN_LIST AZ_REAL AND "worldgen" IN_LIST AZ_REAL)
      target_compile_definitions(${exe} PRIVATE AZ_TEST_GENERATED_DEMAND=1)
    endif()
    if(("${name}" STREQUAL "matter_posts" OR "${name}" STREQUAL "colony_pantry" OR "${name}" STREQUAL "physics_state") AND "snapshot" IN_LIST AZ_REAL)
      target_compile_definitions(${exe} PRIVATE AZ_TEST_HAS_SNAPSHOT=1)
    endif()
    if("${name}" STREQUAL "colony_pantry" AND "econ" IN_LIST AZ_REAL AND "worldgen" IN_LIST AZ_REAL AND "genome" IN_LIST AZ_REAL)
      target_compile_definitions(${exe} PRIVATE AZ_TEST_REAL_PANTRY=1)
      if("colony" IN_LIST AZ_REAL)
        target_compile_definitions(${exe} PRIVATE AZ_TEST_REAL_COLONY_FOUNDING=1)
      endif()
    endif()
    if("${name}" STREQUAL "chem_stock" AND NOT "chem" IN_LIST AZ_REAL)
      target_compile_definitions(${exe} PRIVATE AZ_TEST_STUB_CHEM=1)
    endif()
    if("${name}" STREQUAL "protocol")
      az_link(${exe} az_core_proto)
    else()
      az_link(${exe})
    endif()
    add_test(NAME ${exe} COMMAND ${exe})
    set_tests_properties(${exe} PROPERTIES TIMEOUT 600)
  endif()
endforeach()

# The vendored libirrep oracle keeps upstream's own #include <irrep/...> lines:
# its files are byte-identical copies of the mirror (owners' request, milestone 2).
if(TARGET test_tk_sh)
  target_include_directories(test_tk_sh PRIVATE "${CMAKE_SOURCE_DIR}/tests/upstream/libirrep/include")
endif()

# --- lint as a test (integration builds only) ----------------------------------
if("${AZ_TESTS}" STREQUAL "ALL")
  find_program(AZ_PYTHON NAMES python python3)
  if(AZ_PYTHON)
    add_test(NAME lint_core COMMAND ${AZ_PYTHON} "${CMAKE_SOURCE_DIR}/tools/lint_core.py" "${CMAKE_SOURCE_DIR}")
    # planted violations, one per lint rule (finding v33)
    add_test(NAME lint_core_selftest COMMAND ${AZ_PYTHON} "${CMAKE_SOURCE_DIR}/tools/lint_core.py" --self-test "${CMAKE_SOURCE_DIR}")
    # the compiled core: no floating point, no writable data, only memcpy/memset/memmove/memcmp undefined
    # (a stack frame above one page would pull in ___chkstk_ms; -Werror=stack-usage=2048 keeps frames small)
    add_test(NAME core_obj_gate COMMAND ${AZ_PYTHON} "${CMAKE_SOURCE_DIR}/tools/check_core_obj.py" "$<TARGET_FILE:az_core>")
    add_test(NAME avx_stack_gate COMMAND ${AZ_PYTHON} "${CMAKE_SOURCE_DIR}/tools/check_avx_stack.py" --dir "${CMAKE_BINARY_DIR}")
    add_test(NAME avx_stack_gate_selftest COMMAND ${AZ_PYTHON} "${CMAKE_SOURCE_DIR}/tools/check_avx_stack.py" --self-test)
    add_test(NAME core_obj_gate_selftest COMMAND ${AZ_PYTHON} "${CMAKE_SOURCE_DIR}/tools/check_core_obj.py" --self-test "$<TARGET_FILE:az_core>" --work "${CMAKE_BINARY_DIR}/check_core_obj_selftest")
    # az_run --ledger across --save/--load: one run of 40 ticks against 20 + 20 (milestone 2, finding s2)
    if(TARGET az_run AND TARGET az_ledgerdump)
      add_test(NAME cli_ledger_resume COMMAND ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tests/cli/ledger_resume.py"
               "$<TARGET_FILE:az_run>" "$<TARGET_FILE:az_ledgerdump>" "${CMAKE_BINARY_DIR}/cli_ledger_resume")
    endif()
    # az_snap and az_run in both modes of the world: wild by default (never the same twice, the
    # balance exact), --lab repeatable and equal to az_run --lab; --seed without --lab refused
    if(TARGET az_run AND TARGET az_snap)
      add_test(NAME cli_snap_modes COMMAND ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tests/cli/snap_modes.py"
               "$<TARGET_FILE:az_snap>" "$<TARGET_FILE:az_run>" "${CMAKE_BINARY_DIR}/cli_snap_modes")
    endif()
    # az_live, the live world behind demo/live.html: both modes, exact books every tick, the view an
    # observer, the lab hash equal to az_run --lab's, the stdin commands, save and az_run --load
    if(TARGET az_run AND TARGET az_live)
      add_test(NAME cli_live_smoke COMMAND ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tests/cli/live_smoke.py"
               "$<TARGET_FILE:az_live>" "$<TARGET_FILE:az_run>" "${CMAKE_BINARY_DIR}/cli_live_smoke")
    endif()
    if(TARGET az_intake_core AND TARGET az_run AND TARGET az_live AND "${AZ_ONLY}" STREQUAL "ALL")
      if("econ" IN_LIST AZ_REAL)
        add_test(NAME cli_colony_commons COMMAND ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tests/cli/colony_commons.py"
                 "$<TARGET_FILE:az_run>" "$<TARGET_FILE:az_live>" "${CMAKE_BINARY_DIR}/cli_colony_commons")
      endif()
      if("mate" IN_LIST AZ_REAL AND "social" IN_LIST AZ_REAL)
        add_executable(partner_witness tests/partners/witness.c)
        az_attach_observers(partner_witness)
        az_link(partner_witness)
        add_test(NAME partner_witness COMMAND partner_witness)
        add_test(NAME cli_partner_observations COMMAND ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tests/cli/partner_observations.py"
                 "$<TARGET_FILE:az_run>" "$<TARGET_FILE:az_live>" "${CMAKE_BINARY_DIR}/cli_partner_observations")
      endif()
      if("social" IN_LIST AZ_REAL AND "econ" IN_LIST AZ_REAL AND "field" IN_LIST AZ_REAL)
        add_executable(nest_action_proof tests/cli/nest_action.c)
        az_attach_observers(nest_action_proof)
        az_link(nest_action_proof)
        add_test(NAME nest_action_proof COMMAND nest_action_proof)
        add_test(NAME cli_nest_mark COMMAND ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tests/cli/nest_mark.py"
                 "$<TARGET_FILE:az_run>" "$<TARGET_FILE:az_live>" "${CMAKE_BINARY_DIR}/cli_nest_mark")
      endif()
      if("social" IN_LIST AZ_REAL)
        add_test(NAME cli_funded_predation COMMAND ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tests/cli/funded_predation.py"
                 "$<TARGET_FILE:az_run>" "$<TARGET_FILE:az_live>" "${CMAKE_BINARY_DIR}/cli_funded_predation")
        add_test(NAME cli_recruitment_refusal COMMAND ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tests/cli/recruitment_refusal.py"
                 "$<TARGET_FILE:az_run>" "$<TARGET_FILE:az_live>" "${CMAKE_BINARY_DIR}/cli_recruitment_refusal")
      endif()
      add_test(NAME cli_funded_intake COMMAND ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tests/cli/funded_intake.py"
               "$<TARGET_FILE:az_run>" "$<TARGET_FILE:az_live>" "${CMAKE_BINARY_DIR}/cli_funded_intake")
    endif()
    # Real-time observer and transport checks.
    add_test(NAME issues_register COMMAND ${AZ_PYTHON} -I -B "${CMAKE_SOURCE_DIR}/build/work/scripts/issues.py" --check)
    add_test(NAME issues_register_selftest COMMAND ${AZ_PYTHON} -I -B "${CMAKE_SOURCE_DIR}/build/work/scripts/issues.py" --self-test)
    if(TARGET az_live AND TARGET az_run)
      add_test(NAME cli_stream_smoke COMMAND ${AZ_PYTHON} -I -B "${CMAKE_SOURCE_DIR}/tests/cli/stream_smoke.py"
               "$<TARGET_FILE:az_live>" "$<TARGET_FILE:az_run>" "${CMAKE_BINARY_DIR}/cli_stream_smoke")
      add_test(NAME cli_ws_selftest COMMAND ${AZ_PYTHON} -I -B "${CMAKE_SOURCE_DIR}/tests/cli/ws_selftest.py")
      find_program(AZ_NODE NAMES node)
      if(AZ_NODE)
        add_test(NAME cli_viewer_transport COMMAND ${AZ_NODE} "${CMAKE_SOURCE_DIR}/tests/cli/viewer_transport_test.mjs")
        add_test(NAME cli_viewer_core COMMAND ${AZ_NODE} "${CMAKE_SOURCE_DIR}/tests/cli/viewer_core_test.mjs"
                 "$<TARGET_FILE:az_live>" "${CMAKE_BINARY_DIR}/cli_viewer_core")
      endif()
    endif()

    # the command-line determinism matrix (plan T6): one world under every execution variation that
    # must not change it (workers 1..24 with E cores, pinned and not, reorder periods, work splits,
    # kernels on and off, a snapshot resumed at another worker count); equal final hash, hash trace,
    # ledger chain and counters, exact matter and energy every tick, arena locked
    set(AZ_MATRIX ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tests/cli/determinism_matrix.py")
    if(TARGET az_run_proto)
      # the interacting protocol set: deaths, births, pairs of four kinds; then a binding capacity
      add_test(NAME cli_matrix_protocol COMMAND ${AZ_MATRIX} "$<TARGET_FILE:az_run_proto>"
               "${CMAKE_BINARY_DIR}/cli_matrix_protocol" --interacting -- --n 20000 --ticks 60 --seed 7 --cap 200000)
      add_test(NAME cli_matrix_protocol_cap COMMAND ${AZ_MATRIX} "$<TARGET_FILE:az_run_proto>"
               "${CMAKE_BINARY_DIR}/cli_matrix_protocol_cap" --interacting --expect-denied
               -- --n 20000 --ticks 60 --seed 7 --cap 20224)
      set_tests_properties(cli_matrix_protocol cli_matrix_protocol_cap PROPERTIES TIMEOUT 1800)
    endif()
    if(TARGET az_run)
      # the engine as built (stubs until wave 2: births, no deaths, no pairs; weak evidence until then)
      add_test(NAME cli_matrix_engine COMMAND ${AZ_MATRIX} "$<TARGET_FILE:az_run>"
               "${CMAKE_BINARY_DIR}/cli_matrix_engine" -- --n 20000 --ticks 40 --seed 7)
      set_tests_properties(cli_matrix_engine PROPERTIES TIMEOUT 1800)
    endif()
    # the UBSan trap build as a test of this build (plan T6): a second build folder with
    # -DAZ_UBSAN=ON, planted violations that must trap, then every test program there
    # (minutes, not seconds)
    if(NOT AZ_UBSAN)
      add_test(NAME ubsan_trap_build COMMAND ${AZ_PYTHON} -I "${CMAKE_SOURCE_DIR}/tools/ubsan_check.py"
               "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/ubsan")
      set_tests_properties(ubsan_trap_build PROPERTIES TIMEOUT 3600)
    endif()
  endif()
endif()

# planted violations for the UBSan trap build: tools/ubsan_check.py shows that it traps on each
if(AZ_UBSAN)
  add_executable(ubsan_probe "${CMAKE_SOURCE_DIR}/tests/ubsan_probe.c")
  target_compile_options(ubsan_probe PRIVATE ${AZ_WARN} -O2 ${AZ_SAN})
endif()

# --- optional component with its own build file: gpu ----------------------------
# There is no boot-image or no-OS component: the owner's rule L6 (2026-11-17)
# says the engine runs only as a Windows program from Z:.
az_selected(sel AZ_COMPONENTS "gpu")
if(sel AND EXISTS "${CMAKE_SOURCE_DIR}/gpu/CMakeLists.txt")
  add_subdirectory("${CMAKE_SOURCE_DIR}/gpu" "${CMAKE_BINARY_DIR}/gpu")
endif()

if(AZ_RESIDENCY_RECEIPTS)
  get_property(az_root_tests DIRECTORY PROPERTY TESTS)
  if(az_root_tests)
    set_property(TEST ${az_root_tests} APPEND PROPERTY ENVIRONMENT "AZ_QUAL_LOCK_RECEIPTS=${AZ_RESIDENCY_RECEIPTS}")
  endif()
endif()
