%.check: %
	./$*

if HAVE_GDB
%.gdb: %
	$(LIBTOOL) --mode=execute \
		$(GDB) $*
endif

SUPPRESSIONS = $(top_srcdir)/gst.supp

if HAVE_VALGRIND

%.valgrind: %
	$(LIBTOOL) --mode=execute \
		$(VALGRIND) -q \
		$(foreach s,$(SUPPRESSIONS),--suppressions=$(s)) \
		--tool=memcheck \
		--leak-check=full \
		--track-origins=yes \
		--leak-resolution=high \
		./$* 2>&1 | tee valgrind.log
	@if $(EGREP) '==[0-9]+==' valgrind.log > /dev/null 2>&1; then \
		rm valgrind.log; \
		exit 1; \
	fi
	@rm valgrind.log

%.valgrind.gen-suppressions: %
	$(LIBTOOL) --mode=execute \
		$(VALGRIND) -q \
		$(foreach s,$(SUPPRESSIONS),--suppressions=$(s)) \
		--tool=memcheck \
		--leak-check=full \
		--track-origins=yes \
		--gen-suppressions=all \
		--leak-resolution=high \
		./$* 2>&1 | tee valgrind.log
	@if $(EGREP) '==[0-9]+==' valgrind.log > /dev/null 2>&1; then \
		rm valgrind.log; \
		exit 1; \
	fi
	@rm valgrind.log

check-valgrind: $(TESTS)
	@failed=0; \
	for test in $(TESTS); do \
		$(MAKE) $$test.valgrind; \
		if test "$$?" -ne 0; then \
			echo "Valgrind error for test $$test"; \
			failed=`expr $$failed + 1`; \
			whicht="$$whicht $$test"; \
		fi; \
	done; \
	if test "$$failed" -ne 0; then \
		echo "$$failed tests had leaks or errors under valgrind:"; \
		echo "$$whicht"; \
		false; \
	fi
endif
