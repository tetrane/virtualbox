#ifndef TETRANE_STRUCTS_H_
#define TETRANE_STRUCTS_H_

/* This is stored inside the vm, must fit in the padding (currently 414 bytes) */
struct tetrane_tweaks
{
    void *vm_tweaks;
    bool skip_next_int3;
};

/* Must be 256 bytes max to have around 500 storable sync points */
struct sync_point
{
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;

    uint64_t rsi;
    uint64_t rdi;

    uint64_t rbp;
    uint64_t rsp;

    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    uint64_t rip;
    uint64_t rflags;

    uint64_t cr0;
    uint64_t cr2;
    uint64_t cr3;
    uint64_t cr4;

    uint16_t fpu_sw;
    uint16_t fpu_cw;
    uint8_t  fpu_tw;

    volatile uint64_t tsc;

    uint16_t cs;

    union {
        struct {
            bool is_irq:1;
            union {
                uint8_t interrupt_vector;
                uint8_t vmexit_reason;
            };
        } type;
        uint16_t type_raw;
    };

    uint32_t fault_error_code;
};

// SYNC_POINT_SIZE is the number of bytes in the temporary buffer used to store the sync points when we are in R0
#define SYNC_POINT_SIZE 131072
// SYNC_POINT_BYTES is the size of a record / entry in the file / buffer
#define SYNC_POINT_BYTES 256
// SYNC_POINT_COUNT is the number of sync points storable in the temporary buffer
#define SYNC_POINT_COUNT (SYNC_POINT_SIZE / sizeof(struct sync_point))

struct saved_hardware_access
{
    volatile uint64_t tsc;

    uint64_t address;
    uint64_t type;
    uint64_t length;

    uint64_t device_reg;
    uint32_t device_instance;

    uint32_t data_start;
};

#define HARDWARE_SIZE 40960
#define HARDWARE_COUNT (HARDWARE_SIZE/sizeof(struct saved_hardware_access))


/**
 * This is stored inside each VMCPU. Size must be a multiple of 4096.
 */
struct tetrane_cpu_tweaks
{
    volatile uint32_t dumping;

    volatile uint32_t sync_point_read_index;
    volatile uint32_t sync_point_write_index;

    volatile uint32_t hardware_read_index;
    volatile uint32_t hardware_write_index;

    volatile uint32_t hardware_cur_data;

    volatile uint32_t preempt_timer_value;

    uint8_t padding[4096 - 7*4]; /* align sync points and the like on 4096 bytes, 6 is the number of uint32 above. */

    union {
        struct sync_point sync_point[SYNC_POINT_COUNT];
        uint8_t sp_padding[SYNC_POINT_SIZE];
    };

    union {
        struct saved_hardware_access hardware_access[HARDWARE_COUNT];
        uint8_t ha_padding[HARDWARE_SIZE];      /* multiple of 4096 */
    };

    uint8_t hardware_data[4096 * 100];      /* multiple of 4096 */
};

/**
 * This is stored in the core file for each CPU to give more information.
 */
struct tetrane_cpu_info
{
    uint64_t cr8;
} __attribute__((packed));

#define TETRANE_CPU_SECTION_MAGIC       0x62647570634e5652

// Magic + Size + tetrane_cpu_info
#define TETRANE_CPU_SECTION_SIZE        (2*sizeof(uint64_t) + sizeof(struct tetrane_cpu_info))

#define TETRANE_CPU_SECTION_NOTE_TYPE   0xbeef


#endif // TETRANE_STRUCTS_H_
