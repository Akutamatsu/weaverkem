#include <stdint.h>
#include "params.h"
#include "avx/consts7681.h"

#if WEAVER_Q == 7681

#define Q7681 7681
#define QINV7681 (-7679)
#define V7681 (((1 << 26) + Q7681 / 2) / Q7681)

const q7681data WEAVER_NAMESPACE(_qdata7681) = {
  .q = {
    Q7681, Q7681, Q7681, Q7681, Q7681, Q7681, Q7681, Q7681,
    Q7681, Q7681, Q7681, Q7681, Q7681, Q7681, Q7681, Q7681,
  },
  .qinv = {
    QINV7681, QINV7681, QINV7681, QINV7681, QINV7681, QINV7681, QINV7681, QINV7681,
    QINV7681, QINV7681, QINV7681, QINV7681, QINV7681, QINV7681, QINV7681, QINV7681,
  },
  .v = { V7681, V7681, V7681, V7681, V7681, V7681, V7681, V7681 },
  .q32 = { Q7681, Q7681, Q7681, Q7681, Q7681, Q7681, Q7681, Q7681 },
  .bias = {
    (1 << 25), (1 << 25), (1 << 25), (1 << 25),
    (1 << 25), (1 << 25), (1 << 25), (1 << 25),
  },
};

#endif
