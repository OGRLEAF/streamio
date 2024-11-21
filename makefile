SUBDIRS = lib # test_app io_backend io_client
MAKE_ENV_INCLUDE = ./Makefile.variable

RELEASE_PACK = ./build
USERSPACE_DIR = userspace

subdirs:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

release: subdirs
	for dir in $(SUBDIRS); do \
		$(MAKE) INSTALL_CMD="#" -C $$dir install; \
	done

	# Packup files as SDK
	mkdir -p $(RELEASE_PACK)

	# PACK header files and dynamic lib as SDK
	mkdir -p $(RELEASE_PACK)/lib

	# Files on the board
	mkdir -p $(RELEASE_PACK)/user

	# Remote dynamic lib
	cp -r $(USERSPACE_DIR)/lib/* $(RELEASE_PACK)/lib

	# Add .so for user
	cp -r $(USERSPACE_DIR)/lib/libstreamio.so $(RELEASE_PACK)/user

	# Server file
	cp -r $(USERSPACE_DIR)/io_backend/build/io_backend $(RELEASE_PACK)/user

	# Place test_app as example project
	$(MAKE) -C $(USERSPACE_DIR)/test_app clean
	cp -r $(USERSPACE_DIR)/test_app $(RELEASE_PACK)/example

	# Add README
	cp $(USERSPACE_DIR)/README.md $(RELEASE_PACK)/

.PHONY: clean
clean:
	rm -r $(RELEASE_PACK)

	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

