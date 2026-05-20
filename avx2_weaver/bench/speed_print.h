#ifndef PRINT_SPEED_H
#define PRINT_SPEED_H

#include <stddef.h>
#include <stdint.h>

void print_results(const char *s, uint64_t *t, size_t tlen);

/* median, p10, p90 (after overhead subtraction; needs tlen >= 11). */
void print_results_stats(const char *s, uint64_t *t, size_t tlen);

#endif
