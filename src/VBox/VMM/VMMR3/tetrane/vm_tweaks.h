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
#include "plugin.h"

#include <ostream>

struct IOMMIORANGE;
typedef struct IOMMMIORANGE *PIOMMMIORANGE;

struct IOMIOPORTRANGER3;
typedef struct IOMIOPORTRANGER3 *PIOMIOPORTRANGER3;


namespace tetrane {
namespace tweaks {

struct hook;

struct logical_address
{
    uint16_t segment;
    uint64_t offset;

    logical_address()
        : segment(0)
        , offset(0)
    {
    }

    logical_address(uint16_t s, uint64_t o)
        : segment(s)
        , offset(o)
    {
    }

    bool operator==(const logical_address& other) const
    {
        return segment == other.segment && offset == other.offset;
    }
    bool operator<(const logical_address& other) const
    {
        return segment < other.segment || (segment == other.segment && offset < other.offset);
    }
};

inline std::ostream& operator<<(std::ostream& out, const logical_address& address)
{
    out << std::hex << "0x" << address.segment << ":0x" << address.offset << std::dec;
    return out;
}

struct breakpoint_config
{
    breakpoint_config()
        : saved_byte_(0)
    {}
    uint8_t saved_byte_;

    enum breakpoint_type
    {
        dump,
        allocation_start,
        allocation_return
    };

    logical_address location_;
    logical_address call_origin_;
    bool set_;

    breakpoint_type type_;
};


struct allocator
{
    logical_address address_;
    std::string name_;
};


typedef std::map<logical_address, breakpoint_config> breakpoint_container;
typedef breakpoint_container::value_type breakpoint_pair;
typedef breakpoint_container::iterator breakpoint_iterator;

//! This centralize some tweaking that need to be done on a VM.
class vm_tweaks
{
public:
    //! Create a vm_tweaks object.
    //!
    //! Currently this works on a single CPU so pCVpu == pVM->aCpu
    //! Note that this constructor expects that work_folder exists and is writable.
    vm_tweaks(PVM pVM, PVMCPU pVCpu, const char *work_folder);


    //! Handler when stepping into an instruction
    VBOXSTRICTRC handle_step_instruction();

    //! Handler for int3 instructions
    VBOXSTRICTRC handle_int3();

    //! Handler for non implemented instructions
    VBOXSTRICTRC handle_instruction_not_implemented();

    //! Called whenever a port is read in ring3
    void read_port(RTIOPORT Port, uint32_t read_value, size_t cbValue, PIOMIOPORTRANGER3 pRange);

    //! Called whenever a port is written in ring3
    void write_port(RTIOPORT Port, uint32_t written_value, size_t cbValue, PIOMIOPORTRANGER3 pRange);

    //! Flush the current data to disk. This is called in ring3 whenever this is possible.
    void flush_data();

    //! Register a new hardware access directly (without more copying that necessary)
    void register_hardware_access(struct saved_hardware_access *access, const unsigned char *data);

    //! Register an mmio access into the devices structure
    int register_mmio(PIOMMMIORANGE pRange);

    //! Set the preempt timer value (without the shift)
    void set_preempt_timer(uint32_t value);

    //! dump the framebuffer informations inside framebuffer.conf
    void dump_framebuffer_conf();

    //! write a core dump
    void write_core(const std::string& filename);

    //! get current tsc
    uint64_t read_tsc();

private:
    void dump_core();

    uint8_t setup_byte_at(logical_address location, uint8_t const byte);
    std::pair<bool, uint16_t> read_word(logical_address const& address_to_read);
    uint32_t read_dword_at(uint64_t ptr);
    uint64_t read_qword_at(uint64_t ptr);
    uint32_t read_parameter(unsigned int number);
    uint32_t read_dword_pointed_by_esp();
    uint64_t read_qword_pointed_by_rsp();

    bool read_allocation_file(uint64_t start_address, uint32_t max_bytes);

    /// Remove all registered breakpoints of type allocation by restoring the original code instead of int3.
    void remove_allocation_breakpoints();

    /// Remove all registered breakpoints of type allocation from the container by restoring the original code instead of int3.
    void delete_allocation_breakpoints();

    /// Setup all registered breakpoints disabled by remove_all_breakpoints by modifying the code to int3.
    void install_all_breakpoints();

    /// Install the specified breakpoint in the program.
    bool install_breakpoint(breakpoint_config& bp);

    /// Remove the specified breakpoint from the program.
    bool remove_breakpoint(breakpoint_config& bp);

    //! Add a memory location to dump at every vm exit.
    bool setup_memory_dump(uint32_t buffer, uint32_t lenght);

    //! Rebase the breakpoints offsets
    bool rebase_breakpoint(uint64_t buffer, uint32_t lenght);

    /// Does the necessary steps once a breakpoint at the start of an allocator is reached
    void handle_allocation_start(breakpoint_config &bp, bool stepping);

    /// Does the necessary steps once a breakpoint at the return of an allocator is reached
    void handle_allocation_return(breakpoint_config &bp, bool stepping);

    //! Enable IO dump. Before this is called, all port access is ignored.
    void enable_io_dump();

    //! Reports if we currently should be dumping IO.
    bool is_dumping_io();

    //! Disable io dumping.
    void disable_io_dump();

    //! .
    void stop_hardware_monitoring();


    bool read_allocation_file();

    bool handle_host_command();

    void register_port(PIOMIOPORTRANGER3 pRange);
    device& get_device(PPDMDEVINS dev_ptr);

    void power_off();

    void parse_preloader_breakpoint();

    void remove_all_breakpoints();
    void increment_eip_by_one();

    void open_time_file();
    void open_hardware_file();
    void register_hardware_access_priv(struct saved_hardware_access *access, const unsigned char *data);
    void register_pending_hardware_accesses();
    void open_hardware_files(bool lock);

    // Commands follow

    //! Create a file on the host, return a file descriptor to it or -1 in case of failure
    uint32_t create_file(uint64_t buffer, uint32_t length);

    //! Write to a file on the host, return the number of written bytes or -1 in case of failure
    uint32_t write_file(uint32_t fd, uint64_t buffer, uint32_t length);

    //! Close a host file
    uint32_t close_file(uint32_t fd);

    //! Does a printf on the host (sending data into the log file)
    uint32_t print_trace(uint64_t buffer, uint32_t length);

    //! Copies data from the guest at the specified address, and write it into data
    void fill_buffer(char *data, uint64_t address, uint32_t length);

    //! Finds the allocator at the specified address
    const allocator & find_allocator(logical_address address);


    //! Handle a breakpoint that occurred at the current EIP. Returns true if the breakpoint was handled.
    bool handle_breakpoint();

    //! Handle a preloader breakpoint (the int3 inside the preloader). Returns true if the breakpoint was handled.
    bool handle_preloader_breakpoint();

    //! Writes the sync point to the sync point file.
    void write_sync_point(const sync_point& sp, bool dump_mem);

    //! Writes some chunk of guest logical memory to the data file. Returns the number of added chunks.
    uint32_t write_logical_chunk(uint64_t offset, uint64_t size);

    uint32_t save_sync_point_mem_chunk(uint64_t first, uint64_t last);

    //! Writes the memory of the specified sync point into the data file.
    uint32_t save_sync_point_mem(const sync_point& sp, bool dump_mem);

    PVM vm;

    // Currently, a single CPU is managed.
    PVMCPU cpu;

    // The base name of the cores to create. Doesn't contain the path or the .core suffix
    const char *core_name;

    // Working folder. All generated files should be written here so that they end up in the .tgz file.
    std::string work_folder;

    // Contains the offset of the preloader location when encountered (otherwise it contains nothing)
    logical_address preloader_offset;

    // List of breakpoints, i.e. an instruction that has been replaced by an int3.
    // Breakpoints can be either dump locations, alloc/return addresses, etc.
    breakpoint_container breakpoints;

    // Current number of generated cores.
    uint32_t core_number;

    // True if we're currently dumping the scenario io (after the first core dump)
    bool io_dump_enabled;

    // Output files, created in the constructor.
    stream_file *sync_point_file;
    stream_file *sync_point_data_file;
    stream_file *hardware_file;

    uint64_t sync_point_data_pos;

    // Mutex that should be held while writing in the hardware file
    RTSEMMUTEX hardware_mutex;

    // True when Reven special keys are activated (F6/Return for dump, F7 to stop, etc)
    bool port_key_activated;

    // True if the next read from port 0x60 is a keyboard even, false for mouse events.
    bool keyboard_data;

    // A map of devices that is created when the vm starts
    typedef std::map<std::string, device> device_map;
    device_map devices;

    // A list of allocators that can be fed. For each allocator, we set breakpoints and remember allocation
    // and deallocation, and wrote those into a file.
    std::vector<allocator> allocators_;
    typedef std::vector<allocator>::iterator allocator_iterator;

    // A list of memory chunk that will be dumped at each vm exit
    struct  memory_chunk
    {
        uint64_t offset;
        uint64_t size;
    };
    std::vector<memory_chunk> memory_chunks_;
    typedef std::vector<memory_chunk>::const_iterator chunk_iterator;

    plugin_manager plugin_manager_;

    bool allocators_setup;

    std::vector<FILE*> opened_files;
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
