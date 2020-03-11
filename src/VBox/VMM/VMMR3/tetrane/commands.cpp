#include <VBox/vmm/tetrane.h>

#include <sstream>
#include <set>
#include <vector>
#include <iomanip>
#include <limits>
#include <cassert>
#include <iostream>
#include <cstdlib>

#include <fstream>

#include <sys/stat.h>
#include <sys/types.h>

#include "vm_tweaks.h"

#define MAGIC_VBOX_CALL 0xdeadbabe

using namespace tetrane::tweaks;

enum vbox_command
{
    FIRST_COMMAND = 0xeff1cace,
    COMMAND_DUMP_ALLOCATORS = FIRST_COMMAND,
    COMMAND_TOGGLE_ALLOCATORS,
    COMMAND_REBASE_BREAKPOINT,
    COMMAND_STOP_VM,
    COMMAND_CREATE_FILE,
    COMMAND_WRITE_FILE,
    COMMAND_CLOSE_FILE,
    COMMAND_PRINT_TRACE,
    COMMAND_DUMP_CORE,
    COMMAND_MEMORY_DUMP,

    LAST_COMMAND
};


bool vm_tweaks::read_allocation_file(uint64_t start_address, uint32_t max_bytes)
{
    std::string allocator_name;

    std::string allocation_file_path = work_folder + "allocator_names";
    std::ifstream allocator_file(allocation_file_path.c_str());
    if (not allocator_file.is_open())
    {
        std::cout << "Allocator file " << allocation_file_path << " cannot be opened." << std::endl;
        return false;
    }

    uint64_t ptr = start_address;
    const uint64_t max = ptr + max_bytes;
    const uint32_t cs  = CPUMGetGuestCS(cpu);
    const uint32_t ds  = CPUMGetGuestDS(cpu);
    allocator alloc;

    DBGFADDRESS address;
    address.Sel     = ds;
    address.fFlags  = 0xb;

    VMCPUID idCpu = 0;

    uint32_t data = 0;
    int read_status;

    alloc.address_.segment = cs;

    /// Read the first dword. It should be ALLOCATOR_LIST_START, which indicates that the following dwords
    /// will be allocators, in the order of the allocator_file file.

    address.FlatPtr = ptr;
    address.off     = ptr;

    do
    {
        std::getline(allocator_file, alloc.name_);

        address.FlatPtr = ptr;
        address.off     = ptr;

        read_status = DBGFR3MemRead(vm->pUVM, idCpu, &address, &data, sizeof(uint32_t));
        ptr += 4;
        if (data == 0)
        {
            std::cout << "Allocator " << alloc.name_ << " not found." << std::endl;
        }
        else
        {
            alloc.address_.offset = data;
            allocators_.push_back(alloc);
        }

    } while (read_status == VINF_SUCCESS and ptr < max);

    if (read_status != VINF_SUCCESS)
    {
        std::cout << "Failed to read the stack at " << address.FlatPtr << ". Status= " << read_status << std::endl;
        return false;
    }

    for (allocator_iterator it = allocators_.begin(); it != allocators_.end() ; ++it)
    {
        const allocator& a = *it;
        std::cout << "Found allocator " << a.name_ << " at " << a.address_ << std::endl;

        breakpoint_config bp;

        bp.location_ = a.address_;
        bp.set_ = false;
        bp.type_ = breakpoint_config::allocation_start;

        std::pair<breakpoint_iterator, bool> result = breakpoints.insert(std::make_pair(bp.location_, bp));

        if (result.second == false)
        {
            /// TODO this case should really be handled.
            std::cerr << "Warning: currently, an allocation function cannot be a dump location." << std::endl;
            continue;
        }
        install_breakpoint(result.first->second);
    }
    std::cerr << "Allocators dumping done." << std::endl;

    allocators_setup = true;

    return true;
}

const allocator & vm_tweaks::find_allocator(logical_address address)
{
    unsigned int i = 0;

    for (i = 0 ; i < allocators_.size() ; i++)
    {
        // segment not tested on purpose for now, to allow allocation functions in kernel land.
        // add a segment check if it is really needed.
        if (allocators_[i].address_.offset == address.offset)
        {
            return allocators_[i];
        }
    }

    std::cout << "Unknown allocator at " << std::hex << address.offset << std::dec << std::endl;

    exit(1);

    return *(new allocator);
}




/// Does the necessary steps once a breakpoint at the start of an allocator is reached
void vm_tweaks::handle_allocation_start(breakpoint_config &bp, bool stepping)
{
    if (not stepping)
    {
        remove_breakpoint(bp);
    }

    std::string path = work_folder + "allocations.txt";
    std::ofstream file(path.c_str(), std::ios::app);
    if (!file.is_open())
    {
        std::cout << "Cannot open the allocation file " << path << std::endl;
        return;
    }
    file << std::hex << std::showbase;

    unsigned int i = 0;
    const allocator& alloc = find_allocator(bp.location_);

    file << alloc.name_ << " " << alloc.address_.segment << ":" << alloc.address_.offset << " IN";

    for (i = 0; i < 5; i++)
    {
        uint32_t param = read_parameter(i);
        file << " " << param;
    }

    file << std::endl;

    breakpoint_config return_bp;

    return_bp.location_.segment = CPUMGetGuestCS(cpu);
    return_bp.location_.offset = read_dword_pointed_by_esp();
    return_bp.set_ = false;
    return_bp.type_ = breakpoint_config::allocation_return;
    return_bp.call_origin_ = bp.location_;

    std::pair<breakpoint_iterator, bool> inserted = breakpoints.insert(std::make_pair(return_bp.location_, return_bp));
    if(inserted.second == false)
    {
        std::cerr << "Tried to put two breakpoint at the same address ... " << std::endl;
        exit(1);
    }

    if (not stepping)
    {
        install_breakpoint(inserted.first->second);
    }
}

/// Does the necessary steps once a breakpoint at the return of an allocator is reached
void vm_tweaks::handle_allocation_return(breakpoint_config &bp, bool stepping)
{
    if (not stepping)
    {
        remove_breakpoint(bp);
    }

    std::string path = work_folder + "allocations.txt";
    std::ofstream file(path.c_str(), std::ios::app);
    if (!file.is_open())
    {
        std::cout << "Cannot open the allocation file " << path << std::endl;
        return;
    }
    file << std::hex << std::showbase;
    const allocator& alloc = find_allocator(bp.call_origin_);
    uint32_t eax = CPUMGetGuestEAX(cpu);

    file << alloc.name_ << " " << alloc.address_.segment << ":" << alloc.address_.offset
         << " OUT " << eax << std::endl;

    breakpoint_iterator where = breakpoints.find(bp.call_origin_);
    if (where == breakpoints.end())
    {
        std::cerr << "Cannot find the caller breakpoint at " << bp.call_origin_.offset << " ... " << std::endl;
        exit(1);
    }

    if (not stepping)
    {
        install_breakpoint(where->second);
    }

    breakpoints.erase(bp.location_);
}

void vm_tweaks::fill_buffer(char *data, uint64_t address, uint32_t length)
{
    DBGFADDRESS where;
    where.Sel     = CPUMGetGuestDS(cpu);
    where.fFlags  = 0xb;

    where.FlatPtr = address;
    where.off     = address;

    DBGFR3MemRead(vm->pUVM, 0 /* first cpu */, &where, data, length);
}

uint32_t vm_tweaks::create_file(uint64_t /* buffer */, uint32_t /* length */)
{
    std::cerr << "File commands are now disabled." << std::endl;
    return -1;
}

uint32_t vm_tweaks::write_file(uint32_t /* fd */, uint64_t /* buffer */, uint32_t /* length */)
{
    std::cerr << "File commands are now disabled." << std::endl;
    return -1;
}

uint32_t vm_tweaks::close_file(uint32_t /* fd */)
{
    std::cerr << "File commands are now disabled." << std::endl;
    return -1;
}

uint32_t vm_tweaks::print_trace(uint64_t buffer, uint32_t length)
{
    if (length > 4096)
    {
        std::cerr << "Printing a buffer of " << length << " bytes is not possible. Please split the message in chunks of 4096 bytes max." << std::endl;
        return -1;
    }

    char *data = new char[length+1];

    fill_buffer(data, buffer, length);

    data[length] = 0;

    std::cerr << "GUEST> " << data << std::endl;

    delete[] data;

    return length;
}

bool vm_tweaks::handle_host_command()
{
    uint32_t magic = CPUMGetGuestEDX(cpu);
    if (magic != MAGIC_VBOX_CALL)
        return false;

    uint32_t command = CPUMGetGuestEAX(cpu);
    if (command < FIRST_COMMAND || command >= LAST_COMMAND)
        return false;

    uint32_t data = CPUMGetGuestEBX(cpu);
    uint64_t buffer = CPUMGetGuestRSI(cpu);
    uint32_t length = CPUMGetGuestECX(cpu);

    uint32_t result = 0;

    increment_eip_by_one();

    // alert plugins
    plugin_manager_.command_callback();

    switch(command)
    {
    case COMMAND_DUMP_ALLOCATORS:
        std::cerr << "Dumping allocators..." << std::endl;
        read_allocation_file(buffer, length);
        break;

    case COMMAND_TOGGLE_ALLOCATORS:
        std::cout << "Toggling allocators..." << std::endl;
        if (allocators_setup)
            remove_allocation_breakpoints();
        else
            install_all_breakpoints();
        allocators_setup = not allocators_setup;
        break;

    case COMMAND_REBASE_BREAKPOINT:
        std::cerr << "Updating breakpoint location..." << std::endl;
        rebase_breakpoint(buffer, length);
        break;

    case COMMAND_STOP_VM:
        std::cerr << "Stopping VM..." << std::endl;
        power_off();
        break;

    case COMMAND_CREATE_FILE:
        result = create_file(buffer, length);
        break;

    case COMMAND_WRITE_FILE:
        result = write_file(data, buffer, length);
        break;

    case COMMAND_CLOSE_FILE:
        result = close_file(data);
        break;

    case COMMAND_PRINT_TRACE:
        result = print_trace(buffer, length);
        break;

    case COMMAND_DUMP_CORE:
        dump_core();
        break;

    case COMMAND_MEMORY_DUMP:
        std::cerr << "Setting memory chunk dump..." << std::endl;
        setup_memory_dump(buffer, length);
        break;

    default:
        // should not happen, handled in the if up there
        AssertMsgFailed(("Invalid command after the command check: %X\n", command));
    }

    CPUMSetGuestEAX(cpu, result);

    return true;
}
