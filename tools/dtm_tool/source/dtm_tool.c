/**
 * @file    dtm_tool.c
 * @author  Xiaoyuan (Sean) Ma
 * @date    2025-06-12
 * @brief   Implementation of serial operations to support BLE DTM.
 * @details Just a serial port assistant (since there is no tools like
 * minicom or picocom)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

// For nrf52840 NIC, mounted on ttyS8 in the gateway
// The baud rate is 115200
// And the NIC would return at most 10 bytes when it is in the DTM mode
#define SERIAL_PORT "/dev/ttyS8"
#define BAUD_RATE B115200         // Baud rate
#define MAX_RX_BYTES 10           // Max received bytes

int serial_fd;  // Serial file descriptor

/**
 * @brief Initialization of the serial port
 * @return 0 if everything is OK
 */
int
init_serial() {
  struct termios options;
  speed_t speed = BAUD_RATE;

  // Open the serial port
  serial_fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_NDELAY);
  if(serial_fd == -1)
  {
    perror("[ERR]: Failed to open the serial port");
    return -1;
  }

  // Get current configuration of the serial port
  if(tcgetattr(serial_fd, &options) != 0)
  {
    perror("[ERR]: Failed to get configuration of the serial port");
    close(serial_fd);
    return -1;
  }

  bzero(&options, sizeof(options));

  // Set baud rate
  options.c_cflag |= speed;

  // Set data bit (8 bits), stop bit (1 stop bit), and parity (no parity)
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;

  // Set as general
  options.c_cflag |= CREAD | CLOCAL;

  // Disable soft flow control
  options.c_iflag &= ~(IXON | IXOFF | IXANY);

  // Enable raw input (no process)
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_oflag &= ~OPOST;

  // Set the min. read byte to 1 (to guarantee read() can return
  // immediately)
  options.c_cc[VMIN] = 1;
  // And non-block manner (the timeout is controlled by select)
  options.c_cc[VTIME] = 0;

  // Apply all above configuration
  if(tcsetattr(serial_fd, TCSANOW, &options) != 0)
  {
    perror("[ERR]: Failed to apply configuration to the serial port");
    close(serial_fd);
    return -1;
  }

  return 0;
}
/**
 * @brief Send the command (2 B command)
 * @param hex_str points to a hex string (e.g., 8096) means the command
 * {0x80, 0x96} would be sent
 */
void
send_hex_data(const char *hex_str) {
  unsigned char data[2];
  int bytes_sent;

  if(hex_str == NULL)
  {
    printf("[ERR]: Hex cannot be NULL!\n");
    return;
  }
  if(strlen(hex_str) != 4)
  {
    printf(
      "[ERR]: The input should be a 4 B string to present 2 B HEX (e.g., 8096)\n");
    return;
  }
  if(sscanf(hex_str, "%02hhx%02hhx", &data[0], &data[1]) != 2)
  {
    printf(
      "[ERR]: Failed to convert the string to hex digits. The input should be 2 B HEX.\n");
    return;
  }

  // Send data
  bytes_sent = write(serial_fd, data, 2);
  if(bytes_sent < 0)
  {
    perror("[ERR]: Failed to send");
  }
  else
  {
    printf("[INFO]: Sent %02X %02X\n", data[0], data[1]);
  }
}
/**
 * @brief Receive the response (10 B at most) and timeout is 3 s
 */
void
receive_hex_data() {
  unsigned char buffer[MAX_RX_BYTES];
  fd_set read_fds;
  struct timeval timeout;
  int bytes_read;

  // Timeout is 3 s
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;

  FD_ZERO(&read_fds);
  FD_SET(serial_fd, &read_fds);

  int ret = select(serial_fd + 1, &read_fds, NULL, NULL, &timeout);

  if(ret == -1)
  {
    perror("[ERR]: Failure in select()");
    return;
  }
  else if(ret == 0)
  {
    printf("[INFO]: Receive timeout happened ...\n");
    return;
  }

  // If no timeout or error happens, read the data
  if(FD_ISSET(serial_fd, &read_fds))
  {
    bytes_read = read(serial_fd, buffer, MAX_RX_BYTES);
    if(bytes_read < 0)
    {
      perror("[ERR]: Failed to read from serial port");
    }
    else if(bytes_read > 0)
    {
      printf("[INFO]: Receive (%d B)", bytes_read);
      for(int i = 0; i < bytes_read; i++)
      {
        printf("%02X ", buffer[i]);
      }
      printf("\n");
    }
  }
}
/**
 * @brief The entry function
 */
int
main() {
  char input[5];

  // Initialize the serial port
  if(init_serial() != 0)
  {
    fprintf(stderr, "Failed to initialize serial port\n");
    return EXIT_FAILURE;
  }
  printf(
    "[INFO]: The serial port (%s) has been initialized successfully\n",
    SERIAL_PORT);
#if (BAUD_RATE == B9600)
  printf("[INFO]: Baud rate is 9600\n");
#endif /* BAUD_RATE == B9600 */
#if (BAUD_RATE == B19200)
  printf("[INFO]: Baud rate is 19200\n");
#endif /* BAUD_RATE == B19200 */
#if (BAUD_RATE == B115200)
  printf("[INFO]: Baud rate is 115200\n");
#endif /* BAUD_RATE == B115200 */
#if (BAUD_RATE == B230400)
  printf("[INFO]: Baud rate is 230400\n");
#endif /* BAUD_RATE == B230400 */
#if (BAUD_RATE == B576000)
  printf("[INFO]: Baud rate is 576000\n");
#endif /* BAUD_RATE == B576000 */
#if (BAUD_RATE == B1000000)
  printf("[INFO]: Baud rate is 1000000\n");
#endif /* BAUD_RATE == B1000000 */

  // Main loop
  while(1)
  {
    printf(
      "[INFO]: Please input 2 B HEX digits (e.g., 8096) or 'exit' to exit: ");

_CHECKPOINT:
    if(fgets(input, sizeof(input), stdin) == NULL)
    {
      perror("[ERR]: Failed to get the input");
      break;
    }

    // Ignore "\n"
    if(input[0] == '\n')
    {
      goto _CHECKPOINT;
    }

    printf("[DEBUG]: The input chars: %d %d %d %d %d\n", input[0],
           input[1], input[2], input[3], input[4]);

    // Check "exit"
    if(strcmp(input, "exit") == 0)
    {
      printf("[INFO]: Exit ...\n");
      break;
    }

    // Send data
    send_hex_data(input);
    // And waiting for the response
    receive_hex_data();
  }

  // Close the serial port
  close(serial_fd);

  return EXIT_SUCCESS;
}
