CC=gcc

all: rsdp-follow

clean: rsdp-follow
	rm rsdp-follow

rsdp-follow: rsdp-follow.c
	$(CC) -O2 -g rsdp-follow.c -o rsdp-follow
