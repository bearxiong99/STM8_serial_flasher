# Project: STM8_serial_flasher

CC            = gcc
CFLAGS        = -c -Wall -pedantic -I./STM8_Routines
LDFLAGS       = -g3 -lm
SOURCES       = bootloader.c hexfile.c main.c misc.c serial_comm.c
INCLUDES      = globals.h misc.h bootloader.h hexfile.h serial_comm.h main.h
STM8FLASH     = STM8_Routines/E_W_ROUTINEs_128K_ver_2.1.s19 STM8_Routines/E_W_ROUTINEs_128K_ver_2.0.s19 STM8_Routines/E_W_ROUTINEs_256K_ver_1.0.s19 STM8_Routines/E_W_ROUTINEs_32K_ver_1.3.s19 STM8_Routines/E_W_ROUTINEs_128K_ver_2.1.s19 STM8_Routines/E_W_ROUTINEs_32K_ver_1.4.s19 STM8_Routines/E_W_ROUTINEs_128K_ver_2.2.s19 STM8_Routines/E_W_ROUTINEs_32K_ver_1.0.s19 STM8_Routines/E_W_ROUTINEs_128K_ver_2.4.s19 STM8_Routines/E_W_ROUTINEs_32K_ver_1.2.s19
STM8INCLUDES  = $(STM8FLASH:.s19=.h)
OBJDIR        = Objects
OBJECTS       = $(patsubst %.c, $(OBJDIR)/%.o, $(SOURCES))
BIN           = STM8_serial_flasher
RM            = rm -fr

.PHONY: clean all default objects

.PRECIOUS: $(BIN) $(OBJECTS)

default: $(BIN) $(OBJDIR)

all: $(STM8INCLUDES) $(SOURCES) $(BIN)
	
$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	${RM} $(OBJECTS) $(OBJDIR) $(BIN) $(BIN).exe *~ .DS_Store 
	
%.h: %.s19 $(STM8FLASH)
	xxd -i $< > $@
	  
# link application
$(BIN): $(OBJECTS) $(OBJDIR)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

# compile all *c files
$(OBJDIR)/%.o: %.c $(SOURCES) $(INCLUDES) $(STM8INCLUDES) $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
