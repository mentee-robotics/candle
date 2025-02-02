#include "spiDevice.hpp"

static const char* spiDev = "/dev/spidev0.0";

// #define SPI_VERBOSE
// #define SPI_VERBOSE_ON_CRC_ERROR

SpiDevice::SpiDevice(char* rxBufferPtr, const int rxBufferSize_)
{
	fd = open(spiDev, O_RDWR);
	if (fd < 0)
	{
		const char* msg = "[SPI] Could not open the SPI device... (is SPI bus available on your device?)";
		std::cout << msg << std::endl;
		throw msg;
	}

	int ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret != 0)
	{
		const char* msg = "[SPI] Could not write SPI mode...";
		std::cout << msg << std::endl;
		close(fd);
		throw msg;
	}

	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret != 0)
	{
		const char* msg = "[SPI] Could not write SPI bits per word...";
		std::cout << msg << std::endl;
		close(fd);
		throw msg;
	}

	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spiSpeed);
	if (ret != 0)
	{
		const char* msg = "[SPI] Could not write the SPI max speed...";
		std::cout << msg << std::endl;
		close(fd);
		throw msg;
	}

	rxBuffer = rxBufferPtr;
	rxBufferSize = rxBufferSize_;
	/* set transfer parameters that are constant in each cycle */
	memset(&trx, 0, sizeof(trx));
	trx.bits_per_word = 8;
	trx.speed_hz = spiSpeed;

	crc = new Crc();
}

SpiDevice::~SpiDevice()
{
	delete (crc);
	close(fd);
}

bool SpiDevice::transmit(char* buffer, int commandLen, bool waitForResponse, int timeout, int responseLen, bool faultVerbose)
{
	/* CRC */
	commandLen = crc->addCrcToBuf(buffer, commandLen);

	trx.tx_buf = (unsigned long)buffer;
	trx.rx_buf = (unsigned long)rxBuffer;
	trx.len = commandLen;

	/* send */
	sendMessage(SPI_IOC_MESSAGE(1), &trx);

	if (waitForResponse)
	{
		/* allow for SPI reinit on the slave side */
		usleep(10);
		return receive(timeout, responseLen, faultVerbose);
	}

	return true;
}

bool SpiDevice::receive(int timeout, int responseLen, bool faultVerbose)
{
	memset(rxBuffer, 0, rxBufferSize);
	rxLock.lock();
	int delayUs = 10;
	int timeoutBusOutUs = timeout * 1000;
	uint8_t dummyTx = 0;
	uint8_t byteRx = 0;
	bool firstByteReceived = false;
	bytesReceived = 0;
	int usTimestamp = 0;
	const int timeoutUs = 100;

	while (usTimestamp < timeoutUs && timeoutBusOutUs > 0 && bytesReceived < responseLen)
	{
		trx.tx_buf = (unsigned long)&dummyTx;
		trx.rx_buf = (unsigned long)&byteRx;
		trx.len = 1;

		sendMessage(SPI_IOC_MESSAGE(1), &trx);
		timeoutBusOutUs -= delayUs;	 // If not receiving wait for 100ms and return false
		/* if the received byte is non-zero, continue with a larger transfer for the rest (responseLen -1) */
		if (byteRx != 0) firstByteReceived = true;
		if (firstByteReceived)
		{
			responseLen += crc->getCrcLen();
			rxBuffer[bytesReceived++] = byteRx;
			uint8_t dummyTxa[maxResponseLen];
			memset(&dummyTxa, 0, responseLen);
			/* update SPI structure */
			trx.tx_buf = (unsigned long)&dummyTxa;
			trx.rx_buf = (unsigned long)&rxBuffer[1];
			trx.len = responseLen - 1;
			/* send request */
			sendMessage(SPI_IOC_MESSAGE(1), &trx);
			bytesReceived += (responseLen - 1);
			break;
		}

		if (firstByteReceived && bytesReceived == 0)
			usTimestamp += delayUs;	 // If receiving wait for 100us idle state on the bus
		else
			timeoutBusOutUs -= delayUs;	 // If not receiving wait for 100ms and return false
		usleep(delayUs);
	}

	if (crc->checkCrcBuf(rxBuffer, bytesReceived))
		bytesReceived -= crc->getCrcLen();
	else if (bytesReceived > 0)
	{
#ifdef SPI_VERBOSE_ON_CRC_ERROR
		bytesReceived = responseLen;
		displayDebugMsg(rxBuffer, bytesReceived);
#endif
		errorCnt++;
		/* clear the command byte -> the frame will be rejected */
		rxBuffer[0] = 0;
		bytesReceived = 0;
		std::cout << "[SPI] ERROR CRC!" << std::endl;
	}
	else if (faultVerbose)
		std::cout << "[SPI] Did not receive response from SPI device" << std::endl;

	rxLock.unlock();

#ifdef SPI_VERBOSE
	displayDebugMsg(rxBuffer, bytesReceived);
#endif

	if (bytesReceived > 0)
		return true;

	return false;
}

bool SpiDevice::transmitReceive(char* buffer, int commandLen, int responseLen)
{
	memset(rxBuffer, 0, rxBufferSize);
	rxLock.lock();
	bytesReceived = 0;
	char txBuffer[maxResponseLen];
	memcpy(txBuffer, buffer, commandLen);

	/* CRC */
	commandLen = crc->addCrcToBuf(txBuffer, commandLen);
	/* modify the response len with CRC */
	responseLen += crc->getCrcLen();

	trx.tx_buf = (unsigned long)txBuffer;
	trx.rx_buf = (unsigned long)rxBuffer;
	trx.len = responseLen > commandLen ? responseLen : commandLen;
	/* make the transfer */
	sendMessage(SPI_IOC_MESSAGE(1), &trx);

	/* check CRC */
	if (crc->checkCrcBuf(rxBuffer, responseLen))
		bytesReceived = responseLen - crc->getCrcLen();
	else if (bytesReceived > 0)
	{
#ifdef SPI_VERBOSE_ON_CRC_ERROR
		bytesReceived = responseLen;
		displayDebugMsg(rxBuffer, bytesReceived);
#endif

		errorCnt++;
		/* clear the command byte -> the frame will be rejected */
		rxBuffer[0] = 0;
		bytesReceived = 0;
		std::cout << "[SPI] ERROR CRC!" << std::endl;
	}
	else
		std::cout << "[SPI] Did not receive response from SPI device" << std::endl;

	rxLock.unlock();

#ifdef SPI_VERBOSE
	displayDebugMsg(rxBuffer, bytesReceived);
#endif

	if (bytesReceived > 0)
		return true;

	return false;
}

void SpiDevice::displayDebugMsg(char* buffer, int bytesReceived)
{
	if (bytesReceived > 0)
	{
		std::cout << "Got " << std::dec << bytesReceived << "bytes." << std::endl;
		std::cout << buffer << std::endl;
		for (int i = 0; i < bytesReceived; i++)
			std::cout << std::hex << "0x" << (unsigned short)buffer[i] << " ";
		std::cout << std::dec << std::endl
				  << "#######################################################" << std::endl;
	}
}

void SpiDevice::sendMessage(unsigned long request, spi_ioc_transfer* trx)
{
	errno = 0;
	ioctl(fd, request, trx);

	if (errno != 0)
	{
		std::cout << "[SPI] low level error! Returned (" << errno << ") " << strerror(errno) << std::endl;
		throw("[SPI] low level error!");
	}
}