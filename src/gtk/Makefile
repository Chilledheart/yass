SRC := $(wildcard *.cpp)
TMP := messages.po
MERGED_TMP := messages.po.tmp

all: yass_en.po yass_zh_CN.po

$(TMP): $(SRC)
	xgettext -k_ -o $@ $^
	sed -i s/CHARSET/UTF-8/g $@

yass_en.po: $(TMP)
	msgmerge -N --previous $@ $< > $(MERGED_TMP)
	mv -f $(MERGED_TMP) $@

yass_zh_CN.po: $(TMP)
	msgmerge -N --previous $@ $< > $(MERGED_TMP)
	mv -f $(MERGED_TMP) $@

.PHONY: clean
clean:
	-rm -f $(TMP) $(MERGED_TMP)
