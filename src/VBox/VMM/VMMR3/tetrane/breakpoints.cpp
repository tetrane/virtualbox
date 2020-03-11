#include "vm_tweaks.h"
#include <VBox/vmm/tetrane.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/pdmdev.h>


#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <inttypes.h>

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


namespace tetrane {
namespace tweaks {

void vm_tweaks::parse_preloader_breakpoint()
{
    const char *l = getenv("OFFSET_DUMP");
    int size = 0;

    if (l)
        size = strnlen(l, 30);

    if (size == 0 || size >= 30) {
        std::cerr << "No main dump location (interactive mode)" << std::endl;

        return;
    }

    std::istringstream str(l);

    str >> std::hex;

    breakpoint_config bp;

    str >> bp.location_.segment >> bp.location_.offset;

    std::cerr << "Main dump location is at " << bp.location_ << std::endl;

    bp.set_ = false;
    bp.type_ = breakpoint_config::dump;

    breakpoints.insert(std::make_pair(bp.location_, bp));
}

bool vm_tweaks::install_breakpoint(breakpoint_config& bp)
{
    if (bp.set_)
    {
        return false;
    }
    bp.saved_byte_ = setup_byte_at(bp.location_, 0xCC);
    bp.set_ = true;
    return true;
}

bool vm_tweaks::remove_breakpoint(breakpoint_config& bp)
{
    if (not bp.set_)
    {
        return false;
    }
    uint8_t byte = setup_byte_at(bp.location_, bp.saved_byte_);
    bp.set_ = false;
    return byte == 0xCC;
}

void vm_tweaks::remove_allocation_breakpoints()
{
    for(breakpoint_iterator iter = breakpoints.begin(); iter != breakpoints.end(); ++iter)
    {
        breakpoint_config& bp = iter->second;
        if (bp.type_ == breakpoint_config::allocation_start or bp.type_ == breakpoint_config::allocation_return)
        {
            if (bp.set_)
                remove_breakpoint(bp);
            else
                bp.saved_byte_ = 0xCC; // the int3 was not set. We should not restore it in install_all_breakpoints.
        }
    }
}

void vm_tweaks::delete_allocation_breakpoints()
{
    breakpoint_iterator iter = breakpoints.begin();
    while(iter != breakpoints.end())
    {
        breakpoint_config& bp = iter->second;
        if (bp.type_ == breakpoint_config::allocation_start or bp.type_ == breakpoint_config::allocation_return)
        {
            if (bp.set_)
                remove_breakpoint(bp);

            breakpoints.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }
}

bool vm_tweaks::setup_memory_dump(uint32_t buffer, uint32_t length)
{
    if(length < 2*sizeof(uint32_t))
    {
        std::cerr << "Got malformed memory chunk" << std::endl;
        return false;
    }

    memory_chunk chunk;
    chunk.offset = read_dword_at(buffer);
    chunk.size = read_dword_at(buffer+4);

    memory_chunks_.push_back(chunk);
    return true;
}

bool vm_tweaks::rebase_breakpoint(uint64_t buffer, uint32_t length)
{
    // TODO: Use struct instead of array
    if(length < 3 * sizeof(uint64_t))
    {
        std::cerr << "Got malformed rebase info" << std::endl;
        return false;
    }

    uint64_t cs = read_qword_at(buffer);
    uint64_t old_offset = read_qword_at(buffer+8);
    uint64_t new_offset = read_qword_at(buffer+16);

    breakpoint_iterator it = breakpoints.find(logical_address(cs, old_offset));
    if(it == breakpoints.end())
    {
        std::cerr << "Cannot rebase unexisting breakpoint" << std::endl;
        return false;
    }

    if(it->second.set_)
    {
        std::cerr << "Cannot rebase an installed breakpoint" << std::endl;
        return false;
    }

    // TODO: Also relocate the allocators if the found breakpoint is not a dump breakpoint

    // rebase the config offset
    breakpoint_config new_config = it->second;
    new_config.location_.offset = new_offset;

    // replace the old bp with the new one
    breakpoints.erase(it);
    std::pair<breakpoint_iterator, bool> result = breakpoints.insert(std::make_pair(new_config.location_, new_config));

    if(not result.second)
    {
        std::cerr << "Cannot rebase at an already existing breakpoint offset" << std::endl;
        return false;
    }

    return true;
}

void vm_tweaks::remove_all_breakpoints()
{
    for(breakpoint_iterator iter = breakpoints.begin(); iter != breakpoints.end(); ++iter)
    {
        if (iter->second.set_)
        {
            remove_breakpoint(iter->second);
        }
        else
        {
            // the int3 was not set. We should not restore it in install_all_breakpoints.
            iter->second.saved_byte_ = 0xCC;
        }
    }
}


void vm_tweaks::install_all_breakpoints()
{
    for(breakpoint_iterator iter = breakpoints.begin();
        iter != breakpoints.end();
        ++iter)
    {
        if (iter->second.saved_byte_ != 0xCC)
        {
            install_breakpoint(iter->second);
        }
    }
}

bool vm_tweaks::handle_preloader_breakpoint()
{
    logical_address top_stack_address(CPUMGetGuestCS(cpu), CPUMIsGuestIn64BitCodeEx(&cpu->cpum.GstCtx) ? read_qword_pointed_by_rsp() : read_dword_pointed_by_esp());

    breakpoint_iterator where = breakpoints.find(top_stack_address);
    if (where == breakpoints.end())
        return false;

    preloader_offset = top_stack_address;

    std::cerr << "Reached breakpoint inside the loader, preloaded address=" << preloader_offset << std::endl;

    remove_allocation_breakpoints();

    // don't really execute an int3
    increment_eip_by_one();

    //we arm all the breakpoints where the user want a core dump
    for(breakpoint_iterator iter = breakpoints.begin();
        iter != breakpoints.end();
        ++iter)
    {
        if (iter->second.type_ == breakpoint_config::dump && !iter->second.set_)
        {
            uint8_t old_byte = setup_byte_at(iter->first, 0xcc);
            std::cout << "Setup an int3 at " << iter->first << std::endl;

            iter->second.set_ = true;
            iter->second.saved_byte_ = old_byte;
        }
    }

    return true;
}

bool vm_tweaks::handle_breakpoint()
{
    logical_address current_eip(CPUMGetGuestCS(cpu), CPUMGetGuestRIP(cpu));

    breakpoint_iterator it = breakpoints.find(current_eip);
    if (it == breakpoints.end())
        return false;

    breakpoint_config& bp = it->second;

    switch (bp.type_) {
    case breakpoint_config::dump:
        if(current_eip == preloader_offset) {
            std::cout << "Reached the preloader int3 at ";
        } else {
            std::cout << "Reached an intermediate int3 at ";
        }

        std::cout << current_eip << "\n";

        remove_breakpoint(bp);

        // note: the int3 was removed because the breakpoint was removed, we're not incrementing eip here
        dump_core();

        return true;
    case breakpoint_config::allocation_start:
        handle_allocation_start(bp, false);
        return true;
    case breakpoint_config::allocation_return:
        handle_allocation_return(bp, false);
        return true;
    }

    // in the odd case the breakpoint type is unknown, pass it to the other handlers
    return false;
}


}} // namespace tetrane::tweaks
