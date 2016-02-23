// Servo Test 2 w/ fb
// Jacob Lambert
// /dev/servoblaster


/* Servo 0 is the Pan servo and can tilt from 0-180 degrees, or .5 ms to 2.5 ms
Servo 1 is the Tilt servo and can tilt from 0-150 degrees, or .5 ms to 2.08 ms
*/

#include <unistd.h>
#include <wiringPi.h>
#include <stdio.h>

//./servod --min={50|Nus|2.5%} //2.5% as minimum PW
//./servod --max={200|Nus|75%} //75% to prevent servo1 (tilt) from hitting at max pulse
//./servod --step-size=10us //default step size
// echo 0=120 > /dev/servoblaster  //sends servo 0 a pulse of 1.2 ms (120 us)
// udelay(100) //delays 100 us, or .1 ms

const unsigned char butPin = 18; // Active something

void ADC_Rd()
    {
    short int adc_hex; //16 bits
    system("i2cset -y 1 0x48 0x01 0x83C5 w"); //Turns on and sets up the configuration register of ADC
    adc_hex = system("i2cget -y 1 0x48 0x00");
    printf("%x", adc_hex);
    }

short int Rd_Rev(short int a)
    {
     return (a << 8) | (a >> 8);
    }
int main()
{

    ADC_Rd();
    printf("%d", Rd_Rev(adc_hex));



/*stem("echo ./servod --p1pins=7, 11, 0, 0, 0, 0, 0, 0");
wiringPiSetupGpio();
pinMode(butPin, INPUT);
pullUpDnControl(butPin, PUD_DOWN);
system("i2cset -y 1 0x48 0x01 0x83C5 w") //Turns on and sets up the configuration register of ADC
system("i2cget -y 1 0x48 0x00") //Reads from the ADC

while(1)
{

	if (digitalRead(butPin)==1)
	{
//		for(i=0; i<=5; i++)
 //          	{
		system("echo 0=150 > /dev/servoblaster"); //move servo 0 to center/middle position




		system("echo 1=150 > /dev/servoblaster"); //move servo 1 to center/middle position
		sleep(1); //Wait for 1 second

		system("echo 0=180 > /dev/servoblaster");
		sleep(1);
		system("echo 1=180 > /dev/servoblaster");
		sleep(1);

		system("echo 0=100 > /dev/servoblaster");
		sleep(1);
		system("echo 1=100 > /dev/servoblaster");
		sleep(1);
		}
        }

}*/

return 0;
}
