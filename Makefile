LDFLAGS = -framework IOKit

smc: smc.o
smc.o: smc.c smc.h
