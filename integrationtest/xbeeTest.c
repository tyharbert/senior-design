#ifndef XMESSAGE_HPP
#define XMESSAGE_HPP
#include "xMessage.hpp"
int main(){

	char *device = (char *)"/dev/ttyUSB1";

	Serial xbee(device, 57600);
	Message msg(device);

	int fd = xbee.Open();

	std::cout << "Send Image signal\n";
	msg.sendingImage();

	std::cout << "Transmitting Image\n";
	int result = XSend(fd, "testImage.jpeg");

	if(result == 0){
		std::cout << "Image transmitted successfully\n";
	}
	else{
		std::cout << "Error during image transmission\n";
	}

	xbee.Close();
	return 0;
}

#endif