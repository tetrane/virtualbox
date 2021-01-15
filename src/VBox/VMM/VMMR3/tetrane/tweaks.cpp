#include <VBox/vmm/tetrane.h>

#include "vm_tweaks.h"

#include <cstdlib>

#include <sys/stat.h>
#include <sys/types.h>

void tetrane_init_tweaks(PVM pVM)
{
    const char *work_folder = getenv("RVN_WORK_FOLDER");

    struct stat st;
    if (work_folder && (stat(work_folder, &st) == 0) && S_ISDIR(st.st_mode))
        pVM->tetrane.vm_tweaks = new tetrane::tweaks::vm_tweaks(pVM, VMCC_GET_CPU_0(pVM), work_folder);
    else
        pVM->tetrane.vm_tweaks = NULL;

    pVM->tetrane.skip_next_int3 = false;
}
