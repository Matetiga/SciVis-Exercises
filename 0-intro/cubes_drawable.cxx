// This source code is property of the Computer Graphics and Visualization chair of the
// TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved
//
// The main file of the plugin. It defines a class that demonstrates how to register with
// the scene graph, drawing primitives, creating a GUI, using a config file and various
// other parts of the framework.

// Framework core
#include <cgv/base/register.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/trigger.h>
#include <cgv/render/drawable.h>
#include <cgv/render/shader_program.h>
#include <cgv/render/vertex_buffer.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv/math/ftransform.h>

// Framework standard plugins
#include <cgv_gl/gl/gl.h>

// Local includes
#include "cubes_fractal.h"


// ************************************************************************************/
// Task 0.2a: Create a drawable that provides a (for now, empty) GUI and supports
//            reflection, so that its properties can be set via config file.
// Task 0.2b: Utilize the cubes_fractal class to render a fractal of hierarchically
//            transformed cubes. Expose its recursion depth and color properties to GUI
//            manipulation and reflection. Set reasonable values via the config
//            file.
//
// Task 0.2c: Implement an option (configurable via GUI and config file) to use a vertex
//            array object for rendering the cubes. The vertex array functionality 
//            should support (again, configurable via GUI and config file) both
//            interleaved (as in cgv_demo.cpp) and non-interleaved attributes.


class cubes_drawable :
	public cgv::base::base,
	public cgv::gui::provider,
	public cgv::render::drawable
{
protected:
	float fb_bgcolor_r, fb_bgcolor_g, fb_bgcolor_b;

	// an offscreen framebuffer is a buffer that contains information which should not be directly rendered to the screen
	// but instead is used as a texture for rendering to the screen
	// In the demo, it is used to render text to a texture, which is then applied to a quade
	// so here is not necessary
	cgv::rgba bgcolor;

	struct vertex {
		cgv::vec3 pos;
		cgv::vec3 normal;
	};
	
	std::vector<vertex> vertices;
	cgv::render::vertex_buffer vb;
	cgv::render::attribute_array_binding vertex_array;

	// for the fractal structure
	cubes_fractal fractal;

public:
	cubes_drawable():
		fb_bgcolor_r(0.8f), fb_bgcolor_g(0.8f), fb_bgcolor_b(0.1f),
		bgcolor(fb_bgcolor_r, fb_bgcolor_g, fb_bgcolor_b)
	{
	}

	// what does this make?
	// it is necessary to give a name to the type of the drawable, so that it can be
	// identified in the scene graph and used in config files, etc.
	std::string get_type_name() const
	{
		return "cubes_drawable";
	}


	// used for the cgv::base:;base interface
	// this method allows to use the config file and give the variables a value
	bool self_reflect(cgv::reflect::reflection_handler& rh)
	{
		return
			rh.reflect_member("fb_bgcolor_r", fb_bgcolor_r) &&
			rh.reflect_member("fb_bgcolor_g", fb_bgcolor_g) &&
			rh.reflect_member("fb_bgcolor_b", fb_bgcolor_b);
	}

	void on_set(void* member_ptr)
	{
		if (member_ptr == &fb_bgcolor_r || member_ptr == &fb_bgcolor_g || member_ptr == &fb_bgcolor_b)
		{
			bgcolor.R() = fb_bgcolor_r;
			bgcolor.G() = fb_bgcolor_g;
			bgcolor.B() = fb_bgcolor_b;
			update_member(&bgcolor);
		}

		// is this vice versa necessary ? (as shown in the demo)
		if (member_ptr == &bgcolor)
		{
			fb_bgcolor_r = bgcolor.R();
			fb_bgcolor_g = bgcolor.G();
			fb_bgcolor_b = bgcolor.B();
		}

		if (this->is_visible())
			post_redraw();
	}

	// this was only necessary to for the screen resolution values in demo
	// maybe remove ?
	bool gui_check_value(cgv::gui::control<int>& ctrl)
	{
		return true;
	}


	// used for the cgv::gui::provider interface
	void create_gui(void)
	{
		add_member_control(this, "tex background", bgcolor);
	}

	// used for the cgv::render::drawable interface
	bool init(cgv::render::context& ctx)
	{
		bool success = true;
		cgv::render::shader_program& surf_shader = ctx.ref_surface_shader_program(false);

		init_cube();
		fractal = cubes_fractal();

		// this defined value type is then used for the automatic array binding facilities of the framework, which we will use for rendering the quad
		// vec2type used in the demo is for texCoor -> uv
		cgv::render::type_descriptor vec3type = cgv::render::element_descriptor_traits<cgv::vec3>::get_type_descriptor(vertices[0].pos);

		//&&success creates the condition that the previous statement had to be true for this one to run
		success = vb.create(ctx, &(vertices[0]), vertices.size()) && success;
		success = vertex_array.create(ctx) && success;
		// this is done for the vertex position of the quad 
		success = vertex_array.set_attribute_array(
			ctx, surf_shader.get_position_index(), vec3type, vb,
			0, // position is at start of the struct <-> offset = 0
			vertices.size(), // number of position elements in the array
			sizeof(vertex) // stride from one element to next
		) && success;

		// for the normals in the shader
		success = vertex_array.set_attribute_array(
			ctx, surf_shader.get_normal_index(), vec3type, vb,
			sizeof(cgv::vec3), // normals follow after position
			vertices.size(), // number of normal elements in the array
			sizeof(vertex) // stride from one element to next
		) && success;
		

		//  generate the fractal structure 
		fractal.use_vertex_array(&vertex_array, vertices.size(), GL_TRIANGLES);

		return success;
	}

	void draw(cgv::render::context& ctx)
	{

		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT | GL_POLYGON_BIT);
		glClearColor(bgcolor.R(), bgcolor.G(), bgcolor.B(), bgcolor.alpha());
		glClear(GL_COLOR_BUFFER_BIT);

		// create the def shader twice?? (so is on the demo)
		cgv::render::shader_program& surf_shader = ctx.ref_surface_shader_program(false);
		surf_shader.enable(ctx);
		
		// what does this color affect 
		ctx.set_color(cgv::rgb(1.0f));

		ctx.push_modelview_matrix();


		//ctx.tesselate_unit_square();
		draw_my_unit_cube(ctx);
		fractal.draw_recursive(ctx, cgv::rgb(1.0f, 0.5f, 0.5f), 3, 0);
		
		

		glPopAttrib();
		ctx.pop_modelview_matrix();
		surf_shader.disable(ctx);
	}
	
	void init_cube() 
	{
		vertices.clear();

		// struct for the faces of the cube
		struct Face { int v0, v1, v2, v3; cgv::vec3 normal; };
		float s = 0.5;
		cgv::vec3 points[8] = {
			{-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s}, // back face
			{-s, -s,  s}, {s, -s,  s}, {s, s,  s}, {-s, s,  s}  // front face
		};

		// counter clockwise 
		Face faces[6] = {
			{4, 5, 6, 7, {0, 0, 1}}, // front
			{5, 1, 2, 6, {1, 0, 0}}, // right 
			{1, 0, 3, 2, {0, 0, -1}}, // back
			{0, 4, 7, 3, {-1, 0, 0}}, // left
			{7, 6, 2, 3, {0, 1, 0}}, //top
			{1, 5, 4, 0, {0, -1, 0}} //bottom
		};


		for (const auto& f : faces) {
			// first triangle 
			vertices.push_back({points[f.v0], f.normal});
			vertices.push_back({points[f.v1], f.normal});
			vertices.push_back({points[f.v2], f.normal});
			// second one
			vertices.push_back({points[f.v0], f.normal});
			vertices.push_back({points[f.v2], f.normal});
			vertices.push_back({points[f.v3], f.normal});
		}
	}

	void draw_my_unit_cube(cgv::render::context& ctx)
	{
		vertex_array.enable(ctx);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
		vertex_array.disable(ctx);
	}

// [END] Tasks 0.2a, 0.2b and 0.2c
// ************************************************************************************/


};


// ************************************************************************************/
// Task 0.2a: register an instance of your drawable.


// Both of the following options will create the plugin instance 
// The first one will also automatically load it

//cgv::base::object_registration<cubes_drawable> cgv_demo_registration(
//	"cubes_drawable" 
//);

// To load the plugin with this method, click on the top left corner of the app window
cgv::base::factory_registration<cubes_drawable> cgv_demo_factory(
	"cubes_drawable", // menu path
	'D',            // the shortcut - capital D means ctrl+d
	true            // whether the class is supposed to be a singleton
);
