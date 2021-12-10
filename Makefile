ARCH     = $(shell uname -p)
NPROC    = $(shell nproc)
LIBS     = $(shell ./libconcurrent/libdeps.sh)
CLANG_AR = ar
DIR      = /opt

all:
	make $(ARCH)

debug:
	make $(ARCH) D_ARGS="-DDEBUG"

codecov:
	make $(ARCH) D_ARGS="-DDEBUG -ftest-coverage -fprofile-arcs --coverage -O0"

x86_64:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

x86_64_pwbs:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DSYNCH_COUNT_PWBS -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

x86_64_no_recycle:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DSYNCH_POOL_NODE_RECYCLING_DISABLE -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

x86_64_no_recycle_pwbs:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DSYNCH_POOL_NODE_RECYCLING_DISABLE -DSYNCH_COUNT_PWBS -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

x86_64_no_elimination:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DSYNCH_DISABLE_ELIMINATION_ON_STACKS -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

x86_64_no_elimination_pwbs:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DSYNCH_DISABLE_ELIMINATION_ON_STACKS -DSYNCH_COUNT_PWBS -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

x86_64_no_recycle_no_elimination:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DSYNCH_POOL_NODE_RECYCLING_DISABLE -DSYNCH_DISABLE_ELIMINATION_ON_STACKS -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

x86_64_no_recycle_no_elimination_pwbs:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DSYNCH_POOL_NODE_RECYCLING_DISABLE -DSYNCH_DISABLE_ELIMINATION_ON_STACKS -DSYNCH_COUNT_PWBS -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

x86_64_no_psync:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DSYNCH_DISABLE_PSYNCS -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

x86_64_no_pwb:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DSYNCH_DISABLE_PWBS -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

x86_64_heap_1024:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DINITIAL_HEAP_LEVELS=10 -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'
x86_64_heap_512:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DINITIAL_HEAP_LEVELS=9 -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'
x86_64_heap_256:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DINITIAL_HEAP_LEVELS=8 -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'
x86_64_heap_128:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DINITIAL_HEAP_LEVELS=7 -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'
x86_64_heap_64:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)' CFLAGS='-DINITIAL_HEAP_LEVELS=6 -Wall -std=gnu89 -Ofast -fPIC -flto -ftree-vectorize -finline-functions -march=native -mtune=native'

icx:
	make -f Makefile.generic -j $(NPROC) COMPILER=icx LDLIBS='$(LIBS)' CFLAGS='-Wall -std=gnu89 -O3 -fPIC -ftree-vectorize -finline-functions -march=native -mtune=native'

clang:
	make -f Makefile.generic -j $(NPROC) COMPILER=clang AR=$(CLANG_AR) LDLIBS='$(LIBS)' CFLAGS='-Wall -std=gnu89 -Ofast -fPIC -ftree-vectorize -finline-functions -march=native -mtune=native'

unknown:
	make -f Makefile.generic -j $(NPROC) COMPILER=$(CC) LDLIBS='$(LIBS)'  CFLAGS='-std=gnu89 -Ofast -fPIC -flto'

clean:
	make -f Makefile.generic clean

docs:
	make -f Makefile.generic docs

install:
	make -f Makefile.generic install INSTALL_DIR=$(DIR)

uninstall:
	make -f Makefile.generic uninstall
