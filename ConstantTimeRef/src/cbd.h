#ifndef CBD_H
#define CBD_H

#include <stdint.h>
#include "params.h"
#include "poly.h"

#define cbd_eta1 WEAVER_NAMESPACE(_cbd_eta1)
void cbd_eta1(poly *r, const uint8_t buf[WEAVER_ETA1*WEAVER_N/4]);

#endif
