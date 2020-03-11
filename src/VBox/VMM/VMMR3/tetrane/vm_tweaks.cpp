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
      work_folder(folder),
      core_number(0),
      io_dump_enabled(false),
      sync_point_file(NULL),
      hardware_file(NULL),
      sync_point_data_pos(0),
      port_key_activated(false),
      keyboard_data(false),
      plugin_manager_(this),
      allocators_setup(false)
{
    if (cpu == NULL)
        cpu = vm->aCpus;

    RTSemMutexCreate(&hardware_mutex);

    // ensure that the folder ends with / to ease the naming of files
    if (work_folder[work_folder.size() - 1] != '/')
        work_folder += "/";

    if (!core_name)
        core_name = "default";

    // User may setup python hook
    char* rvn_plugin= getenv("RVN_PLUGIN");
    if (rvn_plugin) {
        std::cerr << "loading plugin " << rvn_plugin << std::endl;
        plugin_manager_.load_plugin(rvn_plugin);
    }

    memset(&cpu->reven, 0, sizeof(cpu->reven));

    parse_preloader_breakpoint();

    open_time_file();
    open_hardware_file();
}

void vm_tweaks::increment_eip_by_one()
{
    const uint32_t eip = CPUMGetGuestEIP(cpu);

    CPUMSetGuestEIP(cpu, eip+1);
    CPUMSetHyperEIP(cpu, eip+1);
}

void vm_tweaks::dump_framebuffer_conf()
{
    // Retrieve the VGA State
    PVGASTATE pVGAState = NULL;
    for (PPDMDEVINS pDevIns = vm->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3) {
        if (pDevIns->pReg->szName && !strcmp(pDevIns->pReg->szName, "vga")) {
            pVGAState = PDMINS_2_DATA(pDevIns, PVGASTATE);
            break;
        }
    }

    if (!pVGAState) {
        std::cerr << "Can't retrieve the VGA State" << std::endl;
        return;
    }

    // Retrieve the VRam region
    RTGCPHYS GCPhysStart = 0;
    bool found = false;
    {
        uint32_t const cu32MemRanges = PGMR3PhysGetRamRangeCount(vm);
        uint16_t const cMemRanges    = RT_MIN(cu32MemRanges, UINT16_MAX - 1);

        for (uint16_t iRange = 0; iRange < cMemRanges; iRange++)
        {
            RTGCPHYS GCPhysEnd;
            const char* pszDesc;
            int rc = PGMR3PhysGetRange(vm, iRange, &GCPhysStart, &GCPhysEnd, &pszDesc, NULL);
            if (!RT_FAILURE(rc)) {
                if (pszDesc && !strcmp(pszDesc, "VRam")) {
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found) {
        std::cerr << "Can't retrieve the VRam region" << std::endl;
        return;
    }

    std::cout << "Dumping framebuffer to " << work_folder << "/framebuffer.conf" << std::endl;
    std::ofstream fb_file((work_folder + "/framebuffer.conf").c_str(), std::ios::trunc | std::ios::out);
    if (!fb_file.good()) {
        std::cerr << "Can't create the framebuffer file" << std::endl;
        return;
    }

    fb_file << "[framebuffer]" << "\n";

    int bpp = pVGAState->get_bpp(pVGAState);
    uint64_t screen_size = 0;

    uint32_t line_size = 0;
    {
        uint32_t start_addr, line_compare;
        pVGAState->get_offsets(pVGAState, &line_size, &start_addr, &line_compare);
    }

    int width, height;
    pVGAState->get_resolution(pVGAState, &width, &height);

    // Some tweaks on text mode to preserve the functionning of the current implementation
    if (bpp == 0) {
        screen_size = 0xc0000 - 0xb8000;
        width = pVGAState->last_width;
        line_size = width * 2;
        height = screen_size / line_size;

        fb_file << "address=" << std::dec << 0xb8000 << "\n";
    } else {
        screen_size = line_size * height;
        fb_file << "address=" << std::dec << GCPhysStart << "\n";
    }

    fb_file << "width=" << std::dec << width << "\n";
    fb_file << "height=" << std::dec << height << "\n";
    fb_file << "bpp=" << std::dec << bpp << "\n";
    fb_file << "line_size=" << std::dec << line_size << "\n";
    fb_file << "screen_size=" << std::dec << screen_size << "\n";

    fb_file.close();
}

void vm_tweaks::dump_core()
{
    if (core_number != 0)
    {
        std::cerr << "This version of VBox only handles one core dump." << std::endl;
        return;
    }

    if (core_number >= 9) {
        std::cerr << "Didn't write a new core because there are already too many." << std::endl;
        install_all_breakpoints();
        return;
    }

    dump_framebuffer_conf();

    enable_io_dump();
    remove_all_breakpoints();

    /// @TODO for now, we delete all breakpoints at the first core dump.
    ///
    /// This means additional breakpoints won't get logged. To have allocation logs working on
    /// several cores, we need to change the file name saved at each file, or write something
    /// inside the file at each core dump. Then we also need to know in which core we are inside
    /// Reven so it can know where to stop parsing the logs.
    delete_allocation_breakpoints();

    std::ostringstream core_buf;
    core_buf << work_folder << core_name << "_" << core_number << "_" << std::hex << TMCpuTickGet(cpu) << ".core";

    std::string name = core_buf.str();
    std::cout << "Writing core " << name << std::endl;
    DBGFR3CoreWrite(vm->pUVM, name.c_str(), true /*overwrite core file*/);

    // Will add more sync points.
    // On my (qbi) machine, 1000 is a nice balance between slowdown and frequency.
    // 300 will generate a _lot_ (millions). be careful with values less than that, they might freeze the VM.
    char* timeout_string = getenv("RVN_PREEMPT_TIMEOUT");
    if (timeout_string) {
        int timeout = atoi(timeout_string);
        if (timeout > 0) {
            set_preempt_timer(timeout);
            std::cout << "Set VMX preemption timeout at " << cpu->reven.preempt_timer_value << std::endl;
        }
    }

    ++core_number;
}

void vm_tweaks::power_off()
{
    disable_io_dump();
    if (getenv("RVN_FINAL_CORE")) {
        std::string name = work_folder + "scenario_end.core";

        std::cout << "Writing final core " << name << std::endl;
        DBGFR3CoreWrite(vm->pUVM, name.c_str(), true /*overwrite core file*/);
    }

    VMR3PowerOff(vm->pUVM);
}

VBOXSTRICTRC vm_tweaks::handle_int3()
{
    // try several methods, and if all fails we just pass the int3
    if (!handle_preloader_breakpoint() &&
        !handle_breakpoint() &&
        !handle_host_command()) {
        /* We don't increment eip, so the int3 will be reexecuted one time, and
         * by setting VMMR0_DO_TETRANE_RESUME this will tell the ring0 in the file HWSVMR0.cpp
         * to not return VINF_EM_DBG_BREAKPOINT but  VINF_EM_RAW_GUEST_TRAP instead,
         * so the int3 will be executed in the vm.
         */
        fprintf(stderr, "Catched an int3 that isn't registered for core dumping at 0x%x:%x.\n",
                CPUMGetGuestCS(cpu), CPUMGetGuestEIP(cpu));

        vm->tetrane.skip_next_int3 = true;
    }

    return VINF_SUCCESS;
}

VBOXSTRICTRC vm_tweaks::handle_step_instruction()
{
    // try to match a breakpoint, or a host command.
    // TODO: Only try host commands if we're on an int3.
    // note: this is most probably dead code already since we don't use instruction log anymore.
    if (!handle_breakpoint())
        handle_host_command();

    // Note: return VINF_SUCCESS to stop stepping
    return VINF_EM_DBG_STEP;
}

VBOXSTRICTRC vm_tweaks::handle_instruction_not_implemented()
{
    uint32_t eip = CPUMGetGuestEIP(cpu);
    uint16_t cs = CPUMGetGuestCS(cpu);
    logical_address current_eip(cs, eip);

    std::pair<bool, uint16_t> read_value = read_word(current_eip);
    std::cerr << "Instruction not implemented at " << current_eip << " "
              << std::hex << read_value.second << " " << std::endl;
    std::cerr << "Execution will switch back to hardware accelerated (not emulated anymore)"
              << std::endl;
    save_sync_point(vm, cpu, CPUMQueryGuestCtxPtr(cpu), 0);

    return VINF_SUCCESS;
}

void vm_tweaks::set_preempt_timer(uint32_t value)
{
    cpu->reven.preempt_timer_value = value;
}

void vm_tweaks::write_core(const std::string& filename)
{
    std::ostringstream core_buf;
    core_buf << work_folder << filename;
    std::string name = core_buf.str();
    std::cout << "Writing core " << name << std::endl;
    DBGFR3CoreWrite(vm->pUVM, name.c_str(), true /*overwrite core file*/);
}

uint64_t vm_tweaks::read_tsc()
{
    return TMCpuTickGet(cpu);
}

}} // namespace tetrane::tweaks
