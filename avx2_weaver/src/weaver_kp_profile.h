/* Optional hooks for indcpa_keypair_derand cycle profiling (profile_encap_decaps only). */
#ifndef WEAVER_KP_PROFILE_H
#define WEAVER_KP_PROFILE_H

#include <stdint.h>

void weaver_kp_prof_segment(int slot, uint64_t raw_delta_cycles);
void weaver_kp_prof_begin_sample(int store_index);

#endif
