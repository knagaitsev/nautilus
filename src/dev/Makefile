obj-$(NAUT_CONFIG_ARCH_X86) += apic.o \
	 ioapic.o \
	 i8254.o \
	 ps2.o \
	 pci.o \
	 vga.o \

ifndef NAUT_CONFIG_XEON_PHI
obj-y += serial.o
endif

obj-$(NAUT_CONFIG_ARCH_RISCV) += sifive.o

obj-$(NAUT_CONFIG_ARCH_ARM64) += pl011.o \
	pci.o

obj-$(NAUT_CONFIG_GIC_VERSION_2) += gicv2.o
obj-$(NAUT_CONFIG_GIC_VERSION_2M) += gicv2.o gicv2m.o
obj-$(NAUT_CONFIG_GIC_VERSION_3) += gicv3.o
obj-$(NAUT_CONFIG_GIC_VERSION_3_ITS) += gicv3.o gicv3-its.o

obj-$(NAUT_CONFIG_HPET) += hpet.o

obj-$(NAUT_CONFIG_VIRTIO_PCI) += virtio_pci.o
obj-$(NAUT_CONFIG_VIRTIO_NET) += virtio_net.o
obj-$(NAUT_CONFIG_VIRTIO_BLK) += virtio_blk.o
obj-$(NAUT_CONFIG_VIRTIO_GPU) += virtio_gpu.o

obj-$(NAUT_CONFIG_E1000_PCI) += e1000_pci.o
obj-$(NAUT_CONFIG_E1000E_PCI) += e1000e_pci.o
obj-$(NAUT_CONFIG_MLX3_PCI) += mlx3_ib.o

obj-$(NAUT_CONFIG_RAMDISK) += ramdisk.o

obj-$(NAUT_CONFIG_ATA) += ata.o

obj-$(NAUT_CONFIG_VESA) += vesa.o

ifdef NAUT_CONFIG_LOAD_LUA
obj-y +=lua_script.o
endif

obj-$(NAUT_CONFIG_PARALLEL_PORT_GPIO) += port_gpio.o

