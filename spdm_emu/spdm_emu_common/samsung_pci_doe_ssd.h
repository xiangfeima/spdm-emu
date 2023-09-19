/**
 *  Copyright Notice:
 *  Copyright 2021-2022 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/spdm-emu/blob/main/LICENSE.md
 **/

#ifndef __SPDM_EMU_SAMSUNG_PCI_DOE_SSD_H__
#define __SPDM_EMU_SAMSUNG_PCI_DOE_SSD_H__

#include "hal/base.h"
#include "os_include.h"

#define CONFIG_SAMSUNG_VENDOR_ID            (0x144D)
#define MAX_DEVICE_NUM                      (0x20)
#define MAX_FUNCTION_NUM                    (0x8)
#define SIZE_OF_PCIE_PORTIO_SPACE           (0x100)
#define SIZE_OF_PCIE_MMIO_EXTENDED_SPACE    (0x1000)

#pragma pack(1)
struct EcmStructure
{
    uint32_t signature;
    uint32_t length;
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t oem_table_id_lsb;
    uint32_t oem_table_id_msb;
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint32_t reserved2;
    uint32_t reserved3;
    uint32_t ecm_base_address_lsb;
    uint32_t ecm_base_address_msb;
    uint16_t pci_seg_grp_num;
    uint8_t start_bus_num;
    uint8_t end_bus_num;
    uint32_t reserved4;
};

struct ConfigHeader
{
    uint16_t vendor_id;                 // 0
    uint16_t device_id;                 // 2
    uint16_t cmd_reg;                   // 4
    uint16_t status_reg;                // 6
    uint32_t rev_id_and_cc;             // 8
    uint8_t cache_line_size;           // 12
    uint8_t latency_timer;             // 13
    uint8_t header_type;               // 14
    uint8_t bist;                      // 15
    uint32_t base_address0;             // 16
    uint32_t base_address1;             // 20
    uint8_t primary_bus_num;           // 24
    uint8_t secondary_bus_num;         // 25
    uint8_t subordinate_bus_num;       // 26
    uint8_t secondary_latency_timer;   // 27
    uint32_t reserved07;                // 28
    uint32_t reserved08;                // 32
    uint32_t reserved09;                // 36
    uint32_t reserved10;                // 40
    uint32_t reserved11;                // 44
    uint32_t reserved12;                // 48
    uint8_t capability_pointer;        // 52
    uint8_t reserved13;                // 53
    uint16_t reserved14;                // 54
    uint32_t reserved15;                // 56
    uint32_t reserved16;                // 60
};

struct PhysicalAddress
{
    union
    {
        struct
        {
            uint32_t low;
            uint32_t high;
        } addr;
        uint64_t ull;
    } u;
};

struct ConfigAddressMem
{
    uint32_t address_msb;
    union
    {
        struct
        {
            uint32_t reg : 12;
            uint32_t function : 3;
            uint32_t device : 5;
            uint32_t bus : 8;
            uint32_t address_lsb : 4;
        } bits;
        uint32_t ul;
    } address_lsb;
};

typedef struct _PcieExtCapaHeader
{
    uint16_t ExtCapaId;
    uint16_t CapaVersion : 4;
    uint16_t NextCapaOffset : 12;
}  PcieExtCapaHeader;

typedef volatile union _RegExtCapHeader
{
    volatile uint32_t value;
    struct
    {
        volatile uint32_t ext_cap_id : 16;
        volatile uint32_t ext_cap_ver : 4;
        volatile uint32_t next_cap_offset : 12;
    } native;
} RegExtCapHeader;

typedef volatile union _RegDoeCap
{
    volatile uint32_t value;
    struct
    {
        volatile uint32_t int_support : 1;
        volatile uint32_t int_msg_num : 11;
        volatile uint32_t reserved : 20;
    } native;
} RegDoeCap;

typedef volatile union _RegDoeCtrl
{
    volatile uint32_t value;
    struct
    {
        volatile uint32_t abort : 1;
        volatile uint32_t int_enable : 1;
        volatile uint32_t reserved : 29;
        volatile uint32_t go : 1;
    } native;
} RegDoeCtrl;

typedef volatile union _RegDoeStatus
{
    volatile uint32_t value;
    struct
    {
        volatile uint32_t busy : 1;
        volatile uint32_t int_status : 1;
        volatile uint32_t error : 1;
        volatile uint32_t reserved : 28;
        volatile uint32_t data_obj_ready : 1;
    } native;
} RegDoeStatus;

typedef volatile union _RegDoeWriteMailBox
{
    volatile uint32_t value;
    struct
    {
        volatile uint32_t mailbox : 32;
    } native;
} RegDoeWriteMailBox;

typedef volatile union _RegDoeReadMailBox
{
    volatile uint32_t value;
    struct
    {
        volatile uint32_t mailbox : 32;
    } native;
} RegDoeReadMailBox;

typedef volatile struct _PcieDoeReg
{
    RegExtCapHeader header;
    RegDoeCap cap;
    RegDoeCtrl ctrl;
    RegDoeStatus status;
    RegDoeWriteMailBox wmb;
    RegDoeReadMailBox rmb;
} PcieDoeReg;
#pragma pack()

extern uint32_t m_pci_doe_ssd_bus;
extern uint32_t m_pci_doe_ssd_reg_offset;
extern int m_dev_mem_fd;
extern int32_t* m_mapped_pci_addr;
extern PcieDoeReg* m_pci_doe_reg_addr;

bool discover_samsung_pci_doe_ssd(uint32_t bus, uint32_t doe_reg_offset);

#endif
