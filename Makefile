SUBDIRS = lars_reactor lars_dns lars_reporter lars_lb_agent api/cpp/lars_api api/cpp/example

.PHONY: all

all:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
		echo "build in $$subdir";\
		$(MAKE) -C $$subdir; \
	done

.PHONY: clean
clean:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
		echo "clien in $$subdir";\
		$(MAKE) -C $$subdir clean; \
	done
