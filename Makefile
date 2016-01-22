cmp: cmp.c
	${CC} ${CFLAGS} -o $@ $<

rg_etc1wrap.o: rg_etc1.cpp rg_etc1wrap.cpp
	${CXX} -c rg_etc1wrap.cpp rg_etc1.cpp

PLUGINS := $(wildcard plugin_*.c)
arc: arc.c ${PLUGINS} lodepng.c efe_pixelformat.c rg_etc1wrap.o
	${CC} -D_BSD_SOURCE -std=c11 -lm ${CFLAGS} -o $@ $< ${PLUGINS} lodepng.c rg_etc1wrap.o rg_etc1.o
