.phony: all clean

dstName = pool
srcFiles = mcsMpool.c pool.c


CFLAGS += -I/dvlp/ -Wall
LIBS += -lpthread

$(dstName): $(srcFiles)
	cc $(CFLAGS) $(LIBS) $^ -g -o $@
	
all: $(dstName)

clean:
	rm $(dstName)