/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Provide a real file - not a symlink - as it would cause multiarch conflicts
   when multiple different arch releases are installed simultaneously.  */

#if defined __arm__
# include "tdep-arm/dwarf-config.h"
#elif defined __hppa__
# include "tdep-hppa/dwarf-config.h"
#elif defined __ia64__
# include "tdep-ia64/dwarf-config.h"
#elif defined __mips__
# include "tdep-mips/dwarf-config.h"
#elif defined __powerpc__ && !defined __powerpc64__
# include "tdep-ppc32/dwarf-config.h"
#elif defined __powerpc64__
# include "tdep-ppc64/dwarf-config.h"
#elif defined __i386__
# include "tdep-x86/dwarf-config.h"
#elif defined __x86_64__ || defined __amd64__
# include "tdep-x86_64/dwarf-config.h"
#else
# error "Unsupported arch"
#endif
