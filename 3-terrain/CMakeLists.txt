
# Define a list of source and header files
set(SOURCES
    terrain.cxx
)

set(HEADERS
    
)

# Define a list of shader files
set(SHADERS
    glsl/terrain.glfs
	glsl/terrain.glgs
	glsl/terrain.glpr
	glsl/terrain.glvs
)

# Add the target to the CGV CMake build system
cgv_add_target(task3_terrain
    TYPE plugin    NO_EXECUTABLE
    SOURCES        ${SOURCES}
    HEADERS        ${HEADERS}
    SHADER_SOURCES ${SHADERS}
    DEPENDENCIES
        cgv_utils cgv_type cgv_reflect cgv_data cgv_signal cgv_base cgv_media cgv_gui cgv_render cgv_gl glew plot cg_fltk crg_stereo_view crg_antialias crg_depth_of_field crg_light cmi_io crg_grid cgv_viewer cg_ext

    ADDITIONAL_CMDLINE_ARGS
        "config:\"${CMAKE_CURRENT_LIST_DIR}/config.def\""
)
