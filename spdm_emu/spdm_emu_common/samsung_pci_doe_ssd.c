/**
 *  Copyright Notice:
 *  Copyright 2021-2022 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/spdm-emu/blob/main/LICENSE.md
 **/

#include "samsung_pci_doe_ssd.h"

bool discover_samsung_pci_doe_ssd(uint32_t bus, uint32_t doe_reg_offset)
{
    int fd = 0;
    struct EcmStructure ecm;
    struct ConfigAddressMem config_addr;
    struct ConfigHeader* config_header;
    struct PhysicalAddress phy_addr;

    bool ssd_found = false;

    const uint16_t pcie_doe_id = 0x002E;
    uint16_t pcie_capa_offset = 0x100;
    int32_t* pci_address_current = NULL;
    PcieExtCapaHeader* pstCapaHeader = NULL;

    printf("Read ECM info ...\n");
    if ((fd = open("/sys/firmware/acpi/tables/MCFG", O_RDONLY)) == -1) {
        if ((fd = open("/sys/firmware/acpi/tables/MCFG1", O_RDONLY)) == -1) {
            return false;
        }
    }

    if ((read(fd, &ecm, sizeof(struct EcmStructure))) == -1)
    {
        close(fd);
        return false;
    }
    printf("/sys/firmware/acpi/tables/MCFG read OK!\n");
    printf("ecm_base_address: 0x%08x_%08x\n", ecm.ecm_base_address_msb, ecm.ecm_base_address_lsb);
    close(fd);

    if (bus >= ecm.end_bus_num) {
        return false;
    }

    printf("Search SAMSUNG SSD on PCI Bus %d ...\n", bus);
    m_dev_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (m_dev_mem_fd == -1) {
        return false;
    }

    config_addr.address_msb = ecm.ecm_base_address_msb;
    config_addr.address_lsb.ul = ecm.ecm_base_address_lsb;
    config_addr.address_lsb.bits.bus |= bus;
    for (uint32_t device_idx = 0; device_idx < MAX_DEVICE_NUM; device_idx++) {
        if (ssd_found) {
            break;
        }

        config_addr.address_lsb.bits.device = device_idx;
        for (uint32_t func_idx = 0; func_idx < MAX_FUNCTION_NUM; func_idx++) {
            config_addr.address_lsb.bits.function = func_idx;
            phy_addr.u.addr.high = config_addr.address_msb;
            phy_addr.u.addr.low  = config_addr.address_lsb.ul;
            config_header = (struct ConfigHeader*)mmap64(NULL, SIZE_OF_PCIE_PORTIO_SPACE,
                                                         PROT_READ | PROT_WRITE, MAP_SHARED,
                                                         m_dev_mem_fd, phy_addr.u.ull);
            if (config_header == (struct ConfigHeader*)-1) {
                close(m_dev_mem_fd);
                m_dev_mem_fd = -1;
                return false;
            }

            // is samsung ssd
            if (config_header->vendor_id == CONFIG_SAMSUNG_VENDOR_ID) {
                printf("Samsung SSD is found! %02d:%02d.%d\n", bus, device_idx, func_idx);
                ssd_found = true;
                munmap(config_header, SIZE_OF_PCIE_PORTIO_SPACE);
                break;
            }

            munmap(config_header, SIZE_OF_PCIE_PORTIO_SPACE);
        }
    }

    if (!ssd_found) {
        printf("Samsung SSD is NOT found on PCI bus %d, exit.\n", bus);
        close(m_dev_mem_fd);
        m_dev_mem_fd = -1;
        return false;
    }

    m_mapped_pci_addr = (int32_t*)mmap64(NULL, SIZE_OF_PCIE_MMIO_EXTENDED_SPACE,
                                         PROT_READ | PROT_WRITE, MAP_SHARED,
                                         m_dev_mem_fd, phy_addr.u.ull);
    if (m_mapped_pci_addr == (int32_t*)-1) {
        close(m_dev_mem_fd);
        m_dev_mem_fd = -1;
        return false;
    }

    printf("Detect offset of PCIe DOE extended capability in Device Configuration register\n");


    printf("[detecting pci capa] target DOE offset: %08lX\n", (uint64_t)doe_reg_offset);

    if (doe_reg_offset != 0) {
        /* use specified address offset */
        printf("[detecting pci capa] use user specified address offset\n");
        m_pci_doe_reg_addr = (PcieDoeReg*)(m_mapped_pci_addr + (doe_reg_offset / sizeof(int)));
    } else {
        /* automatically detection by traversing each PCI extended capability */
        printf("[detecting pci capa] offset of config space : %p\n", m_mapped_pci_addr);
        do {
            printf("[detecting pci capa] next offset            : %X\n", pcie_capa_offset);
            pci_address_current = m_mapped_pci_addr + (pcie_capa_offset / sizeof(int));
            printf("[detecting pci capa]             -> target  : %p\n", pci_address_current);
            pstCapaHeader = (PcieExtCapaHeader*)pci_address_current;

            if (pcie_doe_id == pstCapaHeader->ExtCapaId) {
                m_pci_doe_reg_addr = (PcieDoeReg*)pci_address_current;
                break;
            }

            pcie_capa_offset =  pstCapaHeader->NextCapaOffset;
        } while (pcie_capa_offset != 0x00); // next capability offset field of last capability has zero
    }

    if (m_pci_doe_reg_addr == NULL) {
        printf("PCI-DOE Register NOT detected, exit.\n");
        close(m_dev_mem_fd);
        m_dev_mem_fd = -1;
        return false;
    }

    printf("DOE register is found : %p\n", m_pci_doe_reg_addr);
    return true;
}
