// Servo Test 2 w/ fb
// Jacob Lambert
// /dev/servoblaster


/* Servo 0 is the Pan servo and can tilt from 0-180 degrees, or .5 ms to 2.5 ms
Servo 1 is the Tilt servo and can tilt from 0-150 degrees, or .5 ms to 2.08 ms
*/

#include "xMessage.hpp"
#include <unistd.h>
#include <stdio.h>

extern "C" {
#include <wiringPi.h>
#include <wiringPiI2C.h>
}


//#include <unistd.h>
//#include <wiringPi.h>
//#include <stdio.h>

//./servod --min={50|Nus|2.5%} //2.5% as minimum PW
//./servod --max={200|Nus|75%} //75% to prevent servo1 (tilt) from hitting at max pulse
//./servod --step-size=10us //default step size
// echo 0=120 > /dev/servoblaster  //sends servo 0 a pulse of 1.2 ms (120 us)
// udelay(100) //delays 100 us, or .1 ms
unsigned short ADC_Rd(unsigned short address);
unsigned short Rd_Rev(unsigned short);
void Pan_Gusset(int Pan_Loc, int Lower_Bound, int Upper_Bound);
void Mov_Motor(int Motor_Num, int Motor_Loc);
void Tilt_Gusset(int Tilt_Loc, int Lower_Bound, int Upper_Bound);

const unsigned char butPin = 18; // Active something

unsigned short ADC_Rd(unsigned short address) //Read channel 0 or 1 adc, Pan Motor
    {
    unsigned short adc_hex = 0;
    int result;

    result=wiringPiI2CSetup(0x48); //device address

    wiringPiI2CWriteReg16(result, 0x01, address); //Address for the register, and configuring the read channel 0
    adc_hex= wiringPiI2CReadReg16(result, 0x00);

    wiringPiI2CWriteReg16(result, 0x01, address);
    adc_hex = wiringPiI2CReadReg16(result,0x00); //device address, assigning device to read only

    adc_hex = Rd_Rev(adc_hex); //Reverses the order of the hex value taken in
    printf("%d\n",adc_hex);
    sleep(3);
    return adc_hex;
    }


unsigned short Rd_Rev(unsigned short a)
    {
     return ((a << 4) | (a >> 12)) & 0x0FFF;
    }

void Pan_Gusset(int Pan_Loc, int Lower_Bound, int Upper_Bound)
{
    printf("Pan Gusset....\n");
    static int i=0;
    Mov_Motor(0, Pan_Loc);
    int Read=-1;

    while (Read < Lower_Bound || Read > Upper_Bound)
    {
        Mov_Motor(0, Pan_Loc);
        Read=ADC_Rd(0x83C5);
        i++;

        if(i>=5)
        {
            printf("Issue with Pan Motor for location %d", Pan_Loc);
            break;
        }
    }

    }

void Mov_Motor(int Motor_Num, int Motor_Loc) //Motor number (0 or 1), and Motor Location (50-250)
{
    int n=35;
    int cx=0;
    if(Motor_Num==1)
	system("echo ./servod min={100}");
    char command[n];
    cx=snprintf(command, n, "echo %d=%d > /dev/servoblaster", Motor_Num, Motor_Loc); //assigns the echo call as the command, with the limit of n characters
    if(cx>n)
        printf("Command Length Too Long");
    else
        system(command);
    sleep(2);
}

void Tilt_Gusset(int Tilt_Loc, int Lower_Bound, int Upper_Bound)
{
    printf("Tilt Gusset....\n");
    static int i=0;
    int Read=-1;

    while (Read < Lower_Bound || Read > Upper_Bound) //Upper and Lower Bounds for each Location
    {
        Mov_Motor(1, Tilt_Loc);
        Read=ADC_Rd(0x83D5);
        i++;

        if(i>=5)
        {
            printf("Issue with Tilt Motor for location %d", Tilt_Loc);
            break;
        }
    }
}
void Cap_Image() //Motor number (0 or 1), and Motor Location (50-250)
{
    int n=65;
    int cx=0;
    static int i=0;
    char command[n];
    cx=snprintf(command, n, "fswebcam -r 1280x720 --jpeg 100 -D 30 -S 13 1 testing%d.jpeg", i); //assigns the echo call as the command, with the limit of n characters
    if(cx>n)
        printf("Command Length Too Long");
    else
        system(command);
        i++;

}

int main()
{
system("echo ./servod --p1pins=7, 11, 0, 0, 0, 0, 0, 0");
wiringPiSetupGpio();
pinMode(butPin, INPUT);
pullUpDnControl(butPin, PUD_DOWN);

        char *device = (char *)"/dev/ttyUSB0";
        Serial xbee(device, 57600);
        Message msg;
        int result;
        int fd;
    while(1)
    {
        if (digitalRead(butPin)==1)
        {
         Pan_Gusset(150, 1665, 1675);  //actual value 1669 or 1.669V
         Tilt_Gusset(120, 1322, 1334); //actual value 1637 or 1.637V
         Cap_Image();
	 printf("Sending image signal\n");
	 msg.sendingImage();
	 fd = xbee.Open();
	 sleep(2);
	 printf("Transmitting Image\n");
	 result = XSend(fd, "testing0.jpeg");
         if(result == 0){
                printf("Image transmitted successfully\n");
         }
         else{
                printf("Error during transmission\n");
         }
         Pan_Gusset(180, 1960, 1680);  //actual value 1669 or 1.669V
         Tilt_Gusset(150, 1622, 1650); //actual value 1637 or 1.637V
         Cap_Image();
         printf("Sending image signal\n");
         msg.sendingImage();
         fd = xbee.Open();
         sleep(2);
         printf("Transmitting Image\n");
         result = XSend(fd, "testing1.jpeg");
         if(result == 0){
                printf("Image transmitted successfully\n");
         }
         else{
                printf("Error during transmission\n");
         }
         Pan_Gusset(130, 1455, 1470);  //actual value 1669 or 1.669V
         Tilt_Gusset(120, 1322, 1334); //actual value 1637 or 1.637V
         Cap_Image();
         printf("Sending image signal\n");
         msg.sendingImage();
         fd = xbee.Open();
         sleep(2);
         printf("Transmitting Image\n");
         result = XSend(fd, "testing2.jpeg");
         if(result == 0){
                printf("Image transmitted successfully\n");
         }
         else{
                printf("Error during transmission\n");
         }



/*
         Pan_Gusset(180, 1955, 1965); //actual value 1970 or 1.970V
         Tilt_Gusset(150,1627,1635);  // actual value 1930, or 1.93V
         Cap_Image();


         Pan_Gusset(130,1450, 1460); //actual value 1142 or 1.142V
         Tilt_Gusset(120, 1320, 1331); //actual value 1127 or 1.127V feedback
         Cap_Image();
*/
        }
    }

return 0;
}

//Write up an equation to convert a feedback value to a corresponding pulse width. We will be taking in a feedback
//value when setting up the gusset plate locations.

