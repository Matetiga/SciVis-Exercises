// This source code is property of the Computer Graphics and Visualization chair of the
// TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#include <cgv/base/node.h>
#include "volume.h"
#include "volume_io.h"
#include <cgv/utils/scan.h>
#include <cgv/utils/file.h>
#include <cgv/media/color_scale.h>
#include <cgv/render/drawable.h>
#include <cgv/render/texture.h>
#include <cgv/render/shader_program.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv_gl/gl/gl.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/event_handler.h>
#include <cgv/gui/key_event.h>
#include <cgv/gui/mouse_event.h>
#include <cgv/render/clipped_view.h>

enum NewColorScale {
	CS_TEMPERATURE,
	/************************************************************************************
	 tasks 4.2b: Add new color scales here */

	 /*<your_code_here>*/

	/************************************************************************************/
};

class volume_view : 
	public cgv::base::node,
	public cgv::render::drawable,
	public cgv::gui::event_handler,
	public cgv::gui::provider
{
public:
	/// type for index pairs
	typedef cgv::math::fvec<int, 2> ivec2;
	/// type for index tripels
	typedef cgv::math::fvec<int, 3> ivec3;
	/// type for index quadrupels
	typedef cgv::math::fvec<int, 4> ivec4;
protected:
	// volume data set
	volume V;

	std::string file_name;
	/// whether to use tri-linear texture interpolation
	bool interpolate;
	/// for each major direction the extent of the volume initialized to the extent of the volume after read
	cgv::vec3 extent;
	/// for each major direction the number of voxels in the volume
	ivec3 dimensions;
	ivec3 slice_indices;
	bool show_box;
	bool show_orthogonal_slices[3];
	bool show_oblique_slice;
	bool show_volume;
	cgv::vec3 orthogonal_slice_center;
	cgv::vec3 oblique_slice_normal;
	float oblique_slice_distance;
	cgv::box3 volume_clipping_box;
	float raycasting_step_width;
	float volume_scale;

	/// material used to render the bounding box faces
	cgv::media::illum::surface_material box_material;
	cgv::rgb wire_box_color;

	float emission_gamma;
	float absorption_gamma;
	/// whether volume texture is out of date
	bool volume_texture_out_of_date;
	/// whether volume texture is out of date
	bool transfer_function_texture_out_of_date;
	/// store pointer to view which is controlled by the user
	cgv::render::clipped_view* view_ptr;
	/// texture storing the volume data
	cgv::render::texture volume_texture;
	/// 
	cgv::render::shader_program slicer_prog;

	/// texture storing transfer function
	cgv::render::texture transfer_function_texture;
	/// predefined color scales
	NewColorScale color_scale;
	/// number of texels in transfer function texture
	unsigned transfer_function_texture_resolution;
	///
	bool transfer_function_changed;
	/// overload with new implementation
	void compute_transfer_function_texture(std::vector<cgv::rgba>& clr_samples)
	{
		float scale = 1.0f / (transfer_function_texture_resolution - 1);

		for (unsigned i = 0; i < transfer_function_texture_resolution; ++i) {
			// define new color
			cgv::rgba clr;
			float v = i * scale;
			// set absoprtion value
			clr[3] = v;

			switch(color_scale){
				/************************************************************************************
				 tasks 4.2c: Create your color scale by filling the *clr_samples*-vector with your own
							   cgv::rgba-colors: Add the rbg-values (emission) of the current color *clr*.
							   (cgv::rgba can be used like a vec4<float> with values in range [0,1].)
							   Note: you can reorganize this function, if it doesn't fit your needs or 
							   add variable before the switch-statement. Just keep in mind that the clr_samples-vector 
							   should be of the size transfer_function_texture_resolution. */

				 /*<your_code_here>*/

				/************************************************************************************/
				case CS_TEMPERATURE: {
					// get predefined cgv::rgb-color as emission value
					reinterpret_cast<cgv::rgb&>(clr) = cgv::media::color_scale(v, (cgv::media::ColorScale)this->color_scale);
					break;
				}
			}

			// store current color
			clr_samples[i] = clr;
		}
	}
	/// 
	void ensure_transfer_function_texture(cgv::render::context& ctx)
	{
		if (transfer_function_changed) {
			transfer_function_changed = false;
			if (transfer_function_texture.is_created())
				transfer_function_texture.destruct(ctx);
			std::vector<cgv::rgba> C;
			C.resize(transfer_function_texture_resolution);

			compute_transfer_function_texture(C);

			cgv::data::data_format df("flt32[R,G,B,A]");
			df.set_width(transfer_function_texture_resolution);
			cgv::data::data_view dv(&df, &C[0]);
			transfer_function_texture.create(ctx, dv);
		}
	}
public:
	/// constructor initializes all member variables
	volume_view() : node("volume_view"), transfer_function_texture("[R,G,B,A]")
	{
		view_ptr = 0;
		show_box = true;
		transfer_function_changed = true;
		transfer_function_texture_resolution = 256;
		color_scale = CS_TEMPERATURE;
		show_orthogonal_slices[0] = show_orthogonal_slices[1] = show_orthogonal_slices[2] = false;
		show_oblique_slice = true;
		show_volume = false;
		orthogonal_slice_center = cgv::vec3(0.5f);
		slice_indices = ivec3(0, 0, 0);
		oblique_slice_normal = cgv::vec3(0.0f, 0.0f, 1.0f);
		oblique_slice_distance = 0.5f;
		volume_clipping_box = cgv::box3(cgv::vec3(0.0f, 0.0f, 0.0f), cgv::vec3(1.0f, 1.0f, 1.0f));
		box_material.set_diffuse_reflectance(cgv::rgb(0.2f, 0.2f, 1.0f));
		wire_box_color = cgv::rgb(0, 0.7f, 0.9f);
		raycasting_step_width = 0.01f;
		emission_gamma = 1;
		absorption_gamma = 0;
		volume_scale = 1;
		volume_texture_out_of_date = false;
		transfer_function_texture_out_of_date = false;
	}
	bool ensure_view_ptr()
	{
		if (view_ptr)
			return true;
		view_ptr = dynamic_cast<cgv::render::clipped_view*>(find_view_as_node());
		return view_ptr != 0;
	}
	/// adjusts view to bounding box of all instances
	void auto_adjust_view()
	{
		if (ensure_view_ptr()) {
			view_ptr->set_scene_extent(cgv::box3(-extent, extent));
			view_ptr->set_default_view();
			view_ptr->set_z_near(extent.length() / dimensions.length());
			view_ptr->set_z_far(5.0f*extent.length());
			std::cout << "z_near = " << view_ptr->get_z_near() << ", z_far=" << view_ptr->get_z_far() << std::endl;
		}
	}
	/// read regular volume file
	bool open_volume(const std::string& _file_name)
	{
		// try to read volume from slice based or regular volume file 
		bool success = false;
		volume_info info;
		success = read_volume(file_name, V, &info);
		if (!success)
			return false;
		extent = info.extent;

		// abort in case of failure 
		if (!success)
			return false;

		// copy dimensions
		dimensions = V.get_dimensions();
		slice_indices = dimensions / 2;
		for (unsigned i = 0; i < 3; ++i) {
			update_member(&extent(i));
			update_member(&dimensions(i));
			if (find_control(slice_indices(i)))
				find_control(slice_indices(i))->set("max", dimensions(i) - 1);
			on_set(&slice_indices(i));
		}
		file_name = _file_name;
		update_member(&file_name);
		volume_texture_out_of_date = true;
		// adjust view
		auto_adjust_view();
		return true;
	}
	bool init(cgv::render::context& ctx)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		ctx.set_bg_clr_idx(4);
		return true;
	}
	/// create shader programs and upload texture data
	void init_frame(cgv::render::context& ctx)
	{
		if (volume_texture_out_of_date) {
			if (volume_texture.is_created())
				volume_texture.destruct(*get_context());
			if (!volume_texture.create(*get_context(), V.get_data_view())) {
				std::cerr << "could not create volume texture" << std::endl;
				abort();
			}
			volume_texture_out_of_date = false;
		}

		ensure_transfer_function_texture(ctx);

		if (!slicer_prog.is_created()) {
			if (!slicer_prog.build_program(ctx, "slicer.glpr")) {
				std::cerr << "could not build slicer shader program" << std::endl;
				abort();
			}
		}
	}
	///
	void draw_domain(cgv::render::context& ctx)
	{
		cgv::box3 B(-0.5f*extent, 0.5f*extent);
		if (show_box) {
			cgv::render::shader_program& wire_prog = ctx.ref_default_shader_program();
			wire_prog.enable(ctx);	
			ctx.set_color(wire_box_color);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			ctx.tesselate_box(B, false, true);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			wire_prog.disable(ctx);

			cgv::render::shader_program& prog = ctx.ref_surface_shader_program();
			prog.enable(ctx);
			prog.set_uniform(ctx, "culling_mode", 1);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT_AND_BACK);
			prog.set_uniform(ctx, "culling_mode", 0);
			// draw backfaces next
			ctx.set_material(box_material);
			ctx.tesselate_box(B, true);
			prog.set_uniform(ctx, "culling_mode", 1);
			glDisable(GL_CULL_FACE);
			prog.disable(ctx);
		}
	}
	///
	float signed_distance_from_oblique_slice(const cgv::vec3& p)
	{
		/************************************************************************************
		 tasks 4.1a: Compute the signed distance between the given point p and the slice which
					   is defined through oblique_slice_normal and oblique_slice_distance. */

		 /*<your_code_here>*/
		 return 0;

		/************************************************************************************/
	}
	/// returns the 3D-texture coordinates of the polygon edges describing the current slice through
	/// the volume
	void construct_oblique_slice(std::vector<cgv::vec3>& polygon)
	{
		/************************************************************************************
		 tasks 4.1b: Classify the volume box corners (vertices) as inside or outside vertices.
					   The volume box is a unit cube.
					   You can use the signed_distance_from_oblique_slice()-method to get the
					   distance between each box corner and the slice. Assume that outside vertices
					   have a positive distance.*/

		 /*<your_code_here>*/

		/************************************************************************************/

		/************************************************************************************
		 tasks 4.1c: Construct the edge points on each edge connecting differently classified
					   corners. Remember that the edge point coordinates are in range [0,1] for
					   all dimensions since they are 3D-texture coordinates. These points are
					   stored in the polygon-vector.
		 tasks 4.1d: Arrange the points along face adjacencies for easier tessellation of the
					   polygon. Store the ordered edge points in the polygon-vector. Create your own
					   helper structures for edge-face adjacenies etc.*/

		 /*<your_code_here>*/

		/************************************************************************************/
	}
	/// draw orthogonal and oblique slices
	void draw_slices(cgv::render::context& ctx)
	{
		// construct slices from triangles
		std::vector<cgv::vec3> P;
		for (unsigned i = 0; i < 3; ++i) {
			if (show_orthogonal_slices[i]) {
				unsigned j = (i + 1) % 3;
				unsigned k = (j + 1) % 3;
				cgv::vec3 p00(0.0f); p00(i) = orthogonal_slice_center(i);
				cgv::vec3 p10 = p00; p10(j) = 1.0f;
				cgv::vec3 p11 = p10; p11(k) = 1.0f;
				cgv::vec3 p01 = p00; p01(k) = 1.0f;
				P.push_back(p00);
				P.push_back(p10);
				P.push_back(p01);

				P.push_back(p01);
				P.push_back(p10);
				P.push_back(p11);
			}
		}
		if (show_oblique_slice) {
			std::vector<cgv::vec3> polygon;
			construct_oblique_slice(polygon);

			/************************************************************************************
			 tasks 4.1e: Tessellate the polygon (its points are stored in the *polygon* vector).
						   You can utilize a triangle fan for this where all triangles share one vertex.
					       Fill vector *P* with the vertex coordinates of each triangle. Due to the
						   usage of glDrawArrays(GL_TRIANGLES...) each triangle consists of three vertices. */

			 /*<your_code_here>*/

			/************************************************************************************/
		}
		if (P.empty())
			return;
		glDisable(GL_CULL_FACE);
		slicer_prog.enable(ctx);
		slicer_prog.set_uniform(ctx, "extent", extent);
		slicer_prog.set_uniform(ctx, "volume_texture", 0);
		slicer_prog.set_uniform(ctx, "transfer_function", 1);
		slicer_prog.set_uniform(ctx, "extent", extent);
		slicer_prog.set_uniform(ctx, "dimensions", dimensions);
		slicer_prog.set_uniform(ctx, "emission_gamma", emission_gamma);
		slicer_prog.set_uniform(ctx, "absorption_gamma", absorption_gamma);
		// enable volume texture on texture unit 0
		if (volume_texture.is_created()) {
			volume_texture.set_mag_filter(interpolate ? cgv::render::TF_LINEAR : cgv::render::TF_NEAREST);
			volume_texture.enable(ctx, 0);
		}
		if (transfer_function_texture.is_created())
			transfer_function_texture.enable(ctx, 1);
		// draw quad using vertex array pointers which is a bit deprecated but still ok
		int tex_coords_idx = slicer_prog.get_attribute_location(ctx, "tex_coords");
		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, tex_coords_idx, P);
		cgv::render::attribute_array_binding::enable_global_array(ctx, tex_coords_idx);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)P.size());
		cgv::render::attribute_array_binding::disable_global_array(ctx, tex_coords_idx);
		// disable 3d texture
		if (volume_texture.is_created())
			volume_texture.disable(ctx);
		if (transfer_function_texture.is_created())
			transfer_function_texture.disable(ctx);
		slicer_prog.disable(ctx);
		glEnable(GL_CULL_FACE);
	}
	///
	void draw(cgv::render::context& ctx)
	{
		draw_slices(ctx);
		draw_domain(ctx);
	}
	///
	void finish_frame(cgv::render::context& ctx)
	{

	}
	/// convert world to texture coordinates
	cgv::vec3 texture_from_world_coordinates(const cgv::vec3& p_world) const
	{
		cgv::vec3 p_texture = (p_world + 0.5f*extent) / extent;
		return p_texture;
	}
	/// convert texture to voxel coordinates
	cgv::vec3 voxel_from_texture_coordinates(const cgv::vec3& p_texture) const
	{
		cgv::vec3 p_voxel = p_texture * dimensions;
		return p_voxel;
	}
	/// convert texture to world coordinates
	cgv::vec3 world_from_texture_coordinates(const cgv::vec3& p_texture) const
	{
		cgv::vec3 p_world = p_texture * extent - 0.5f*extent;
		return p_world;
	}
	/// convert voxel to texture coordinates
	cgv::vec3 texture_from_voxel_coordinates(const cgv::vec3& p_voxel) const
	{
		cgv::vec3 p_texture = p_voxel / dimensions;
		return p_texture;
	}
	/// returns "volume_view"
	std::string get_type_name() const
	{
		return "volume_view";
	}
	/// callback used to notify instance of member changes
	void on_set(void* member_ptr)
	{
		if (member_ptr == &transfer_function_texture_resolution || member_ptr == &color_scale) {
			transfer_function_changed = true;
		}
		// in case that file_name changed, read new volume
		if (member_ptr == &file_name) {
			open_volume(file_name);
			post_redraw();
		}
		if (member_ptr >= &slice_indices && member_ptr < &slice_indices + 1) {
			unsigned i = (int*)member_ptr - &slice_indices(0);
			orthogonal_slice_center(i) = (slice_indices(i) + 0.5f) / dimensions(i);
			update_member(&orthogonal_slice_center(i));
		}
		if (member_ptr >= &orthogonal_slice_center && member_ptr < &orthogonal_slice_center + 1) {
			unsigned i = (float*)member_ptr - &orthogonal_slice_center(0);
			slice_indices(i) = int(orthogonal_slice_center(i)*dimensions(i));
			update_member(&slice_indices(i));
		}
		// update gui of member
		update_member(member_ptr);
		// for all value changes schedule a redraw of the 3D window
		post_redraw();
	}
	/// self reflection is used to publish standard member variables to the set and get mechanism
	bool self_reflect(cgv::reflect::reflection_handler& rh)
	{
		return
			rh.reflect_member("file_name", file_name) &&
			rh.reflect_member("show_orthogonal_slice_x", show_orthogonal_slices[0]) &&
			rh.reflect_member("show_orthogonal_slice_y", show_orthogonal_slices[1]) &&
			rh.reflect_member("show_orthogonal_slice_z", show_orthogonal_slices[2]) &&
			rh.reflect_member("show_oblique_slice", show_oblique_slice) &&
			rh.reflect_member("show_volume", show_volume) &&
			rh.reflect_member("orthogonal_slice_center", orthogonal_slice_center) &&
			rh.reflect_member("oblique_slice_normal", oblique_slice_normal) &&
			rh.reflect_member("oblique_slice_distance", oblique_slice_distance) &&
			rh.reflect_member("raycasting_step_width", raycasting_step_width) &&
			rh.reflect_member("emission_gamma", emission_gamma) &&
			rh.reflect_member("absorption_gamma", absorption_gamma) &&
			rh.reflect_member("volume_scale", volume_scale) &&
			rh.reflect_member("show_box", show_box);
	}
	/// overload and implement this method to handle events
	bool handle(cgv::gui::event& e)
	{
		// check for key events
		if (e.get_kind() == cgv::gui::EID_KEY) {
			cgv::gui::key_event& ke = static_cast<cgv::gui::key_event&>(e);
			if (ke.get_action() == cgv::gui::KA_RELEASE)
				return false;
			switch (ke.get_key()) {
				// view all when space key pressed
			case cgv::gui::KEY_Space:
				auto_adjust_view();
				return true;
				// handle key press events for 'X'-key
			case 'X':
			case 'Y':
			case 'Z':
			{
				unsigned i = ke.get_key() - 'X';
				// we only want to use event if user holds CTRL and SHIFT
				if (ke.get_modifiers() == 0) {
					show_orthogonal_slices[i] = !show_orthogonal_slices[i];
					on_set(&show_orthogonal_slices[i]);
					return true;
				}
				else if (ke.get_modifiers() == cgv::gui::EM_SHIFT) {
					oblique_slice_normal = cgv::vec3(0.0f);
					oblique_slice_normal(i) = 1.0f;
					on_set(&oblique_slice_normal[0]);
					on_set(&oblique_slice_normal[1]);
					on_set(&oblique_slice_normal[2]);
					return true;
				}
				return false;
			}
			default:
				return false;
			}
		}
		return false;
	}
	/// overload to stream help information to the given output stream
	void stream_help(std::ostream& os)
	{
		os << "volume_view:\a\nSpace ... auto adjust view; X/Y/Z ... toggle orthogonal slices; Shift-X/Y/Z ... set oblique slice normal\n";
	}
	///
	void create_gui()
	{
		// font size of level=2 is larger than other gui fonts
		add_decorator("volume view", "heading", "level=2");
		
		// start tree node to interact with volume
		if (begin_tree_node("Volume", dimensions, false, "level=3")) {
			align("\a");
			// add gui of type "file_name" for member file_name
			// file_name gui adds an open button that opens a file dialog to query new file_name
			// the options 'title' and 'filter' configure the file dialog
			add_gui("file_name", file_name, "file_name", "title='open volume';filter='Volume Files(vox,qim,tif,avi) :*.vox;*.qim;*.tif;*.avi|All Files:*.*'");
			// add gui for the vector of pixel counts, where "dimensions" is used only in label of gui element for first vector component
			// using view as gui_type will only show the values but not allow modification
			// by align=' ' the component views are arranged with a small space in one row
			// by options='...' specify options for the component views
			// w=50 specifies the width of each component view in 50 pixels
			add_gui("dimensions", dimensions, "vector", "main_label='first';align=' ';gui_type='view';options='w=50'"); align("\n");
			// add gui for extent vector with component controls of type value_input
			add_gui("extent", extent, "vector", "main_label='first';gui_type='value_input';options='w=50;min=0;max=10';align=' '");
			// start new gui row with smaller Y-spacing by reducing height of gui rows by 6 pixels, going to next row and restoring row height
			align("%Y-=6\n%Y+=6");
			// add another gui for the extent vector with component guis of type wheel within one row
			add_gui("box_extent", extent, "vector", "gui_type='wheel';options='align=\"B\";w=50;h=10;step=0.01;min=0;max=10;log=true';align=' '");
			// start new row with regular spacing
			align("\n");
			add_member_control(this, "interpolate", interpolate, "toggle");
			align("\b");
			end_tree_node(dimensions);
		}
		
		// use member slice_distance_tex to hash the node status and open node initially (true in 3rd argument)
		if (begin_tree_node("Slicing", show_orthogonal_slices[0], true, "level=3")) {
			// tabify gui
			align("\a");
			add_decorator("orthogonal", "heading", "level=3");
			for (unsigned i = 0; i < 3; ++i) {
				add_member_control(this, std::string("show slice ") + std::string("xyz")[i], show_orthogonal_slices[i], "toggle");
				add_member_control(this, "slice texcoord", orthogonal_slice_center(i), "value_slider", "min=0;max=1;ticks=true");
				add_member_control(this, "slice index", slice_indices(i), "value_slider", "min=0;max=1;ticks=true");
				find_control(slice_indices)->set("max", dimensions(i) - 1);
			}
			add_decorator("oblique", "heading", "level=3");
			add_member_control(this, "show oblique", show_oblique_slice, "toggle");
			add_member_control(this, "oblique_distance", oblique_slice_distance, "value_slider", "min=0;max=1;step=0.0001;ticks=true");
			add_decorator("oblique normal", "heading", "level=4");
			add_gui("oblique normal", oblique_slice_normal, "direction");
			// untabify gui
			align("\b");
			// end gui node definition specifying the same member used for hashing the node status 
			end_tree_node(show_orthogonal_slices[0]);
		}
		if (begin_tree_node("transfer function", transfer_function_changed, true, "level=3")) {
			align("\a");
			add_member_control(this, "texture resolution", transfer_function_texture_resolution, "drowdown", "enums='2=2,4=4,8=8,16=16,32=32,64=64,128=128,256=256,512=512,1024=1024'");
			/************************************************************************************
			 tasks 4.2b: Add the name of the new color scales in the enums-string */

			 add_member_control(this, "color_scale", color_scale, "dropdown", "enums='temperature'");

			/************************************************************************************/

			add_member_control(this, "emission_gamma", emission_gamma, "value_slider", "min=0.01;max=100;log=true;ticks=true");
			add_member_control(this, "absorption_gamma", absorption_gamma, "value_slider", "min=0;max=100;log=true;ticks=true");
			
			align("\b");
			end_tree_node(transfer_function_changed);
		}
		if (begin_tree_node("Rendering", show_box, false, "level=3")) {
			align("\a");
			add_member_control(this, "show_box", show_box, "check");
			add_member_control(this, "wire_box_color", wire_box_color, "check");
			add_gui("box_mat", box_material);
			align("\b");
			end_tree_node(show_box);
		}		
	}
};

cgv::base::object_registration<volume_view> volume_view_reg("volume_view");
