#pragma once
#include <cgv/base/node.h>
#include <cgv/render/drawable.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/event_handler.h>
#include <cgv_gl/box_renderer.h>
#include <cgv_gl/gl/mesh_render_info.h>
#include <cgv/render/shader_program.h>
#include <cgv_gl/rounded_cone_renderer.h>
#include <cgv/render/frame_buffer.h>
#include "intersection.h"
#include <rl/math/Transform.h>
#include <rl/math/Unit.h>
#include <rl/mdl/Kinematic.h>
#include <rl/mdl/Model.h>
#include <rl/mdl/XmlFactory.h>
#include <rl/mdl/JacobianInverseKinematics.h>
#include <rl/mdl/NloptInverseKinematics.h>
#include <rl/sg/Model.h>
#include <rl/sg/SimpleScene.h>
#include <rl/sg/Body.h>
#include <iostream>
#include <fstream>
#include <rl/sg/so/Scene.h>
#include <rl/sg/solid/Scene.h>
#include <Inventor/SoDB.h>
#include <Inventor/actions/SoGLRenderAction.h>
using namespace std;
///@ingroup VR
///@{

/**@file
   example plugin for vr usage
*/

// these are the vr specific headers
#include <vr/vr_driver.h>
#include <cg_vr/vr_server.h>
#include <vr_view_interactor.h>
#include <vr_render_helpers.h>

class pose_Vis :
	public cgv::base::node,
	public cgv::render::drawable,
	public cgv::gui::event_handler,
	public cgv::gui::provider {
protected:
	// different interaction states for the controllers
	enum InteractionState {
		IS_NONE,
		IS_OVER,
		IS_GRAB
	};

	// store the scene as colored boxes
	std::vector<box3> boxes;
	std::vector<rgb> box_colors;

	// rendering styles
	cgv::render::box_render_style style;
	cgv::render::rounded_cone_render_style cone_style;

	// sample for rendering a mesh
	double mesh_scale;
	dvec3 mesh_location;
	dquat mesh_orientation;

	// render information for mesh
	cgv::render::mesh_render_info MI;

	// sample for rendering text labels
	std::string label_text;
	int label_font_idx;
	bool label_upright;
	float label_size;
	rgb label_color;
	
	//Rendering Scene
	rl::sg::so::Scene* sc1;
	rl::sg::solid::Scene* sc2;
	rl::mdl::Kinematic* kinematics;

	float x_length;
	float y_length;
	float z_length;
	float a_length;
	float b_length;
	float c_length;

	std::vector<vec3> data_position;
	std::vector<float> radi;
	std::vector<rgb>  point_colors;



private:
	bool label_outofdate; // whether label texture is out of date

protected:
	unsigned label_resolution; // resolution of label texture
	cgv::render::texture label_tex; // texture used for offline rendering of label
	cgv::render::frame_buffer label_fbo; // fbo used for offline rendering of label

	// general font information
	std::vector<const char*> font_names;
	std::string font_enum_decl;

	// current font face used
	cgv::media::font::font_face_ptr label_font_face;
	cgv::media::font::FontFaceAttributes label_face_type;

	// manage controller input configuration for left controller
	std::vector<vr::controller_input_config> left_inp_cfg;

	// store handle to vr kit of which left deadzone and precision is configured
	void* last_kit_handle;

	// length of to be rendered rays
	float ray_length;

	// keep reference to vr_view_interactor
	vr_view_interactor* vr_view_ptr;

	// store the movable boxes
	std::vector<box3> movable_boxes;
	std::vector<rgb> movable_box_colors;
	std::vector<vec3> movable_box_translations;
	std::vector<quat> movable_box_rotations;

	// intersection points
	std::vector<vec3> intersection_points;
	std::vector<rgb>  intersection_colors;
	std::vector<int>  intersection_box_indices;
	std::vector<int>  intersection_controller_indices;

	// state of current interaction with boxes for all controllers
	InteractionState state[4];

	// render style for interaction
	cgv::render::sphere_render_style srs;
	cgv::render::box_render_style movable_style;

	int nr_cameras;
	int frame_width, frame_height;
	int frame_split;
	float seethrough_gamma;
	mat4 camera_to_head_matrix[2];
	cgv::math::fmat<float, 4, 4> camera_projection_matrix[4];
	vec2 focal_lengths[4];
	vec2 camera_centers[4];
	cgv::render::texture camera_tex;
	cgv::render::shader_program seethrough;
	GLuint camera_tex_id;
	bool undistorted;
	bool shared_texture;
	bool max_rectangle;
	float camera_aspect;
	bool show_seethrough;
	bool use_matrix;
	float background_distance;
	float background_extent;
	vec2 extent_texcrd;
	vec2 center_left;
	vec2 center_right;

public:
	void init_cameras(vr::vr_kit* kit_ptr);

	void start_camera();

	void stop_camera();

	/// compute intersection points of controller ray with movable boxes
	void compute_intersections(const vec3& origin, const vec3& direction, int ci, const rgb& color);
	/// keep track of status changes
	void on_status_change(void* kit_handle, int ci, vr::VRStatus old_status, vr::VRStatus new_status);
	/// register on device change events
	void on_device_change(void* kit_handle, bool attach);
	/// construct boxes that represent a table of dimensions tw,td,th and leg width tW
	void construct_table(float tw, float td, float th, float tW);
	/// construct boxes that represent a room of dimensions w,d,h and wall width W
	void construct_room(float w, float d, float h, float W, bool walls, bool ceiling);
	/// construct boxes for environment
	void construct_environment(float s, float ew, float ed, float w, float d, float h);
	/// construct boxes that represent a table of dimensions tw,td,th and leg width tW
	void construct_movable_boxes(float tw, float td, float th, float tW, size_t nr);
	/// construct a scene with a table
	void build_scene(float w, float d, float h, float W, float tw, float td, float th, float tW);





public:
	pose_Vis();



	std::string get_type_name() { return "vr_test"; }

	void stream_help(std::ostream& os);

	void on_set(void* member_ptr);

	bool handle(cgv::gui::event& e);
	
	bool init(cgv::render::context& ctx);

	void clear(cgv::render::context& ctx);

	void init_frame(cgv::render::context& ctx);

	void draw(cgv::render::context& ctx);

	void finish_draw(cgv::render::context& ctx);

	void create_gui();

	rl::math::Transform setTransform(double x, double y, double z, double a, double b, double c);
	vector<vector<double>> calEbene(double x, double y, double z, double a, double b, double c, rl::mdl::JacobianInverseKinematics ik);
	vector<vector<double>> calLine(double x, double y, double z, double a, double b, double c, rl::mdl::JacobianInverseKinematics ik);
	vector<vector<double>> calPonit(double x, double y, double z, double a, double b, double c, rl::mdl::JacobianInverseKinematics ik);
};

///@}
