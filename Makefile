NPM ?= npm
NODE ?= 

# main targets
.PHONY: all
all: package

# cleanup targets
.PHONY: clean-node
clean-node:
	$(RM) -rf node_modules/

.PHONY: clean-pkg
clean-pkg:
	$(RM) -f rpi-stereo-cam-stream.tar.gz

.PHONY: clean
clean: clean-node clean-pkg

# init targets
.PHONY: init
init:
	$(NODE) $(NPM) install

# make a deployable package
PACKAGE_NAME=rpi-stereo-cam-stream
.PHONY: package
package: init
	$(RM) -rf $(PACKAGE_NAME)
	mkdir $(PACKAGE_NAME)
	cp -r node_modules/ $(PACKAGE_NAME)/node_modules
	cp -r static/       $(PACKAGE_NAME)/static
	cp -r target/       $(PACKAGE_NAME)/target
	mkdir -p $(PACKAGE_NAME)/target/opt/vs/bin
	cp -f raspbian/raspistill.gpsd $(PACKAGE_NAME)/target/opt/vs/bin/raspistill.gpsd
	mkdir -p $(PACKAGE_NAME)/target/boot
	cp -f device-tree/dt-blob-dualcam-pin4pin5.dtb $(PACKAGE_NAME)/target/boot/dt-blob.bin
	cp index.js         $(PACKAGE_NAME)
	cp index.html       $(PACKAGE_NAME)
	cp package.json     $(PACKAGE_NAME)
	cp README.md        $(PACKAGE_NAME)
	tar czf $(PACKAGE_NAME).tar.gz $(PACKAGE_NAME)
	$(RM) -rf $(PACKAGE_NAME)
