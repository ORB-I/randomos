#pragma once
#include <core/std.h>
#include <drivers/apic.h>
#include <arch/idt.h>
#include <arch/gdt.h>

#define SMP_STATUS_DEAD    0x00
#define SMP_STATUS_WAITING 0x01
#define SMP_STATUS_WORKING 0x02
typedef struct {
    u64 apicid;
    u8 tid;
    madt_plapic_t* acpi_ent;
    u8 status;
    u8 current_pid;
    u8 preempt_pending;
} smp_info_t;

typedef struct {
    u64 apicid;
    struct gdt_entry gdt[8];
    struct gdtr gdtr;
    struct tss_entry tss;
} thread_gdt_t;

typedef struct {
    u64 apicid;
    idt_entry_t idt[IDT_SIZE];
    idtr_t idtr;
} thread_idt_t;


typedef struct {
    u64 apicid;
    u8 stack[16384];
} __packed smp_stack_t;

extern u64 bsp_apicid;
extern smp_info_t* smp_info;
extern usize ncores;
int init_cores();