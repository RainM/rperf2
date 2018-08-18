#pragma once

#if defined(ENABLE_DEBUG)
#  define DEBUG(x) (x)
#else
#  define DEBUG(x) (0)
#endif
