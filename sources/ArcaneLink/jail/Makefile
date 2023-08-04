CFLAGS  += -Os
LDFLAGS += -static

.PHONY: clean

nsjail-bash: nsjail-bash.c
	$(CROSS_COMPILE)gcc $(CFLAGS) $(LDFLAGS) -o $@ $^
	$(CROSS_COMPILE)strip $@

clean:
	rm -f nsjail-bash
