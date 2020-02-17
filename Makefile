CFLAGS=-Wall -O3 -g -Wextra -Wno-unused-parameter -DDISABLE_HARDWARE_PULSES -DDEFAULT_HARDWARE="adafruit-hat"
CXXFLAGS=$(CFLAGS)

# Where our library resides. You mostly only need to change the
# RGB_LIB_DISTRIBUTION, this is where the library is checked out.
RGB_LIB_DISTRIBUTION=..
RGB_INCDIR=include
RGB_LIBDIR=lib
RGB_LIBRARY_NAME=rgbmatrix
LDFLAGS+=-L$(RGB_LIBDIR) -lrgbmatrix -lrt -lm -lpthread -lpigpio

all : gymtimer.bin

# All the binaries that have the same name as the object file.q
gymtimer.bin : gymtimer.o 
	$(CXX) $< -o $@ $(LDFLAGS)

%.o : %.cc
	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) -c -o $@ $<

%.o : %.c
	$(CC) -I$(RGB_INCDIR) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.bin

test: gymtimer.bin
	sudo ./gymtimer.bin --led-rows=64 --led-cols=64 -f fonts/gohufont-11.bdf



FORCE:
.PHONY: FORCE

