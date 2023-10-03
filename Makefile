OBJS= fs.o fs_mod.o
EXEC= fs
HEADER= fs.h

$(EXEC): $(OBJS) $(HEADER)
	gcc $(OBJS) -o $(EXEC) -lrt -pthread -lm

clean:
	rm -f $(OBJS) $(EXEC)