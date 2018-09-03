#include "time.hpp"

#include <inttypes.h>

#ifndef _MSC_VER
#include <sys/time.h>
#endif

#ifdef __MACH__
#include <mach/mach_time.h>
#include "thread_local.hpp"
#include "utils.hpp"
#endif

#include "config/args.hpp"
#include "errors.hpp"

#ifdef __MACH__
TLS(mach_timebase_info_data_t, mach_time_info);
#endif  // __MACH__

