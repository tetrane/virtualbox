#ifndef TETRANE_VBOX_VM_TWEAKS_H
#define TETRANE_VBOX_VM_TWEAKS_H

#define VMCPU_INCL_CPUM_GST_CTX

#include "../../include/PDMInternal.h"

#include <VBox/types.h>
#include <VBox/vmm/vm.h>

#include <map>
#include <string>

#include <iprt/semaphore.h>

#include "stream_file.h"
#include "device.h"

#include <ostream>

struct IOMMIORANGE;
typedef struct IOMMMIORANGE *PIOMMMIORANGE;

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

    //! Called whenever a port is read in ring3
    void read_port(RTIOPORT Port, uint32_t read_value, size_t cbValue);

    //! Called whenever a port is written in ring3
    void write_port(RTIOPORT Port, uint32_t written_value, size_t cbValue);

    //! dump the framebuffer informations inside framebuffer.conf
    void dump_framebuffer_conf();

private:
    void dump_core();

    //! Enable IO dump. Before this is called, all port access is ignored.
    void enable_io_dump();

    //! Reports if we currently should be dumping IO.
    bool is_dumping_io();

    //! Disable io dumping.
    void disable_io_dump();

    void register_port(PIOMIOPORTENTRYR3 pRange);
    device& get_device(PPDMDEVINS dev_ptr);

    void power_off();

    //! Register an mmio access into the devices structure
    int register_mmio(PIOMMMIOENTRYR3 pRegEntry);

    PVM vm;

    // Currently, a single CPU is managed.
    PVMCPU cpu;

    // The base name of the cores to create. Doesn't contain the path or the .core suffix
    const char *core_name;

    // Working folder. All generated files should be written here so that they end up in the .tgz file.
    std::string work_folder;

    // Current number of generated cores.
    uint32_t core_number;

    // True if we're currently dumping the scenario io (after the first core dump)
    bool io_dump_enabled;

    // True when Reven special keys are activated (F6/Return for dump, F7 to stop, etc)
    bool port_key_activated;

    // True if the next read from port 0x60 is a keyboard even, false for mouse events.
    bool keyboard_data;

    // A map of devices that is created when the vm starts
    typedef std::map<std::string, device> device_map;
    device_map devices;
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
