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

// for cgv_demo
#define FB_MAX_RESOLUTION 2048

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
	float cube_color_r, cube_color_g, cube_color_b;
	int recursion_level;

	// an offscreen framebuffer is a buffer that contains information which should not be directly rendered to the screen
	// but instead is used as a texture for rendering to the screen
	// In the demo, it is used to render text to a texture, which is then applied to a quade
	// so here is not necessary
	cgv::rgba cube_color;

	enum RenderMode {
		BUILTIN,
		INTERLEAVED,
		NON_INTERLEAVED,
		SINGLE_VERTEX_BUFFER,
	};

	RenderMode mode;

	struct vertex {
		cgv::vec3 pos;
		cgv::vec3 normal;
		cgv::vec4 color;
	};
	
	std::vector<vertex> vertices;
	cgv::render::vertex_buffer vb;
	cgv::render::attribute_array_binding vertex_array;

	std::vector<cgv::vec3> positions;
	std::vector<cgv::vec3> normals;

	cgv::render::vertex_buffer vb_pos, vb_norm;
	cgv::render::attribute_array_binding vao_non_interleaved;

	// for the fractal structure
	cubes_fractal fractal;


	std::vector<vertex> all_vertices;
	cgv::render::vertex_buffer vb_all;
	cgv::render::attribute_array_binding vao_all;

	bool rebuild_geometry;

public:
	cubes_drawable() :
		cube_color_r(1.0f), cube_color_g(1.0f), cube_color_b(0.0f),
		cube_color(cube_color_r, cube_color_g, cube_color_b),
		recursion_level(3),
		mode(BUILTIN), rebuild_geometry(false)
	{
	}

	// what does this make? --> it names the plugin (top left corner of the app window)
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
			rh.reflect_member("cube_color_r", cube_color_r) &&
			rh.reflect_member("cube_color_g", cube_color_g) &&
			rh.reflect_member("cube_color_b", cube_color_b) &&
			rh.reflect_member("recursion_level", recursion_level);
	}


	// this method is called after the value of a reflected variable has been changed with the GUI
	// so this method is not used to assign the changes of the GUI to the backend
	// it is only needed if extra functionality must be added after changing somethin
	void on_set(void* member_ptr)
	{
		 /// Color update 
		if (member_ptr == &cube_color_r|| member_ptr == &cube_color_g|| member_ptr == &cube_color_b)
		{
			cube_color.R() = cube_color_r;
			cube_color.G() = cube_color_g;
			cube_color.B() = cube_color_b;
			rebuild_geometry = true; 
			update_member(&cube_color);
		}

		// this will be used when the user sets new values with the GUI
		if (member_ptr == &cube_color)
		{
			cube_color_r = cube_color.R();
			cube_color_g = cube_color.G();
			cube_color_b = cube_color.B();
			rebuild_geometry = true;
		}

		if (member_ptr == &mode) {
			rebuild_geometry = true;
		}

		if (mode == BUILTIN)
			fractal.use_vertex_array(nullptr, 0, GL_TRIANGLES);
		else if (mode == INTERLEAVED)
			fractal.use_vertex_array(&vertex_array, vertices.size(), GL_TRIANGLES);
		else if (mode == NON_INTERLEAVED)
			fractal.use_vertex_array(&vao_non_interleaved, vertices.size(), GL_TRIANGLES);
		

		// make sure the GUI reflects new state in the case the write did not originate form GUI interaction
		update_member(member_ptr);

		if (this->is_visible())
			post_redraw();
	}

	cgv::vec4 compute_color(int level) {
		float t = level / float(recursion_level);
		// Use the RGB values from the GUI (cube_color_r, etc.) 
		// and perhaps just dim them based on the recursion level
		return cgv::vec4(
			cube_color_r * (1.0f - t * 0.5f),
			cube_color_g * (1.0f - t * 0.5f),
			cube_color_b * (1.0f - t * 0.5f),
			1.0f
		);
	}

	void build_fractal_geometry(
		const cgv::math::fmat<double, 4, 4>& transform,
		int max_depth,
		int level
	)
	{
		auto T = transform * cgv::math::scale4<double>(0.5, 0.5, 0.5);

		for (auto& v : vertices) {
			vertex out;

			cgv::vec3 p = v.pos;
			out.pos = cgv::vec3(
				T(0, 0) * p.x() + T(0, 1) * p.y() + T(0, 2) * p.z() + T(0, 3),
				T(1, 0) * p.x() + T(1, 1) * p.y() + T(1, 2) * p.z() + T(1, 3),
				T(2, 0) * p.x() + T(2, 1) * p.y() + T(2, 2) * p.z() + T(2, 3)
			);

			out.normal = v.normal;
			out.normal.normalize();

			cgv::vec4 col = compute_color(level);

			out.color = col;

			all_vertices.push_back(out);
		}

		if (level >= max_depth)
			return;

		int num_children = (level == 0) ? 4 : 3;

		for (int i = 0; i < num_children; i++) {
			auto childT =
				T *
				cgv::math::rotate4<double>(i * 90 - 90, 0, 0, 1) *
				cgv::math::translate4<double>(2, 0, 0);

			build_fractal_geometry(childT, max_depth, level + 1);
		}
	}

	void regenerate_geometry(cgv::render::context& ctx)
	{
		all_vertices.clear();

		cgv::math::fmat<double, 4, 4> I;
		I.identity();

		build_fractal_geometry(I, recursion_level, 0);

		if (all_vertices.empty())
			return;

		auto vec3type =
			cgv::render::element_descriptor_traits<cgv::vec3>
			::get_type_descriptor(all_vertices[0].pos);

		auto vec4type =
			cgv::render::element_descriptor_traits<cgv::vec4>
			::get_type_descriptor(all_vertices[0].color);

		vb_all.destruct(ctx);
		vao_all.destruct(ctx);

		vb_all.create(ctx, &all_vertices[0], all_vertices.size());
		vao_all.create(ctx);

		auto& shader = ctx.ref_surface_shader_program(true);

		vao_all.set_attribute_array(
			ctx, shader.get_position_index(),
			vec3type, vb_all,
			0,
			all_vertices.size(),
			sizeof(vertex)
		);

		vao_all.set_attribute_array(
			ctx, shader.get_normal_index(),
			vec3type, vb_all,
			sizeof(cgv::vec3),
			all_vertices.size(),
			sizeof(vertex)
		);

		vao_all.set_attribute_array(
			ctx,
			shader.get_color_index(),
			vec4type,
			vb_all,
			2 * sizeof(cgv::vec3),      // offset after pos + normal
			all_vertices.size(),
			sizeof(vertex)
		);

	}

	bool gui_check_value(cgv::gui::control<int>& ctrl)
	{
		if (ctrl.controls(&recursion_level))
		{
			// clamping the recursion
			if (recursion_level < 0)
				recursion_level = 0;
			else if (recursion_level > 8)
				recursion_level = 8;
		}
		return true;
	}

	// this is called when the user changes a value in the GUI, after the value has been validated by gui_check_value
	void gui_value_changed(cgv::gui::control<int>& ctrl)
	{
		post_redraw();
	}

	// used for the cgv::gui::provider interface
	void create_gui(void)
	{
		cgv::gui::control<int>* ctrl = add_control(
			"Recusion Level", recursion_level, "value_slider", "min=0;max=8;step=1;ticks=true"
		).operator->();
		cgv::signal::connect(ctrl->check_value, this, &cubes_drawable::gui_check_value);
		cgv::signal::connect(ctrl->value_change, this, &cubes_drawable::gui_value_changed);

		add_member_control(this, "Cube Color", cube_color);

		add_member_control(this, "Render Mode", mode, "dropdown",
			"enums='BUILTIN,INTERLEAVED,NON_INTERLEAVED,SINGLE_VERTEX_BUFFER'");
	}

	// used for the cgv::render::drawable interface
	// called somewhere within the framework 
	bool init(cgv::render::context& ctx)
	{
		bool success = true;
		cgv::render::shader_program& surf_shader = ctx.ref_surface_shader_program(true);

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

		positions.clear();
		normals.clear();
		for (auto& v : vertices) {
			positions.push_back(v.pos);
			normals.push_back(v.normal);
		}

		vb_pos.create(ctx, &positions[0], positions.size());
		vb_norm.create(ctx, &normals[0], normals.size());

		vao_non_interleaved.create(ctx);

		// POSITION
		vao_non_interleaved.set_attribute_array(
			ctx, surf_shader.get_position_index(),
			vec3type, vb_pos,
			0,
			positions.size(),
			sizeof(cgv::vec3)
		);

		// NORMAL
		vao_non_interleaved.set_attribute_array(
			ctx, surf_shader.get_normal_index(),
			vec3type, vb_norm,
			0,
			normals.size(),
			sizeof(cgv::vec3)
		);
		

		if (mode == BUILTIN)
			fractal.use_vertex_array(nullptr, 0, GL_TRIANGLES);
		else if (mode == INTERLEAVED)
			fractal.use_vertex_array(&vertex_array, vertices.size(), GL_TRIANGLES);
		else if (mode == NON_INTERLEAVED)
			fractal.use_vertex_array(&vao_non_interleaved, vertices.size(), GL_TRIANGLES);
		else if (mode == SINGLE_VERTEX_BUFFER) {
			regenerate_geometry(ctx);

		}


		return success;
	}


	void draw(cgv::render::context& ctx)
	{

		

		// saving the current OpenGL state
		// if glClearColor is used, the quad will also be affected
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT | GL_POLYGON_BIT);

		ctx.push_modelview_matrix();

		cgv::render::shader_program& surf_shader = ctx.ref_surface_shader_program(true);
		surf_shader.enable(ctx);

		//ctx.tesselate_unit_square();
		if (mode == SINGLE_VERTEX_BUFFER) {
			if(rebuild_geometry)
			{
				regenerate_geometry(ctx);
				rebuild_geometry = false;
			}

			cgv::media::illum::surface_material mat;
			mat.diffuse_reflectance = cube_color;
			ctx.set_material(mat);

			vao_all.enable(ctx);
			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)all_vertices.size());
			vao_all.disable(ctx);
		}
		else {
			// draw_my_unit_cube(ctx);
			fractal.draw_recursive(ctx, cube_color, recursion_level, 0);
		}
		
		ctx.pop_modelview_matrix();
		surf_shader.disable(ctx);
		glPopAttrib();
	}
	
	void init_cube() 
	{
		vertices.clear();

		// struct for the faces of the cube
		struct Face { int v0, v1, v2, v3; cgv::vec3 normal; };
		float s = 1;
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
