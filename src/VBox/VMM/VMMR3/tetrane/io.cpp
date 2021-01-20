#include <VBox/vmm/iom.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/pgm.h>
#include <VBox/sup.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pdmdev.h>
#include "IOMInternal.h"
#include <VBox/vmm/vm.h>

#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <VBox/log.h>
#include <VBox/err.h>

#include "IOMInline.h"

#include <VBox/vmm/tetrane.h>
#include <sstream>

#include "device.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <map>
#include <iostream>

#include "vm_tweaks.h"

using tetrane::tweaks::vm_tweaks;

#define IO_FILE_MAGIC 0x69636e79734e5652

device& vm_tweaks::get_device(PPDMDEVINS dev_ptr)
{
    device_map::iterator where = devices.find(dev_ptr->pReg->szName);
    if (where != devices.end())
    {
        return where->second;
    }

    device& dev = devices[dev_ptr->pReg->szName];
    dev.name = dev_ptr->pReg->szName;
    dev.id = (uint64_t)dev_ptr->pReg;
    if (dev_ptr->pReg->pszDescription)
        dev.description = dev_ptr->pReg->pszDescription;

    return dev;
}

int vm_tweaks::register_mmio(PIOMMMIOENTRYR3 pRegEntry)
{
    device &dev = get_device(pRegEntry->pDevIns);

    memory_range range;

    if (pRegEntry->pszDesc)
        range.description = pRegEntry->pszDesc;
    range.physical_address = pRegEntry->GCPhysMapping;
    range.length = pRegEntry->cbRegion;
    range.instance = pRegEntry->pDevIns->iInstance;

    dev.memory_ranges.push_back(range);

    return 0;
}

void vm_tweaks::register_port(PIOMIOPORTENTRYR3 pRange)
{
    device& dev = get_device(pRange->pDevIns);

    port_range range;

    if (pRange->pszDesc)
        range.description = pRange->pszDesc;
    range.port = pRange->uPort;
    range.length = pRange->cPorts;
    range.instance = pRange->pDevIns->iInstance;

    dev.port_ranges.push_back(range);
}


void vm_tweaks::enable_io_dump()
{
    if (is_dumping_io())
        return;

    std::string io_path = work_folder + "io.bin";

    int rc2 = IOM_LOCK_SHARED(vm);
    AssertRC(rc2);

    for (uint32_t i = 0; i < vm->iom.s.cIoPortRegs; ++i) {
        PIOMIOPORTENTRYR3 pEntry = &vm->iom.s.paIoPortRegs[i];
        register_port(pEntry);
    }

    IOM_UNLOCK_SHARED(vm);

    for (uint32_t i = 0; i < vm->iom.s.cMmioRegs; ++i) {
        PIOMMMIOENTRYR3 pEntry = &vm->iom.s.paMmioRegs[i];
        register_mmio(pEntry);
    }

    io_dump_enabled = true;
    cpu->reven.dumping = 1;

    std::cerr << "Dumping devices to " << io_path << "..." << std::endl;
    {
        stream_file file;
        file.open(io_path);

        uint64_t magic = IO_FILE_MAGIC;
        uint32_t version = 0;
        uint64_t length = devices.size();

        file << magic << version << length;
        for (std::map<std::string, device>::iterator it = devices.begin(); it != devices.end(); ++it)
        {
            file <<  it->second;
        }
    }
    std::cerr << "Devices dumped." << std::endl;
}

void vm_tweaks::read_port(RTIOPORT Port, uint32_t read_value, size_t cbValue)
{
    if (keyboard_data && Port == 0x60 && cbValue == 1)
    {
        if (read_value == 0x43 && !port_key_activated) // F9
        {
            std::cerr << "Activated Reven special keys." << std::endl;
            port_key_activated = true;
        }
        if (read_value == 0x44 && port_key_activated) // F10
        {
            std::cerr << "Deactivated Reven special keys." << std::endl;
            port_key_activated = false;
        }

        if (port_key_activated)
        {
            if (read_value == 0x1c // return
                || read_value == 0x40 // F6
            )
            {
                std::cerr << "Pressed dumping key, scancode=0x" << std::hex << read_value << std::hex << std::endl;

                dump_core();
            }
            else if (read_value == 0x41 // F7
            )
            {
                std::cerr << "Stopping VM after F7 keypress..." << std::endl;
                power_off();
            }
        }
    }
    if (Port == 0x64 && cbValue == 1) {
        // On PS/2 controllers, the 5th bit indicates if the event is a mouse(1) or keyboard(0) event.
        keyboard_data = (read_value & (1<<5)) == 0;
    }
}

void vm_tweaks::write_port(RTIOPORT Port, uint32_t written_value, size_t cbValue)
{
}

bool vm_tweaks::is_dumping_io()
{
    return io_dump_enabled;
}

void vm_tweaks::disable_io_dump()
{
    io_dump_enabled = false;
    cpu->reven.dumping = 0;
}
