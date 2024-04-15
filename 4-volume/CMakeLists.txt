
# Define a list of source and header files
set(SOURCES
    volume.cxx
	volume_io.cxx
	volume_view.cxx
)

set(HEADERS
    lib_begin.h
	volume.h
	volume_io.h
)

# Define a list of shader files
set(SHADERS
    glsl/slicer.glfs
	glsl/slicer.glpr
	glsl/slicer.glvs
)

# Add the target to the CGV CMake build system
cgv_add_target(task4_volume
    TYPE plugin    NO_EXECUTABLE
    SOURCES        ${SOURCES}
    HEADERS        ${HEADERS}
    SHADER_SOURCES ${SHADERS}
    DEPENDENCIES
        cgv_utils cgv_type cgv_reflect cgv_data cgv_signal cgv_base cgv_media cgv_gui cgv_render cgv_gl glew plot cg_fltk crg_stereo_view crg_antialias crg_depth_of_field crg_light cmi_io crg_grid cgv_viewer cg_ext

    OVERRIDE_SHARED_EXPORT_DEFINE
		VOL_DATA_EXPORTS

    ADDITIONAL_CMDLINE_ARGS
        "config:\"${CMAKE_CURRENT_LIST_DIR}/config.def\""
)
