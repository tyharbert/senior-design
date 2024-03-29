#include "xmodem.h"

// internal structure definitions

// Windows requires a different way of specifying structure packing
#ifdef WIN32
#define PACKED
#pragma pack(push,1)
#else // POSIX, ARDUINO
#define PACKED __attribute__((__packed__))
#endif // WIN32 vs THE REST OF THE WORLD

#define _SOH_ 1 /* start of packet - note XMODEM-1K uses '2' */
#define _EOT_ 4
#define _ENQ_ 5
#define _ACK_ 6
#define _NAK_ 21 /* NAK character */
#define _CAN_ 24 /* CAN character CTRL+X */

typedef struct _XMODEM_BUF_
{
   char cSOH;                   ///< SOH byte goes here
   unsigned char aSEQ, aNotSEQ; ///< 1st byte = seq#, 2nd is ~seq#
   char aDataBuf[128];          ///< the actual data itself!
   unsigned char bCheckSum;     ///< checksum gets 1 byte
} PACKED XMODEM_BUF;

typedef struct _XMODEMC_BUF_
{
   char cSOH;                   ///< SOH byte goes here
   unsigned char aSEQ, aNotSEQ; ///< 1st byte = seq#, 2nd is ~seq#
   char aDataBuf[128];          ///< the actual data itself!
   unsigned short wCRC;         ///< CRC gets 2 bytes, high endian
} PACKED XMODEMC_BUF;

#ifdef WIN32
// restore default packing
#pragma pack(pop)
#endif // WIN32

typedef struct _XMODEM_
{
  SERIAL_TYPE ser;     ///< identifies the serial connection, data type is OS-dependent
  FILE_TYPE file;      ///< identifies the file handle, data type is OS-dependent

  union
  {
    XMODEM_BUF xbuf;   ///< XMODEM CHECKSUM buffer
    XMODEMC_BUF xcbuf; ///< XMODEM CRC buffer
  } buf;               ///< union of both buffers, total length 133 bytes

  unsigned char bCRC;  ///< non-zero for CRC, zero for checksum

} XMODEM;


#ifdef DEBUG_CODE
static char szERR[32]; // place for error messages, up to 16 characters

const char *XMGetError(void)
{
  return szERR;
}
#endif // DEBUG_CODE

#if defined(STAND_ALONE) && defined(DEBUG_CODE)
void debug_dump_buffer(int iDir, const void *pBuf, int cbBuf)
{
int i1, i2;
const unsigned char *p1, *p2;

  if(cbBuf <= 0)
  {
    return;
  }

  p1 = p2 = (const unsigned char *)pBuf;

  for(i1=0, i2=0; i1 <= cbBuf; i1++, p1++)
  {
    if(!i1 || i2 >= 16 || i1 == cbBuf)
    {
      if(i1)
      {
        while(i2 < 16)
        {
          fputs("    ", stderr); // fill up spaces where data would be
          i2++;
        }

        fputs(" : ", stderr);

        while(p2 < p1)
        {
          if(*p2 >= 32 && *p2 <= 127)
          {
            fputc(*p2, stderr);
          }
          else
          {
            fputc('.', stderr);
          }

          p2++;
        }

        fputc('\n', stderr);
      }

      if(!i1 && iDir > 0)
      {
        fputs("--> ", stderr);
      }
      else if(!i1 && iDir < 0)
      {
        fputs("<-- ", stderr);
      }
      else
      {
        fputs("    ", stderr);
      }

      i2 = 0;
      p2 = p1; // make sure
    }

    if(i1 < cbBuf)
    {
      if(!i2)
      {
        fprintf(stderr, "%02x: %02x", i1, *p1);
      }
      else
      {
        fprintf(stderr, ", %02x", *p1);
      }

      i2++;
    }
  }

  fputc('\n', stderr);
  fflush(stderr);
}
#endif // STAND_ALONE, DEBUG_CODE

unsigned char CalcCheckSum(const char *lpBuf, short cbBuf)
{
short iC, i1;

  iC = 0;

  for(i1 = 0; i1 < cbBuf; i1++)
  {
    iC += lpBuf[i1];
  }

  return (unsigned char)(iC & 0xff);
}

static unsigned short my_htons(unsigned short sVal)
{
  union
  {
    unsigned char aVal[2];
    unsigned short sVal;
  } a, b;

  b.sVal = sVal;

#ifdef ARDUINO

  a.aVal[0] = b.aVal[1]; // no math involved, pre-optimized code
  a.aVal[1] = b.aVal[0];

#else

  a.aVal[0] = (unsigned char)(sVal >> 8); // less optimized but universal code
  a.aVal[1] = (unsigned char)(sVal & 0xff);

#endif // ARDUINO

  return a.sVal;
}
unsigned short CalcCRC(const char *lpBuf, short cbBuf)
{
unsigned short wCRC;
short i1, i2, iAX;
char cAL;

  // ** this function returns 2-byte string containing
  // ** the CRC calculation result, as high endian

  wCRC = 0;

  for(i1=0; i1 < cbBuf; i1++)
  {
    cAL = lpBuf[i1];

    iAX = (unsigned short)cAL << 8;

    wCRC = iAX ^ wCRC;

    for(i2=0; i2 < 8; i2++)
    {
      iAX = wCRC;

      if(iAX & 0x8000)
      {
        wCRC <<= 1;
        wCRC ^= 0x1021;
      }
      else
      {
        wCRC <<= 1;
      }
    }
  }

  return my_htons(wCRC);
}

#ifndef ARDUINO
#ifdef WIN32
#define MyMillis GetTickCount
#else // WIN32

unsigned long MyMillis(void)
{
struct timeval tv;

  gettimeofday(&tv, NULL); // 2nd parameter is obsolete anyway

  // NOTE:  this won't roll over the way 'GetTickCount' does in WIN32 so I'll truncate it
  //        down to a 32-bit value to make it happen.  Everything that uses 'MyGetTickCount'
  //        must handle this rollover properly using 'int' and not 'long' (or cast afterwards)
  return((unsigned int)((unsigned long)tv.tv_sec * 1000L + (unsigned long)tv.tv_usec / 1000L));
}
#endif // WIN32
#endif // ARDUINO

void GenerateSEQ(XMODEM_BUF *pBuf, unsigned char bSeq)
{
  pBuf->aSEQ = bSeq;
  pBuf->aNotSEQ = ~bSeq;
}

void GenerateSEQC(XMODEMC_BUF *pBuf, unsigned char bSeq)
{
  pBuf->aSEQ = bSeq;
  pBuf->aNotSEQ = (255 - bSeq);//~bSeq; these should be the same but for now I do this...
}

short GetXmodemBlock(SERIAL_TYPE ser, char *pBuf, short cbSize)
{
unsigned long ulCur;
short cb1;
char *p1;

#ifdef ARDUINO
short i1;

  p1 = pBuf;
  cb1 = 0;

  ulCur = millis();
  ser->setTimeout(SILENCE_TIMEOUT); // 5 seconds [of silence]

  for(i1=0; i1 < cbSize; i1++)
  {
    if(ser->readBytes(p1, 1) != 1) // 5 seconds of "silence" is what fails this
    {
      break;
    }

    cb1++;
    p1++;

    if((millis() - ulCur) > (unsigned long)(10L * SILENCE_TIMEOUT)) // 10 times SILENCE TIMEOUT for TOTAL TIMEOUT
    {
      break; // took too long, I'm going now
    }
  }

#elif defined(WIN32)

#error no win32 code yet

#else // POSIX
int i1, i2;
unsigned long ulStart;


  if(fcntl(ser, F_SETFL, O_NONBLOCK) == -1)
  {
    static int iFailFlag = 0;

    if(!iFailFlag)
    {
      fprintf(stderr, "Warning:  'fcntl(O_NONBLOCK)' failed, errno = %d\n", errno);
      fflush(stderr);
      iFailFlag = 1;
    }
  }

  p1 = pBuf;
  cb1 = 0;

  ulStart = ulCur = MyMillis();

  for(i1=0; i1 < cbSize; i1++)
  {
    while((i2 = read(ser, p1, 1)) != 1)
    {
      if(i2 < 0 && errno != EAGAIN)
      {
        // read error - exit now
//        return cb1; // how many bytes I actually read
        goto the_end;
      }
      else
      {
        usleep(1000); // 1 msec

        if((MyMillis() - ulCur) > SILENCE_TIMEOUT || // too much silence?
           (MyMillis() - ulStart) > 10 * SILENCE_TIMEOUT) // too long for transfer
        {
//          return cb1; // finished (return how many bytes I actually read)
          goto the_end;
        }
      }
    }

    // here it succeeds

    cb1++;
    p1++;

    if((MyMillis() - ulStart) > 10 * SILENCE_TIMEOUT) // 10 times SILENCE TIMEOUT for TOTAL TIMEOUT
    {
      break; // took too long, I'm going now
    }
  }

the_end:

#ifdef STAND_ALONE
  fprintf(stderr, "GetXmodemBlock - request %d, read %d  errno=%d\n", cbSize, cb1, errno);
  fflush(stderr);
#ifdef DEBUG_CODE
  debug_dump_buffer(-1, pBuf, cb1);
#endif // DEBUG_CODE
#endif // STAND_ALONE

#endif // ARDUINO

  return cb1; // what I actually read
}

int WriteXmodemChar(SERIAL_TYPE ser, unsigned char bVal)
{
int iRval;
#ifdef ARDUINO

  iRval = ser->write(bVal);
//  ser->flush(); // force sending it

#elif defined(WIN32)

#error no win32 code yet

#else // POSIX
char buf[2]; // use size of '2' to avoid warnings about array size of '1'

  if(fcntl(ser, F_SETFL, 0) == -1) // set blocking mode
  {
    static int iFailFlag = 0;

    if(!iFailFlag)
    {
      fprintf(stderr, "Warning:  'fcntl(O_NONBLOCK)' failed, errno = %d\n", errno);
      iFailFlag = 1;
    }
  }

  buf[0] = bVal; // in case args are passed by register

  iRval = write(ser, buf, 1);

#if defined(STAND_ALONE) && defined(DEBUG_CODE)
  fprintf(stderr, "WriteXmodemChar - returns %d\n", iRval);
  if(iRval > 0)
  {
    debug_dump_buffer(1, buf, 1);
  }
#endif // STAND_ALONE, DEBUG_CODE
#endif // ARDUINO

  return iRval;
}

int WriteXmodemBlock(SERIAL_TYPE ser, const void *pBuf, int cbSize)
{
int iRval;
#ifdef ARDUINO

  iRval = ser->write((const uint8_t *)pBuf, cbSize);
//  ser->flush(); // force sending it before returning

#elif defined(WIN32)

#error no win32 code yet

#else // POSIX


  if(fcntl(ser, F_SETFL, 0) == -1) // set blocking mode
  {
    static int iFailFlag = 0;

    if(!iFailFlag)
    {
      fprintf(stderr, "Warning:  'fcntl(O_NONBLOCK)' failed, errno = %d\n", errno);
      fflush(stderr);
      iFailFlag = 1;
    }
  }

  iRval = write(ser, pBuf, cbSize);

#if defined(STAND_ALONE) && defined(DEBUG_CODE)
  fprintf(stderr, "\r\nWriteXmodemBlock - returns %d\n", iRval);
  fflush(stderr);

  if(iRval > 0)
  {
    debug_dump_buffer(1, pBuf, cbSize);
  }
#endif // STAND_ALONE, DEBUG_CODE
#endif

  return iRval;
}

void XModemFlushInput(SERIAL_TYPE ser)
{
unsigned long ulStart;
#ifdef ARDUINO

  ulStart = millis();

  do
  {
    if(ser->available())
    {
      ser->read(); // don't care about the data
      ulStart = millis(); // reset time
    }
    else
    {
      delay(1);
    }

  } while((millis() - ulStart) < 1000);

#elif defined(WIN32)

#error no win32 code yet

#else // POSIX
int i1;
#ifdef DEBUG_CODE
unsigned char buf[16];
int cbBuf;
#else // DEBUG_CODE
unsigned char buf[2];
#endif // DEBUG_CODE  

  if(fcntl(ser, F_SETFL, O_NONBLOCK) == -1)
  {
    static int iFailFlag = 0;

    if(!iFailFlag)
    {
      fprintf(stderr, "Warning:  'fcntl(O_NONBLOCK)' failed, errno = %d\n", errno);
      iFailFlag = 1;
    }
  }

  ulStart = MyMillis();
#ifdef DEBUG_CODE
  cbBuf = 0;
#endif // DEBUG_CODE
  while((MyMillis() - ulStart) < 1000)
  {
#ifdef DEBUG_CODE
    i1 = read(ser, &(buf[cbBuf]), 1);
#else // DEBUG_CODE
    i1 = read(ser, buf, 1);
#endif // DEBUG_CODE
    if(i1 == 1)
    {
#if defined(STAND_ALONE) && defined(DEBUG_CODE)
      cbBuf++;
      if(cbBuf >= sizeof(buf))
      {
        debug_dump_buffer(-1, buf, cbBuf);
        cbBuf = 0;
      }
#endif // STAND_ALONE, DEBUG_CODE
      ulStart = MyMillis();
    }
    else
    {
      usleep(1000);
    }
  }

#if defined(STAND_ALONE) && defined(DEBUG_CODE)
  if(cbBuf > 0)
  {
    debug_dump_buffer(-1, buf, cbBuf);
  }
#endif // STAND_ALONE, DEBUG_CODE
#endif // ARDUINO
}

void XmodemTerminate(XMODEM *pX)
{
  XModemFlushInput(pX->ser);

  // TODO:  close files?
}

short ValidateSEQ(XMODEM_BUF *pX, unsigned char bSeq)
{
  return pX->aSEQ != 255 - pX->aNotSEQ || // ~(pX->aNotSEQ) ||
         pX->aSEQ != bSeq; // returns TRUE if not valid
}

short ValidateSEQC(XMODEMC_BUF *pX, unsigned char bSeq)
{
  return pX->aSEQ != 255 - pX->aNotSEQ || // ~(pX->aNotSEQ) ||
         pX->aSEQ != bSeq; // returns TRUE if not valid
}

int ReceiveXmodem(XMODEM *pX)
{
int ecount, ec2;
long etotal, filesize, block;
unsigned char cY; // the char to send in response to a packet
// NOTE:  to allow debugging the CAUSE of an xmodem block's failure, i1, i2, and i3
//        are assigned to function return values and reported in error messages.
#ifdef DEBUG_CODE
short i1, i2, i3;
#define DEBUG_I1 i1 =
#define DEBUG_I2 i2 =
#define DEBUG_I3 i3 =
#else // DEBUG_CODE
#define DEBUG_I1 /*normally does nothing*/
#define DEBUG_I2 /*normally does nothing*/
#define DEBUG_I3 /*normally does nothing*/
#endif // DEBUG_CODE

  ecount = 0;
  etotal = 0;
  filesize = 0;
  block = 1;

  // ** already got the first 'SOH' character on entry to this function **

  //   Form2.Show 0      '** modeless show of form2 (CANSEND) **
  //   Form2!Label1.FloodType = 0
  //   Form2.Caption = "* XMODEM(Checksum) BINARY RECEIVE *"
  //   Form2!Label1.Caption = "Errors: 0  Bytes: 0"

  pX->buf.xbuf.cSOH = (char)1; // assumed already got this, put into buffer

  do
  {
    if(!pX->bCRC &&
       ((DEBUG_I1 GetXmodemBlock(pX->ser, ((char *)&(pX->buf.xbuf)) + 1, sizeof(pX->buf.xbuf) - 1))
        != sizeof(pX->buf.xbuf) - 1 ||
        (DEBUG_I2 ValidateSEQ(&(pX->buf.xbuf), block & 255)) ||
        (DEBUG_I3 CalcCheckSum(pX->buf.xbuf.aDataBuf, sizeof(pX->buf.xbuf.aDataBuf)) != pX->buf.xbuf.bCheckSum)))
    {
      // did not receive properly
      // TODO:  deal with repeated packet, sequence number for previous packet

#ifdef DEBUG_CODE
      sprintf(szERR,"A%ld,%d,%d,%d,%d,%d",block,i1,i2,i3,pX->buf.xbuf.aSEQ, pX->buf.xbuf.aNotSEQ);
//#ifdef STAND_ALONE
//      fprintf(stderr, "TEMPORARY (csum):  seq=%x, ~seq=%x  i1=%d, i2=%d, i3=%d\n", pX->buf.xbuf.aSEQ, pX->buf.xbuf.aNotSEQ, i1, i2, i3);
//#endif // STAND_ALONE
#endif // DEBUG_CODE

      XModemFlushInput(pX->ser);  // necessary to avoid problems

      cY = _NAK_; // send NAK (to get the checksum version)
      ecount ++; // for this packet
      etotal ++;
    }
    else if(pX->bCRC &&
            ((DEBUG_I1 GetXmodemBlock(pX->ser, ((char *)&(pX->buf.xcbuf)) + 1, sizeof(pX->buf.xcbuf) - 1))
             != sizeof(pX->buf.xcbuf) - 1 ||
            (DEBUG_I2 ValidateSEQC(&(pX->buf.xcbuf), block & 255)) ||
            (DEBUG_I3 CalcCRC(pX->buf.xcbuf.aDataBuf, sizeof(pX->buf.xbuf.aDataBuf)) != pX->buf.xcbuf.wCRC)))
    {
      // did not receive properly
      // TODO:  deal with repeated packet, sequence number for previous packet

#ifdef DEBUG_CODE
      sprintf(szERR,"B%ld,%d,%d,%d,%d,%d",block,i1,i2,i3,pX->buf.xcbuf.aSEQ, pX->buf.xcbuf.aNotSEQ);
#endif // DEBUG_CODE

      XModemFlushInput(pX->ser);  // necessary to avoid problems

      if(block > 1)
      {
        cY = _NAK_; // TODO do I need this?
      }
      else
      {
        cY = 'C'; // send 'CRC' NAK (the character 'C') (to get the CRC version)
      }
      ecount ++; // for this packet
      etotal ++;
    }
    else
    {
#ifdef ARDUINO
      if(pX->file.write((const uint8_t *)&(pX->buf.xbuf.aDataBuf), sizeof(pX->buf.xbuf.aDataBuf)) != sizeof(pX->buf.xbuf.aDataBuf))
      {
        return -2; // write error on output file
      }
#else // ARDUINO
      if(write(pX->file, &(pX->buf.xbuf.aDataBuf), sizeof(pX->buf.xbuf.aDataBuf)) != sizeof(pX->buf.xbuf.aDataBuf))
      {
        XmodemTerminate(pX);
        return -2; // write error on output file
      }
#endif // ARDUINO
      cY = _ACK_; // send ACK
      block ++;
      filesize += sizeof(pX->buf.xbuf.aDataBuf); // TODO:  need method to avoid extra crap at end of file
      ecount = 0; // zero out error count for next packet
    }

#ifdef STAND_ALONE
    fprintf(stderr, "block %ld  %ld bytes  %d errors\r\n", block, filesize, ecount);
#endif // STAND_ALONE

    ec2 = 0;   //  ** error count #2 **

    while(ecount < TOTAL_ERROR_COUNT && ec2 < ACK_ERROR_COUNT) // ** loop to get SOH or EOT character **
    {
      WriteXmodemChar(pX->ser, cY); // ** output appropriate command char **

      if(GetXmodemBlock(pX->ser, &(pX->buf.xbuf.cSOH), 1) == 1)
      {
        if(pX->buf.xbuf.cSOH == _CAN_) // ** CTRL-X 'CAN' - terminate
        {
          XmodemTerminate(pX);
          return 1; // terminated
        }
        else if(pX->buf.xbuf.cSOH == _EOT_) // ** EOT - end
        {
          WriteXmodemChar(pX->ser, _ACK_); // ** send an ACK (most XMODEM protocols expect THIS)
//          WriteXmodemChar(pX->ser, _ENQ_); // ** send an ENQ

          return 0; // I am done
        }
        else if(pX->buf.xbuf.cSOH == _SOH_) // ** SOH - sending next packet
        {
          break; // leave this loop
        }
        else
        {
          // TODO:  deal with repeated packet, i.e. previous sequence number

          XModemFlushInput(pX->ser);  // necessary to avoid problems (since the character was unexpected)
          // if I was asking for the next block, and got an unexpected character, do a NAK; otherwise,
          // just repeat what I did last time

          if(cY == _ACK_) // ACK
          {
            cY = _NAK_; // NACK
          }

          ec2++;
        }
      }
      else
      {
        ecount++; // increase total error count, and try writing the 'ACK' or 'NACK' again
      }
    }

    if(ec2 >= ACK_ERROR_COUNT) // wasn't able to get a packet
    {
      break;
    }

  } while(ecount < TOTAL_ERROR_COUNT);

  XmodemTerminate(pX);
  return 1; // terminated
}


int SendXmodem(XMODEM *pX)
{
int ecount, ec2;
short i1;
long etotal, filesize, filepos, block;


  ecount = 0;
  etotal = 0;
  filesize = 0;
  filepos = 0;
  block = 1;

  pX->bCRC = 0; // MUST ASSIGN TO ZERO FIRST or XMODEM-CHECKSUM may not work properly

  // ** already got first 'NAK' character on entry as pX->buf.xbuf.cSOH  **

#ifdef ARDUINO

  filesize = pX->file.size();

#else // ARDUINO

  filesize = (long)lseek(pX->file, 0, SEEK_END);
  if(filesize < 0) // not allowed
  {
#ifdef STAND_ALONE
    fputs("SendXmodem fail (file size)\n", stderr);
#endif // STAND_ALONE
    return -1;
  }

  lseek(pX->file, 0, SEEK_SET); // position at beginning

#endif // ARDUINO

  do
  {
    // ** depending on type of transfer, place the packet
    // ** into pX->buf with all fields appropriately filled.

    if(filepos >= filesize) // end of transfer
    {
      for(i1=0; i1 < 8; i1++)
      {
        WriteXmodemChar(pX->ser, _EOT_); // ** send an EOT marking end of transfer

        if(GetXmodemBlock(pX->ser, &(pX->buf.xbuf.cSOH), 1) != 1) // this takes up to 5 seconds
        {
          // nothing returned - try again?
          // break; // for now I loop, uncomment to bail out
        }
        else if(pX->buf.xbuf.cSOH == _ENQ_    // an 'ENQ' (apparently some expect this)
                || pX->buf.xbuf.cSOH == _ACK_ // an 'ACK' (most XMODEM implementations expect this)
                || pX->buf.xbuf.cSOH == _CAN_) // CTRL-X = TERMINATE
        {
          // both normal and 'abnormal' termination.
          break;
        }
      }

      XmodemTerminate(pX);

#ifdef STAND_ALONE
      fprintf(stderr, "SendXmodem return %d\n", i1 >= 8 ? 1 : 0);
#endif // STAND_ALONE
      return i1 >= 8 ? 1 : 0; // return 1 if receiver choked on the 'EOT' marker, else 0 for 'success'
    }

//  TODO:  progress indicator [can be LCD for arduino, blinky lights, ???  and of course stderr for everyone else]
//  If filesize& <> 0 Then Form2!Label1.FloodPercent = 100 * filepos& / filesize&

#ifdef STAND_ALONE
    fprintf(stderr, "block %ld  %ld of %ld bytes  %d errors\r\n", block, filepos, filesize, ecount);
#endif // STAND_ALONE

    if(pX->buf.xbuf.cSOH != 'C' // XMODEM CRC
       && pX->buf.xbuf.cSOH != (char)_NAK_) // NAK
    {
      // increase error count, bail if it's too much

      ec2++;
    }


#ifdef ARDUINO
    pX->file.seek(filepos); // in case I'm doing a 'retry' and I have to re-read part of the file
#else  // ARDUINO
    lseek(pX->file, filepos, SEEK_SET); // same reason as above
#endif // ARDUINO

    // fortunately, xbuf and xcbuf are the same through the end of 'aDataBuf' so
    // I can read the file NOW using 'xbuf' for both CRC and CHECKSUM versions

    if((filesize - filepos) >= sizeof(pX->buf.xbuf.aDataBuf))
    {
#ifdef ARDUINO
      i1 = pX->file.read(pX->buf.xbuf.aDataBuf, sizeof(pX->buf.xcbuf.aDataBuf));
#else  // ARDUINO
      i1 = read(pX->file, pX->buf.xbuf.aDataBuf, sizeof(pX->buf.xcbuf.aDataBuf));
#endif // ARDUINO

      if(i1 != sizeof(pX->buf.xcbuf.aDataBuf))
      {
        // TODO:  read error - send a ctrl+x ?
      }
    }
    else
    {
      memset(pX->buf.xcbuf.aDataBuf, '\x1a', sizeof(pX->buf.xcbuf.aDataBuf)); // fill with ctrl+z which is what the spec says
#ifdef ARDUINO
      i1 = pX->file.read(pX->buf.xbuf.aDataBuf, filesize - filepos);
#else  // ARDUINO
      i1 = read(pX->file, pX->buf.xbuf.aDataBuf, filesize - filepos);
#endif // ARDUINO

      if(i1 != (filesize - filepos))
      {
        // TODO:  read error - send a ctrl+x ?
      }
    }

    if(pX->buf.xbuf.cSOH == 'C' ||  // XMODEM CRC 'NAK' (first time only, typically)
       ((pX->buf.xbuf.cSOH == _ACK_ || pX->buf.xbuf.cSOH == _NAK_) && pX->bCRC)) // identifies ACK/NACK with XMODEM CRC
    {
      pX->bCRC = 1; // make sure (only matters the first time, really)

      // calculate the CRC, assign to the packet, and then send it

      pX->buf.xcbuf.cSOH = 1; // must send SOH as 1st char
      pX->buf.xcbuf.wCRC = CalcCRC(pX->buf.xcbuf.aDataBuf, sizeof(pX->buf.xcbuf.aDataBuf));

      GenerateSEQC(&(pX->buf.xcbuf), block);

      // send it

      i1 = WriteXmodemBlock(pX->ser, &(pX->buf.xcbuf), sizeof(pX->buf.xcbuf));
      if(i1 != sizeof(pX->buf.xcbuf)) // write error
      {
        // TODO:  handle write error (send ctrl+X ?)
      }
    }
    else if(pX->buf.xbuf.cSOH == _NAK_ || // 'NAK' (checksum method, may also be with CRC method)
            (pX->buf.xbuf.cSOH == _ACK_ && !pX->bCRC)) // identifies ACK with XMODEM CHECKSUM
    {
      pX->bCRC = 0; // make sure (this ALSO allows me to switch modes on error)

      // calculate the CHECKSUM, assign to the packet, and then send it

      pX->buf.xbuf.cSOH = 1; // must send SOH as 1st char
      pX->buf.xbuf.bCheckSum = CalcCheckSum(pX->buf.xbuf.aDataBuf, sizeof(pX->buf.xbuf.aDataBuf));

      GenerateSEQ(&(pX->buf.xbuf), block);

      // send it

      i1 = WriteXmodemBlock(pX->ser, &(pX->buf.xbuf), sizeof(pX->buf.xbuf));
      if(i1 != sizeof(pX->buf.xbuf)) // write error
      {
        // TODO:  handle write error (send ctrl+X ?)
      }
    }

    ec2 = 0;

    while(ecount < TOTAL_ERROR_COUNT && ec2 < ACK_ERROR_COUNT) // loop to get ACK or NACK
    {
      if(GetXmodemBlock(pX->ser, &(pX->buf.xbuf.cSOH), 1) == 1)
      {
        if(pX->buf.xbuf.cSOH == _CAN_) // ** CTRL-X - terminate
        {
          XmodemTerminate(pX);

          return 1; // terminated
        }
        else if(pX->buf.xbuf.cSOH == _NAK_ || // ** NACK
                pX->buf.xbuf.cSOH == 'C') // ** CRC NACK
        {
          break;  // exit inner loop and re-send packet
        }
        else if(pX->buf.xbuf.cSOH == _ACK_) // ** ACK - sending next packet
        {
          filepos += sizeof(pX->buf.xbuf.aDataBuf);
          block++; // increment file position and block count

          break; // leave inner loop, send NEXT packet
        }
        else
        {
          XModemFlushInput(pX->ser);  // for now, do this here too
          ec2++;
        }
      }
      else
      {
        ecount++; // increase total error count, then loop back and re-send packet
        break;
      }
    }

    if(ec2 >= ACK_ERROR_COUNT)
    {
      break;  // that's it, I'm done with this
    }

  } while(ecount < TOTAL_ERROR_COUNT * 2); // twice error count allowed for sending

   // ** at this point it is important to indicate the errors
   // ** and flush all buffers, and terminate process!

  XmodemTerminate(pX);
#ifdef STAND_ALONE
  fputs("SendXmodem fail (total error count)\n", stderr);
#endif // STAND_ALONE
  return -2; // exit on error
}

int XReceiveSub(XMODEM *pX)
{
int i1;

  // start with CRC mode [try 8 times to get CRC]

  pX->bCRC = 1;

  for(i1=0; i1 < 8; i1++)
  {
    WriteXmodemChar(pX->ser, 'C'); // start with NAK for XMODEM CRC

    if(GetXmodemBlock(pX->ser, &(pX->buf.xbuf.cSOH), 1) == 1)
    {
      if(pX->buf.xbuf.cSOH == _SOH_) // SOH - packet is on its way
      {
        return ReceiveXmodem(pX);
      }
      else if(pX->buf.xbuf.cSOH == _EOT_) // an EOT [blank file?  allow this?]
      {
        return 0; // for now, do this
      }
      else if(pX->buf.xbuf.cSOH == _CAN_) // cancel
      {
        return 1; // canceled
      }
    }
  }

  pX->bCRC = 0;

  // try again, this time using XMODEM CHECKSUM
  for(i1=0; i1 < 8; i1++)
  {
    WriteXmodemChar(pX->ser, _NAK_); // switch to NAK for XMODEM Checksum

    if(GetXmodemBlock(pX->ser, &(pX->buf.xbuf.cSOH), 1) == 1)
    {
      if(pX->buf.xbuf.cSOH == _SOH_) // SOH - packet is on its way
      {
        return ReceiveXmodem(pX);
      }
      else if(pX->buf.xbuf.cSOH == _EOT_) // an EOT [blank file?  allow this?]
      {
        return 0; // for now, do this
      }
      else if(pX->buf.xbuf.cSOH == _CAN_) // cancel
      {
        return 1; // canceled
      }
    }
  }

  XmodemTerminate(pX);

  return -3; // fail
}

int XSendSub(XMODEM *pX)
{
unsigned long ulStart;

  // waiting up to 30 seconds for transfer to start.  this is part of the spec?


#ifdef ARDUINO
  ulStart = millis();
#else // ARDUINO
  ulStart = MyMillis();
#endif // ARDUINO

  do
  {
    if(GetXmodemBlock(pX->ser, &(pX->buf.xbuf.cSOH), 1) == 1)
    {
      if(pX->buf.xbuf.cSOH == 'C' || // XMODEM CRC
         pX->buf.xbuf.cSOH == _NAK_) // NAK - XMODEM CHECKSUM
      {
#ifdef STAND_ALONE
        fprintf(stderr, "Got %d, continuing\n", pX->buf.xbuf.cSOH);
#endif // STAND_ALONE
        return SendXmodem(pX);
      }
      else if(pX->buf.xbuf.cSOH == _CAN_) // cancel
      {
#ifdef STAND_ALONE
        fputs("XSendSub fail (cancel)\n", stderr);
#endif // STAND_ALONE
        return 1; // canceled
      }
    }
  }    
#ifdef ARDUINO
  while((short)(millis() - ulStart) < 30000);   // 30 seconds
#else // ARDUINO
  while((int)(MyMillis() - ulStart) < 30000);
#endif // ARDUINO

  XmodemTerminate(pX);

#ifdef STAND_ALONE
  fputs("XSendSub fail (timeout)\n", stderr);
#endif // STAND_ALONE
  return -3; // fail
}

#ifdef ARDUINO

short XReceive(SDClass *pSD, HardwareSerial *pSer, const char *szFilename)
{
short iRval;
XMODEM xx;

  memset(&xx, 0, sizeof(xx));

  xx.ser = pSer;

  if(pSD->exists((char *)szFilename))
  {
    pSD->remove((char *)szFilename);
  }

  xx.file = pSD->open((char *)szFilename, FILE_WRITE);
  if(!xx.file)
  {
    return -9; // can't create file
  }

  iRval = XReceiveSub(&xx);  

  xx.file.close();

  if(iRval)
  {
    WriteXmodemChar(pSer, _CAN_); // cancel (make sure)

    pSD->remove((char *)szFilename); // delete file on error
  }

  return iRval;
}

int XSend(SDClass *pSD, HardwareSerial *pSer, const char *szFilename)
{
short iRval;
XMODEM xx;

  memset(&xx, 0, sizeof(xx));

  xx.ser = pSer;

  xx.file = pSD->open(szFilename, FILE_READ);
  if(!xx.file)
  {
    return -9; // can't open file
  }

  iRval = XSendSub(&xx);  

  xx.file.close();

  return iRval;
}

#else // ARDUINO

int XReceive(SERIAL_TYPE hSer, const char *szFilename, int nMode)
{
int iRval;
XMODEM xx;
#ifndef ARDUINO
int iFlags;
#endif // !ARDUINO

#ifdef DEBUG_CODE
  szERR[0]=0;
#endif // DEBUG_CODE
  memset(&xx, 0, sizeof(xx));

  xx.ser = hSer;

  unlink(szFilename); // make sure it does not exist, first
  xx.file = open(szFilename, O_CREAT | O_TRUNC | O_WRONLY, nMode);

  if(!xx.file)
  {
#ifdef STAND_ALONE
    fprintf(stderr, "XReceive fail \"%s\"  errno=%d\n", szFilename, errno);
#endif // STAND_ALONE
    return -9; // can't create file
  }

#ifndef ARDUINO
  iFlags = fcntl(hSer, F_GETFL);
#endif // !ARDUINO

  iRval = XReceiveSub(&xx);  

#ifndef ARDUINO
  if(iFlags == -1 || fcntl(hSer, F_SETFL, iFlags) == -1)
  {
    fprintf(stderr, "Warning:  'fcntl' call to restore flags failed, errno=%d\n", errno);
  }
#endif // !ARDUINO

  close(xx.file);

  if(iRval)
  {
    unlink(szFilename); // delete file on error
  }

#ifdef STAND_ALONE
  fprintf(stderr, "XReceive returns %d\n", iRval);
#endif // STAND_ALONE
  return iRval;
}

int XSend(SERIAL_TYPE hSer, const char *szFilename)
{
int iRval;
XMODEM xx;
#ifndef ARDUINO
int iFlags;
#endif // !ARDUINO

#ifdef DEBUG_CODE
  szERR[0]=0;
#endif // DEBUG_CODE
  memset(&xx, 0, sizeof(xx));

  xx.ser = hSer;

  xx.file = open(szFilename, O_RDONLY, 0);

  if(!xx.file)
  {
#ifdef STAND_ALONE
    fprintf(stderr, "XSend fail \"%s\"  errno=%d\n", szFilename, errno);
#endif // STAND_ALONE
    return -9; // can't open file
  }

#ifndef ARDUINO
  iFlags = fcntl(hSer, F_GETFL);
#endif // !ARDUINO

  iRval = XSendSub(&xx);  

  if(iFlags == -1 || fcntl(hSer, F_SETFL, iFlags) == -1)
  {
    fprintf(stderr, "Warning:  'fcntl' call to restore flags failed, errno=%d\n", errno);
  }

  close(xx.file);

#ifdef STAND_ALONE
  fprintf(stderr, "XSend returning %d\n", iRval);
#endif // STAND_ALONE
//  fprintf(stderr, "Test output");
  return iRval;
}

#endif // ARDUINO



#ifdef STAND_ALONE

static const char szSER[]="/dev/ttyU0";

#include <termios.h>

void ttyconfig(int iFile, int iBaud, int iParity, int iBits, int iStop)
{
int i1;
struct termios sIOS;

  i1 = fcntl(iFile, F_GETFL);

  i1 |= O_NONBLOCK; // i1 &= ~O_NONBLOCK); // turn OFF non-blocking?

	fcntl(iFile, F_SETFL, i1);

  if(!tcgetattr(iFile, &sIOS))
  {
    cfsetspeed(&sIOS, iBaud);
    sIOS.c_cflag &= ~(CSIZE|PARENB|CS5|CS6|CS7|CS8);
	  sIOS.c_cflag |= iBits == 5 ? CS5 : iBits == 6 ? CS6 : iBits == 7 ? CS7 : CS8; // 8 is default
	  if(iStop == 2)
	  {
	    sIOS.c_cflag |= CSTOPB;
	  }
	  else
	  {
	    sIOS.c_cflag &= ~CSTOPB;
	  }

    sIOS.c_cflag &= ~CRTSCTS; // hardware flow control _DISABLED_ (so I can do the reset)
    sIOS.c_cflag |= CLOCAL; // ignore any modem status lines

    if(!iParity)
    {
      sIOS.c_cflag &= ~(PARENB | PARODD);
    }
    else if(iParity > 0) // odd
    {
      sIOS.c_cflag |= (PARENB | PARODD);
    }
    else // even (negative)
    {
      sIOS.c_cflag &= PARODD;
      sIOS.c_cflag |= PARENB;
    }

//    sIOS.c_iflag |= IGNCR; // ignore CR

    // do not translate characters or xon/xoff and ignore break
    sIOS.c_iflag &= ~(IGNBRK | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY | IMAXBEL | ISTRIP); // turn these off

#if defined(__FreeBSD__)
    sIOS.c_oflag &= ~(OPOST | ONLCR | OCRNL | TABDLY | ONOEOT | ONOCR | ONLRET); // FreeBSD version
#else // Linux? YMMV
    sIOS.c_oflag &= ~(OPOST | ONLCR | OCRNL | TABDLY | ONOCR | ONLRET); // turn these off too (see man termios)
#endif // FBSD vs Linux

    // make sure echoing is disabled and control chars aren't translated or omitted
#if defined(__FreeBSD__)
    sIOS.c_lflag &= ~(ECHO | ECHOKE | ECHOE | ECHONL | ECHOPRT | ECHOCTL | ICANON | IEXTEN | ISIG | ALTWERASE);
#else // Linux? YMMV
    sIOS.c_lflag &= ~(ECHO | ECHOKE | ECHOE | ECHONL | ECHOPRT | ECHOCTL | ICANON | IEXTEN | ISIG);
#endif // FBSD vs Linux
    sIOS.c_cc[VMIN] = 0;  // ensures no 'grouping' of input
    sIOS.c_cc[VTIME] = 0; // immediate return

    if(tcsetattr(iFile, TCSANOW, &sIOS))
    {
      fprintf(stderr, "error %d setting attributes\n", errno);
    }
  }
  else
  {
    fprintf(stderr, "error %d getting attributes\n", errno);
  }
}

void reset_arduino(int iFile)
{
unsigned int sFlags;
unsigned long ulStart;
int i1;

// toggle the RTS and DTR high, low, then high - so much easier via POSIX-compatible OS!

  ioctl(iFile, TIOCMGET, &sFlags);

  sFlags &= ~(TIOCM_DTR | TIOCM_RTS); // the high to low transition discharges the capacitor (signal is inverted on board)
  if(ioctl(iFile, TIOCMSET, &sFlags) < 0)
  {
    fprintf(stderr, "WARNING:  ioctl() returns < 0, errno=%d (%xH)\n", errno, errno);
  }

  usleep(250000); // avrdude does this for 50 msecs, my change has it at 50msecs

  sFlags |= TIOCM_DTR | TIOCM_RTS; // leave it in THIS state when I'm done
  if(ioctl(iFile, TIOCMSET, &sFlags) < 0)
  {
    fprintf(stderr, "WARNING:  ioctl() returns < 0, errno=%d (%xH)\n", errno, errno);
  }

  usleep(50000); // avrdude does this for 50 msecs (no change)

  ulStart = MyMillis();

  // flush whatever is there, (5 seconds)

  while((MyMillis() - ulStart) < 5000)
  {
    i1 = read(iFile, &i1, 1);
    if(i1 == 1)
    {
      ulStart = MyMillis();
    }
    else
    {
      usleep(1000);
    }
  }

}

int main(int argc, char *argv[])
{
int hSer;
char tbuf[256];
int i1, iSR = 0;


  if(argc < 3)
  {
    fputs("Usage:  [prog] [S|R] filename\n", stderr);
    return 1;
  }

  if(argv[1][0] == 'R' || argv[1][1]=='r')
  {
    iSR = -1;
  }
  else if(argv[1][0] == 'S' || argv[1][1]=='s')
  {
    iSR = 1;
  }
  else if(argv[1][0] == 'X' || argv[1][1]=='x')
  {
    iSR = 0; // test function
  }
  else
  {
    fputs("Usage:  [prog] [S|R] filename     (b)\n", stderr);
    return 1;
  }

  hSer = open(szSER, (O_RDWR | O_NONBLOCK), 0);
  if(hSer == -1)
  {
    fprintf(stderr, "Unable to open \"%s\" errno=%d\n", szSER, errno);
    return 3;
  }

  fputs("TTYCONFIG\n", stderr);
  ttyconfig(hSer, 9600, 0, 8, 1);

  reset_arduino(hSer);

  fprintf(stderr, "Sleeping for 10 seconds to allow reset\n");

//  usleep(10000000);
  for(i1=0; i1 < 10; i1++)
  {
    XModemFlushInput(hSer);  
  }

  for(i1=0; i1 < 3; i1++)
  {
    sprintf(tbuf, "X%c%s", argv[1][0], argv[2]);

    fprintf(stderr, "writing: \"%s\"\n", tbuf);
    strcat(tbuf, "\r");
    WriteXmodemBlock(hSer, tbuf, strlen(tbuf));

    fputs("flush input\n", stderr);
    XModemFlushInput(hSer);  

    // wait for an LF response

    if(iSR > 0)
    {
      fputs("XSEND\n", stderr);
      if(XSend(hSer, argv[2]))
      {
        fputs("ERROR\n", stderr);
      }
      else
      {
        fputs("SUCCESS!\n", stderr);
        i1 = 0;
        break;
      }
    }
    else if(iSR < 0)
    {
      fputs("XRECEIVE\n", stderr);
      if(XReceive(hSer, argv[2], 0664))
      {
        fputs("ERROR\n", stderr);
      }
      else
      {
        fputs("SUCCESS!\n", stderr);
        i1 = 0;
        break;
      }
    }
    else
    {
      // test function
      XModemFlushInput(hSer); // continue doing this
      break; // done (once only)
    }
  }

  fputs("EXIT\n", stderr);
  close(hSer);

  return i1 ? 1 : -1;
}
#endif // STAND_ALONE

