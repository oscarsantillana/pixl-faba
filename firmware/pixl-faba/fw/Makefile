.PHONY: app bl clean size_report size_report_files

BOARD ?= OLED
RELEASE ?= 1
APP_VERSION ?= 1

all: bl app ota

bl:
	+@$(MAKE) -C bootloader/ BOARD=$(BOARD) RELEASE=$(RELEASE)

app:
	+@$(MAKE) -C application/ BOARD=$(BOARD) RELEASE=$(RELEASE) APP_VERSION=$(APP_VERSION)

ota:
	+@$(MAKE) -C application/ full BOARD=$(BOARD) RELEASE=$(RELEASE) APP_VERSION=$(APP_VERSION)
	+@$(MAKE) -C application/ ota BOARD=$(BOARD) RELEASE=$(RELEASE) APP_VERSION=$(APP_VERSION)

clean:
	@rm -rf _build/

size_report:
	+@$(MAKE) -C application/ size_report BOARD=$(BOARD) RELEASE=$(RELEASE) APP_VERSION=$(APP_VERSION)

size_report_files:
	+@$(MAKE) -C application/ size_report_files BOARD=$(BOARD) RELEASE=$(RELEASE) APP_VERSION=$(APP_VERSION)
