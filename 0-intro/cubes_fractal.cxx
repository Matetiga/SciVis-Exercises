// This source code is property of the Computer Graphics and Visualization chair of the
// TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved
//
// The main file of the plugin. It defines a class that demonstrates how to register with
// the scene graph, drawing primitives, creating a GUI, using a config file and various
// other parts of the framework.

// Framework core
#include <cgv/render/context.h>
#include <cgv/render/shader_program.h>
#include <cgv/render/vertex_buffer.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv/media/illum/surface_material.h>
#include <cgv/math/ftransform.h>

// Framework standard plugins
#include <cgv_gl/gl/gl.h>

// Implemented header
#include "cubes_fractal.h"


cubes_fractal::cubes_fractal()
	: va(nullptr), va_num_verts(0), va_primitive_type(GL_QUADS)
{
	material.set_brdf_type(
		(cgv::media::illum::BrdfType)(cgv::media::illum::BT_LAMBERTIAN | cgv::media::illum::BT_PHONG)
	);
	material.ref_specular_reflectance() = {.0625f, .0625f, .0625f};
	material.ref_roughness() = .03125f;
}

void cubes_fractal::use_vertex_array (
	cgv::render::attribute_array_binding *vertex_array, size_t num_vertices,
	GLenum primitive_type
)
{
	va = vertex_array;
	va_num_verts = num_vertices;
	va_primitive_type = primitive_type;
}

void cubes_fractal::draw_recursive (
	cgv::render::context& ctx, const cgv::media::color<float> &color, unsigned max_depth,
	unsigned level
)
{
	// Shrink by factor of 1/2 relative to parent node
	ctx.mul_modelview_matrix(cgv::math::scale4<double>(0.5, 0.5, 0.5));

	// Render the cube
	material.set_diffuse_reflectance(color);
	ctx.set_material(material);
	if (va)
	{
		// Use the vertex array
		va->enable(ctx);
		glDrawArrays(va_primitive_type, 0, (GLsizei)va_num_verts);
		va->disable(ctx);
	}
	else
		// Use built-in unit quad
		ctx.tesselate_unit_cube();
	cgv::media::color<float, cgv::media::HLS> color_next(color);
	color_next.H() = std::fmod(color_next.H()+0.2f,  1.0f);
	color_next.S() = std::fmod(color_next.S()-0.05f, 1.0f);

	// Recurse
	unsigned num_children = level==0 ? 4 : 3;
	if (level < max_depth) for (unsigned i=0; i<num_children; i++)
	{
		// Save transformation before recursion
		ctx.push_modelview_matrix();
		// Transform to node position
		ctx.mul_modelview_matrix(
			  // rotate around z-axis by -90, 0, 90 or 180 degrees
			  cgv::math::rotate4<double>(signed(i)*90 - 90, 0, 0, 1)
			  // move along x-axis by 2 relative units
			* cgv::math::translate4<double>(2, 0, 0)
		);
		// Actual recursion
		draw_recursive(ctx, color_next, max_depth, level+1);
		// Restore pre-recursion transformation
		ctx.pop_modelview_matrix();
	}
}
