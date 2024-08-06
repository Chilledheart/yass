TAG:=$(shell git describe --abbrev=0 --tags HEAD)

all: it.gui.yass.yml

it.gui.yass.checksum:
	../scripts/build-src.sh
	cp -fv ../yass-$(TAG).tar.bz2 .
	sha256sum yass-$(TAG).tar.bz2 > it.gui.yass.checksum

it.gui.yass.yml: it.gui.yass.checksum it.gui.yass.yml.tmpl
	cp -fv it.gui.yass.yml.tmpl it.gui.yass.yml
	sed -i s#PWD#$(PWD)#g it.gui.yass.yml
	sed -i s/TAG_NAME/$(TAG)/g it.gui.yass.yml
	$(eval SHA256 := $(shell cat it.gui.yass.checksum | cut -d ' ' -f1))
	sed -i s/SHA256/$(SHA256)/g it.gui.yass.yml

clean:
	rm -f yass-$(TAG).tar.bz2
	rm -f it.gui.yass.checksum
	rm -f it.gui.yass.yml
