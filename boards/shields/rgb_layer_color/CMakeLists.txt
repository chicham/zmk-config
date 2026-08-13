if(CONFIG_SHIELD_RGB_LAYER_COLOR)
  # zmk_keymap_highest_layer_active() and layer_state_changed only exist
  # where the full keymap is compiled in, i.e. the central half.
  if(NOT CONFIG_ZMK_SPLIT OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zephyr_library_sources(src/rgb_layer_color.c)
  endif()
endif()
