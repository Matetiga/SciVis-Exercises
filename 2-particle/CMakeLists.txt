
# Define a list of source and header files
set(SOURCES
    particle.cxx
)

set(HEADERS
    
)

# Define a list of shader files
set(SHADERS
    glsl/cylinder_raycast.glfs
	glsl/cylinder_raycast.glgs
	glsl/cylinder_raycast.glpr
	glsl/cylinder_raycast.glvs
	glsl/sphere_raycast.glfs
	glsl/sphere_raycast.glgs
	glsl/sphere_raycast.glpr
	glsl/sphere_raycast.glvs
)

cgv_add_target(task2_particle
    TYPE plugin    NO_EXECUTABLE
    SOURCES        ${SOURCES}
    HEADERS        ${HEADERS}
    SHADER_SOURCES ${SHADERS}
    DEPENDENCIES
        cgv_utils cgv_type cgv_reflect cgv_data cgv_signal cgv_base cgv_media cgv_gui cgv_render cgv_gl glew plot cg_fltk crg_stereo_view crg_antialias crg_depth_of_field crg_light cmi_io crg_grid cgv_viewer cg_ext

    ADDITIONAL_CMDLINE_ARGS
        "config:\"${CMAKE_CURRENT_LIST_DIR}/config.def\""
)
