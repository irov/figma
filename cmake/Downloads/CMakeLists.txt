cmake_minimum_required(VERSION 3.20)

option(FIGMA_DOWNLOADS_SILENT "Figma thirdparty download silent mode" OFF)

project(FigmaDownloads NONE)

set(FIGMA_REPOSITORY "${CMAKE_CURRENT_LIST_DIR}/../..")
set(THIRDPARTY_DIR "${FIGMA_REPOSITORY}/thirdparty")

include("${FIGMA_REPOSITORY}/cmake/download_template.cmake")

GIT_CLONE(graphics https://github.com/irov/graphics.git)
GIT_CLONE(json https://github.com/irov/json.git)
GIT_CLONE(freetype https://gitlab.freedesktop.org/freetype/freetype.git "VER-2-14-3")
GIT_CLONE(kiwi git@github.com:irov/kiwi.git)
GIT_CLONE(libpng https://github.com/glennrp/libpng.git "v1.6.48")
GIT_CLONE(libjpeg https://github.com/libjpeg-turbo/libjpeg-turbo.git "3.1.4.1")

