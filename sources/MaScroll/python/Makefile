CHECKER_PREFIX := ../../../checkers/MaScroll
EXPLOITS_DIR := ../../../exploits/MaScroll

CHECKER1_DEPS := utils.py
CHECKER2_DEPS := macro.py utils.py

EXPLOITS_DEPS :=

CHECKER1_DIR := $(CHECKER_PREFIX)-1
CHECKER2_DIR := $(CHECKER_PREFIX)-2

TARGETS := \
	$(addprefix $(CHECKER1_DIR)/,$(CHECKER1_DEPS)) \
	$(addprefix $(CHECKER2_DIR)/,$(CHECKER2_DEPS)) \
	$(addprefix $(EXPLOITS_DIR)/,$(EXPLOITS_DEPS))

.SUFFIXES:
.DEFAULT_GOAL := dist

$(CHECKER1_DIR)/%.py: %.py
	@echo 'CP $@'
	@cp $< $@

$(CHECKER2_DIR)/%.py: %.py
	@echo 'CP $@'
	@cp $< $@

$(EXPLOITS_DIR)/%.py: %.py
	@echo 'CP $@'
	@cp $< $@

dist: $(TARGETS)
.PHONY: dist

distclean:
	rm -f $(TARGETS)
.PHONY: distclean
