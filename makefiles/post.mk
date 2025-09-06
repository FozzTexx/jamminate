$(OBJDIR) $(R2R_PD)::
	$(MKDIR_P) $@

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(compile)
$(OBJDIR)/%.o: common/%.c | $(OBJDIR)
	$(compile)
$(OBJDIR)/%.o: bus/$(PLATFORM)/%.c | $(OBJDIR)
	$(compile)

$(OBJDIR)/%.o: common/%.s | $(OBJDIR)
	$(assemble)
$(OBJDIR)/%.o: bus/$(PLATFORM)/%.s | $(OBJDIR)
	$(assemble)
