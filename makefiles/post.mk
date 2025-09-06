$(error DO NOT USE)

$(OBJ_DIR) $(R2R_PD)::
	$(MKDIR_P) $@

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(compile)
$(OBJ_DIR)/%.o: common/%.c | $(OBJ_DIR)
	$(compile)
$(OBJ_DIR)/%.o: bus/$(PLATFORM)/%.c | $(OBJ_DIR)
	$(compile)

$(OBJ_DIR)/%.o: common/%.s | $(OBJ_DIR)
	$(assemble)
$(OBJ_DIR)/%.o: bus/$(PLATFORM)/%.s | $(OBJ_DIR)
	$(assemble)
