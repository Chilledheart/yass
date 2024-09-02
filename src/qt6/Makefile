# use make -B update-qm to update all qm files
QT_DIR ?= /usr/lib64/qt6
LUPDATE := $(QT_DIR)/bin/lupdate
LRELEASE := $(QT_DIR)/bin/lrelease

ifeq ("$(VERBOSE)","1")
	Q :=
else
	Q := -silent
endif

SRC := $(wildcard *.cc) $(wildcard *.cpp) $(wildcard *.hpp)
TARGET_TS := $(wildcard lang/yass_*.ts)
TARGET_QMS := $(TARGET_TS:.ts=.qm)
PWD := $(shell pwd)
PARENT_DIR := $(dir $(PWD))

.PHONY: all update-ts update-qm

all: update-qm update-ts

update-ts: $(TARGET_TS)

%.ts: $(SRC)
	$(LUPDATE) -extensions cc,cpp,hpp $(Q) -locations none -I $(PARENT_DIR) $(PWD) -ts $@

update-qm: $(TARGET_QMS)

%.qm: %.ts
	$(LRELEASE) -removeidentical $(Q) $^ $@
