add_lld_library(lldELF
  ELFLinkingContext.cpp
  Reader.cpp
  Writer.cpp
  )

target_link_libraries(lldELF
  lldHexagonELFTarget
  lldPPCELFTarget
  lldPasses
  lldReaderWriter
  lldX86ELFTarget
  lldX86_64ELFTarget
  lldCpu0ELFTarget
  )

include_directories(.)

file(GLOB all_valid_subdirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*/CMakeLists.txt")

foreach(dir ${all_valid_subdirs})
    if(${dir} MATCHES "^([^/]*)//CMakeLists.txt")
        string(REGEX REPLACE "^([^/]*)//CMakeLists.txt" "\\1" dir_trimmed ${dir})
        add_subdirectory(${dir_trimmed})
    endif()
endforeach(dir)
