TARGETS = dyninst-nontrivial-param \
	  elfutils-nontrivial-param \
	  libdwarf-nontrivial-param

all: $(TARGETS)

clean:
	rm -vf *.o $(TARGETS)

.PHONY: clean all

CFLAGS = -g -Og -std=gnu11 -Wall -Wextra -Werror
CXXFLAGS = -g -Og -std=gnu++11 -Wall -Wextra -Werror

dyninst-nontrivial-param: CPPFLAGS += -isystem /usr/include/dyninst
dyninst-nontrivial-param: LDFLAGS += -L/usr/lib64/dyninst
dyninst-nontrivial-param: LDLIBS += -lsymtabAPI

elfutils-nontrivial-param: CPPFLAGS += -isystem /usr/include/elfutils/
elfutils-nontrivial-param: LDLIBS += -ldw -liberty

libdwarf-nontrivial-param: CPPFLAGS += -isystem /usr/include/libdwarf/
libdwarf-nontrivial-param: LDLIBS += -ldwarf -lelf -liberty
