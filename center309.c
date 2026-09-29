//====================================================================================================//
// Serial Port Programming using Win32 API in C                                                       //
// (Write/Reads data from serial port)                                                                //
//====================================================================================================//

//====================================================================================================//
// Serial Port Programming using Win32 API in C                                                       //
// (Handshake: Send 'K' -> Wait for '309\r' -> Poll with 'A' every 2s -> Print Rx as Hex 0xXX)        //
//====================================================================================================//

// CENTER 309 K-type Thermocouple Data Logger Interface
// xiao_laba_cn@yahoo.com
// 2005-OCT-19 testing done.
// source code tree, C:\MinGW\msys\1.0\home\user0\RS232
// uses make to build exe, or
// uses command line to compile,
//		gcc center309.c -Wall -Wextra -o2 -o center309.exe


// 台灣群特科技股份有限公司提供, 304, 309 PROTOCOL OF SERIAL INTERFACE
/*
##    
##    command = 0x4B, ASCII "K", returns 4 bytes model number, 309\r or 304\r
##    command = 0x41, ASCII "A", returns 45 bytes (8x5 + 5 = 45) as follows:
##    
##    "\x02                               frame start, 1 byte
##    "\x80                               status of logger, bit7=Celsi/Faren, bit6=batter low, bit5=Hold, bit4=REL, bit3=T1-T2, bit2:1=Max/Min, bit0=recording
##     \xYY\                              status of logger, bit7=auto_off, bit6:1=not used, bit0=Memory_full
##     \xYY\xYY\xYY\xYY\                  T1_State to T4_State, 4 bytes, not used
##    "\xAA\xAA\xBB\xBB\xCC\xCC\xDD\xDD"  Temprerature, T1 = AAAA, T2=BBBB, T3= CCCC, T4 = DDDD, 4 words; /10 or /1
##    "\x00\x00\x00\x00\x00\x00\x00\x00"  T1_rel to T4_rel, 4 words, high byte first
##    "\x00\x00\x00\x00\x00\x00\x00\x00"  T1_min to T4_min, 4 words, high byte first
##    "\x00\x00\x00\x00\x00\x00\x00\x00"  T1_max to T4_max, 4 words, high byte first
##    "\x00                               40th byte, Channel_OL_set, bit3:0=T4-T1
##    "\x00                               41th byte, Rel_OL_set, bit3:0=T4-T1                                
##    "\x00                               42th byte, Max_OL_set, bit3:0=T4-T1
##    "\x00                               43th byte, Min_OL_set, bit3:0=T4-T1
##    "\x0E                               44th byte, Channel_X1_X10, bit3:0=T4-T1, x1 or x10
##    "\x03                               45th byte, frame end, 1 byte
##    
*/


#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <string.h>

#define BLOCK_SIZE 45

// Function prototype for parsing the 45-byte frame
void ProcessTemperatureFrame(const unsigned char *frame, DWORD len);


int main(int argc, char *argv[])
{
    HANDLE hComm;                           // Handle to the Serial port
    char   ComPortName[32];                 // Formatted COM port path string
    BOOL   Status;                          // Status of operations 
    char   TempChar;                        // Received character
    DWORD  NoBytesRead;                     // Bytes read by ReadFile()
    DWORD  BytesWritten = 0;                // Bytes written by WriteFile()
    
    char   RxBuffer[256] = {0};             // Sliding window buffer for trigger matching
    size_t rxLen = 0;
    BOOL   handshakeComplete = FALSE;



    printf("\n +=============================================================+");
    printf("\n |    CENTER 309 K-type Thermocouple Data Logger Interface     |");
    printf("\n |    by xiao_laba_cn@yahoo.com, 2005                          |");	
    printf("\n +=============================================================+\n");

    /*------------------------------------ Parse Command Line Argument ---------------------------------------*/




	// Extract base filename from argv[0]
    const char *exeName = strrchr(argv[0], '\\');
    if (!exeName) {
        exeName = strrchr(argv[0], '/');
    }
    exeName = (exeName) ? exeName + 1 : argv[0];

    if (argc < 2)
    {
        printf("\n Error: Missing COM port number argument!");
        printf("\n Usage: %s <port_number>   (e.g., '%s 12' for COM12)\n", exeName, exeName);
        return 1;
    }

    int portNum = atoi(argv[1]);
    if (portNum <= 0)
    {
        printf("\n Error: Invalid COM port number '%s'\n", argv[1]);
        return 1;
    }

    // Format full Win32 device namespace string (e.g., \\.\COM12)
    snprintf(ComPortName, sizeof(ComPortName), "\\\\.\\COM%d", portNum);
/*
    printf("\n +=============================================================+");
    printf("\n |    CENTER 309 K-type Thermocouple Data Logger Interface     |");
    printf("\n +=============================================================+\n");
	
*/

    /*---------------------------------- Opening the Serial Port -------------------------------------------*/

    hComm = CreateFile( ComPortName,                  // Name of Port
                        GENERIC_READ | GENERIC_WRITE, // Read/Write Access
                        0,                            // No Sharing
                        NULL,                         // No Security
                        OPEN_EXISTING,                // Open existing port only
                        0,                            // Non-Overlapped I/O
                        NULL);                        // Null for Comm Devices

    if (hComm == INVALID_HANDLE_VALUE)
    {
        printf("\n    Error! - Port %s (COM%d) can't be opened\n", ComPortName, portNum);
        return 1;
    }
    printf("\n    Port %s Opened Successfully\n", ComPortName);

    /*------------------------------- Setting DCB Parameters ------------------------------------------------*/

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hComm, &dcbSerialParams))
        printf("\n    Error! in GetCommState()");

    dcbSerialParams.BaudRate = CBR_9600;      // 9600 Baud
    dcbSerialParams.ByteSize = 8;             // 8 Data Bits
    dcbSerialParams.StopBits = ONESTOPBIT;    // 1 Stop Bit
    dcbSerialParams.Parity   = NOPARITY;      // No Parity

    if (!SetCommState(hComm, &dcbSerialParams))
    {
        printf("\n    Error! in Setting DCB Structure");
    }
    else
    {
        printf("\n    DCB Configured (9600 8N1)\n");
    }

    /*------------------------------------ Setting Timeouts --------------------------------------------------*/

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = 1000; // 1 second timeout
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 100;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    SetCommTimeouts(hComm, &timeouts);

    // Flush port to clear stale noise
    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);

    /*------------------------------------ Step 1: Send 'K' First --------------------------------------------*/

    char txSingle = 'K';
    Status = WriteFile(hComm, &txSingle, 1, &BytesWritten, NULL);
    if (Status && BytesWritten == 1)
    {
        printf("\n -> Issued initial command: '%c' (0x%02X)", txSingle, (unsigned char)txSingle);
        printf("\n <- Waiting for \"309\\r\" handshake from device...\n\n");
    }
    else
    {
        printf("\n Error sending initial 'K'\n");
    }

    /*------------------------------------ Phase 1: Wait for "309\r" Trigger -----------------------------------*/

    while (!handshakeComplete)
    {
        if (_kbhit() && _getch() == 27) // ESC to exit
            goto CLEANUP;

        Status = ReadFile(hComm, &TempChar, 1, &NoBytesRead, NULL);
        if (Status && NoBytesRead > 0)
        {
            unsigned char uChar = (unsigned char)TempChar;
            printf("0x%02X ", uChar);
            fflush(stdout);

            if (rxLen < sizeof(RxBuffer) - 1)
            {
                RxBuffer[rxLen++] = TempChar;
                RxBuffer[rxLen] = '\0';
            }
            else
            {
                memmove(RxBuffer, RxBuffer + 1, sizeof(RxBuffer) - 2);
                RxBuffer[sizeof(RxBuffer) - 2] = TempChar;
                RxBuffer[sizeof(RxBuffer) - 1] = '\0';
            }

            // Look for "309\r" or "309"
            if (strstr(RxBuffer, "309\r") != NULL || strstr(RxBuffer, "309\n") != NULL)
            {
                handshakeComplete = TRUE;
                printf("\n\n*** Trigger \"309\\r\" Received! Starting polling loop. ***\n");

				printf("\n\n*** Press ESC to exit ***\n");
            }
        }
    }

    /*------------------------------------ Phase 2: Query 'A' & Parse Temperatures ---------------------------*/

    char          cmdA = 'A'; // Single byte 'A' (0x41) command
    unsigned char rxBlockBuffer[BLOCK_SIZE];

    while (1)
    {
        if (_kbhit() && _getch() == 27) // ESC to exit
            break;


				
        // 1. Send single 1-byte 'A' (0x41) command
        Status = WriteFile(hComm, &cmdA, 1, &BytesWritten, NULL);

        if (Status && BytesWritten == 1)
        {
            // 2. Read exact 45-byte frame from device
            DWORD totalBytesRead = 0;
            DWORD bytesRead = 0;

            // Loop read until full 45-byte payload is collected or timeout
            while (totalBytesRead < BLOCK_SIZE)
            {
                Status = ReadFile(hComm, rxBlockBuffer + totalBytesRead, 
                                  BLOCK_SIZE - totalBytesRead, &bytesRead, NULL);
                if (!Status || bytesRead == 0)
                {
                    break; // Read timeout or connection error
                }
                totalBytesRead += bytesRead;
            }

            if (totalBytesRead == BLOCK_SIZE)
            {
                // 3. Unpack and print parsed T1, T2, T3, T4 values
                ProcessTemperatureFrame(rxBlockBuffer, totalBytesRead);
            }
            else
            {
                printf(" [Read Error: Expected 45 Bytes, Received %lu Bytes]\n", totalBytesRead);
            }
            fflush(stdout);
        }
        else
        {
            printf("\n Error! Failed to write 'A' command\n");
        }

        // Wait 2 seconds between polling intervals
        Sleep(1000);
    }

CLEANUP:
    CloseHandle(hComm);
    printf("\n +==========================================+\n");
    printf(" COM Port closed. Exiting program.\n");
    return 0;
}



/*------------------------------------ Helper: Unpack 45-Byte Protocol Frame -----------------------------------*/

void ProcessTemperatureFrame(const unsigned char *frame, DWORD len)
{
    if (len < 45 || frame[0] != 0x02 || frame[44] != 0x03)
    {
        printf(" [Frame Framing Error: Missing STX(0x02) or ETX(0x03)]\n");
        return;
    }

    // Byte 1 Bit 7: 1 = Celsius, 0 = Fahrenheit
    char unit = (frame[1] & 0x80) ? 'C' : 'F';

    // Bytes 7..14 contain T1..T4 raw Big-Endian word readings
    short rawT1 = (short)((frame[7]  << 8) | frame[8]);
    short rawT2 = (short)((frame[9]  << 8) | frame[10]);
    short rawT3 = (short)((frame[11] << 8) | frame[12]);
    short rawT4 = (short)((frame[13] << 8) | frame[14]);

    // Byte 39 (0-indexed) contains Overload/Disconnected flags (Bit 0=T1, Bit 1=T2, Bit 2=T3, Bit 3=T4)
    unsigned char olFlags = frame[39];

    printf(" Teperature Reading -> ");

    // Parse T1
    if (olFlags & 0x01) printf("T1:  OL   | ");
    else                printf("T1: %5.1f%c | ", rawT1 / 10.0, unit);

    // Parse T2
    if (olFlags & 0x02) printf("T2:  OL   | ");
    else                printf("T2: %5.1f%c | ", rawT2 / 10.0, unit);

    // Parse T3
    if (olFlags & 0x04) printf("T3:  OL   | ");
    else                printf("T3: %5.1f%c | ", rawT3 / 10.0, unit);

    // Parse T4
    if (olFlags & 0x08) printf("T4:  OL\n");
    else                printf("T4: %5.1f%c\n", rawT4 / 10.0, unit);
}