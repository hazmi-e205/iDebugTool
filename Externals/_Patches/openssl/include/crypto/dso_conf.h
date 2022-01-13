#pragma once

#if defined(WIN32)
    #include "dso_conf_win32.h"
#else
    #include "dso_conf_linux.h"
#endif