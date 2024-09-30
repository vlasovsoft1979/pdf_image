project(cpp)

cmake_minimum_required(VERSION 3.16)

set(PDFIO_ROOT "/home/sergv/Documents/build/pdfio")
set(STB_ROOT "/home/sergv/Documents/install/stb")

add_library(pdfio STATIC IMPORTED)
set_property(TARGET pdfio PROPERTY IMPORTED_LOCATION "${PDFIO_ROOT}/lib/libpdfio.a")
target_include_directories(pdfio INTERFACE "${PDFIO_ROOT}/include")
target_link_libraries(pdfio INTERFACE z)

add_library(stb INTERFACE)
target_include_directories(pdfio INTERFACE "${STB_ROOT}")

add_executable(pdf_image main.cpp pdf_image_extractor.cpp)
target_link_libraries(pdf_image PRIVATE pdfio stb)
