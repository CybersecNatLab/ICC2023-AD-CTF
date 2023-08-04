CFLAGS  += -O0
LDFLAGS += -static

.PHONY: clean

arcane-cli: arcane-cli.c
	$(CROSS_COMPILE)gcc $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f arcane-cli
