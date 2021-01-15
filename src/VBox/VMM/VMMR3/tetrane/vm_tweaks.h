#ifndef TETRANE_VBOX_VM_TWEAKS_H
#define TETRANE_VBOX_VM_TWEAKS_H

#define VMCPU_INCL_CPUM_GST_CTX

#include "../../include/PDMInternal.h"

#include <VBox/types.h>
#include <VBox/vmm/vm.h>

#include <map>
#include <string>

#include <iprt/semaphore.h>

#include <ostream>

struct IOMMMIOENTRYR3;
typedef struct IOMMMIOENTRYR3 *PIOMMMIOENTRYR3;

struct IOMIOPORTENTRYR3;
typedef struct IOMIOPORTENTRYR3 *PIOMIOPORTENTRYR3;

namespace tetrane {
namespace tweaks {


//! This centralize some tweaking that need to be done on a VM.
class vm_tweaks
{
public:
    //! Create a vm_tweaks object.
    //!
    //! Currently this works on a single CPU so pCVpu == pVM->aCpu
    //! Note that this constructor expects that work_folder exists and is writable.
    vm_tweaks(PVM pVM, PVMCPU pVCpu, const char *work_folder);

private:
    PVM vm;

    // Currently, a single CPU is managed.
    PVMCPU cpu;

    // The base name of the cores to create. Doesn't contain the path or the .core suffix
    const char *core_name;

    // Working folder. All generated files should be written here so that they end up in the .tgz file.
    std::string work_folder;

    // Current number of generated cores.
    uint32_t core_number;

}; // class vm_tweaks

}} // namespace tetrane::tweaks


// Find the tweaks of a VM
static inline tetrane::tweaks::vm_tweaks *tweaks(PVM pVM)
{
    return static_cast<tetrane::tweaks::vm_tweaks*>(pVM->tetrane.vm_tweaks);
}

// Find the tweaks of a VM from the CPU
static inline tetrane::tweaks::vm_tweaks *tweaks(PVMCPU pVCpu)
{
    return tweaks(pVCpu->pVMR3);
}


#endif // TETRANE_VBOX_VM_TWEAKS_H
