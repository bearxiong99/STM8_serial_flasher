/**
  \file serial_comm.c
   
  \author G. Icking-Konert
  \date 2008-11-02
  \version 0.1
   
  \brief implementation of RS232 comm port routines
   
  implementation of of routines for RS232 communication using the Win32 or Posix API.
  For Win32, see e.g. http://msdn.microsoft.com/en-us/library/default.aspx
  For Posix see http://www.easysw.com/~mike/serial/serial.html
  
*/

// include files
#include "serial_comm.h"
#include "misc.h"
#include "globals.h"


/**
  \fn void list_ports(void)
   
  \brief print list all available comm ports
   
  print list of all available COM ports. I don't know how to do this in
  a better way than to actually try and open each of them...
*/
void list_ports(void) {
  
/////////
// Win32
/////////
#if defined(WIN32)

  HANDLE        fpCom = NULL;
  uint16_t      i, j;
  char          port_tmp[100];
    
  // loop 1..255 over ports and list each available
  j=1;
  for (i=1; i<=255; i++) {

    // required to allow COM ports >COM9
    sprintf(port_tmp,"\\\\.\\COM%d", i);    
  
    // try to open COM-port
    fpCom = CreateFile(port_tmp,
      GENERIC_READ | GENERIC_WRITE,  // both read & write
      0,    // must be opened with exclusive-access
      NULL, // no security attributes
      OPEN_EXISTING, // must use OPEN_EXISTING
      0,    // not overlapped I/O
      NULL  // hTemplate must be NULL for comm devices
    );
    if (fpCom != INVALID_HANDLE_VALUE) {
      if (j!=1) printf(", ");
      printf("COM%d", i);
      CloseHandle(fpCom);
      j++;
    }
  }
  
#endif // WIN32


/////////
// Posix
/////////
#if defined(__APPLE__) || defined(__unix__) 

  // list all /dev/tty.usbserial-* (see http://bytes.com/groups/net-vc/545618-list-files-current-directory)
  uint16_t        i;
  struct dirent   *ent;
  DIR             *dir = opendir("/dev");
  if(dir) {
    i = 1;
    while((ent = readdir(dir)) != NULL) {
      
      // FTDI FT232 based USB-RS232 adapter (MacOS X)
      if (strstr(ent->d_name, "tty.usbserial")) {
        if (i!=1) printf(", ");
        printf("/dev/%s", ent->d_name);
        i++;
      }

      // Prolific PL2303 based USB-RS232 adapter
      if (strstr(ent->d_name, "tty.PL2303")) {
        if (i!=1) printf(", ");
        printf("/dev/%s", ent->d_name);
        i++;
      }
      
      // FTDI FT232 based USB-RS232 adapter (Ubuntu)
      if (strstr(ent->d_name, "ttyUSB")) {
        if (i!=1) printf(", ");
        printf("/dev/%s", ent->d_name);
        i++;
      }

    }
  }
  else {
    fprintf(stderr, "cannot list, see /dev/tty*");
  }
  fflush(stdout);

#endif // __APPLE__ || __unix__

} // list_ports



/**
  \fn HANDLE init_port(const char *port, uint32_t baudrate, uint32_t timeout, uint8_t numBits, uint8_t parity, uint8_t numStop, uint8_t RTS, uint8_t DTR)
   
  \brief open comm port
   
  \param[in] port       name of port as string
  \param[in] baudrate   comm port speed in Baud
  \param[in] timeout    timeout between chars in ms
  \param[in] numBits    number of data bits per byte
  \param[in] parity     parity control by HW (0=none, 1=odd, 2=even)
  \param[in] numStop    number of stop bits (1=1; 2=2; other=1.5)
  \param[in] RTS        Request To Send (required for some multimeter optocouplers)
  \param[in] DTR        Data Terminal Ready (required for some multimeter optocouplers)

  \return           handle to comm port
  
  open comm port for communication, set properties (baudrate, timeout,...). 
  Note: baudrate must be supported by COM port driver. 
*/
HANDLE init_port(const char *port, uint32_t baudrate, uint32_t timeout, uint8_t numBits, uint8_t parity, uint8_t numStop, uint8_t RTS, uint8_t DTR) {

  
/////////
// Win32
/////////
#if defined(WIN32)

  char          port_tmp[100];
  HANDLE        fpCom = NULL;
  DCB           fDCB;
  BOOL          fSuccess;
  COMMTIMEOUTS  fTimeout;
    
  // required to allow COM ports >COM9
  sprintf(port_tmp,"\\\\.\\%s", port);    
  
  // create handle to COM-port
  fpCom = CreateFile(port_tmp,
    GENERIC_READ | GENERIC_WRITE,  // both read & write
    0,    // must be opened with exclusive-access
    NULL, // no security attributes
    OPEN_EXISTING, // must use OPEN_EXISTING
    0,    // not overlapped I/O
    NULL  // hTemplate must be NULL for comm devices
  );
  if (fpCom == INVALID_HANDLE_VALUE) {
    fpCom = NULL;
    fprintf(stderr, "\n\nerror in 'init_port(%s)': open port failed with code %d, exit!\n\n", port, (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }

  // reset COM port error buffer
  PurgeComm(fpCom, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
  
  // get the current port configuration
  fSuccess = GetCommState(fpCom, &fDCB);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'init_port(%s)': get port attributes failed with code %d, exit!\n\n", port, (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }

  // change port settings
  fDCB.BaudRate = baudrate;         // set the baud rate (19200, 57600, 115200)
  fDCB.ByteSize = numBits;          // number of data bits per byte
  if (parity) {
    fDCB.fParity = TRUE;            // Enable parity checking
    fDCB.Parity  = parity;          // 0-4=no,odd,even,mark,space
  }
  else {
    fDCB.fParity  = FALSE;          // disable parity checking
    fDCB.Parity   = NOPARITY;       // just to make sure
  }
  if (numStop == 1) 
    fDCB.StopBits = ONESTOPBIT;     // one stop bit
  else if (numStop == 2) 
    fDCB.StopBits = TWOSTOPBITS;    // two stop bit
  else
    fDCB.StopBits = ONE5STOPBITS;   // 1.5 stop bit
  fDCB.fRtsControl = (RTS != 0);    // RTS off(=0=-12V) or on(=1=+12V)
  fDCB.fDtrControl = (DTR != 0);    // DTR off(=0=-12V) or on(=1=+12V)

  // set new COM state
  fSuccess = SetCommState(fpCom, &fDCB);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'init_port(%s)': set port attributes failed with code %d, exit!\n\n", port, (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }


  // set timeouts for port to avoid hanging of program. For simplicity set all timeouts to same value.
  // For timeout=0 set values to query for buffer content
  if (timeout == 0)
    fTimeout.ReadIntervalTimeout        = MAXDWORD;    // --> no read timeout
  else
    fTimeout.ReadIntervalTimeout        = 0;           // max. ms between following read bytes (0=not used)
  fTimeout.ReadTotalTimeoutMultiplier   = 0;           // time per read byte (use contant timeout instead)
  fTimeout.ReadTotalTimeoutConstant     = timeout;     // total read timeout in ms
  fTimeout.WriteTotalTimeoutMultiplier  = 0;           // time per write byte (use contant timeout instead) 
  fTimeout.WriteTotalTimeoutConstant    = timeout;
  fSuccess = SetCommTimeouts(fpCom, &fTimeout);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'init_port(%s)': set port timeout failed with code %d, exit!\n\n", port, (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }

  // return hande
  return(fpCom);

#endif // WIN32


/////////
// Posix
/////////
#if defined(__APPLE__) || defined(__unix__) 

  HANDLE          fpCom;
  struct termios  toptions;
  int             status;
  
    
  // open port
  fpCom = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
  if (fpCom == -1) {
    fpCom = 0;
    fprintf(stderr, "\n\nerror in 'init_port(%s)': open port failed, exit!\n\n", port);
    Exit(1, g_pauseOnExit);
  }
  
  // get attributes
  if (tcgetattr(fpCom, &toptions) < 0) {
    fprintf(stderr, "\n\nerror in 'init_port(%s)': get port attributes failed, exit!\n\n", port);
    Exit(1, g_pauseOnExit);
  }

  // set baudrate
  speed_t brate = baudrate; // let you override switch below if needed
  switch(baudrate) {
    case 4800:   brate=B4800;   break;
    case 9600:   brate=B9600;   break;
#ifdef B14400
    case 14400:  brate=B14400;  break;
#endif
    case 19200:  brate=B19200;  break;
#ifdef B28800
    case 28800:  brate=B28800;  break;
#endif
    case 38400:  brate=B38400;  break;
    case 57600:  brate=B57600;  break;
    case 115200: brate=B115200; break;
  }
  cfsetispeed(&toptions, brate);    // receive
  cfsetospeed(&toptions, brate);    // send

  if (numBits == 7)
    toptions.c_cflag |=  CS7;       // 8 data bits
  else
    toptions.c_cflag |=  CS8;       // 8 data bits
  if (parity == 0)
    toptions.c_cflag &= ~PARENB;    // 0=no parity
  else if (parity==1) {             // 1=odd parity
    toptions.c_cflag |=  PARENB;
    toptions.c_cflag |=  PARODD;
  }
  else if (parity==2) {             // even parity
    toptions.c_cflag |=  PARENB;
    toptions.c_cflag &=  ~PARODD;
  }
  if (numStop == 1) 
    toptions.c_cflag &= ~CSTOPB;    // one stop bit
  else
    toptions.c_cflag |= CSTOPB;     // two stop bit
  toptions.c_cflag &= ~CSIZE;       // clear data bits entry

  // disable flow control
  toptions.c_cflag &= ~CRTSCTS;
  toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
  toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl

  // make raw
  toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
  toptions.c_oflag &= ~OPOST; // make raw

  // set timeout (see: http://unixwiz.net/techtips/termios-vmin-vtime.html)
  toptions.c_cc[VMIN]  = 255;
  toptions.c_cc[VTIME] = timeout/100;   // convert ms to 0.1s
    
  // set term properties
  if (tcsetattr(fpCom, TCSANOW, &toptions) < 0) {
    fprintf(stderr, "\n\nerror in 'init_port(%s)': set port attributes failed, exit!\n\n", port);
    Exit(1, g_pauseOnExit);
  }


  // set static RTS and DTR status (required for some multimeter optocouplers)
  ioctl(fpCom, TIOCMGET, &status);
  if (RTS==1)
    status |= TIOCM_RTS;
  else
    status &= ~TIOCM_RTS;
  if (DTR==1)
    status |= TIOCM_DTR;
  else
    status &= ~TIOCM_DTR;
  if (ioctl(fpCom, TIOCMSET, &status)) {
    fprintf(stderr, "\n\nerror in 'init_port(%s)': cannot set RTS status, exit!\n\n", port);
    Exit(1, g_pauseOnExit);
  }

  // return comm port handle
  return fpCom;

#endif // __APPLE__ || __unix__

} // init_port



/**
  \fn void close_port(HANDLE *fpCom)
   
  \brief close comm port
  
  \param[in] fpCom    handle to comm port

  close & release comm port. 
*/
void close_port(HANDLE *fpCom) {

/////////
// Win32
/////////
#if defined(WIN32)

  BOOL      fSuccess;

  if (*fpCom != NULL) {
    fSuccess = CloseHandle(*fpCom);
    if (!fSuccess) {
      *fpCom = NULL;
      fprintf(stderr, "\n\nerror in 'close_port': close port failed with code %d, exit!\n\n", (int) GetLastError());
      Exit(1, g_pauseOnExit);
    }
  }
  *fpCom = NULL;

#endif // WIN32


/////////
// Posix
/////////
#if defined(__APPLE__) || defined(__unix__) 

  if (*fpCom != 0) {
    if (close(*fpCom) != 0) {
      *fpCom = 0;
      fprintf(stderr, "\n\nerror in 'close_port': close port failed, exit!\n\n");
      Exit(1, g_pauseOnExit);
    }
  }
  *fpCom = 0;

#endif // __APPLE__ || __unix__

} // close_port



/**
  \fn void get_port_attribute(HANDLE fpCom, uint32_t *baudrate, uint32_t *timeout, uint8_t *numBits, uint8_t *parity, uint8_t *numStop, uint8_t *RTS, uint8_t *DTR)
   
  \brief get comm port settings
   
  \param[in]  fpCom      handle to comm port
  \param[out] baudrate   comm port speed in Baud
  \param[out] timeout    timeout between chars in ms
  \param[out] numBits    number of data bits per byte
  \param[out] parity     parity control by HW
  \param[out] numStop    number of stop bits
  \param[out] RTS        Request To Send (required for some multimeter optocouplers)
  \param[out] DTR        Data Terminal Ready (required for some multimeter optocouplers)

  get current attributes of an already open comm port.
*/
void get_port_attribute(HANDLE fpCom, uint32_t *baudrate, uint32_t *timeout, uint8_t *numBits, uint8_t *parity, uint8_t *numStop, uint8_t *RTS, uint8_t *DTR) {

/////////
// Win32
/////////
#ifdef WIN32

  DCB           fDCB;
  COMMTIMEOUTS  fTimeout;
  BOOL          fSuccess;

  // get the current port configuration
  fSuccess = GetCommState(fpCom, &fDCB);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'get_port_attribute': GetCommState() failed with code %d, exit!\n\n", (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }

  // get port settings
  *baudrate = fDCB.BaudRate;        // baud rate (19200, 57600, 115200)
  *numBits  = fDCB.ByteSize;        // number of data bits per byte
  *parity   = fDCB.Parity;          // parity bit by HW
  *numStop  = fDCB.StopBits;        // number of stop bits
  if (fDCB.StopBits == ONESTOPBIT) 
    *numStop = 1;                      // 1 stop bit
  else if (fDCB.StopBits == TWOSTOPBITS) 
    *numStop = 2;                      // 2 stop bits
  else
    *numStop = 3;                      // 1.5 stop bits
  *RTS      = fDCB.fRtsControl;     // RTS off(=0=-12V) or on(=1=+12V)
  *DTR      = fDCB.fDtrControl;     // DTR off(=0=-12V) or on(=1=+12V)
  
  // get port timeout
  fSuccess = GetCommTimeouts(fpCom, &fTimeout);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'get_port_attribute': GetCommTimeouts() failed with code %d, exit!\n\n", (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }
  *timeout = fTimeout.ReadTotalTimeoutConstant;       // this parameter fits also for timeout=0
  
#endif // WIN32


/////////
// Posix
/////////
#if defined(__APPLE__) || defined(__unix__) 

  struct termios  toptions;
  int             status;
  
  
  // get attributes
  if (tcgetattr(fpCom, &toptions) < 0) {
    fprintf(stderr, "\n\nerror in 'get_port_attribute': get port attributes failed, exit!\n\n");
    Exit(1, g_pauseOnExit);
  }
  
  
  // get baudrate
  speed_t brate = cfgetospeed(&toptions);
  switch (brate) {
    case B4800:    *baudrate = 4800;    break;
    case B9600:    *baudrate = 9600;    break;
#ifdef B14400
    case B14400:   *baudrate = 14400;   break;
#endif
    case B19200:   *baudrate = 19200;   break;
#ifdef B28800
    case B28800:   *baudrate = 28800;   break;
#endif
    case B38400:   *baudrate = 38400;   break;
    case B57600:   *baudrate = 57600;   break;
    case B115200:  *baudrate = 115200;  break;

    default: *baudrate = UINT32_MAX;
    
  } // switch (brate)
  
  
  // get timeout (see: http://unixwiz.net/techtips/termios-vmin-vtime.html)
  *timeout = toptions.c_cc[VTIME] * 100;   // convert 0.1s to ms
  
  
  // number of bits
  if (toptions.c_cflag & CS8)
    *numBits = 8;
  else
    *numBits = 7;
  
  
  // get parity
  if (toptions.c_cflag & PARENB)
    *parity = 1;
  else
    *parity = 0;
  
  
  // get number of stop bits
  if (toptions.c_cflag | CSTOPB)
    *numStop = 2;
  else
    *numStop = 1;
  

  // get static RTS and DTR status (required for some multimeter optocouplers)
  ioctl(fpCom, TIOCMGET, &status);
  if (status | TIOCM_RTS)
    *RTS = 1;
  else
    *RTS = 0;
  if (status | TIOCM_DTR)
    *DTR = 1;
  else
    *DTR = 0;

#endif // __APPLE__ || __unix__
     
} // get_port_attribute



/**
  \fn void set_port_attribute(HANDLE fpCom, uint32_t baudrate, uint32_t timeout, uint8_t numBits, uint8_t parity, uint8_t numStop, uint8_t RTS, uint8_t DTR)
   
  \brief modify comm port settings
   
  \param[in] fpCom      handle to comm port
  \param[in] baudrate   comm port speed in Baud
  \param[in] timeout    timeout between chars in ms
  \param[in] numBits    number of data bits per byte
  \param[in] parity     parity control by HW
  \param[in] numStop    number of stop bits
  \param[in] RTS        Request To Send (required for some multimeter optocouplers)
  \param[in] DTR        Data Terminal Ready (required for some multimeter optocouplers)

  change attributes of an already open comm port.
*/
void set_port_attribute(HANDLE fpCom, uint32_t baudrate, uint32_t timeout, uint8_t numBits, uint8_t parity, uint8_t numStop, uint8_t RTS, uint8_t DTR) {
  
/////////
// Win32
/////////
#ifdef WIN32

  DCB           fDCB;
  BOOL          fSuccess;
  COMMTIMEOUTS  fTimeout;
  
  // reset COM port error buffer
  PurgeComm(fpCom, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
  
  // get the current port configuration
  fSuccess = GetCommState(fpCom, &fDCB);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'set_port_attribute()': get port attributes failed with code %d, exit!\n\n", (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }

  // change port settings
  fDCB.BaudRate = baudrate;         // set the baud rate (19200, 57600, 115200)
  fDCB.ByteSize = numBits;          // number of data bits per byte
  if (parity) {
    fDCB.fParity = TRUE;            // Enable parity checking
    fDCB.Parity  = parity;          // 0-4=no,odd,even,mark,space
  }
  else {
    fDCB.fParity  = FALSE;          // disable parity checking
    fDCB.Parity   = NOPARITY;       // just to make sure
  }
  if (numStop == 1) 
    fDCB.StopBits = ONESTOPBIT;     // one stop bit
  else if (numStop == 2) 
    fDCB.StopBits = TWOSTOPBITS;    // two stop bit
  else
    fDCB.StopBits = ONE5STOPBITS;   // 1.5 stop bit
  fDCB.fRtsControl = (RTS != 0);    // RTS off(=0=-12V) or on(=1=+12V)
  fDCB.fDtrControl = (DTR != 0);    // DTR off(=0=-12V) or on(=1=+12V)

  // set new COM state
  fSuccess = SetCommState(fpCom, &fDCB);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'set_port_attribute()': set port attributes failed with code %d, exit!\n\n", (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }


  // set timeouts for port to avoid hanging of program. For simplicity set all timeouts to same value.
  // For timeout=0 set values to query for buffer content
  if (timeout == 0)
    fTimeout.ReadIntervalTimeout        = MAXDWORD;    // --> no read timeout
  else
    fTimeout.ReadIntervalTimeout        = 0;           // max. ms between following read bytes (0=not used)
  fTimeout.ReadTotalTimeoutMultiplier   = 0;           // time per read byte (use contant timeout instead)
  fTimeout.ReadTotalTimeoutConstant     = timeout;     // total read timeout in ms
  fTimeout.WriteTotalTimeoutMultiplier  = 0;           // time per write byte (use contant timeout instead) 
  fTimeout.WriteTotalTimeoutConstant    = timeout;
  fSuccess = SetCommTimeouts(fpCom, &fTimeout);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'set_port_attribute()': set port timeout failed with code %d, exit!\n\n", (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }

#endif // WIN32


/////////
// Posix
/////////
#if defined(__APPLE__) || defined(__unix__) 

  struct termios  toptions;
  int             status;
  
  
  // get attributes
  if (tcgetattr(fpCom, &toptions) < 0) {
    fprintf(stderr, "\n\nerror in 'set_port_attribute()': get port attributes failed, exit!\n\n");
    Exit(1, g_pauseOnExit);
  }
  
  // set baudrate
  speed_t brate = baudrate; // let you override switch below if needed
  switch(baudrate) {
    case 4800:   brate=B4800;   break;
    case 9600:   brate=B9600;   break;
#ifdef B14400
    case 14400:  brate=B14400;  break;
#endif
    case 19200:  brate=B19200;  break;
#ifdef B28800
    case 28800:  brate=B28800;  break;
#endif
    case 38400:  brate=B38400;  break;
    case 57600:  brate=B57600;  break;
    case 115200: brate=B115200; break;
  }
  cfsetispeed(&toptions, brate);    // receive
  cfsetospeed(&toptions, brate);    // send
  
  if (numBits == 7)
    toptions.c_cflag |=  CS7;       // 8 data bits
  else
    toptions.c_cflag |=  CS8;       // 8 data bits
  toptions.c_cflag &= ~PARENB;      // no parity
  //toptions.c_cflag |=  PARENB;    // with parity
  if (numStop == 1)
    toptions.c_cflag &= ~CSTOPB;    // one stop bit
  else
    toptions.c_cflag |= CSTOPB;     // two stop bit
  toptions.c_cflag &= ~CSIZE;       // clear data bits entry
  
  // disable flow control
  toptions.c_cflag &= ~CRTSCTS;
  toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
  toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
  
  // make raw
  toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
  toptions.c_oflag &= ~OPOST; // make raw
  
  // set timeout (see: http://unixwiz.net/techtips/termios-vmin-vtime.html)
  toptions.c_cc[VMIN]  = 255;
  toptions.c_cc[VTIME] = timeout/100;   // convert ms to 0.1s
  
  // set term properties
  if (tcsetattr(fpCom, TCSANOW, &toptions) < 0) {
    fprintf(stderr, "\n\nerror in 'set_port_attribute()': set port attributes failed, exit!\n\n");
    Exit(1, g_pauseOnExit);
  }
  
  
  // set static RTS and DTR status (required for some multimeter optocouplers)
  ioctl(fpCom, TIOCMGET, &status);
  if (RTS==1)
    status |= TIOCM_RTS;
  else
    status &= ~TIOCM_RTS;
  if (DTR==1)
    status |= TIOCM_DTR;
  else
    status &= ~TIOCM_DTR;
  if (ioctl(fpCom, TIOCMSET, &status)) {
    fprintf(stderr, "\n\nerror in 'set_port_attribute()': cannot set RTS status, exit!\n\n");
    Exit(1, g_pauseOnExit);
  }
  
#endif // __APPLE__ || __unix__

     
} // set_port_attribute



/**
  \fn void set_baudrate(HANDLE fpCom, uint32_t baudrate)
   
  \brief modify comm port baudrate
   
  \param[in] fpCom      handle to comm port
  \param[in] baudrate   new comm port speed in Baud

  set new baudrate for an already open comm port. Baudrate must be supported by PC driver.
*/
void set_baudrate(HANDLE fpCom, uint32_t baudrate) {
  
/////////
// Win32
/////////
#ifdef WIN32

  DCB       fDCB;
  BOOL      fSuccess;

  // get the current port configuration
  fSuccess = GetCommState(fpCom, &fDCB);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'set_baudrate(%d)': get port attributes failed with code %d, exit!\n\n", (int) baudrate, (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }

  // change port settings
  fDCB.BaudRate = baudrate;     // set the baud rate (19200, 57600, 115200)
  fSuccess = SetCommState(fpCom, &fDCB);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'set_baudrate(%d)': set port attributes failed with code %d, exit!\n\n", (int) baudrate, (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }

#endif // WIN32


/////////
// Posix
/////////
#if defined(__APPLE__) || defined(__unix__) 

  struct termios  toptions;
    
  // get attributes
  if (tcgetattr(fpCom, &toptions) < 0) {
    fprintf(stderr, "\n\nerror in 'set_baudrate(%d)': get port attributes failed, exit!\n\n", (int) baudrate);
    Exit(1, g_pauseOnExit);
  }

  // set baudrate
  speed_t brate = baudrate; // let you override switch below if needed
  switch(baudrate) {
    case 4800:   brate=B4800;   break;
    case 9600:   brate=B9600;   break;
#ifdef B14400
    case 14400:  brate=B14400;  break;
#endif
    case 19200:  brate=B19200;  break;
#ifdef B28800
    case 28800:  brate=B28800;  break;
#endif
    case 38400:  brate=B38400;  break;
    case 57600:  brate=B57600;  break;
    case 115200: brate=B115200; break;
  }
  cfsetispeed(&toptions, brate);    // receive
  cfsetospeed(&toptions, brate);    // send

  // set term properties
  if( tcsetattr(fpCom, TCSANOW, &toptions) < 0) {
    fprintf(stderr, "\n\nerror in 'set_baudrate(%d)': set port attributes failed, exit!\n\n", (int) baudrate);
    Exit(1, g_pauseOnExit);
  }

#endif // __APPLE__ || __unix__

} // set_baudrate



/**
  \fn void set_timeout(HANDLE fpCom, uint32_t timeout)
   
  \brief modify comm port timeout
   
  \param[in] fpCom      handle to comm port
  \param[in] timeout    new timeout in ms

  set new timeout for an already open comm port
*/
void set_timeout(HANDLE fpCom, uint32_t timeout) {

/////////
// Win32
/////////
#ifdef WIN32

  BOOL          fSuccess;
  COMMTIMEOUTS  fTimeout;

  // set timeouts for port to avoid hanging of program. For simplicity set all timeouts to same value.
  // For timeout=0 set values to query for buffer content
  if (timeout == 0)
    fTimeout.ReadIntervalTimeout        = MAXDWORD;    // --> no read timeout
  else
    fTimeout.ReadIntervalTimeout        = 0;           // max. ms between following read bytes (0=not used)
  fTimeout.ReadTotalTimeoutMultiplier   = 0;           // time per read byte (use contant timeout instead)
  fTimeout.ReadTotalTimeoutConstant     = timeout;     // total read timeout in ms
  fTimeout.WriteTotalTimeoutMultiplier  = 0;           // time per write byte (use contant timeout instead) 
  fTimeout.WriteTotalTimeoutConstant    = timeout;
  fSuccess = SetCommTimeouts(fpCom, &fTimeout);
  if (!fSuccess) {
    fprintf(stderr, "\n\nerror in 'set_timeout(%d)': set timeout failed with code %d, exit!\n\n", (int) timeout, (int) GetLastError());
    Exit(1, g_pauseOnExit);
  }

#endif // WIN32


/////////
// Posix
/////////
#if defined(__APPLE__) || defined(__unix__) 

  struct termios  toptions;
    
  // get attributes
  if (tcgetattr(fpCom, &toptions) < 0) {
    fprintf(stderr, "\n\nerror in 'set_timeout(%d)': get port attributes failed, exit!\n\n", (int) timeout);
    Exit(1, g_pauseOnExit);
  }

  // set timeout (see: http://unixwiz.net/techtips/termios-vmin-vtime.html)
  toptions.c_cc[VMIN]  = 255;
  toptions.c_cc[VTIME] = timeout/100;   // convert ms to 0.1s
    
  // set term properties
  if( tcsetattr(fpCom, TCSANOW, &toptions) < 0) {
    fprintf(stderr, "\n\nerror in 'set_timeout(%d)': set port attributes failed, exit!\n\n", (int) timeout);
    Exit(1, g_pauseOnExit);
  }

#endif // __APPLE__ || __unix__

} // set_timeout



/**
  \fn uint32_t send_port(HANDLE fpCom, uint32_t lenTx, char *Tx)
   
  \brief send data via comm port
  
  \param[in] fpCom    handle to comm port
  \param[in] lenTx    number of bytes to send
  \param[in] Tx       array of bytes to send

  \return         number of sent bytes
  
  send data via comm port. Use this function to facilitate serial communication
  on different platforms, e.g. Win32 and Posix
*/
uint32_t send_port(HANDLE fpCom, uint32_t lenTx, char *Tx) {
  
/////////
// Win32
/////////
#ifdef WIN32

  DWORD   numChars;
  
  // send data & return number of sent bytes
  PurgeComm(fpCom, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
  WriteFile(fpCom, Tx, lenTx, &numChars, NULL);
  return((uint32_t) numChars);

#endif // WIN32


/////////
// Posix
/////////
#if defined(__APPLE__) || defined(__unix__) 

  uint32_t  numChars;
  
  // send data & return number of sent bytes
  numChars = write(fpCom, Tx, lenTx);
  return(numChars);

#endif // __APPLE__ || __unix__

} // send_port



/**
  \fn uint32_t receive_port(HANDLE fpCom, uint32_t lenRx, char *Rx)
   
  \brief receive data via comm port
  
  \param[in]  fpCom   handle to comm port
  \param[in]  lenRx   number of bytes to receive
  \param[out] Rx      array containing bytes received
  
  \return         number of received bytes
  
  receive data via comm port. Use this function to facilitate serial communication
  on different platforms, e.g. Win32 and Posix
*/
uint32_t receive_port(HANDLE fpCom, uint32_t lenRx, char *Rx) {
  
/////////
// Win32
/////////
#ifdef WIN32

  DWORD   numChars;
  
  // receive data & return number of received bytes
  ReadFile(fpCom, Rx, lenRx, &numChars, NULL);
  return((uint32_t) numChars);

#endif // WIN32


/////////
// Posix
/////////
#if defined(__APPLE__) || defined(__unix__) 

  char            *dest = Rx;
  uint32_t        remaining = lenRx, got;
  struct          timeval tv;
  fd_set          fdr;
  struct termios  toptions;
  uint32_t        timeout;
  
  
  // get terminal attributes
  if (tcgetattr(fpCom, &toptions) < 0) {
    fprintf(stderr, "\n\nerror in 'receive_port': get port attributes failed, exit!\n\n");
    Exit(1, g_pauseOnExit);
  }
  
  // get terminal timeout (see: http://unixwiz.net/techtips/termios-vmin-vtime.html)
  timeout = toptions.c_cc[VTIME] * 100;        // convert 0.1s to ms
  
  // while there are bytes left to read...
  while (remaining) {
  
    // reinit timeval structure each time through loop
    tv.tv_sec  = (timeout / 1000L);
    tv.tv_usec = (timeout % 1000L) * 1000L;

    // wait for data to come in using select
    FD_ZERO(&fdr);
    FD_SET(fpCom, &fdr);
    if (select(fpCom + 1, &fdr, NULL, NULL, &tv) != 1)
      return(lenRx-remaining);

    // read a response, we know there's data waiting
    got = read(fpCom, dest, remaining);

    // handle errors. retry on EAGAIN, fail on anything else, ignore if no bytes read
    if (got == -1) {
      if (errno == EAGAIN)
        continue;
      else
        return(lenRx-remaining);
    } 
    else if (got > 0) {
      // figure out how many bytes are left and increment dest pointer through buffer
      dest += got;
      remaining -= got;
    }

  }
  
  // return number of received bytes
  return(lenRx);
  
#endif // __APPLE__ || __unix__

} // receive_port



/**
  \fn void flush_port(HANDLE fpCom)
   
  \brief flush port buffers
  
  \param[in]  fpCom   handle to comm port
  
  flush port input & output buffer. Use this function to facilitate serial communication
  on different platforms, e.g. Win32 and Posix
*/
void flush_port(HANDLE fpCom) {

/////////
// Win32
/////////
#ifdef WIN32

  // purge all port buffers (see http://msdn.microsoft.com/en-us/library/windows/desktop/aa363428%28v=vs.85%29.aspx)
  PurgeComm(fpCom, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

#endif // WIN32


/////////
// Posix
/////////
#if defined(__APPLE__) || defined(__unix__) 
  
  // purge all port buffers (see http://linux.die.net/man/3/tcflush)
  tcflush(fpCom, TCIOFLUSH);

#endif // __APPLE__ || __unix__

} // flush_port

// end of file
