#include <stdio.h>
#include <stdint.h>
#include "params.h"
#include "poly.h"
#include "symmetric.h"

static void write_poly(FILE *out, const poly *p)
{
  unsigned i;
  for(i = 0; i < WEAVER_N; i++)
    fwrite(&p->coeffs[i], sizeof(int16_t), 1, out);
}

int main(void)
{
  uint8_t buf[WEAVER_ETA1 * WEAVER_N / 4];
  poly r;
  unsigned t, i;

  for(t = 0; t < 50; t++) {
    for(i = 0; i < sizeof(buf); i++)
      buf[i] = (uint8_t)(0x37 ^ (t * 19 + i * 7));
    cbd_eta1(&r, buf);
    if(fwrite(buf, 1, sizeof(buf), stdout) != sizeof(buf)) return 1;
    write_poly(stdout, &r);
  }
  return 0;
}
