// myos/kernel/io.h

#ifndef IO_H
#define IO_H

void port_byte_out(unsigned short port, unsigned char data);
unsigned char port_byte_in(unsigned short port);
void pic_remap(int offset1, int offset2); // remapping interrupt vectors func

#endif