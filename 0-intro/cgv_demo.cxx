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
#include <cgv/render/texture.h>
#include <cgv/render/frame_buffer.h>
#include <cgv/render/vertex_buffer.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv/math/ftransform.h>

// Framework standard libraries
#include <cgv_gl/gl/gl.h>

// Framework standard plugins
#include <plugins/cmf_tt_gl_font/tt_gl_font_server.h>

// Some constant symbols
#define FB_MAX_RESOLUTION 2048



// The CGV framework demonstration class
class cgv_demo
	: public cgv::base::base,      // This class supports reflection
	  public cgv::gui::provider,   // Instances of this class provde a GUI
	  public cgv::render::drawable // Instances of this class can be rendered
{
protected:

	////
	// Stuff we expose via reflection

	// Some text
	std::string text;

	// Text size in pixels
	unsigned int text_size;

	// Whether the text should be rendered in normal or italic face
	enum FontFaceAttrib { REGULAR, BOLD, ITALIC, BOLD_ITALIC } text_face_attrib;

	// Text position in cursor coordinates
	cgv::math::fvec<int, 2> text_pos;

	// Resolution of the offscreen framebuffer
	cgv::math::fvec<int, 2> fb_resolution;

	// Background color of the offscreen framebuffer. We use individual floats because
	// the cgv::media::color datatype does not currently support reflection, and fvecs
	// can not currently be set easily in a config file. We still want to use
	// cgv::media::color because of its nice GUI representation, so we have to copy the
	// values around.
	float fb_bgcolor_r, fb_bgcolor_g, fb_bgcolor_b;

	// Whether to draw the backside of the quad
	bool draw_backside;

	//*********************************************************************************/
	// Task 1.1: Add some data member to indicate whether to use custom tesselation of
	//           the quad or the one built into the cgv::render::context. Make sure to
	//           initialize this to "do not use custom quad" when constructing the
	//           object.

	// < your code here >

	// [END] Task 1.1
	//*********************************************************************************/


	////
	// Internal stuff we don't expose via reflection

	// Font face for rendering the text
	cgv::media::font::font_face_ptr font_face;

	// Offscreen framebuffer
	cgv::render::frame_buffer fb;
	cgv::render::texture texture;
	cgv::media::color<float> bgcolor;

	// Geometry buffers
	struct vertex {
		cgv::vec3 pos;
		cgv::vec2 tcoord;
	};
	std::vector<vertex> vertices;
	cgv::render::vertex_buffer vb;
	cgv::render::attribute_array_binding vertex_array;

	// Flag for checking whether we have to reinit due to change in desired offscreen
	// framebuffer resolution
	bool fb_invalid;


public:

	// Default constructor
	cgv_demo()
		: text("Hello World"), text_size(24), text_face_attrib(REGULAR), text_pos(8, 32),
		  fb_resolution(512, 256), fb_invalid(true),
		  texture("uint8[R,G,B,A]", cgv::render::TF_LINEAR, cgv::render::TF_LINEAR),
		  fb_bgcolor_r(0.9f), fb_bgcolor_g(0.9f), fb_bgcolor_b(0.9f),
		  bgcolor(fb_bgcolor_r, fb_bgcolor_g, fb_bgcolor_b), draw_backside(true)
	{
		// Make sure the font server knows about the fonts packaged with the exercise
		cgv::scan_fonts("./data/Fonts");
		font_face = cgv::media::font::find_font_or_default("Open Sans", true)->get_font_face(
			(cgv::media::font::FontFaceAttributes)text_face_attrib
		);
		
	}

	// Should be overwritten to sensibly implement the cgv::base::named interface
	std::string get_type_name(void) const
	{
		return "cgv_demo";
	}

	// Part of the cgv::base::base interface, can be implemented to make data members of
	// this class available as named properties, e.g. for use with config files
	bool self_reflect(cgv::reflect::reflection_handler& rh)
	{
		// This is currently necessary to trick the framework into treating enums like
		// integers for the purpose of reflection. A fix is in development.
		unsigned* text_face_attrib_uint = (unsigned*)&text_face_attrib;

		// Task 1.1: make sure your quad tesselation toggle can be set via config
		//           file.
		// Reflect the properties
		return
			rh.reflect_member("text", text) &&
			rh.reflect_member("text_face_attrib", *text_face_attrib_uint) &&
			rh.reflect_member("text_size", text_size) &&
			rh.reflect_member("text_pos", text_pos) &&
			rh.reflect_member("fb_resolution", fb_resolution) &&
			rh.reflect_member("fb_bgcolor_r", fb_bgcolor_r) &&
			rh.reflect_member("fb_bgcolor_g", fb_bgcolor_g) &&
			rh.reflect_member("fb_bgcolor_b", fb_bgcolor_b) &&
			rh.reflect_member("draw_backside", draw_backside);
	}

	// Part of the cgv::base::base interface, should be implemented to respond to write
	// access to reflected data members of this class, e.g. from config file processing
	// or gui interaction.
	void on_set(void* member_ptr)
	{
		// Check the text_size to make sure the text fits in the offscreen buffer
		// vertically
		if (member_ptr == &text_size)
			text_size = std::min(text_size, (unsigned)fb_resolution.y());
		// Recreate the font face if the face attribute changed
		if (member_ptr == &text_face_attrib)
			font_face = cgv::media::font::find_font_or_default("Open Sans", true)->get_font_face(
				(cgv::media::font::FontFaceAttributes)text_face_attrib
			);

		// Reflect the change to fb_bgcolor in bgcolor (can only happen via reflection)
		if (member_ptr == &fb_bgcolor_r || member_ptr == &fb_bgcolor_g ||
			member_ptr == &fb_bgcolor_b)
		{
			bgcolor.R() = fb_bgcolor_r;
			bgcolor.G() = fb_bgcolor_g;
			bgcolor.B() = fb_bgcolor_b;
			update_member(&bgcolor);
		}
		// ...and vice versa (can only happen via GUI interaction)
		if (member_ptr == &bgcolor)
		{
			fb_bgcolor_r = bgcolor.R();
			fb_bgcolor_g = bgcolor.G();
			fb_bgcolor_b = bgcolor.B();
		}

		//*****************************************************************************/
		// Task 1.1: [OPTIONAL] If necessary, add additional handling for your custom
		//           quad toggle here.

		//*****************************************************************************/

		// Make sure the GUI reflects the new state, in case the write access did not
		// originate from GUI interaction
		update_member(member_ptr);

		// Also trigger a redraw in case the drawable node is active
		if (this->is_visible())
			post_redraw();
	}

	// We use this for validating GUI input
	bool gui_check_value(cgv::gui::control<int>& ctrl)
	{
		if (ctrl.controls(&text_pos.x()))
		{
			// Bounds check
			if (ctrl.get_new_value() < 0)
				ctrl.set_new_value(0);
			else if (ctrl.get_new_value() >= fb_resolution.x())
				ctrl.set_new_value(fb_resolution.x() - 1);
		}
		else if (ctrl.controls(&text_pos.y()))
		{
			// Bounds check
			if (ctrl.get_new_value() < 0)
				ctrl.set_new_value(0);
			else if (ctrl.get_new_value() >= fb_resolution.y())
				ctrl.set_new_value(fb_resolution.y() - 1);
		}
		else if (ctrl.controls(&fb_resolution.x()))
		{
			// Bounds check
			if (ctrl.get_new_value() < signed(text_size)
				|| ctrl.get_new_value() > FB_MAX_RESOLUTION)
				// Don't accapt this value
				return false;

			// Also make sure the text stays inside bounds when changing resolution
			if (ctrl.controls(&fb_resolution.x()) && text_pos.x() >= ctrl.get_new_value())
			{
				text_pos.x() = ctrl.get_new_value() - 1;
				update_member(&text_pos.x());
			}
		}
		else if (ctrl.controls(&fb_resolution.y()))
		{
			// Bounds check
			if (ctrl.get_new_value() < signed(text_size)
				|| ctrl.get_new_value() > FB_MAX_RESOLUTION)
				// Don't accapt this value
				return false;

			// Also make sure the text stays inside bounds when changing resolution
			if (ctrl.controls(&fb_resolution.y()) && text_pos.y() >= ctrl.get_new_value())
			{
				text_pos.y() = ctrl.get_new_value() - 1;
				update_member(&text_pos.y());
			}
		}

		//*****************************************************************************/
		// Task 1.1: [OPTIONAL] If necessary, add additional handling for your custom
		//           quad toggle here.

		//*****************************************************************************/

		// Check passed
		return true;
	}

	// We use this for acting upon validated GUI input
	void gui_value_changed(cgv::gui::control<int>& ctrl)
	{
		if (ctrl.controls(&fb_resolution.x()) || ctrl.controls(&fb_resolution.y()))
			// Indicate that the framebuffer must be reinitialized
			fb_invalid = true;

		//*****************************************************************************/
		// Task 1.1: [OPTIONAL] If necessary, add additional handling for your custom
		//           quad toggle here.

		//*****************************************************************************/

		// Redraw the scene
		post_redraw();
	}

	// Required interface for cgv::gui::provider
	void create_gui(void)
	{
		// Simple controls. Notifies us of GUI input via the on_set() method.
		// - section header
		add_decorator("Text properties", "heading", "level=1");
		// - the text
		add_member_control(this, "contents", text);
		// - the text size
		add_member_control(this, "text size", text_size);
		// - font face attribute selection
		add_member_control(
			this, "text face", text_face_attrib, "dropdown",
			"enums='REGULAR,BOLD,ITALIC,BOLD_ITALIC'"
		);

		// There are currently no vector controls, so add each component individually as
		// a slider. Also, we obtain the actual control object to do more advanced
		// callback logic than the implicit on_set() above.
		// - section header
		add_decorator("Offscreen framebuffer", "heading", "level=1");
		// - text x position
		cgv::gui::control<int>* ctrl = add_control(
			"text pos x", text_pos.x(), "value_slider",
			"min=0;max=" + std::to_string(FB_MAX_RESOLUTION - 1) + ";ticks=false"
		).operator->();
		cgv::signal::connect(ctrl->check_value, this, &cgv_demo::gui_check_value);
		cgv::signal::connect(ctrl->value_change, this, &cgv_demo::gui_value_changed);
		// - text y position
		ctrl = add_control(
			"text pos y", text_pos.y(), "value_slider",
			"min=0;max=" + std::to_string(FB_MAX_RESOLUTION - 1) + ";ticks=false"
		).operator->();
		cgv::signal::connect(ctrl->check_value, this, &cgv_demo::gui_check_value);
		cgv::signal::connect(ctrl->value_change, this, &cgv_demo::gui_value_changed);
		// - offscreen texture width
		ctrl = add_control(
			"texture width", fb_resolution.x(), "value_slider",
			"min=32;max=" + std::to_string(FB_MAX_RESOLUTION) + ";ticks=false"
		).operator->();
		cgv::signal::connect(ctrl->check_value, this, &cgv_demo::gui_check_value);
		cgv::signal::connect(ctrl->value_change, this, &cgv_demo::gui_value_changed);
		// - offscreen texture height
		ctrl = add_control(
			"texture height", fb_resolution.y(), "value_slider",
			"min=32;max=" + std::to_string(FB_MAX_RESOLUTION) + ";ticks=false"
		).operator->();
		cgv::signal::connect(ctrl->check_value, this, &cgv_demo::gui_check_value);
		cgv::signal::connect(ctrl->value_change, this, &cgv_demo::gui_value_changed);

		// Finally, another simple control for setting the offscreen framebuffer
		// background color
		add_member_control(this, "tex background", bgcolor);

		// ... and for the switch to disable drawing the backside of the quad.
		add_member_control(this, "draw backside", draw_backside);

		//*****************************************************************************/
		// Task 1.1: add a GUI control to switch between custom tesselation of the quad
		//           and the one built into the cgv::render::context.

		// < Your code here >

		// [END] Task 1.1
		//*****************************************************************************/
	}

	// Part of the cgv::render::drawable interface, can be overwritten if there is some
	// intialization work to be done that needs a set-up and ready graphics context,
	// which usually you don't have at object construction time. Should return true if
	// the initialization was successful, false otherwise.
	bool init(cgv::render::context& ctx)
	{
		// Keep track of success - do it this way (instead of e.g. returning false
		// immediatly) to perform every init step even if some go wrong.
		bool success = true;

		// Init framebuffer texture
		texture.set_width(fb_resolution.x());
		texture.set_height(fb_resolution.y());
		success = texture.create(ctx) && success;

		// Init framebuffer object
		success = fb.create(ctx, fb_resolution.x(), fb_resolution.y()) && success;
		success = fb.attach(ctx, texture) && success;
		// note that we don't attach any depth buffer (we don't need it here), only rgba
		success = fb.is_complete(ctx) && success;

		// Init geometry buffers
		// - get a reference to the default shader, from which we're going to query named
		//   locations of the vertex layout
		cgv::render::shader_program& default_shader
			= ctx.ref_default_shader_program(true /* true for texture support */);
		// - generate actual geometry
		init_unit_square_geometry();
		// - obtain type descriptors for the automatic array binding facilities of the
		//   framework
		cgv::render::type_descriptor
			vec2type =
			cgv::render::element_descriptor_traits<cgv::vec2>
			::get_type_descriptor(vertices[0].tcoord),
			vec3type =
			cgv::render::element_descriptor_traits<cgv::vec3>
			::get_type_descriptor(vertices[0].pos);
		// - create buffer objects
		success = vb.create(ctx, &(vertices[0]), vertices.size()) && success;
		success = vertex_array.create(ctx) && success;
		success = vertex_array.set_attribute_array(
			ctx, default_shader.get_position_index(), vec3type, vb,
			0, // position is at start of the struct <-> offset = 0
			vertices.size(), // number of position elements in the array
			sizeof(vertex) // stride from one element to next
		) && success;
		success = vertex_array.set_attribute_array(
			ctx, default_shader.get_texcoord_index(), vec2type, vb,
			sizeof(cgv::vec3), // tex coords follow after position
			vertices.size(), // number of texcoord elements in the array
			sizeof(vertex) // stride from one element to next
		) && success;

		// Flag offscreen framebuffer as taken care of
		fb_invalid = false;

		// All initialization has been attempted
		return success;
	}

	// Part of the cgv::render::drawable interface, can be overwritten if there is some
	// work to be done before actually rendering a frame.
	void init_frame(cgv::render::context& ctx)
	{
		// Check if we need to recreate anything
		if (fb_invalid)
		{
			fb.destruct(ctx);
			texture.destruct(ctx);
			init(ctx);
		}
	}

	// Should be overwritten to sensibly implement the cgv::render::drawable interface
	void draw(cgv::render::context& ctx)
	{
		////
		// Render text to texture via framebuffer object

		// Make our offscreen framebuffer the active render target
		fb.enable(ctx);

		// Usually, the framework takes care of clearing the (main) framebuffer, so in
		// order to not disturb that mechanism, we save the OpenGL state pertaining to
		// the color part of the framebuffer when we set a clear-color ourselves for
		// offscreen use.
		// Also, we're adjusting the framework-managed viewport to be in line with the
		// offscreen framebuffer dimensions, so save the current viewport as well.
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT | GL_POLYGON_BIT);
		// Clear the offscreen color buffer with the desired background color
		glClearColor(bgcolor.R(), bgcolor.G(), bgcolor.B(), bgcolor.alpha());
		glClear(GL_COLOR_BUFFER_BIT);

		// Adjust viewport
		glViewport(0, 0, fb.get_width(), fb.get_height());

		// Draw text
		ctx.ref_default_shader_program().enable(ctx);
		ctx.set_color(cgv::rgb(0.0f));
		ctx.set_cursor(text_pos.x(), text_pos.y());
		ctx.enable_font_face(font_face, (float)text_size);
		ctx.output_stream() << text;
		ctx.output_stream().flush();
		ctx.ref_default_shader_program().disable(ctx);
		glPopAttrib();
		fb.disable(ctx);


		////
		// Draw the contents of this node.

		// Shortcut to the built-in default shader with lighting and texture support
		cgv::render::shader_program& default_shader =
			ctx.ref_default_shader_program(true /* true for texture support */);

		// Enable test texture in texture unit 0
		texture.enable(ctx, 0);

		// Enable shader program we want to use for drawing
		default_shader.enable(ctx);

		// Set the "color" vertex attribute for all geometry drawn hereafter, except if
		// it explicitely specifies its own color data by means of an attribute array.
		// Note that this only works for shaders that define a vec4 attribute named
		// "color" in their layout specification.
		// We want white to retain the original color information in the texture.
		ctx.set_color(cgv::rgb(1.0f));

		// Draw the node's scene geometry - save current modelview matrix because we have
		// some node-internal transformations that we do not want to spill over to other
		// drawables.
		ctx.push_modelview_matrix();
		// Account for aspect ratio of the offscreen texture
		ctx.mul_modelview_matrix(cgv::math::scale4(
			1.0, double(fb_resolution.y()) / double(fb_resolution.x()), 1.0
		));
		// Draw front side
		//*********************************************************************/
		// Task 1.1: If enabled, render the quad with custom tesselation
		//           instead of using tesselate_unit_square(). You can invoke
		//           the method draw_my_unit_square() for this.
			ctx.tesselate_unit_square();

		//*********************************************************************/

		// Draw back side
		ctx.mul_modelview_matrix(cgv::math::rotate4(180.0, 0.0, 1.0, 0.0));
		ctx.mul_modelview_matrix(cgv::math::scale4(-1.0, 1.0, 1.0));
		glPushAttrib(GL_POLYGON_BIT);
		glCullFace(GL_FRONT);
		//*****************************************************************/
		// Task 1.1: If enabled, render the quad with custom tesselation
		//           instead of using tesselate_unit_square(). Again, you
		//           can invoke the method draw_my_unit_square() for this.
			if (draw_backside)
				ctx.tesselate_unit_square();

		//*****************************************************************/
		glPopAttrib();
		ctx.pop_modelview_matrix();

		// Disable shader program and texture
		default_shader.disable(ctx);
		texture.disable(ctx);
	}

	// Creates the custom geometry for the quad
	void init_unit_square_geometry(void)
	{
		// Prepare array
		vertices.resize(4);
		// lower-left
		vertices[0].pos.set(-1, -1, 0); vertices[0].tcoord.set(0, 0);
		// lower-right
		vertices[1].pos.set(1, -1, 0); vertices[1].tcoord.set(1, 0);
		// top-left
		vertices[2].pos.set(-1, 1, 0); vertices[2].tcoord.set(0, 1);
		// top-right
		vertices[3].pos.set(1, 1, 0); vertices[3].tcoord.set(1, 1);
	}

	// Draw method for a custom quad
	void draw_my_unit_square(cgv::render::context& ctx)
	{
		vertex_array.enable(ctx);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)vertices.size());
		vertex_array.disable(ctx);
	}
};

// Create an instance of the demo class at plugin load and register it with the framework
cgv::base::object_registration<cgv_demo> cgv_demo_registration("");

// The following could be used to register the class with the framework but NOT create it
// upon plugin load. Instead, the user can create an instance from the application menu.
// However, config files are not straight-forward to use in this case, which is why we
// go for the method above.
/*
	cgv::base::factory_registration<cgv_demo> cgv_demo_factory(
		"new/cgv_demo", // menu path
		'D',            // the shortcut - capital D means ctrl+d
		true            // whether the class is supposed to be a singleton
	);
*/
