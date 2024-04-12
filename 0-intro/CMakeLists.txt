
# compile a list of source files for each specific source type the CGV CMake build system knows about
set(SOURCES
	cgv_demo.cxx
	cubes_drawable.cxx
	cubes_fractal.cxx
)
set(HEADERS
	cubes_fractal.h
)

# add our target to the CGV CMake build system
cgv_add_target(task0_framework
	TYPE plugin    NO_EXECUTABLE
	SOURCES        ${SOURCES}
	HEADERS        ${HEADERS}

	DEPENDENCIES
		cgv_viewer glew fltk cgv_utils cgv_type cgv_reflect cgv_data cgv_signal cgv_base cgv_media cgv_gui
		cgv_render cgv_os cgv_gl cg_fltk cmf_tt_gl_font crg_stereo_view cg_ext cg_meta cmi_io

	LINKTIME_PLUGIN_DEPENDENCIES
		cmf_tt_gl_font

	ADDITIONAL_CMDLINE_ARGS
		"config:\"${CMAKE_CURRENT_LIST_DIR}/config.def\""
)
