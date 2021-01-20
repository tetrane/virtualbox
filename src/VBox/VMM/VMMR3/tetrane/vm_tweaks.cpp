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
      port_key_activated(false),
      keyboard_data(false)
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

typedef struct VGAState   * PVGASTATE;

void vm_tweaks::dump_framebuffer_conf()
{
    // Retrieve the VGA State
    PVGASTATER3 pVGAStateCC = NULL;
    PVGASTATE pVGAState = NULL;
    for (PPDMDEVINS pDevIns = (PPDMDEVINS)vm->pdm.s.pDevInstances; pDevIns; pDevIns = pDevIns->Internal.s.pNextR3) {
        PCPDMDEVREGR3 reg = (PCPDMDEVREGR3)pDevIns->pReg;

        if (reg->szName && !strcmp(reg->szName, "vga")) {
            pVGAStateCC = PDMDEVINS_2_DATA_CC(pDevIns, PVGASTATECC);
            pVGAState = PDMINS_2_DATA(pDevIns, PVGASTATE);
            break;
        }
    }

    if (!pVGAState or !pVGAStateCC) {
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

    int bpp = pVGAStateCC->get_bpp(pVGAState);
    uint64_t screen_size = 0;

    uint32_t line_size = 0;
    {
        uint32_t start_addr, line_compare;
        pVGAStateCC->get_offsets(pVGAState, &line_size, &start_addr, &line_compare);
    }

    int width, height;
    pVGAStateCC->get_resolution(pVGAState, &width, &height);

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
        return;
    }

    dump_framebuffer_conf();

    enable_io_dump();

    std::ostringstream core_buf;
    core_buf << work_folder << core_name << "_" << core_number << "_" << std::hex << TMCpuTickGet(cpu) << ".core";

    std::string name = core_buf.str();
    std::cout << "Writing core " << name << std::endl;
    DBGFR3CoreWrite(vm->pUVM, name.c_str(), true /*overwrite core file*/);

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

}} // namespace tetrane::tweaks
