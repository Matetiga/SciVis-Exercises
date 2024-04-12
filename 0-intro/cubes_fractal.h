// This source code is property of the Computer Graphics and Visualization chair of the
// TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved
//
// The main file of the plugin. It defines a class that demonstrates how to register with
// the scene graph, drawing primitives, creating a GUI, using a config file and various
// other parts of the framework.

// Framework core

class cubes_fractal
{

protected:

	// The vertex array to use for drawing the cubes
	cgv::render::attribute_array_binding *va;

	// The number of vertices in the array
	size_t va_num_verts;

	// The type of OpenGL primitive used in the array
	GLenum va_primitive_type;

	// Used to pass reflectance information to shaders for coloring the cubes
	cgv::media::illum::surface_material material;


public:

	/** The default constructor. */
	cubes_fractal();

	/** Set a vertex array for drawing the cubes. By default, no vertex array is used and
	    the tesselate_unit_cube() method of the render context is used.
	    \param vertex_array
	    	The vertex array to use. Passing nullptr disables vertex array based drawing.
	    \param num_vertices
	    	The number of vertices comprising the geometry supplied in the array. Ignored
	    	if vertex_array is nullptr.
	    \param primitive_type
	    	The primitive type associated with the geometry in the vertex array. Can be
	    	any valid OpenGL primitive type (GL_TRIANGLES, GL_TRIANGLE_STRIP,
			GL_TRIANGLE_FAN, GL_QUADS etc.)
	*/
	void use_vertex_array (cgv::render::attribute_array_binding *vertex_array,
	                       size_t num_vertices, GLenum primitive_type);

	/** Draw the fractal cube hierarchy via recursion. Needs a suitable shader program to
	    be enabled when called. Suitable means that the shader program must define a
	    vertex layout that contains at least a vec4 location named "position". For
	    lighting, it should also define a vec3 location called "normal" and for coloring,
	    accept a CGV framework Material struct as a uniform named "material". The default
	    surface shader (context::ref_surface_shader_program()) fits these requirements.
	    \param ctx
	    	The render context to use for drawing.
	    \param color
	    	The desired color of the node.
	    \param max_depth
	    	The ecursion depth to terminate at.
	    \param level
	    	The next recursion level. Should be set to 0 when invoking from the root node.
	*/
	void draw_recursive (cgv::render::context& ctx, const cgv::media::color<float> &color,
	                     unsigned max_depth, unsigned level=0);
};
