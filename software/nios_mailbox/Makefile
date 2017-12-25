all:
	@echo 'KDIR=$(KDIR)'
	@echo 'CROSS_COMPILE=$(CROSS_COMPILE)'
	@if [ ! -d $(CURDIR)/$(TARGET_DIR) ]; then mkdir $(CURDIR)/$(TARGET_DIR); fi
	@if [ ! -d $(CURDIR)/$(TARGET_DIR)/$(STRIPPED_DIR) ]; then mkdir $(CURDIR)/$(TARGET_DIR)/$(STRIPPED_DIR); fi
	cp $(TARGET).c $(CURDIR)/$(TARGET_DIR)
	cp $(TARGET).h $(CURDIR)/$(TARGET_DIR)
	cp Kbuild $(CURDIR)/$(TARGET_DIR)
	$(MAKE) -C $(KDIR) ARCH=arm M=$(CURDIR)/$(TARGET_DIR)
	rm $(CURDIR)/$(TARGET_DIR)/$(TARGET).c
	rm $(CURDIR)/$(TARGET_DIR)/$(TARGET).h
	rm $(CURDIR)/$(TARGET_DIR)/Kbuild

clean:
	rm -rf main $(CURDIR)/$(TARGET_DIR) $(CURDIR)/$(TARGET_DIR)/$(STRIPPED_DIR)
