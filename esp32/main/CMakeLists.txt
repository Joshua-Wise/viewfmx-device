get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

idf_component_register(
    SRCS
        "app_main.c"
        "${REPO_ROOT}/gui/gui.c"
        "${REPO_ROOT}/gui/screens/room_screen.c"
        "${REPO_ROOT}/gui/screens/settings_overlay.c"
        "${REPO_ROOT}/gui/assets/logo.c"
        "${REPO_ROOT}/data/mock_provider.c"
        "${REPO_ROOT}/data/http_provider_parse.c"
        "${REPO_ROOT}/data/http_provider_esp.c"
        "${REPO_ROOT}/platform/esp32/compat.c"
        "${REPO_ROOT}/platform/esp32/display.c"
        "${REPO_ROOT}/platform/esp32/input.c"
        "${REPO_ROOT}/platform/esp32/net.c"
        "${REPO_ROOT}/platform/esp32/sysinfo.c"
        "${REPO_ROOT}/platform/esp32/motion.c"
        "${REPO_ROOT}/platform/esp32/fetcher.c"
    INCLUDE_DIRS
        "."
        "${REPO_ROOT}"
    EMBED_TXTFILES
        "certs/gdig2.pem"
)
