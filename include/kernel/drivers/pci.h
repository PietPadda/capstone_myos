// myos/include/kernel/drivers/pci.h

#ifndef PCI_H
#define PCI_H

#include <kernel/types.h>

// I/O ports for PCI configuration space access
#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

// Virtio specific device identifiers
#define VIRTIO_VENDOR_ID      0x1AF4
#define VIRTIO_DEV_ID_SOUND   0x1059 // 0x1040 (base) + 25 (sound device)

// Scans the PCI bus for devices.
void pci_scan();

#endif // PCI_H