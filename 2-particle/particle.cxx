// This source code is property of the Computer Graphics and Visualization chair of the
// TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#include <fstream>
#include <sstream>

#include <cgv/base/node.h> // this should be first include to avoid warning under VS
#include <cgv/base/register.h>
#include <cgv/data/data_view.h>
#include <cgv/math/ftransform.h>
#include <cgv/media/mesh/simple_mesh.h>
#include <cgv_reflect_types/media/color.h>
#include <cgv_gl/gl/gl.h> // includes also GL/gl.h
#include <cgv/render/drawable.h>
#include <cgv/render/shader_program.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv/render/vertex_buffer.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/event_handler.h>
#include <cgv/gui/key_event.h>
#include <random>

using namespace cgv::render;
using namespace cgv::math;

class particle :
	public cgv::base::node,
	public cgv::render::multi_pass_drawable,
	public cgv::gui::event_handler,
	public cgv::gui::provider
{
private:

protected:
	/// file name of rendered scene mesh
	std::string file_name;

	/// data storage for molecule
	std::vector<cgv::vec3> positions;     // (x,y,z)-coordinate of atom center
	std::vector<cgv::vec4> colors;        // colors for each atom
	std::vector<GLuint> connections;                            // connections stored as indices

	cgv::render::view* view_ptr;

	/// rendering
	cgv::media::illum::surface_material material_mol, material_plane;
	shader_program sphere_prog;
	shader_program cylinder_prog;
	cgv::render::vertex_buffer vb_pos;
	cgv::render::vertex_buffer vb_col;
	cgv::render::attribute_array_binding vertex_array_spheres;
	cgv::render::attribute_array_binding vertex_array_cylinder;
	float radius;

	bool show_spheres;
	bool show_cylinders;

	bool show_plane;
	float plane_size, plane_offset;
public:
	particle() :
		node("particle")
	{
		file_name = "./data/molecule_C34H40F2N4O4.sdf";
		radius = 0.25f;
		show_spheres = true;
		show_cylinders = true;
		plane_size = 1;
		plane_offset = 0;
		show_plane = false;

		material_mol.set_brdf_type(
			(cgv::media::illum::BrdfType)(cgv::media::illum::BT_LAMBERTIAN | cgv::media::illum::BT_PHONG)
		);
		material_mol.ref_specular_reflectance() = { .0625f, .0625f, .0625f };
		material_mol.ref_roughness() = .0625f;
		material_plane = material_mol;
		material_plane.ref_diffuse_reflectance() = { .389f, .354f, .0084f };
		material_plane.ref_roughness() = .015625f;
	}


	std::string get_type_name() const
	{
		return "particle";
	}


	/// makes data member of this class available as named properties (which can be set in a config file)
	bool self_reflect(cgv::reflect::reflection_handler& rh)
	{
		return
			rh.reflect_member("file_name", file_name);
	}


	/// callback for all changed UI elements
	void on_set(void* member_ptr)
	{
		update_member(member_ptr);
		post_redraw();
	}

	bool read_sdf_file(std::string file_name)
	{
		std::ifstream file;
		file.open(file_name.c_str());
		if (!file)
			std::cout << "error opening file: " << file_name << std::endl;
		std::string line;

		cgv::vec3 position;
		cgv::vec4 color;
		cgv::vec2 index;

		positions.clear();
		connections.clear();
		colors.clear();

		// get number of atoms and connections for this molecule
		int number_atoms;
		int number_connections;
		// this information is stored in the first line
		std::getline(file, line);
		std::istringstream iiss(line);
		iiss >> number_atoms;
		iiss >> number_connections;

		int iteration = 0;
		while (std::getline(file, line))
		{
			if (line.length() == 0 || line[0] == '#')
				continue;
			else
			{
				// first read atoms
				if (iteration < number_atoms)
				{
					std::stringstream ss(line);
					// center coordinates
					ss >> position.x() >> position.y() >> position.z();
					positions.push_back(position);

					// color
					std::string atom;
					ss >> atom;

					if (atom == "H")
						colors.push_back(cgv::vec4(0.0f, 0.0f, 1.0f, 1.0f));
					else if (atom == "O")
						colors.push_back(cgv::vec4(1.0f, 0.0f, 0.0f, 1.0f));
					else if (atom == "C")
						colors.push_back(cgv::vec4(0.7f, 0.7f, 0.7f, 1.0f));
					else if (atom == "F")
						colors.push_back(cgv::vec4(0.0f, 1.0f, 0.0f, 1.0f));
					else if (atom == "N")
						colors.push_back(cgv::vec4(0.0f, 0.0f, 1.0f, 1.0f));
					else
						colors.push_back(cgv::vec4(0.2f, 0.2f, 0.2f, 1.0f));
				}
				// read connections
				else
				{
					std::istringstream iss(line);
					int from, to;
					iss >> from >> to;

					// store start and end position of this connection
					connections.push_back(GLuint(from - 1));
					connections.push_back(GLuint(to - 1));
				}
				iteration++;
			}
		}

		file.close();
		std::cout << "read number of atoms: " << positions.size() << " connections: " << connections.size() / 2 << std::endl;

		return true;
	}


	/// initialize everything that needs the context
	bool init(context& ctx)
	{
		// load data
		if (file_name != "")
			read_sdf_file(file_name);

		view_ptr = find_view_as_node();

		// set background color to white
		ctx.set_bg_clr_idx(4);

		cgv::render::type_descriptor vec3type = cgv::render::element_descriptor_traits<cgv::vec3>::get_type_descriptor(positions[0]);
		cgv::render::type_descriptor vec4type = cgv::render::element_descriptor_traits<cgv::vec4>::get_type_descriptor(colors[0]);
		
		// build shader program for spheres
		if (!sphere_prog.build_program(ctx, "sphere_raycast.glpr")) {
			std::cerr << "could not build sphere shader program" << std::endl;
			exit(0);
		}

		// - create buffer objects
		bool success = true;
		success = vb_pos.create(ctx, &(positions[0]), positions.size()) && success;
		success = vb_col.create(ctx, &(colors[0]), colors.size()) && success;
		success = vertex_array_spheres.create(ctx) && success;
		
		// enable program to access attribute positions
		sphere_prog.enable(ctx);
		success = vertex_array_spheres.set_attribute_array(
			ctx,
			sphere_prog.get_position_index(),
			vec3type,
			vb_pos,
			0,                                      // position is at start of the vector <-> offset = 0
			positions.size(),                       // number of position elements in the array
			sizeof(cgv::vec3) // stride from one element to next
		) && success;
		success = vertex_array_spheres.set_attribute_array(
			ctx,
			sphere_prog.get_color_index(),
			vec4type,
			vb_col,
			0,                                      // position is at start of the vector <-> offset = 0
			colors.size(),                          // number of color elements in the array
			sizeof(cgv::vec4) // stride from one element to next
		) && success;
		sphere_prog.disable(ctx);


		// build shader program for spheres
		if (!cylinder_prog.build_program(ctx, "cylinder_raycast.glpr")) {
			std::cerr << "could not build cylinder shader program" << std::endl;
			exit(0);
		}

		success = vertex_array_cylinder.create(ctx) && success;

		cylinder_prog.enable(ctx);
		success = vertex_array_cylinder.set_attribute_array(
			ctx,
			cylinder_prog.get_position_index(),
			vec3type,
			vb_pos,
			0,                                          // position is at start of the vector <-> offset = 0
			positions.size(),                         
			sizeof(cgv::vec3) // stride from one element to next
		) && success;
		success = vertex_array_cylinder.set_attribute_array(
			ctx,
			cylinder_prog.get_color_index(),
			vec4type,
			vb_col,
			0,                                      // position is at start of the vector <-> offset = 0
			colors.size(),                          // number of color elements in the array
			sizeof(cgv::vec4) // stride from one element to next
		) && success;
		cylinder_prog.disable(ctx);
		return true;
	}


	/// this method is called before the draw call of the current frame
	void init_frame(context& ctx)
	{
		
	}


	/// this method is called to draw a frame
	void draw(context& ctx)
	{
		ctx.set_material(material_mol);
		if (show_spheres) {
			// Enable shader program we want to use for drawing
			sphere_prog.enable(ctx);

			float pixel_extent_per_depth = (float)(2.0*tan(0.5*0.0174532925199*view_ptr->get_y_view_angle()) / ctx.get_height());
			sphere_prog.set_uniform(ctx, "pixel_extent_per_depth", pixel_extent_per_depth);
			sphere_prog.set_uniform(ctx, "radius", radius);

			// this is necessary for the compute_appearance functions to incorporate the passed color
			sphere_prog.set_uniform(ctx, "map_color_to_material", 3);

			vertex_array_spheres.enable(ctx);
			glDrawArrays(GL_POINTS, 0, (GLsizei)positions.size());
			vertex_array_spheres.disable(ctx);

			// Disable shader program and texture
			sphere_prog.disable(ctx);
		}
		if (show_cylinders) {

			// Enable shader program we want to use for drawing
			cylinder_prog.enable(ctx);
			cylinder_prog.set_uniform(ctx, "radius", 0.5f*radius);

			// this is necessary for the compute_appearance functions to incorporate the passed color
			cylinder_prog.set_uniform(ctx, "map_color_to_material", 3);

			vertex_array_cylinder.enable(ctx);
			glDrawElements(GL_LINES, (GLsizei)connections.size(), GL_UNSIGNED_INT, &connections.front());
			vertex_array_cylinder.disable(ctx);

			// Disable shader program and texture
			cylinder_prog.disable(ctx);
		}
		if (show_plane) {
			glDisable(GL_CULL_FACE);
			ctx.set_material(material_plane);
			ctx.push_modelview_matrix();
			ctx.mul_modelview_matrix(
				cgv::math::scale4<double>(plane_size, plane_size, plane_size)*
				cgv::math::translate4<double>(0,0,plane_offset)
			);
			ctx.ref_surface_shader_program().enable(ctx);
			ctx.tesselate_unit_square();
			ctx.ref_surface_shader_program().disable(ctx);
			ctx.pop_modelview_matrix();
			glEnable(GL_CULL_FACE);
		}
	}


	void destruct(context& ctx)
	{

	}


	bool handle(cgv::gui::event& e)
	{
		return false;
	}


	void stream_help(std::ostream& os)
	{
		os << "particle: ..." << std::endl;
	}


	void stream_stats(std::ostream& os)
	{

	}


	void create_gui()
	{
		add_decorator("particle", "heading");

		bool parameter = true;
		if (begin_tree_node("molecule", parameter , true)) {
			align("\a");
			add_member_control(this, "sphere radius", radius, "value_slider", "min=0.1;max=1;ticks=true;log=true");
			add_member_control(this, "show_spheres", show_spheres, "toggle");
			add_member_control(this, "show_cylinders", show_cylinders, "toggle");
			align("\b");
			end_tree_node(parameter);
		}
		if (begin_tree_node("plane", parameter , true)) {
			align("\a");
			add_member_control(this, "show_plane", show_plane, "toggle");
			add_member_control(this, "plane_size", plane_size, "value_slider", "min=0.01;max=100;log=true;ticks=true");
			add_member_control(this, "plane_offset", plane_offset, "value_slider", "min=-2;max=2;ticks=true");
			align("\b");
			end_tree_node(parameter);
		}
		if (begin_tree_node("molecule material", material_mol, false)) {
            align("\a");
            add_gui("material", material_mol);
            align("\b");
            end_tree_node(material_mol);
        }
		if (begin_tree_node("plane material", material_plane, false)) {
			align("\a");
			add_gui("material", material_plane);
			align("\b");
			end_tree_node(material_plane);
		}
	}
};

cgv::base::object_registration<particle> particle_reg("particle");
