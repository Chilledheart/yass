SRC := $(wildcard *.cpp)
PO_FILES := $(wildcard *.po)

TEMPLATE_PO := messages.po.tmp

.PHONY: all clean

all: $(PO_FILES)

$(TEMPLATE_PO): $(SRC)
	xgettext -k_ -o $@ $^
	sed -i s/CHARSET/UTF-8/g $@

%.po: $(TEMPLATE_PO)
	$(eval TMP := $(shell mktemp))
	msgmerge -N --previous $@ $< > $(TMP)
	mv -f $(TMP) $@

clean:
	-rm -f $(TEMPLATE_PO)
