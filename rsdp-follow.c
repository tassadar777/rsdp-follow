/* 
rsdp-follow - a program to dump ACPI tables on Linux via /dev/mem interface

Copyright (C) Andrew Ivanchuk 2025

 This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct RSDP_t {
 char Signature[8];
 uint8_t Checksum;
 char OEMID[6];
 uint8_t Revision;
 uint32_t RsdtAddress;
} __attribute__ ((packed));

struct XSDP_t {
 char Signature[8];
 uint8_t Checksum;
 char OEMID[6];
 uint8_t Revision;
 uint32_t RsdtAddress;      // deprecated since version 2.0

 uint32_t Length;
 uint64_t XsdtAddress;
 uint8_t ExtendedChecksum;
 uint8_t reserved[3];
} __attribute__ ((packed));

struct ACPISDTHeader {
  char Signature[4];
  uint32_t Length;
  uint8_t Revision;
  uint8_t Checksum;
  char OEMID[6];
  char OEMTableID[8];
  uint32_t OEMRevision;
  uint32_t CreatorID;
  uint32_t CreatorRevision;
};

struct GenericAddressStructure_s
{
  uint8_t AddressSpace;
  uint8_t BitWidth;
  uint8_t BitOffset;
  uint8_t AccessSize;
  uint64_t Address;
};

typedef struct GenericAddressStructure_s GenericAddressStructure;

struct FADT
{
    struct   ACPISDTHeader h;
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    uint8_t  Reserved;

    uint8_t  PreferredPowerManagementProfile;
    uint16_t SCI_Interrupt;
    uint32_t SMI_CommandPort;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BIOS_REQ;
    uint8_t  PSTATE_Control;
    uint32_t PM1aEventBlock;
    uint32_t PM1bEventBlock;
    uint32_t PM1aControlBlock;
    uint32_t PM1bControlBlock;
    uint32_t PM2ControlBlock;
    uint32_t PMTimerBlock;
    uint32_t GPE0Block;
    uint32_t GPE1Block;
    uint8_t  PM1EventLength;
    uint8_t  PM1ControlLength;
    uint8_t  PM2ControlLength;
    uint8_t  PMTimerLength;
    uint8_t  GPE0Length;
    uint8_t  GPE1Length;
    uint8_t  GPE1Base;
    uint8_t  CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t  DutyOffset;
    uint8_t  DutyWidth;
    uint8_t  DayAlarm;
    uint8_t  MonthAlarm;
    uint8_t  Century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    uint16_t BootArchitectureFlags;

    uint8_t  Reserved2;
    uint32_t Flags;

    // 12 byte structure; see below for details
    GenericAddressStructure ResetReg;

    uint8_t  ResetValue;
    uint8_t  Reserved3[3];

    // 64bit pointers - Available on ACPI 2.0+
    uint64_t                X_FirmwareControl;
    uint64_t                X_Dsdt;

    GenericAddressStructure X_PM1aEventBlock;
    GenericAddressStructure X_PM1bEventBlock;
    GenericAddressStructure X_PM1aControlBlock;
    GenericAddressStructure X_PM1bControlBlock;
    GenericAddressStructure X_PM2ControlBlock;
    GenericAddressStructure X_PMTimerBlock;
    GenericAddressStructure X_GPE0Block;
    GenericAddressStructure X_GPE1Block;
};

int doChecksumXsdp(struct XSDP_t *tableHeader)
{
    unsigned char sum = 0;

    for (int i = 0; i < tableHeader->Length; i++)
    {
        sum += ((char *) tableHeader)[i];
    }

    return sum == 0;
}

int doChecksum(struct ACPISDTHeader *tableHeader)
{
    unsigned char sum = 0;

    for (int i = 0; i < tableHeader->Length; i++)
    {
        sum += ((char *) tableHeader)[i];
    }

    return sum == 0;
}

void dump_table(int fd,off_t offset)
{
	off_t curr;
	char *p=NULL;
	struct ACPISDTHeader h;
	ssize_t i,count;
	char name[10];

	int outfd;

	curr=lseek(fd,offset,SEEK_SET);
	if(curr!=offset){
		printf("Cannot seek to %lx\n",offset);
		goto error;
	}
	count=sizeof(h);
	i=read(fd,(char*) &h,count);
	if(i!=count)
		goto error;

	p=malloc(h.Length);
	curr=lseek(fd,offset,SEEK_SET);
        if(curr!=offset)
		goto error;
	count=h.Length;
        i=read(fd,(char*) p,count);
        if(i!=count)
                goto error;

	if(!doChecksum((struct ACPISDTHeader*) p))
		goto error;
	for(i=0;i<4;++i)
		name[i]=h.Signature[i];
	name[4]=0;
	strncat(name,".tbl",sizeof(name)-1);
	printf("Checksum correct for %s of size %d\n",name,h.Length);

	outfd=open(name,O_CREAT|O_WRONLY,0644);
	if(outfd<0)
		goto error;

	i=write(outfd,p,count);
	if(i!=count){
		close(outfd);
		goto error;
	}
	close(outfd);

	//follow FADT DSDT pointer and try to fetch DSDT
	if(!strncmp(name,"FACP",4)){
		struct FADT *fp;
		uint64_t dsdtp;

		fp=(struct FADT*) p;
		dsdtp=fp->X_Dsdt;
		if(!dsdtp) //if no 64 bit DSDP pointer
			dsdtp=(uint64_t) fp->Dsdt;
		printf("FACP found, trying to fetch DSDT at %lx...\n",dsdtp);
		dump_table(fd,dsdtp);
	}

	if(p!=NULL)
		free(p);
	return;	
error:
	if(p!=NULL)
		free(p);
	if(!errno)
		errno=5;
	perror(NULL);
}

int main(int argc,char **argv)
{
	ssize_t i,count;
	int fd,out_fd;
	const char *filename="/dev/mem";
	const char *out_pointer="xsdp.bin";
	const char *out_header="xsdt.tbl";
	off_t offset,curr_offset;
	struct XSDP_t xsdp;
	struct ACPISDTHeader header;
	char *pheader=NULL;
	char signature[8];

	printf("Size of RSDP_t = %ld\n",sizeof(struct RSDP_t));
	printf("Size of XSDP = %ld\n",sizeof(struct XSDP_t));

	if(argc!=2){
		printf("Usage: rsdp-follow <address>\n");
		goto usage;
	}
	
	offset=strtol(argv[1],NULL,16);
	
	fd=open(filename,O_RDONLY);
	
	if(fd>=0)
		printf("Opened %s\n",filename);
	else
		goto error;
	
	curr_offset=lseek(fd,offset,SEEK_SET);
	
	if(curr_offset==offset)
		printf("Positioned at %lx\n",offset);
	else
		goto error;

	count=sizeof(xsdp);
	i=read(fd,(char*) &xsdp,count);
	if(i==count)
		printf("Read %ld bytes\n",i);
	else
		goto error;

	memcpy(signature,&(xsdp.Signature),sizeof(xsdp.Signature));
	signature[7]=0;
	printf("Signature is %s\n",signature);

	if(strncmp(signature,"RSD PTR",7)){ //the only check that is standing before you and chaos
		printf("Incorrect RSDP signature\n");
		goto exit;
	}

	if(doChecksumXsdp(&xsdp))
		printf("Checksum for XSDP is correct\n");
	else
		goto error;

	out_fd=open(out_pointer,O_WRONLY|O_CREAT,0644);
	i=write(out_fd,(char*) &xsdp,count);
	close(out_fd);
	
	if(i==count)
		printf("Wrote %ld bytes to %s\n",i,out_pointer);
	else
		goto error;

	printf("XsdtAddress %lx\n",xsdp.XsdtAddress);
	printf("Length %d\n",xsdp.Length);

	offset=xsdp.XsdtAddress;
	curr_offset=lseek(fd,offset,SEEK_SET);
	if(curr_offset!=offset)
		goto error;
	count=sizeof(header);
	i=read(fd,(char*) &header,count);
	if(i!=count)
		goto error;
	pheader=malloc(header.Length);
	curr_offset=lseek(fd,offset,SEEK_SET);
	count=header.Length;
	i=read(fd,(char*) pheader,count);
        if(i!=count)
		goto error;

	printf("Signature is ");
	for(i=0;i<4;++i)
		printf("%c",header.Signature[i]);
	printf("\n");
	if(!doChecksum((struct ACPISDTHeader*)pheader))
		goto error;
	else
		printf("Checksum for XSDT is correct\n");

	out_fd=open(out_header,O_WRONLY|O_CREAT,0644);
	i=write(out_fd,(char*)pheader,header.Length);
	close(out_fd);

	count=(header.Length-sizeof(header))/8;
	printf("Sizeof XSDT is %ld, XSDT length is %d, %ld tables found\n",sizeof(header),header.Length,count);

	for(i=0;i<count;++i){
		char *p;
		p=pheader+sizeof(header)+8*i;
		printf("Dumping table %.2ld at %lx...\n",i, *((uint64_t*)p) );
		dump_table(fd,*((uint64_t*)p));
	}
exit:
	if(pheader!=NULL)
		free(pheader);	
	return 0;
error:
	if(pheader!=NULL)
		free(pheader);
	if(!errno)
		errno=5;
	perror(NULL);
usage:
	return 1;
}
