target = yarvis_cmodel
sources = main.c mem.c yarvis.c
objects = ${sources:.c=.o}
depends = ${objects:.o=.d}

CFLAGS = -g -MMD -std=c11 -Wpedantic -Wall -Wextra -Werror
FLAGS =

ifneq ($(RV32E),)
	CFLAGS := $(CFLAGS) -DRV32E=$(RV32E)
endif
ifneq ($(RV64I),)
	CFLAGS := $(CFLAGS) -DRV64I=$(RV64I)
endif

.PHONY: all clean

all: ${target}

clean:
	$(RM) $(target) $(objects) $(depends)

${target}: ${objects}
	${CC} ${CFLAGS} -o $@ $^

-include ${depends}
