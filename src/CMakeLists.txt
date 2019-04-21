include_directories(../jmp)
aux_source_directory(. DIR_SRC)
add_library(jmpool SHARED ${DIR_SRC})
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
