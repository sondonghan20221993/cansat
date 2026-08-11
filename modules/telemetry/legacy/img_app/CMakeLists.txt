project(CFE_IMG_APP C)

set(APP_SRC_FILES
  fsw/src/img_app.c
  fsw/src/img_app_cmds.c
  fsw/src/img_app_utils.c
)

if (CFE_EDS_ENABLED)
  list(APPEND APP_SRC_FILES
    fsw/src/img_app_eds_dispatch.c
  )
else()
  list(APPEND APP_SRC_FILES
    fsw/src/img_app_dispatch.c
  )
endif()

add_cfe_app(img_app ${APP_SRC_FILES})

target_include_directories(img_app PUBLIC fsw/inc)
