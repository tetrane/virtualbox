#include <VBox/vmm/tetrane.h>

#include "vm_tweaks.h"

#include <cstdio>

namespace tetrane {
namespace tweaks {

//Returns the overwritten byte
uint8_t vm_tweaks::setup_byte_at(
    logical_address location, uint8_t const byte)
{
    DBGFADDRESS address;
    address.FlatPtr = location.offset;
    address.off     = location.offset;
    address.Sel     = location.segment;
    address.fFlags  = 0xb;

    VMCPUID idCpu = 0;

    //we read the byte that we overwrite
    uint8_t byte_read = 0x0;
    int read_status = DBGFR3MemRead(vm->pUVM, idCpu, &address, &byte_read, 1);

    AssertMsg(
        read_status == VINF_SUCCESS,
        ("Read to the address 0x%x:0x%x failed ...\n", location.segment, location.offset));

    int write_status = DBGFR3MemWrite(vm->pUVM, idCpu, &address, &byte, 1);

    AssertMsg(
        write_status == VINF_SUCCESS,
        ("Write to the address 0x%x:0x%x failed ...\n", location.segment, location.offset));

    return byte_read;
}


//returns a std::pair the bool tell wether the read succeed, and the uint16_t is the value read.
std::pair<bool, uint16_t> vm_tweaks::read_word(logical_address const& address_to_read)
{
    DBGFADDRESS address;
    address.FlatPtr = address_to_read.offset;
    address.off     = address_to_read.offset;
    address.Sel     = address_to_read.segment;
    address.fFlags  = 0xb;

    VMCPUID idCpu = 0;

    uint16_t word_read = 0;
    int read_status = DBGFR3MemRead(vm->pUVM, idCpu, &address, &word_read, sizeof(word_read));

    if(read_status != VINF_SUCCESS)
    {
        printf("Read to the address 0x%x:0x%x failed ... %d\n",
            address_to_read.segment, address_to_read.offset, read_status);
        return std::make_pair(false, 0);
    }
    else
    {
        return std::make_pair(true, word_read);
    }
}

uint32_t vm_tweaks::read_dword_at(uint64_t ptr)
{
    const uint32_t ds  = CPUMGetGuestDS(cpu);

    DBGFADDRESS address;
    address.FlatPtr = ptr;
    address.off     = ptr;
    address.Sel     = ds;
    address.fFlags  = 0xb;

    VMCPUID idCpu = 0;

    uint32_t value = 0;
    int read_status = DBGFR3MemRead(vm->pUVM, idCpu, &address, &value, sizeof(uint32_t));

    AssertMsg(
        read_status == VINF_SUCCESS,
        ("Read to the address 0x%x:0x%lx failed ...\n", ds, ptr));

    return value;
}

uint64_t vm_tweaks::read_qword_at(uint64_t ptr)
{
    const uint32_t ds  = CPUMGetGuestDS(cpu);

    DBGFADDRESS address;
    address.FlatPtr = ptr;
    address.off     = ptr;
    address.Sel     = ds;
    address.fFlags  = 0xb;

    VMCPUID idCpu = 0;

    uint64_t value = 0;
    int read_status = DBGFR3MemRead(vm->pUVM, idCpu, &address, &value, sizeof(uint64_t));

    AssertMsg(
        read_status == VINF_SUCCESS,
        ("Read to the address 0x%x:0x%lx failed ...\n", ds, ptr));

    return value;
}


uint32_t vm_tweaks::read_parameter(unsigned int number)
{
    const uint32_t esp = CPUMGetGuestESP(cpu);

    return read_dword_at(esp - (number - 1) * sizeof(uint32_t));
}

uint32_t vm_tweaks::read_dword_pointed_by_esp()
{
    const uint32_t esp = CPUMGetGuestESP(cpu);

    return read_dword_at(esp);
}

uint64_t vm_tweaks::read_qword_pointed_by_rsp()
{
    const uint64_t rsp = CPUMGetGuestRSP(cpu);

    return read_qword_at(rsp);
}

}}
