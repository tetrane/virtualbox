#include "vm_tweaks.h"
#include <VBox/vmm/tetrane.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/pdmdev.h>

#include "../../../Devices/Graphics/DevVGA.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <inttypes.h>
#include <fstream>

#include <fstream>
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <vector>

#include <VBox/vmm/hm.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>
#include <VBox/vmm/cpumdis.h>
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/vmm/dbgf.h>

#include <iostream>


#define UD2_OPCODE 0x0b0f

namespace tetrane {
namespace tweaks {



vm_tweaks::vm_tweaks(PVM pVM, PVMCPU pVCpu, const char *folder)
    : vm(pVM),
      cpu(pVCpu),
      core_name(getenv("CORE_FILE_NAME")),
      work_folder(folder)
{
    if (cpu == NULL)
        cpu = (PVMCPU)vm->apCpusR3;

    // ensure that the folder ends with / to ease the naming of files
    if (work_folder[work_folder.size() - 1] != '/')
        work_folder += "/";

    if (!core_name)
        core_name = "default";

    memset(&cpu->reven, 0, sizeof(cpu->reven));
}

}} // namespace tetrane::tweaks
