/* tests/ubsan_probe.c - planted violations for the UBSan trap build (plan T6).
 *
 * Built only with -DAZ_UBSAN=ON, with the same sanitizer flags as the core.
 * tools/ubsan_check.py runs it once per case: "none" must exit 0 and every
 * other case must die on the sanitizer's trap (an illegal instruction), which
 * shows that the build really stops on undefined behaviour of that kind. A
 * trap build that lets one of these through proves nothing about the engine.
 *
 * The operands come from the command line through volatile variables, so the
 * compiler can neither fold the violation away nor refuse it at compile time.
 */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *const cases[] = { "none", "overflow", "shift", "bounds", "divzero", "null", "align" };

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: ubsan_probe CASE\ncases:");
        for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) printf(" %s", cases[i]);
        printf("\n");
        return 2;
    }
    const char *c = argv[1];
    volatile int big = INT_MAX, one = 1, zero = 0, forty = 40, seven = 7;
    int table[4] = { 1, 2, 3, 4 };
    _Alignas(8) unsigned char bytes[16] = { 0 };
    int *volatile nowhere = NULL;
    unsigned char *volatile odd = bytes + 1;
    int r = 0;

    if (!strcmp(c, "none")) r = big - one;                              /* defined */
    else if (!strcmp(c, "overflow")) r = big + one;                     /* signed overflow */
    else if (!strcmp(c, "shift")) r = (int)(1u << forty);               /* shift count >= width */
    else if (!strcmp(c, "bounds")) r = table[seven];                    /* index past the array */
    else if (!strcmp(c, "divzero")) r = big / zero;                     /* division by zero */
    else if (!strcmp(c, "null")) r = *nowhere;                          /* null pointer read */
    else if (!strcmp(c, "align")) r = (int)*(uint32_t *)(void *)odd;    /* misaligned read */
    else { printf("unknown case '%s'\n", c); return 2; }

    printf("%s: survived, value %d\n", c, r);
    return 0;
}
