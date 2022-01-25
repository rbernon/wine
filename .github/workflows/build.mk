.SILENT:
MAKEFLAGS += --quiet --no-print-directory
OBJCOPY ?= objcopy

ifneq ($(IMAGE),)
SHELL := podman run --rm -w $(CURDIR) -v $(CURDIR):$(CURDIR) -e CCACHE_DIR $(IMAGE) sh -c
endif

export CCACHE_DIR := $(shell mkdir -p $(CURDIR)/.ccache && echo $(CURDIR)/.ccache)

all: Makefile
	ccache -s
	$(MAKE) -j4 $(MFLAGS) $(MAKEOVERRIDES) install DESTDIR=install
	-find install/bin install/lib* -type f -not -iname '*.a' -not -iname '*.debug' | \
            sed -Ee 'p;s:$$:.debug:' | \
            xargs -n2 $(OBJCOPY) --only-keep-debug # 2>/dev/null
	-find install/bin install/lib* -type f -not -iname '*.a' -not -iname '*.debug' | \
            sed -Ee 'h;s:^:--add-gnu-debuglink=:;s:$$:.debug:p;g' | \
            xargs -n2 $(OBJCOPY) --file-alignment=4096 --strip-debug # 2>/dev/null
	ccache -s
.PHONY: all

Makefile:
	tools/make_makefiles && \
	tools/make_requests && \
	autoreconf -fi && \
	./configure --cache-file=$(CURDIR)/.ccache/configure $(CONFIG) --prefix=/ \
		CFLAGS="-O2 -ggdb -ffunction-sections -fdata-sections -fno-omit-frame-pointer -ffile-prefix-map=$(CURDIR)=." \
		CROSSCFLAGS="-O2 -ggdb -ffunction-sections -fdata-sections -fno-omit-frame-pointer -ffile-prefix-map=$(CURDIR)=." \
		CROSSLDFLAGS="-Wl,--file-alignment=4096" \
