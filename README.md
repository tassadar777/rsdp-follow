THIS APPLICATION READS YOUR DEVICE PHYSICAL MEMORY, USE IT AT YOUR OWN RISK

rsdp-follow is a utility which dumps ACPI tables using Linux /dev/mem interface.

On some systems with UEFI and ACPI running a Device Tree (DT) kernel it is impossible 
to access ACPI tables with utilities such as acpidump, because ACPI is disabled in favor of DT
and corresponding entries are not exposed to /sys and /proc filesystems. 
The only possibility is to read them directly from the device physical memory.
This program can also be used as an alternative to acpidump on "normal" ACPI systems.

First, the kernel must be compiled with CONFIG_DEVMEM enabled (for a /dev/mem interface) and
CONFIG_STRICT_DEVMEM disabled (to access all of the physical memory).

Compile the program and install it to your executable search path.

For this program to work we need the location of the RSDP pointer, which points to the location of the ACPI tables.
Such systems use an EFI bootloader, and there is a message in the kernel log buffer which contains this address.

sudo dmesg | grep -i efi

gives us something like this:

efi: SMBIOS=<...> SMBIOS 3.0=<...> TPMFinalLog=<...> ACPI 2.0=0xabcdabcd MEMATTR=<...> ESRT=<...> 
MOKvar=<...> TPMEventLog=<...> INITRD=<...> RNG=<...> MEMRESERVE=<...>

We are interested in ACPI 2.0=0xabcdabcd part, which gives the location of an XSDP pointer (0xabcdabcd is an abstraction,
use the value found in your machine kernel log). In my case, it lies somewhere in the memory space between 3GiB and 4GiB.

Make a directory, cd to it and run:

sudo rsdp-follow 0xabcdabcd

With any luck, it will dump to the current folder XSDP pointer as xsdp.bin and all the tables found to .tbl files 
(you can change extensions later if you like).
