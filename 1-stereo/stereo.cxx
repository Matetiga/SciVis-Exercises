// This source code is property of the Computer Graphics and Visualization chair of the
// TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved

#include <cgv/base/node.h> // this should be first include to avoid warning under VS
#include <cgv/base/register.h>
#include <cgv/data/data_view.h>
#include <cgv/math/ftransform.h>
#include <cgv/media/mesh/simple_mesh.h>
#include <cgv_reflect_types/media/color.h>
#include <cgv_gl/gl/gl.h> // includes also GL/gl.h
#include <cgv_gl/gl/mesh_render_info.h>
#include <cgv_gl/sphere_renderer.h>
#include <cgv/render/drawable.h>
#include <cgv/render/texture.h>
#include <cgv/render/frame_buffer.h>
#include <cgv/render/shader_program.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv/render/clipped_view.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/event_handler.h>
#include <cgv/gui/key_event.h>
#include <random>

using namespace cgv::render;
using namespace cgv::math;

enum ImageType {
    IT_COLOR,
    IT_DEPTH
};

enum StereoRenderMode {
    SRM_INPUT,          // shows the output of the per eye rendering passes (input to the finalization pass)
    SRM_RANDOM_DOTS,    // shows the random dot pattern
    SRM_REMAPPING,      // shows occlusions of each view
    SRM_PARALLAX,       // shows parallax values
    SRM_ANAGLYPH,       // shows 
    SRM_AUTOSTEREOGRAM  // autostereogram based on random dot pattern
};

/// anaglyph modes as explained on http://3dtv.at/Knowhow/AnaglyphComparison_en.aspx
enum AnaglyphMode {
    AM_TRUE,
    AM_GRAY,
    AM_COLOR,
    AM_HALF_COLOR,
    AM_OPTIMIZED    // this mode is for students own creative solution
};

/// noise types for random dot patterns
enum NoiseType {
    NT_WHITE,
    NT_COLORED,
    NT_SPECKLE,
    NT_COLORED_SPECKLE
};

class stereo :
    public cgv::base::node,
    public cgv::render::multi_pass_drawable,
    public cgv::gui::event_handler,
    public cgv::gui::provider
{
private:
    /// used to announce whether a new mesh has to be read in init_frame function
    bool read_mesh;
    /// used to announce a change in the random texture parameters
    bool recompute_rnd_dots;
    /// used to announce a change in the indirect rendering resolution
    bool resolution_changed;
    /// current extent of the scene mesh
    cgv::box3 box;

    /// mesh render information
    mesh_render_info MI;
    /// per eye color and depth texture plus one random texture
    texture col_texs[2], dep_texs[2], rnd_tex;
    /// per eye frame buffer objects 
    frame_buffer fbos[2];
    /// shader program for pass that combines the per eye images into a single one
    shader_program finalize_prog;
    /// pointer to current view with support for scene extent based clipping plane (z_near/z_far) computation
    clipped_view* view_ptr;
    /// shader program to render boxes
    shader_program box_prog;
    /// container for boxes
    std::vector<cgv::box3> boxes;
    /// container for box colors
    std::vector<cgv::rgb> box_colors;
protected:
    /**@name scene*/
    //@{
    /// whether to show spheres 
    bool show_spheres;
    /// whether to show boxes
    bool show_boxes;
    /// whether to draw boxes with one draw call per box
    bool multiple_calls;
    /// file name of rendered scene mesh
    std::string file_name;
    /// surface material to use when shading geometry
    cgv::media::illum::surface_material material;
    //@}

    /**@name rendering*/
    //@{
    /// whether stereo rendering should be performed
    bool enabled;
    /// clear color during stereo rendering
    cgv::rgb clear_color;
    /// divisor for resolution of stereo rendering
    int indirect_resolution_divisor;
    /// scene translation to allow eye separation adjustment
    double translate_z;
    /// render style used for sphere
    sphere_render_style srs;
    //@}

    /**@name stereo parameters*/
    //@{
    /// separation of eyes relative to screen extent
    double eye_separation;
    /// distance of screen from observer
    double parallax_zero_depth;
    /// whether to automatically set parallax_zero_depth from distance to focus point
    bool sync_parallax_zero_depth;
    /// whether to perform lighting in cyclopic eye
    bool cyclopic_lighting;
    /// near and far clipping plane distances
    double z_near, z_far;
    /// whether to automatically set clipping planes from scene extent
    bool sync_clip_planes;
    //@}

    /**@name finalization pass*/
    /// the finalization mode
    StereoRenderMode mode;
    /// index of to be rendered image (0 ... left, 1 ... right) for debugging modes (INPUT, REMAP, PARALLAX)
    int image_idx;
    /// image type of to be rendered image for debugging modes (INPUT, REMAP, PARALLAX)
    ImageType image_type;
    /// depth range and scale used for depth image visualization
    float min_depth, max_depth, depth_scale;
    /// fragments on far clipping plane are detected with z_far_epsion and discarded
    bool discard_fragments_on_far_clipping_plane;
    /// epsilon on depth used to check whether point is not on far clipping place in remapping
    float z_far_epsilon;
    /// epsilon used on depth value difference used to check remapping visibility
    float remap_epsilon;
    /// scale to multiply to float parallax values for PARALLAX mode
    float parallax_scale;
    /// anaglyph mode in ANAGLYPH mode
    AnaglyphMode anaglyph_mode;

    /*<your-ui-elements-here>*/
    //@}

    /**@name noise */
    /// selected noise type
    NoiseType noise_type;
    /// speckle percentage for speckle noise 
    float speckle_percentage;
    /// resolution divisor for random noise texture
    int rnd_dot_divisor;
    //@}
public:
    stereo() :
        node("stereo"),
        col_texs{ texture("[R,G,B]"), texture("[R,G,B]") },
        dep_texs{ texture("[D]"),     texture("[D]") },
        rnd_tex("[R,G,B]")
    {
        read_mesh = false;
        recompute_rnd_dots = false;
        resolution_changed = false;
        for (int i = 0; i < 2; ++i) {
            col_texs[i].set_mag_filter(TF_NEAREST);
            col_texs[i].set_min_filter(TF_NEAREST);
            dep_texs[i].set_mag_filter(TF_NEAREST);
            dep_texs[i].set_min_filter(TF_NEAREST);
        }
        rnd_tex.set_min_filter(TF_NEAREST);
        rnd_tex.set_mag_filter(TF_NEAREST);

        srs.material.set_brdf_type(
            (cgv::media::illum::BrdfType)(cgv::media::illum::BT_LAMBERTIAN | cgv::media::illum::BT_PHONG)
        );
        srs.material.ref_specular_reflectance() = {.0625f, .0625f, .0625f};
        srs.material.ref_roughness() = .0625f;
        material = srs.material;

        enabled = false;
        clear_color = cgv::rgb(0, 0, 0.3f);
        indirect_resolution_divisor = 1;
        translate_z = 0.0;

        eye_separation = 0.1;
        parallax_zero_depth = 5.0;
        sync_parallax_zero_depth = true;
        cyclopic_lighting = false;
        z_near = 0.001;
        z_far = 100.0;
        sync_clip_planes = true;

        mode = SRM_ANAGLYPH;
        image_idx = 0;
        image_type = IT_COLOR;
        min_depth = 0;
        max_depth = depth_scale = 1;
        discard_fragments_on_far_clipping_plane = true;
        z_far_epsilon = 0.001f;
        remap_epsilon = 0.01f;
        parallax_scale = 16.0f;
        anaglyph_mode = AM_TRUE;

        /*<your-code-here>*/

        noise_type = NT_COLORED_SPECKLE;
        speckle_percentage = 0.05f;
        rnd_dot_divisor = 3;

        // create random boxes
        box = cgv::box3(cgv::vec3(-1.0f), cgv::vec3(1.0f));
        std::default_random_engine R;
        std::uniform_real_distribution<float> D(0.0f, 1.0f);
        for (unsigned i = 0; i < 100000; ++i) {
            cgv::vec3 ctr(2*D(R)-1, 2*D(R)-1, 2*D(R)-1);
            cgv::vec3 ext(D(R), D(R), D(R));
            ext *= 0.02f;
            boxes.push_back(cgv::box3(ctr - ext, ctr + ext));
            box_colors.push_back(cgv::rgb(0.2f*D(R)+0.7f*ctr[0]+0.15f, 0.2f*D(R)+0.7f*ctr[1]+0.15f, 0.2f*D(R) + 0.7f*ctr[2] + 0.15f));
        }
        show_spheres = false;
        show_boxes = false;
        multiple_calls = false;
    }


    std::string get_type_name() const
    {
        return "stereo";
    }


    /// makes data member of this class available as named properties (which can be set in a config file)
    bool self_reflect(cgv::reflect::reflection_handler& rh)
    {
        return
            rh.reflect_member("file_name", file_name) &&
            rh.reflect_member("show_spheres", show_spheres) &&
            rh.reflect_member("show_boxes", show_boxes) &&
            rh.reflect_member("multiple_calls", multiple_calls) &&

            rh.reflect_member("enabled", enabled) &&
            rh.reflect_member("clear_color", clear_color) &&
            rh.reflect_member("indirect_resolution_divisor", indirect_resolution_divisor) &&
            rh.reflect_member("translate_z", translate_z) &&

            rh.reflect_member("eye_separation", eye_separation) &&
            rh.reflect_member("parallax_zero_depth", parallax_zero_depth) &&
            rh.reflect_member("sync_parallax_zero_depth", sync_parallax_zero_depth) &&
            rh.reflect_member("cyclopic_lighting", cyclopic_lighting) &&
            rh.reflect_member("z_near", z_near) &&
            rh.reflect_member("z_far", z_far) &&
            rh.reflect_member("sync_clip_planes", sync_clip_planes) &&

            rh.reflect_member("mode", (int&)mode) &&
            rh.reflect_member("image_idx", image_idx) &&
            rh.reflect_member("image_type", (int&)image_type) &&
            rh.reflect_member("min_depth", min_depth) &&
            rh.reflect_member("max_depth", max_depth) &&
            rh.reflect_member("depth_scale", depth_scale) &&
            rh.reflect_member("discard_fragments_on_far_clipping_plane", discard_fragments_on_far_clipping_plane) &&
            
            rh.reflect_member("z_far_epsilon", z_far_epsilon) &&
            rh.reflect_member("remap_epsilon", remap_epsilon) &&

            rh.reflect_member("parallax_scale", parallax_scale) &&
            rh.reflect_member("anaglyph_mode", (int&)anaglyph_mode) &&

            /*<your-code-here>*/

            rh.reflect_member("noise_type", (int&)noise_type) &&
            rh.reflect_member("rnd_dot_divisor", rnd_dot_divisor) &&
            rh.reflect_member("speckle_percentage", speckle_percentage);
    }


    /// callback for all changed UI elements
    void on_set(void* member_ptr)
    {
        if (member_ptr == &rnd_dot_divisor || member_ptr == &noise_type || member_ptr == &speckle_percentage) {
            recompute_rnd_dots = true;
        }
        if (member_ptr == &indirect_resolution_divisor) {
            resolution_changed = true;
        }
        if (member_ptr == &file_name) {
            read_mesh = true;
        }
        if (member_ptr == &translate_z && view_ptr && box.is_valid()) {
            cgv::dbox3 translated_box(
                cgv::dvec3(box.get_min_pnt()) + cgv::dvec3(0, 0, translate_z),
                cgv::dvec3(box.get_max_pnt()) + cgv::dvec3(0, 0, translate_z)
            );
            view_ptr->set_scene_extent(translated_box);
        }
        if (member_ptr == &min_depth) {
            if (max_depth - min_depth < 0.001f) {
                if (min_depth + 0.001f <= 1.0f) {
                    max_depth = min_depth + 0.001f;
                    on_set(&max_depth);
                }
                else
                    min_depth = max_depth - 0.001f;
            }
        }
        if (member_ptr == &max_depth) {
            if (max_depth - min_depth < 0.001f) {
                if (max_depth - 0.001f >= 0.0f) {
                    min_depth = max_depth - 0.001f;
                    on_set(&min_depth);
                }
                else
                    max_depth = min_depth+0.001f;
            }
        }
        update_member(member_ptr);
        post_redraw();
    }


    /// initialize everything that needs the context
    bool init(context& ctx)
    {
        // get view pointer
        view_ptr = dynamic_cast<clipped_view*>(find_view_as_node());

        // build shader program for screen rectangle that combines two views
        if (!finalize_prog.build_program(ctx, "finalize.glpr")) {
            std::cerr << "ups" << std::endl;
            exit(0);
        }

        // build shader program for random boxes
        if (!box_prog.build_program(ctx, "stereo_box.glpr")) {
            view_ptr->set_scene_extent(box);
            view_ptr->set_default_view();
            std::cerr << "ups" << std::endl;
            exit(0);
        }

        finalize_prog.specify_standard_vertex_attribute_names(ctx, false, false, true);

        // initialize sphere renderer for showing eye positions
        ref_sphere_renderer(ctx, 1);
        return true;
    }


    /// this method is called before the draw call of the current frame
    void init_frame(context& ctx)
    {
        // read a mesh and display boxes if displayed
        if (read_mesh) {
            cgv::media::mesh::simple_mesh<> M;
            if (M.read(file_name)) {
                if (!M.has_normals())
                    M.compute_vertex_normals();
                MI.destruct(ctx);
                MI.construct(ctx, M);
				MI.bind(ctx, ctx.ref_surface_shader_program(true), true);
                if (view_ptr) {
                    box = M.compute_box();
                    view_ptr->set_scene_extent(box);
                    view_ptr->set_default_view();
                }
            }
            read_mesh = false;
            show_boxes = false;
            on_set(&show_boxes);
        }

        // resolution of indirect stereo framebuffer can be changed
        unsigned w = ctx.get_width() / indirect_resolution_divisor;
        unsigned h = ctx.get_height() / indirect_resolution_divisor;

        // delete framebuffers and their textures if viewport was resized
        if (fbos[0].is_created() && (resolution_changed || fbos[0].get_width() != w || fbos[0].get_height() != h)) {
            for (int i = 0; i < 2; ++i) {
                fbos[i].destruct(ctx);
                dep_texs[i].destruct(ctx);
                col_texs[i].destruct(ctx);
            }
            rnd_tex.destruct(ctx);
        }
        else if (recompute_rnd_dots) {
            rnd_tex.destruct(ctx);
        }

        // create framebuffer and their corresponding color and depth textures
        if (!fbos[0].is_created()) {
            for (int i = 0; i < 2; ++i) {
                col_texs[i].set_width(w);
                col_texs[i].set_height(h);
                col_texs[i].create(ctx);
                dep_texs[i].set_width(w);
                dep_texs[i].set_height(h);
                dep_texs[i].create(ctx);
                fbos[i].create(ctx, w, h); 
                fbos[i].attach(ctx, dep_texs[i]);
                fbos[i].attach(ctx, col_texs[i]);
                if (!fbos[i].is_complete(ctx)) {
                    std::cerr << "ups should be complete!" << std::endl;
                }
            }
            resolution_changed = false;
        }

        // create a random texture
        w /= rnd_dot_divisor;
        h /= rnd_dot_divisor;
        if (!rnd_tex.is_created()) {
            cgv::data::data_format df("uint8[R,G,B]");
            df.set_width(w);
            df.set_height(h);
            cgv::data::data_view dv(&df);
            cgv::type::uint8_type* data_ptr = dv.get_ptr<cgv::type::uint8_type>();
            std::default_random_engine G;
            unsigned N = w * h;
            switch (noise_type) {
            case NT_WHITE:
            {
                std::uniform_int_distribution<int> D(0, 255);
                for (unsigned i = 0; i < N; ++i)
                    data_ptr[3 * i] = data_ptr[3 * i + 1] = data_ptr[3 * i + 2] = (cgv::type::uint8_type)D(G);
                break;
            }
            case NT_COLORED:
            {
                std::uniform_int_distribution<int> D(0, 255);
                for (unsigned i = 0; i < 3 * N; ++i)
                    data_ptr[i] = (cgv::type::uint8_type)D(G);
                break;
            }
            case NT_SPECKLE:
            {
                std::uniform_real_distribution<float> D(0.0f, 1.0f);
                for (unsigned i = 0; i < N; ++i)
                    data_ptr[3 * i] = data_ptr[3 * i + 1] = data_ptr[3 * i + 2] = (D(G) < speckle_percentage ? 255 : 0);
                break;
            }
            case NT_COLORED_SPECKLE:
            {
                std::uniform_real_distribution<float> D(0.0f, 1.0f);
                for (unsigned i = 0; i < 3 * N; ++i)
                    data_ptr[i] = (D(G) < speckle_percentage ? 255 : 0);
                break;
            }
            }
            rnd_tex.set_width(w);
            rnd_tex.set_height(h);
            rnd_tex.create(ctx, dv);
            recompute_rnd_dots = false;
        }
    }


    /// enables box shader program, sets all attributes and draws the boxes 
    void render_boxes(context& ctx)
    {
        box_prog.enable(ctx);
        int min_pnt_idx = box_prog.get_attribute_location(ctx, "min_pnt");
        int max_pnt_idx = box_prog.get_attribute_location(ctx, "max_pnt");
        int color_idx = box_prog.get_attribute_location(ctx, "color");
        box_prog.set_uniform(ctx, "map_color_to_material", 3);
        ctx.set_material(material);

        attribute_array_binding::enable_global_array(ctx, min_pnt_idx);
        attribute_array_binding::enable_global_array(ctx, max_pnt_idx);
        attribute_array_binding::enable_global_array(ctx, color_idx);

        attribute_array_binding::set_global_attribute_array(ctx, min_pnt_idx, &boxes[0].ref_min_pnt(), boxes.size(), sizeof(cgv::box3));
        attribute_array_binding::set_global_attribute_array(ctx, max_pnt_idx, &boxes[0].ref_max_pnt(), boxes.size(), sizeof(cgv::box3));
        attribute_array_binding::set_global_attribute_array(ctx, color_idx, box_colors);
        
        if (multiple_calls) {
            for (unsigned i = 0; i < (unsigned)boxes.size(); ++i)
                glDrawArrays(GL_POINTS, i, 1);
        } else {
            glDrawArrays(GL_POINTS, 0, (unsigned)boxes.size());
        }

        attribute_array_binding::disable_global_array(ctx, min_pnt_idx);
        attribute_array_binding::disable_global_array(ctx, max_pnt_idx);
        attribute_array_binding::disable_global_array(ctx, color_idx);

        box_prog.disable(ctx);
    }


    /// either renders random boxes, a loaded mesh or an icosahedron
    void render_scene(context& ctx)
    {
        // apply z translation from ui slider
        ctx.mul_modelview_matrix(translate4<double>(0, 0, translate_z));

        // shows random boxes
        if (show_boxes) {
            render_boxes(ctx);
            return;
        }

        // show loaded mesh
        if (MI.is_constructed()) {
            MI.draw_all(ctx);
            return;
        }

        // fall back: rendering of icosahedron
        ctx.ref_surface_shader_program(false).enable(ctx);
        ctx.set_material(material);
        ctx.tesselate_unit_icosahedron();
        ctx.ref_surface_shader_program(false).disable(ctx);
    }


    /// renders a rectangle that fills the whole screen
    void render_screen_filling_quad(context& ctx, shader_program& prog)
    {
        // create vertices and texture coordinates
        std::vector<cgv::vec2> P, T;
        P.push_back(cgv::vec2(-1, -1)); T.push_back(cgv::vec2(0.0f, 0.0f));
        P.push_back(cgv::vec2( 1, -1)); T.push_back(cgv::vec2(1.0f, 0.0f));
        P.push_back(cgv::vec2(-1,  1)); T.push_back(cgv::vec2(0.0f, 1.0f));
        P.push_back(cgv::vec2( 1,  1)); T.push_back(cgv::vec2(1.0f, 1.0f));

        // save current modelview and projection matrix
        ctx.push_modelview_matrix();
        ctx.push_projection_matrix();

            // set matrices to identity since this rectangle should not be moveable
            ctx.set_modelview_matrix(identity4<double>());
            ctx.set_projection_matrix(identity4<double>());

            // enable and set vertex and texture coordinate attributes
            int pos_idx = prog.get_position_index();
            int tex_idx = prog.get_texcoord_index();
            attribute_array_binding::enable_global_array(ctx, pos_idx);
            attribute_array_binding::enable_global_array(ctx, tex_idx);
                attribute_array_binding::set_global_attribute_array(ctx, pos_idx, P);
                attribute_array_binding::set_global_attribute_array(ctx, tex_idx, T);

                // enable shader program and draw the rectangle
                prog.enable(ctx);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                prog.disable(ctx);

            // disable attributes
            attribute_array_binding::disable_global_array(ctx, tex_idx);
            attribute_array_binding::disable_global_array(ctx, pos_idx);

        // reset modelview and projection matrix
        ctx.pop_projection_matrix();
        ctx.pop_modelview_matrix();
    }


    /// renders two spheres at the top of the viewport showing the current eye seperation
    void render_spheres(context& ctx)
    {
        // collect eye independent stereo parameters from view
        double aspect = double(ctx.get_width()) / ctx.get_height();
        double screen_height = view_ptr->get_y_extent_at_focus();
        double screen_width = aspect * screen_height;

        // center of the spheres
        std::vector<cgv::vec4> P;
        cgv::dvec3 x_dir = normalize(cross(view_ptr->get_view_dir(), view_ptr->get_view_up_dir()));
        cgv::dvec3 y_dir = cross(x_dir, view_ptr->get_view_dir());
        double half_eye_distance = 0.5*screen_width*eye_separation;
        double radius = 0.4;
        cgv::dvec3 center = view_ptr->get_focus() + (0.5*screen_height - radius * srs.radius_scale)*y_dir;
        cgv::dvec3 x_offset = half_eye_distance * x_dir;
        P.push_back(cgv::vec4(center + x_offset, radius));
        P.push_back(cgv::vec4(center - x_offset, radius));

        // get instance of sphere renderer
        sphere_renderer& sr = ref_sphere_renderer(ctx);

        // set render style and sphere center
        sr.set_render_style(srs);
        sr.set_sphere_array(ctx, P);

        // draw spheres
        glDisable(GL_DEPTH_TEST);
        if (sr.validate_and_enable(ctx)) {
            glDrawArrays(GL_POINTS, 0, 2);
            sr.disable(ctx);
        }
        glEnable(GL_DEPTH_TEST);
    }


    /// renders each view in seperate framebuffer and combines them in a third rendering pass
    void indirect_two_pass_stereo(context& ctx)
    {
        // collect eye independent stereo parameters from view
        double aspect = double(ctx.get_width()) / ctx.get_height();
        double fovy = view_ptr->get_y_view_angle();
        double screen_height = view_ptr->get_y_extent_at_focus();
        double screen_width = aspect * screen_height;

        // adapt to be synched parameters
        if (sync_parallax_zero_depth) {
            parallax_zero_depth = float(view_ptr->get_depth_of_focus());
            update_member(&parallax_zero_depth);
        }
        if (sync_clip_planes) {
            view_ptr->compute_clipping_planes(z_near, z_far);
            update_member(&z_near);
            update_member(&z_far);
        }

        // set viewport according to reduced indirect rendering resolution
        // this viewport to render to the frame buffer
        unsigned w = ctx.get_width() / indirect_resolution_divisor;
        unsigned h = ctx.get_height() / indirect_resolution_divisor;
        glViewport(0, 0, w, h);

        cgv::mat4 MVP[2];
        // render scene for each view to the corresponding framebuffer
        for (int i = 0; i < 2; ++i) {
            // current view (-1 = left & 1 = right) which can be used as sign in the matrix computations
            int eye = 2 * i - 1;

            // save current projection and modelview matrices
            ctx.push_projection_matrix();
            ctx.push_modelview_matrix();
 
            // enable current framebuffer
            fbos[i].enable(ctx);

            // clear screen with inversely gamma corrected color
            float g = ctx.get_gamma();
            glClearColor(pow(clear_color[0], g), pow(clear_color[1], g), pow(clear_color[2], g), 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            /************************************************************************************
             tasks 1.1.a:  tranformation matrices
                  Compute the correct model view and projection matrices for the current eye and set it
                  with ctx.set_projection_matrix() and ctx.set_modelview_matrix(). You can get the
                  matrices for the current camera position with ctx.get_projection_matrix() and 
                  ctx.get_modelview_matrix().
                  You can use fmat<float, 4, 4> for matrix computations.
			      You may have a look at the variables: eye, eye_separation, screen_width, screen_height,
					parallax_zero_depth, z_near, z_far.
             tasks 1.1.b:  cyclopic lighting
                  Instead of using the lighting from both eye positions, set the correct projection and modelview
                  matriices for cyclopic lighting.
                  Use the variable 'cyclopic_lighting' to make it switch between both lightings */

             /*<your_code_here>*/

            /***********************************************************************************/

            // store per eye modelview projection matrix to hand over to finalization pass
            MVP[i] = ctx.get_projection_matrix() * ctx.get_modelview_matrix();

            // render scene
            render_scene(ctx);

            // recover previous state
            fbos[i].disable(ctx);
            ctx.pop_projection_matrix();
            ctx.pop_modelview_matrix();
        }

        // recover original viewport which is used to render the screen filling rectangle
        glViewport(0, 0, ctx.get_width(), ctx.get_height());

        // get the inverse model-view-projection matrices
        cgv::mat4 iMVP[2] = { inv(MVP[0]), inv(MVP[1]) };

        // enable color and depth textures
        col_texs[0].enable(ctx, 0);
        col_texs[1].enable(ctx, 1);
        dep_texs[0].enable(ctx, 2);
        dep_texs[1].enable(ctx, 3);

        // enable random texture
        rnd_tex.enable(ctx, 4);

        // set all uniform variable for the screen rectangle shader program
        finalize_prog.set_uniform(ctx, "col_tex_0", 0);
        finalize_prog.set_uniform(ctx, "col_tex_1", 1);
        finalize_prog.set_uniform(ctx, "dep_tex_0", 2);
        finalize_prog.set_uniform(ctx, "dep_tex_1", 3);
        finalize_prog.set_uniform(ctx, "rnd_tex", 4);
        finalize_prog.set_uniform(ctx, "MVP_l", MVP[0]);
        finalize_prog.set_uniform(ctx, "MVP_r", MVP[1]);
        finalize_prog.set_uniform(ctx, "iMVP_l", iMVP[0]);
        finalize_prog.set_uniform(ctx, "iMVP_r", iMVP[1]);
        finalize_prog.set_uniform(ctx, "w", (int)ctx.get_width());
        finalize_prog.set_uniform(ctx, "h", (int)ctx.get_height());
        finalize_prog.set_uniform(ctx, "mode", (int&)mode);
        finalize_prog.set_uniform(ctx, "image_idx", image_idx);
        finalize_prog.set_uniform(ctx, "image_type", (int&)image_type);
        finalize_prog.set_uniform(ctx, "min_depth", min_depth);
        finalize_prog.set_uniform(ctx, "max_depth", max_depth);
        finalize_prog.set_uniform(ctx, "depth_scale", depth_scale);
        finalize_prog.set_uniform(ctx, "discard_fragments_on_far_clipping_plane", discard_fragments_on_far_clipping_plane);     
        finalize_prog.set_uniform(ctx, "z_far_epsilon", z_far_epsilon);
        finalize_prog.set_uniform(ctx, "remap_epsilon", remap_epsilon);
        finalize_prog.set_uniform(ctx, "parallax_scale", parallax_scale);
        finalize_prog.set_uniform(ctx, "anaglyph_mode", (int&)anaglyph_mode);
        /*<your-uniforms-here>*/

        // renders the screen filling rectangle
        glDisable(GL_DEPTH_TEST);
        render_screen_filling_quad(ctx, finalize_prog);
        glEnable(GL_DEPTH_TEST);

        // disable all textures
        col_texs[0].disable(ctx);
        col_texs[1].disable(ctx);
        dep_texs[0].disable(ctx);
        dep_texs[1].disable(ctx);
        rnd_tex.disable(ctx);
    }


    /// this method is called to draw a frame
    void draw(context& ctx)
    {
        // the scene is rendered normally if stereo is disabled
        if (!view_ptr || !enabled) {
            render_scene(ctx);
            return;
        }

        /************************************************************************************
         tasks 1.4.c:  one pass rendering
              Exchange the inderect_two_pass_stereo() method with a one pass rendering method */

         /*<your_code_here>*/

        /***********************************************************************************/

        // if stereo is enabled a multi pass rendering is done
        indirect_two_pass_stereo(ctx);

        // renders spheres representing the eye seperation
        if (show_spheres)
            render_spheres(ctx);
    }


    void destruct(context& ctx)
    {
        ref_sphere_renderer(ctx, -1);
    }


    bool handle(cgv::gui::event& e)
    {
        if (e.get_kind() != cgv::gui::EID_KEY)
            return false;
        cgv::gui::key_event& ke = static_cast<cgv::gui::key_event&>(e);
        if (ke.get_action() == cgv::gui::KA_RELEASE)
            return false;
        switch (ke.get_key()) {
        case '0': 
        case '1':
            if (ke.get_modifiers() == 0) {
                image_idx = int(ke.get_key() - '0');
                on_set(&image_idx);
                return true;
            }
            break;
        case 'T':
            if (ke.get_modifiers() == 0) {
                image_type = ImageType(1-image_type);
                on_set(&image_type);
                return true;
            }
            break;
        case 'I':
        case 'R':
        case 'M':
        case 'P':
        case 'A':
        case 'S':
            if (ke.get_modifiers() == 0) {
                switch (ke.get_key()) {
                case 'I':mode = SRM_INPUT; break;
                case 'R': mode = SRM_RANDOM_DOTS; break;
                case 'M':mode = SRM_REMAPPING; break;
                case 'P':mode = SRM_PARALLAX; break;
                case 'A':mode = SRM_ANAGLYPH; break;
                case 'S':mode = SRM_AUTOSTEREOGRAM; break;
                }               
                on_set(&mode);
                return true;
            }
            break;
        case 'E': enabled = !enabled; on_set(&enabled); break;
        }
        return false;
    }


    void stream_help(std::ostream& os)
    {
        os << "stereo: image <0|1> idx, <t>ype; <I|R|M|P|A|S> mode; <e>nables" << std::endl;
    }


    void stream_stats(std::ostream& os)
    {
        static const char* mode_names[] = { "input","random_dots","remapped","parallax","anaglyph","autostereogram" };
        static const char* anaglyph_names[] = { "true","gray","color","half_color","optimized" };
        os << "stereo: " << (image_type == IT_COLOR ? "color":"depth")
            << "[" << image_idx << "], mode = " << mode_names[mode] << ", anaglyph = " << anaglyph_names[anaglyph_mode] << std::endl;
    }


    void create_gui()
    {
        add_decorator("stereo", "heading");
        if (begin_tree_node("scene", file_name, true)) {
            align("\a");
            add_member_control(this, "translate_z", translate_z, "value_slider", "min=0.0;max=1000;ticks=true;log=true");
            add_member_control(this, "show_boxes", show_boxes, "toggle");
            add_member_control(this, "multiple_calls", multiple_calls, "toggle");
            add_gui("mesh", file_name, "file_name", "title='Open Mesh';filter='Mesh Files (obj):*.obj|All Files:*.*'");
            add_member_control(this, "show eyes separation", show_spheres, "toggle");
            if (begin_tree_node("sphere style", srs, false)) {
                align("\a");
                add_gui("sphere_style", srs);
                align("\b");
                end_tree_node(srs);
            }
            if (begin_tree_node("boxes & fallback material", material, false)) {
                align("\a");
                add_gui("material", material);
                align("\b");
                end_tree_node(material);
            }
            align("\b");
            end_tree_node(file_name);
        }
        if (begin_tree_node("rendering", enabled, true)) {
            align("\a");
            add_member_control(this, "enabled stereo", enabled, "toggle");
            add_member_control(this, "clear_color", clear_color);
            add_member_control(this, "indirect_resolution_divisor", indirect_resolution_divisor, "value_slider", "min=1;max=16;ticks=true");
            align("\b");
            end_tree_node(enabled);
        }
        if (begin_tree_node("stereo parameters", eye_separation, true)) {
            align("\a");
            add_member_control(this, "eye_separation", eye_separation, "value_slider", "min=0;max=0.5;step=0.00001;ticks=true;log=true");
            add_member_control(this, "parallax_zero_depth", parallax_zero_depth, "value_slider", "min=0.001;max=1000;ticks=true;log=true");
            add_member_control(this, "sync_parallax_zero_depth", sync_parallax_zero_depth, "toggle");
            add_member_control(this, "cyclopic_lighting", cyclopic_lighting, "toggle");
            add_member_control(this, "z_near", z_near, "value_slider", "min=0.001;max=1000;ticks=true;log=true");
            add_member_control(this, "z_far", z_far, "value_slider", "min=0.001;max=1000;ticks=true;log=true");
            add_member_control(this, "sync_clip_planes", sync_clip_planes, "toggle");
            align("\b");
            end_tree_node(eye_separation);
        }
        if (begin_tree_node("finalization pass", mode, true)) {
            align("\a");
                add_member_control(this, "mode", mode, "dropdown", "enums='input,random_dots,remapped,parallax,anaglyph,autostereogram'");
                add_member_control(this, "image_idx", (cgv::type::DummyEnum&)image_idx, "dropdown", "enums='0,1'");
                add_member_control(this, "image_type", image_type, "dropdown", "enums='color,depth'");
                add_member_control(this, "min_depth", min_depth, "value_slider", "min=0;max=1;ticks=true");
                add_member_control(this, "max_depth", max_depth, "value_slider", "min=0;max=1;ticks=true");
                add_member_control(this, "depth_scale", depth_scale, "value_slider", "min=1;max=10000;ticks=true;log=true");
                add_member_control(this, "discard_fragments_on_far_clipping_plane", discard_fragments_on_far_clipping_plane, "toggle");             
                add_member_control(this, "z_far_epsilon", z_far_epsilon, "value_slider", "min=0.0;step=0.00001;max=0.001;ticks=true;log=true");
                add_member_control(this, "remap_epsilon", remap_epsilon, "value_slider", "min=0.00001;step=0.0000001;max=0.1;ticks=true;log=true");
                add_member_control(this, "parallax_scale", parallax_scale, "value_slider", "min=1;max=1000;step=0.00001;ticks=true;log=true");
                add_member_control(this, "anaglyph_mode", anaglyph_mode, "dropdown", "enums='true,gray,color,half_color,optimized'");
                /*<your-code-here>*/
            align("\b");
            end_tree_node(mode);
        }
        if (begin_tree_node("noise", noise_type, false)) {
            align("\a");
            add_member_control(this, "noise_type", noise_type, "dropdown", "enums='white,colored,speckle,colored_speckle'");
            add_member_control(this, "speckle_percentage", speckle_percentage, "value_slider", "min=0.0001;max=1;step=0.00001;log=true;ticks=true");
            add_member_control(this, "rnd_dot_divisor", rnd_dot_divisor, "value_slider", "min=1;max=16;ticks=true");
            align("\b");
            end_tree_node(noise_type);
        }
    }
};


cgv::base::object_registration<stereo> stereo_reg("stereo");
