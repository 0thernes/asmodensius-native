# Asmodensius: an overview

*A public summary, November 2026. The internal design, data formats, algorithms, parameters and source
code are confidential and are not described here.*

## In one line

Asmodensius is an artificial-life engine: a self-running 3D alien world in which digital organisms are
born, feed, compete, cooperate, mate and learn, and pass inherited, mutating instructions to their young,
with no script and no score telling them what to do.

## What it is

- A purpose-built simulation engine. It is not a game engine with built-in AI, and it does not use any
  language model.
- It runs on a single high-end PC.
- It generates a whole alien world: terrain, ocean, atmosphere, climate, chemistry, its own sky and its
  own gravity.
- Each world starts from simple founder species and takes it from there.

## What it does

- Every organism carries inherited instructions, a body and a brain. Children inherit from their parents,
  with variation and mutation.
- Organisms need resources from their world to keep going and to reproduce. They feed, fight, cooperate,
  trade, choose mates and live in groups, among other behaviours.
- By design, organisms are not built to a fixed template: their makeup is allowed to grow in size and
  complexity over generations, within what their world can supply.
- Matter and energy always balance: nothing appears from nowhere and nothing vanishes.
- Every world is unique and unpredictable by default. A repeatable mode exists for scientific tests.
- There is no fitness score and no designer-chosen goal. What lasts is whatever manages to survive and
  reproduce in that world.

## What makes it different

- No hidden steering and no decoration: every part of the biology has to show, by experiment, that it
  really affects what happens. The build is still working toward this; in early tests some parts do not
  yet show an effect.
- No borrowed intelligence: no language model, no pretrained network, no training data. Behaviour comes
  only from each organism's inherited brain and what it learns in its world.
- Scientific rules set from the start: repeatable test worlds (working today), statistics fixed before
  each experiment, and independent review before anything counts as done (the formal experiments and
  reviews are still ahead).

## Where it stands (November 2026)

- A working proof of concept. Its automated test suite passes in full.
- Built: the world itself and a large part of the biology.
- Still simple placeholders: how creatures sense their world and move through it, and the population
  statistics. The world has gravity, but the creatures' bodies do not feel it yet.
- It already runs worlds of up to about a million organisms on one PC, but large worlds are slow today;
  making them fast is a main goal of the coming stages.
- A local web page shows a running world as a series of still pictures with live charts. A smooth,
  real-time 3D view is designed and is next in line.
- The first four of ten planned stages are complete and the fifth is under way. The later stages hold
  most of the hard work, including all of the scientific testing.

## End goals of the full build

1. **A complete biology**, with no placeholders left: organisms that sense their world, move through it
   under real physics (gravity, ground, water), feed and reproduce. Plus one experiment, designed in
   advance, that tests whether they really use their senses to find food and turn it into offspring,
   reported honestly whether or not it works.
2. **Find out whether evolution keeps going.** The hope is worlds that keep producing new body forms,
   behaviours, species and colonies over long runs, from small simple founders toward large, complex
   creatures and colony superorganisms, with no designer choosing the outcome. This is not promised: it
   will be measured over long runs and reported as found.
3. **A smooth real-time view:** the world's land, ocean and creatures drawn on the graphics card at 60
   frames per second, first for worlds of a few hundred thousand creatures, later for millions. A smooth
   picture does not make the world itself step faster; the world's own speed is a separate goal.
4. **Bigger worlds on the same computer:** more than a million organisms, on the processor first; later,
   once the foundation is proven, the graphics card and other accelerators, each kept only if it gives
   exactly the same results.
5. **Measured, not claimed:** the published list of 14 indicator properties by Butlin and colleagues, plus
   the project's own extensive checklists and benchmarks (hundreds of items), all run with statistics
   fixed in advance and with independent audits. Completion means every item gets an evidenced verdict or
   a stated reason. It does not mean every item passes, and there is no overall score.
6. **A final report** in which every claim is backed by evidence.

## What it does not claim

Asmodensius does not claim to be alive, conscious, sentient or generally intelligent. Its evaluations are
indicator scorecards: they test whether specific properties are present and make a measurable difference,
and nothing more.

Not shown yet: that evolution in these worlds favours any particular trait; open-ended evolution; and any
checklist result for this engine (earlier results came from an older prototype and do not carry over).
