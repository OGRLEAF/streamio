SUBDIRS = ../lib test_app 
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
	mkdir -p $(RELEASE_PACK)/lib

	# Files on the board
	mkdir -p $(RELEASE_PACK)/user/lib

	# Remote dynamic lib
	cp -r lib/* $(RELEASE_PACK)/lib

	# Add .so for user
	cp -r lib/libstreamio.so $(RELEASE_PACK)/user/lib

	# Place test_app as example project
	cp -r test_app $(RELEASE_PACK)/example

	cp -r Makefile.variable.build $(RELEASE_PACK)/Makefile.variable

	# Add README
	cp README.md $(RELEASE_PACK)/
	
	cp -r ../scripts $(RELEASE_PACK)

.PHONY: clean
clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

	rm -r $(RELEASE_PACK)

