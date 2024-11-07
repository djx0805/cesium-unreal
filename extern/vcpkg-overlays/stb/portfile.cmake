vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO nothings/stb
    REF f75e8d1cad7d90d72ef7a4661f1b994ef78b4e31 # committed on 2024-07-29
    SHA512 4a733aefb816a366c999663e3d482144616721b26c321ee5dd0dce611a34050b6aef97d46bd2c4f8a9631d83b097491a7ce88607fd9493d880aaa94567a68cce
    HEAD_REF master
)

file(GLOB HEADER_FILES "${SOURCE_PATH}/*.h" "${SOURCE_PATH}/stb_vorbis.c")
file(COPY ${HEADER_FILES} DESTINATION "${CURRENT_PACKAGES_DIR}/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/FindStb.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/vcpkg-cmake-wrapper.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# Even when told to create static symbols, STB creates two symbols globally, which breaks packaging in UE 5.4. Fix that.
vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/stb_image_resize2.h" "STBIR__SIMDI_CONST(stbir__s" "STBIRDEF STBIR__SIMDI_CONST(stbir__s")
