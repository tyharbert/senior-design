#ifndef _SPI_H
#define _SPI_H

/**
  Section: Included Files
*/

void SPI_initialize(void);
unsigned char SPI_transfer(unsigned char data);
void SPI_enable();
void SPI_disable();

#endif 
