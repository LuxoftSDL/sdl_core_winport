execute_process(
  COMMAND ${CMAKE_SOURCE_DIR}/FindQt.cmd -v ${qt_version} Qt5CoreConfig.cmake
  OUTPUT_VARIABLE config_file
)

if(config_file STREQUAL "")
  message(FATAL_ERROR "Qt5 Core module not found")
endif(config_file STREQUAL "")

STRING(REGEX REPLACE "\"" "" config_file ${config_file})
STRING(REGEX REPLACE "\n" "" config_file ${config_file})

include(${config_file})
