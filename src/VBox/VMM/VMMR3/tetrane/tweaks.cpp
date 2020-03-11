#include <VBox/vmm/tetrane.h>

#include "vm_tweaks.h"

#include <cstdlib>

#include <sys/stat.h>
#include <sys/types.h>

VBOXSTRICTRC tetrane_handle_step_instruction(PVM pVM)
{
    tetrane::tweaks::vm_tweaks* vm_tweaks = tweaks(pVM);
    if (vm_tweaks)
        return tweaks(pVM)->handle_step_instruction();
    return VINF_SUCCESS;
}

VBOXSTRICTRC tetrane_handle_int3(PVM pVM)
{
    tetrane::tweaks::vm_tweaks* vm_tweaks = tweaks(pVM);
    if (vm_tweaks)
        return tweaks(pVM)->handle_int3();
    return VINF_SUCCESS;
}

VBOXSTRICTRC tetrane_handle_instruction_not_implemented(PVM pVM)
{
    tetrane::tweaks::vm_tweaks* vm_tweaks = tweaks(pVM);
    if (vm_tweaks)
        return tweaks(pVM)->handle_instruction_not_implemented();

    return VINF_SUCCESS;
}

void read_port(PVM pVM, RTIOPORT Port, uint32_t read_value, size_t cbValue, PIOMIOPORTRANGER3 pRange)
{
    tetrane::tweaks::vm_tweaks* vm_tweaks = tweaks(pVM);
    if (vm_tweaks)
        vm_tweaks->read_port(Port, read_value, cbValue, pRange);
}

void write_port(PVM pVM, RTIOPORT Port, uint32_t written_value, size_t cbValue, PIOMIOPORTRANGER3 pRange)
{
    tetrane::tweaks::vm_tweaks* vm_tweaks = tweaks(pVM);
    if (vm_tweaks)
        vm_tweaks->write_port(Port, written_value, cbValue, pRange);
}

void flush_data(PVM pVM)
{
    tetrane::tweaks::vm_tweaks* vm_tweaks = tweaks(pVM);
    if (vm_tweaks)
        vm_tweaks->flush_data();
}

void register_hardware_access(PVM pVM, struct saved_hardware_access *access, const unsigned char *data)
{
    tetrane::tweaks::vm_tweaks* vm_tweaks = tweaks(pVM);
    if (vm_tweaks)
        vm_tweaks->register_hardware_access(access, data);
}


void tetrane_init_tweaks(PVM pVM)
{
    const char *work_folder = getenv("RVN_WORK_FOLDER");

    struct stat st;
    if (work_folder && (stat(work_folder, &st) == 0) && S_ISDIR(st.st_mode))
        pVM->tetrane.vm_tweaks = new tetrane::tweaks::vm_tweaks(pVM, &pVM->aCpus[0], work_folder);
    else
        pVM->tetrane.vm_tweaks = NULL;

    pVM->tetrane.skip_next_int3 = false;
}
