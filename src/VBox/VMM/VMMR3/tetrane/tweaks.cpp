#include <VBox/vmm/tetrane.h>

#include "vm_tweaks.h"

#include <cstdlib>

#include <sys/stat.h>
#include <sys/types.h>

void read_port(PVM pVM, RTIOPORT Port, uint32_t read_value, size_t cbValue)
{
    tetrane::tweaks::vm_tweaks* vm_tweaks = tweaks(pVM);
    if (vm_tweaks)
        vm_tweaks->read_port(Port, read_value, cbValue);
}

void write_port(PVM pVM, RTIOPORT Port, uint32_t written_value, size_t cbValue)
{
    tetrane::tweaks::vm_tweaks* vm_tweaks = tweaks(pVM);
    if (vm_tweaks)
        vm_tweaks->write_port(Port, written_value, cbValue);
}

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
