SUBDIRS = ../lib test_app io_backend io_client
MAKE_ENV_INCLUDE = ../Makefile.variable

RELEASE_PACK = ../build

subdirs:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

release: subdirs
	# Packup files as SDK
	mkdir -p $(RELEASE_PACK)

	# PACK header files and dynamic lib as SDK
	mkdir -p $(RELEASE_PACK)/sdk

	# Files on the board
	mkdir -p $(RELEASE_PACK)/user/lib

	# Remote dynamic lib
	cp -r lib/* $(RELEASE_PACK)/sdk

	# Add .so for user
	cp -r lib/libstreamio.so $(RELEASE_PACK)/user/lib

	# Server file
	cp -r io_backend/build/io_backend $(RELEASE_PACK)/user

	# Place test_app as example project
	cp -r test_app $(RELEASE_PACK)/example

	# Add README
	cp README.md $(RELEASE_PACK)/

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

	rm -r $(RELEASE_PACK)

