if(NOT ENABLE_URDF)
  return()
endif()

include_directories(${PUGIXML_STATIC_INCLUDE_DIRS})
if(PUGIXML_STATIC_LIBRARY_DIRS)
  link_directories(${PUGIXML_STATIC_LIBRARY_DIRS})
endif()

make_gettext_mofiles(CnoidURDFBodyLoader mofiles)
choreonoid_add_library(CnoidURDFBodyLoader SHARED URDFBodyLoader.cpp HEADERS URDFBodyLoader.h)
target_link_libraries(CnoidURDFBodyLoader CnoidBody ${PUGIXML_STATIC_LIBRARIES})

# copy and installation of xacro library
set(xacro_dir ${PROJECT_SOURCE_DIR}/thirdparty/xacro-1.14.10)
file(COPY ${xacro_dir}/src/xacro DESTINATION {CHOREONOID_PYTHON_SUBDIR}/cnoid/)
configure_file(
  ${xacro_dir}/scripts/cnoid-xacro.py
  ${PROJECT_BINARY_DIR}/${CHOREONOID_PYTHON_SUBDIR}/cnoid/ COPYONLY)

install(DIRECTORY ${xacro_dir}/src/xacro
        DESTINATION ${CHOREONOID_PYTHON_SUBDIR}/cnoid)
install(FILES ${xacro_dir}/scripts/cnoid-xacro.py
        DESTINATION ${CHOREONOID_PYTHON_SUBDIR}/cnoid)
