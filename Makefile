##################################
# Uncomment the right setting for your system
##################################

# Linux 
CFLAGS = -O1 -Wall -std=gnu99
LDFLAGS =

##################################
### Avoid touching anything below this line
##################################

SOURCES=tmt.c main.c
OBJECTS=${SOURCES:.c=.o}

OUT=tailer
LIBS = -lutil

all: ${OBJECTS}
	cc $(OBJECTS) -o $(OUT) $(LDFLAGS) $(LIBS)

.c.o:
	$(CC) -c $(CPPFLAGS) $(DEFS) $(CFLAGS) $<

clean:
	rm -rf $(OBJECTS) $(OUT)
