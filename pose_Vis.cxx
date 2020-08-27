#include "pose_Vis.h"

#include <cgv/signal/rebind.h>
#include <cgv/base/register.h>
#include <cgv/math/ftransform.h>
#include <cgv/utils/scan.h>
#include <cgv/utils/options.h>
#include <cgv/gui/dialog.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv_gl/sphere_renderer.h>
#include <cgv/media/mesh/simple_mesh.h>
#include <cg_vr/vr_events.h>

#include <random>

#include "intersection.h"
#include <rl/math/Transform.h>
#include <rl/math/Unit.h>
#include <rl/mdl/Kinematic.h>
#include <rl/mdl/Model.h>
#include <rl/mdl/XmlFactory.h>
#include <rl/mdl/JacobianInverseKinematics.h>
//#include <rl/mdl/NloptInverseKinematics.h>
#include <rl/sg/Model.h>
#include <rl/sg/SimpleScene.h>
#include <rl/sg/Body.h>
#include <iostream>
#include <fstream>
#include <rl/sg/so/Scene.h>
#include <rl/sg/solid/Scene.h>
#include <Inventor/SoDB.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <cgv_gl/arrow_renderer.h>
#include <direct.h>

using namespace std;

void pose_Vis::init_cameras(vr::vr_kit* kit_ptr)
{
	vr::vr_camera* camera_ptr = kit_ptr->get_camera();
	if (!camera_ptr)
		return;
	nr_cameras = camera_ptr->get_nr_cameras();
	frame_split = camera_ptr->get_frame_split();
	for (int i = 0; i < nr_cameras; ++i) {
		std::cout << "camera " << i << "(" << nr_cameras << "):" << std::endl;
		camera_ptr->put_camera_intrinsics(i, false, &focal_lengths[i](0), &camera_centers[i](0));
		camera_ptr->put_camera_intrinsics(i, true, &focal_lengths[2 + i](0), &camera_centers[2 + i](0));
		std::cout << "  fx=" << focal_lengths[i][0] << ", fy=" << focal_lengths[i][1] << ", center=[" << camera_centers[i] << "]" << std::endl;
		std::cout << "  fx=" << focal_lengths[2+i][0] << ", fy=" << focal_lengths[2+i][1] << ", center=[" << camera_centers[2+i] << "]" << std::endl;
		float camera_to_head[12];
		camera_ptr->put_camera_to_head_matrix(i, camera_to_head);
		kit_ptr->put_eye_to_head_matrix(i, camera_to_head);
		camera_to_head_matrix[i] = vr::get_mat4_from_pose(camera_to_head);
		std::cout << "  C2H=" << camera_to_head_matrix[i] << std::endl;
		camera_ptr->put_projection_matrix(i, false, 0.001f, 10.0f, &camera_projection_matrix[i](0, 0));
		camera_ptr->put_projection_matrix(i, true, 0.001f, 10.0f, &camera_projection_matrix[2+i](0, 0));
		std::cout << "  dP=" << camera_projection_matrix[i] << std::endl;
		std::cout << "  uP=" << camera_projection_matrix[2+i] << std::endl;
	}
	post_recreate_gui();
}

void pose_Vis::start_camera()
{
	if (!vr_view_ptr)
		return;
	vr::vr_kit* kit_ptr = vr_view_ptr->get_current_vr_kit();
	if (!kit_ptr)
		return;
	vr::vr_camera* camera_ptr = kit_ptr->get_camera();
	if (!camera_ptr)
		return;
	if (!camera_ptr->start())
		cgv::gui::message(camera_ptr->get_last_error());
}

void pose_Vis::stop_camera()
{
	if (!vr_view_ptr)
		return;
	vr::vr_kit* kit_ptr = vr_view_ptr->get_current_vr_kit();
	if (!kit_ptr)
		return;
	vr::vr_camera* camera_ptr = kit_ptr->get_camera();
	if (!camera_ptr)
		return;
	if (!camera_ptr->stop())
		cgv::gui::message(camera_ptr->get_last_error());
}

/// compute intersection points of controller ray with movable boxes
void pose_Vis::compute_intersections(const vec3& origin, const vec3& direction, int ci, const rgb& color)
{
	for (size_t i = 0; i < movable_boxes.size(); ++i) {
		vec3 origin_box_i = origin - movable_box_translations[i];
		movable_box_rotations[i].inverse_rotate(origin_box_i);
		vec3 direction_box_i = direction;
		movable_box_rotations[i].inverse_rotate(direction_box_i);
		float t_result;
		vec3  p_result;
		vec3  n_result;
		if (cgv::media::ray_axis_aligned_box_intersection(
			origin_box_i, direction_box_i,
			movable_boxes[i],
			t_result, p_result, n_result, 0.000001f)) {

			// transform result back to world coordinates
			movable_box_rotations[i].rotate(p_result);
			p_result += movable_box_translations[i];
			movable_box_rotations[i].rotate(n_result);

			// store intersection information
			intersection_points.push_back(p_result);
			intersection_colors.push_back(color);
			intersection_box_indices.push_back((int)i);
			intersection_controller_indices.push_back(ci);
		}
	}
}

/// keep track of status changes
void pose_Vis::on_status_change(void* kit_handle, int ci, vr::VRStatus old_status, vr::VRStatus new_status)
{
	// ignore all but left controller changes
	if (ci != 0)
		return;
	vr::vr_kit* kit_ptr = vr::get_vr_kit(kit_handle);
	// check for attaching of controller
	if (old_status == vr::VRS_DETACHED) {
		left_inp_cfg.resize(kit_ptr->get_device_info().controller[0].nr_inputs);
		for (int ii = 0; ii < (int)left_inp_cfg.size(); ++ii)
			left_inp_cfg[ii] = kit_ptr->get_controller_input_config(0, ii);
		post_recreate_gui();
	}
	// check for attaching of controller
	if (new_status == vr::VRS_DETACHED) {
		left_inp_cfg.clear();
		post_recreate_gui();
	}
}

/// register on device change events
void pose_Vis::on_device_change(void* kit_handle, bool attach)
{
	if (attach) {
		if (last_kit_handle == 0) {
			vr::vr_kit* kit_ptr = vr::get_vr_kit(kit_handle);
			init_cameras(kit_ptr);
			if (kit_ptr) {
				last_kit_handle = kit_handle;
				// copy left controller input configurations from new device in order to make it adjustable
				left_inp_cfg.resize(kit_ptr->get_device_info().controller[0].nr_inputs);
				for (int ii = 0; ii < (int)left_inp_cfg.size(); ++ii)
					left_inp_cfg[ii] = kit_ptr->get_controller_input_config(0, ii);
				post_recreate_gui();
			}
		}
	}
	else {
		if (kit_handle == last_kit_handle) {
			last_kit_handle = 0;
			post_recreate_gui();
		}
	}
}

/// construct boxes that represent a table of dimensions tw,td,th and leg width tW
void pose_Vis::construct_table(float tw, float td, float th, float tW) {
	// construct table
	rgb table_clr(0.3f, 0.2f, 0.0f);
	boxes.push_back(box3(
		vec3(-0.5f*tw - 2 * tW, th - tW, -0.5f*td - 2 * tW),
		vec3(0.5f*tw + 2 * tW, th, 0.5f*td + 2 * tW)));
	box_colors.push_back(table_clr);

	boxes.push_back(box3(vec3(-0.5f*tw, 0, -0.5f*td), vec3(-0.5f*tw - tW, th - tW, -0.5f*td - tW)));
	boxes.push_back(box3(vec3(-0.5f*tw, 0, 0.5f*td), vec3(-0.5f*tw - tW, th - tW, 0.5f*td + tW)));
	boxes.push_back(box3(vec3(0.5f*tw, 0, -0.5f*td), vec3(0.5f*tw + tW, th - tW, -0.5f*td - tW)));
	boxes.push_back(box3(vec3(0.5f*tw, 0, 0.5f*td), vec3(0.5f*tw + tW, th - tW, 0.5f*td + tW)));
	box_colors.push_back(table_clr);
	box_colors.push_back(table_clr);
	box_colors.push_back(table_clr);
	box_colors.push_back(table_clr);
}

/// construct boxes that represent a room of dimensions w,d,h and wall width W
void pose_Vis::construct_room(float w, float d, float h, float W, bool walls, bool ceiling) {
	// construct floor
	boxes.push_back(box3(vec3(-0.5f*w, -W, -0.5f*d), vec3(0.5f*w, 0, 0.5f*d)));
	box_colors.push_back(rgb(0.2f, 0.2f, 0.2f));

	if(walls) {
		// construct walls
		boxes.push_back(box3(vec3(-0.5f*w, -W, -0.5f*d - W), vec3(0.5f*w, h, -0.5f*d)));
		box_colors.push_back(rgb(0.8f, 0.5f, 0.5f));
		boxes.push_back(box3(vec3(-0.5f*w, -W, 0.5f*d), vec3(0.5f*w, h, 0.5f*d + W)));
		box_colors.push_back(rgb(0.8f, 0.5f, 0.5f));

		boxes.push_back(box3(vec3(0.5f*w, -W, -0.5f*d - W), vec3(0.5f*w + W, h, 0.5f*d + W)));
		box_colors.push_back(rgb(0.5f, 0.8f, 0.5f));
	}
	if(ceiling) {
		// construct ceiling
		boxes.push_back(box3(vec3(-0.5f*w - W, h, -0.5f*d - W), vec3(0.5f*w + W, h + W, 0.5f*d + W)));
		box_colors.push_back(rgb(0.5f, 0.5f, 0.8f));
	}
}

/// construct boxes for environment
void pose_Vis::construct_environment(float s, float ew, float ed, float w, float d, float h) {
	std::default_random_engine generator;
	std::uniform_real_distribution<float> distribution(0, 1);
	unsigned n = unsigned(ew / s);
	unsigned m = unsigned(ed / s);
	float ox = 0.5f*float(n)*s;
	float oz = 0.5f*float(m)*s;
	for(unsigned i = 0; i < n; ++i) {
		float x = i * s - ox;
		for(unsigned j = 0; j < m; ++j) {
			float z = j * s - oz;
			if(fabsf(x) < 0.5f*w && fabsf(x + s) < 0.5f*w && fabsf(z) < 0.5f*d && fabsf(z + s) < 0.5f*d)
				continue;
			float h = 0.2f*(std::max(abs(x) - 0.5f*w, 0.0f) + std::max(abs(z) - 0.5f*d, 0.0f))*distribution(generator) + 0.1f;
			boxes.push_back(box3(vec3(x, 0.0f, z), vec3(x + s, h, z + s)));
			rgb color = cgv::media::color<float, cgv::media::HLS>(distribution(generator), 0.1f*distribution(generator) + 0.15f, 0.3f);
			box_colors.push_back(color);
			/*box_colors.push_back(
				rgb(0.3f*distribution(generator) + 0.3f,
					0.3f*distribution(generator) + 0.2f,
					0.2f*distribution(generator) + 0.1f));*/
		}
	}
}

/// construct boxes that can be moved around
void pose_Vis::construct_movable_boxes(float tw, float td, float th, float tW, size_t nr) {
	/*
	vec3 extent(0.75f, 0.5f, 0.05f);
	movable_boxes.push_back(box3(-0.5f * extent, 0.5f * extent));
	movable_box_colors.push_back(rgb(0, 0, 0));
	movable_box_translations.push_back(vec3(0, 1.2f, 0));
	movable_box_rotations.push_back(quat(1, 0, 0, 0));
	*/
	std::default_random_engine generator;
	std::uniform_real_distribution<float> distribution(0, 1);
	std::uniform_real_distribution<float> signed_distribution(-1, 1);
	for(size_t i = 0; i < nr; ++i) {
		float x = distribution(generator);
		float y = distribution(generator);
		vec3 extent(distribution(generator), distribution(generator), distribution(generator));
		extent += 0.01f;
		extent *= std::min(tw, td)*0.1f;

		vec3 center(-0.5f*tw + x * tw, th + tW, -0.5f*td + y * td);
		movable_boxes.push_back(box3(-0.5f*extent, 0.5f*extent));
		movable_box_colors.push_back(rgb(distribution(generator), distribution(generator), distribution(generator)));
		movable_box_translations.push_back(center);
		quat rot(signed_distribution(generator), signed_distribution(generator), signed_distribution(generator), signed_distribution(generator));
		rot.normalize();
		movable_box_rotations.push_back(rot);
	}
}

/// construct a scene with a table
void pose_Vis::build_scene(float w, float d, float h, float W, float tw, float td, float th, float tW)
{
	construct_room(w, d, h, W, false, false);
	construct_table(tw, td, th, tW);
	construct_environment(0.3f, 3 * w, 3 * d, w, d, h);
	//construct_environment(0.4f, 0.5f, 1u, w, d, h);
	construct_movable_boxes(tw, td, th, tW, 50);
}

pose_Vis::pose_Vis() 
{
	//init variable
	//x_length = -0.4115;
	//y_length = 0.1291;
	//z_length = 1.1348;
	x_length = -0.15;
	y_length = 0.15;
	z_length = 1.15;
	a_length = 0;
	b_length = 0;
	c_length = 0;
	//for arrow
	ars.nr_subdivisions = 12;
	ars.radius_lower_bound = 0.005f;
	ars.radius_relative_to_length = 0.0;

	frame_split = 0;
	extent_texcrd = vec2(0.5f, 0.5f);
	center_left  = vec2(0.5f,0.25f);
	center_right = vec2(0.5f,0.25f);
	seethrough_gamma = 0.33f;
	frame_width = frame_height = 0;
	background_distance = 2;
	background_extent = 2;
	undistorted = true;
	shared_texture = true;
	max_rectangle = false;
	nr_cameras = 0;
	camera_tex_id = -1;
	camera_aspect = 1;
	use_matrix = true;
	show_seethrough = false;
	set_name("pose_Vis");
	build_scene(5, 7, 3, 0.2f, 1.6f, 0.8f, 0.7f, 0.03f);
	vr_view_ptr = 0;
	ray_length = 2;
	last_kit_handle = 0;
	connect(cgv::gui::ref_vr_server().on_device_change, this, &pose_Vis::on_device_change);
	connect(cgv::gui::ref_vr_server().on_status_change, this, &pose_Vis::on_status_change);

	mesh_scale = 0.0005f;
	mesh_location = dvec3(0, 0.85f, 0);
	mesh_orientation = dquat(1, 0, 0, 0);

	srs.radius = 0.005f;

	label_outofdate = true;
	label_text = "Info Board";
	label_font_idx = 0;
	label_upright = true;
	label_face_type = cgv::media::font::FFA_BOLD;
	label_resolution = 256;
	label_size = 20.0f;
	label_color = rgb(1, 1, 1);

	cgv::media::font::enumerate_font_names(font_names);
	font_enum_decl = "enums='";
	for (unsigned i = 0; i < font_names.size(); ++i) {
		if (i>0)
			font_enum_decl += ";";
		std::string fn(font_names[i]);
		if (cgv::utils::to_lower(fn) == "calibri") {
			label_font_face = cgv::media::font::find_font(fn)->get_font_face(label_face_type);
			label_font_idx = i;
		}
		font_enum_decl += std::string(fn);
	}
	font_enum_decl += "'";
	state[0] = state[1] = state[2] = state[3] = IS_NONE;
}
	
void pose_Vis::stream_help(std::ostream& os) {
	os << "pose_Vis: no shortcuts defined" << std::endl;
}
	
void pose_Vis::on_set(void* member_ptr)
{
	if (member_ptr == &label_face_type || member_ptr == &label_font_idx) {
		label_font_face = cgv::media::font::find_font(font_names[label_font_idx])->get_font_face(label_face_type);
		label_outofdate = true;
	}
	if ((member_ptr >= &label_color && member_ptr < &label_color + 1) ||
		member_ptr == &label_size || member_ptr == &label_text) {
		label_outofdate = true;
	}

	vr::vr_kit* kit_ptr = vr::get_vr_kit(last_kit_handle);
	if (kit_ptr) {
		for (int ii = 0; ii < (int)left_inp_cfg.size(); ++ii)
			if (member_ptr >= &left_inp_cfg[ii] && member_ptr < &left_inp_cfg[ii] + 1)
				kit_ptr->set_controller_input_config(0, ii, left_inp_cfg[ii]);
	}

	rl::mdl::XmlFactory factory;
	//kinematics = dynamic_cast<rl::mdl::Kinematic*>(factory.create("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlmdl\\unimation-puma560.xml"));
	kinematics = dynamic_cast<rl::mdl::Kinematic*>(factory.create("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlmdl\\comau-racer-999.xml"));
	rl::mdl::JacobianInverseKinematics ik(kinematics);
	//rl::mdl::NloptInverseKinematics ik(kinematics);
	//std::cout << "rl::mdl::JacobianInverseKinematics";
	//ik.duration = std::chrono::milliseconds(10);

	vector<vector<double>> finalres = calPonit(-x_length, y_length, z_length, a_length, b_length, c_length, ik);
	
	
	//kinematics->calculateJacobian();


	rl::math::Vector q(3);
	q << 0, 0, 0;
	/*
	rl::math::Transform trans = setTransform(x_length, y_length, z_length, a_length, b_length, c_length);
	ik.goals.push_back(::std::make_pair(trans, 0));
	bool result = ik.solve();
	ik.goals.clear();
	*/
	if (finalres.at(0).at(3) == true) {
		for (std::size_t i = 0; i < sc2->getModel(0)->getNumBodies(); ++i)
		{
			sc2->getModel(0)->getBody(i)->setFrame(kinematics->getFrame(i));
		}
	}

	bool isColliding = false;

	for (size_t i = 1; i < sc2->getModel(0)->getNumBodies(); i++) {
		bool areColliding = dynamic_cast<rl::sg::SimpleScene*>(sc2)->areColliding(
			sc2->getModel(0)->getBody(i), sc2->getModel(1)->getBody(0)
		);
		isColliding = isColliding || areColliding;
	}


	size_t numberbody1 = sc2->getModel(0)->getNumBodies();

	//label_text = "isColliding";
	//normal_cal
	double cosa=cos(rl::math::DEG2RAD * -a_length);
	double cosc=cos(rl::math::DEG2RAD * -c_length);
	double sina =sin(rl::math::DEG2RAD * -a_length);
	double sinc =sin(rl::math::DEG2RAD * -c_length);
	vec3 nml = vec3(-sina*cosc, cosa * cosc, sinc);

	std::cout << isColliding << nml << std::endl;

	posedata.push_back(-x_length);
	posedata.push_back(y_length);
	posedata.push_back(z_length);
	posedata.push_back(a_length);
	posedata.push_back(b_length);
	posedata.push_back(c_length);

	if (finalres.at(0).at(3) == true) {
		if(!isColliding){
		label_text = "noColliding";
		for (int i = 0; i < finalres.size(); ++i) {
			posespace_data.push_back(posedata);
			data_position.push_back(vec3(finalres.at(i).at(0), finalres.at(i).at(1), finalres.at(i).at(2)));
			point_colors.push_back(rgb(1.f,1.f,1.f));
			arc_normal.push_back(nml);
			radi.push_back(0.03);
			}
		}
		else {
			if (std::find(posespace_data.begin(), posespace_data.end(), posedata) != posespace_data.end()) {
			}
			else {
				/* v does not contain x */
				posespace_data.push_back(posedata);
				for (int i = 0; i < finalres.size(); ++i) {
					data_position.push_back(vec3(finalres.at(i).at(0), finalres.at(i).at(1), finalres.at(i).at(2)));
					point_colors.push_back(rgb(1.f, 0.f, 0.f));
					arc_normal.push_back(nml);
					radi.push_back(0.03);
				}
				
			}
		}
	}else{
		kinematics->setPosition(q);
		kinematics->forwardPosition();
		if (std::find(posespace_data.begin(), posespace_data.end(), posedata) != posespace_data.end()) {

		}
		else {
		for (int i = 0; i < finalres.size(); ++i) {
			posespace_data.push_back(posedata);
			data_position.push_back(vec3(finalres.at(i).at(0), finalres.at(i).at(1), finalres.at(i).at(2)));
			point_colors.push_back(rgb(0.f, 0.f, 0.f));
			radi.push_back(0.03);
			arc_normal.push_back(nml);
			}
		}
	}
	posedata.clear();
	for (std::size_t i = 0; i < sc1->getModel(0)->getNumBodies(); ++i)
	{
		sc1->getModel(0)->getBody(i)->setFrame(kinematics->getFrame(i));
	}

	update_member(member_ptr);
	post_redraw();
}
	
bool pose_Vis::handle(cgv::gui::event& e)
{
	// check if vr event flag is not set and don't process events in this case
	if ((e.get_flags() & cgv::gui::EF_VR) == 0)
		return false;
	// check event id
	switch (e.get_kind()) {
	case cgv::gui::EID_KEY:
	{
		cgv::gui::vr_key_event& vrke = static_cast<cgv::gui::vr_key_event&>(e);
		if (vrke.get_action() != cgv::gui::KA_RELEASE) {
			switch (vrke.get_key()) {
			case vr::VR_GRIP:
				std::cout << "grip button " << (vrke.get_controller_index() == 0 ? "left":"right") << " controller pressed" << std::endl;
				return true;
			case vr::VR_DPAD_RIGHT:
				std::cout << "touch pad of " << (vrke.get_controller_index() == 0 ? "left" : "right") << " controller pressed at right direction" << std::endl;
				return true;
			}
		}
		break;
	}
	case cgv::gui::EID_THROTTLE:
	{
		cgv::gui::vr_throttle_event& vrte = static_cast<cgv::gui::vr_throttle_event&>(e);
		std::cout << "throttle " << vrte.get_throttle_index() << " of controller " << vrte.get_controller_index()
			<< " adjusted from " << vrte.get_last_value() << " to " << vrte.get_value() << std::endl;
		return true;
	}
	case cgv::gui::EID_STICK:
	{
		cgv::gui::vr_stick_event& vrse = static_cast<cgv::gui::vr_stick_event&>(e);
		switch (vrse.get_action()) {
		case cgv::gui::SA_TOUCH:
			if (state[vrse.get_controller_index()] == IS_OVER)
				state[vrse.get_controller_index()] = IS_GRAB;
			break;
		case cgv::gui::SA_RELEASE:
			if (state[vrse.get_controller_index()] == IS_GRAB)
				state[vrse.get_controller_index()] = IS_OVER;
			break;
		case cgv::gui::SA_PRESS:
		case cgv::gui::SA_UNPRESS:
			std::cout << "stick " << vrse.get_stick_index()
				<< " of controller " << vrse.get_controller_index()
				<< " " << cgv::gui::get_stick_action_string(vrse.get_action())
				<< " at " << vrse.get_x() << ", " << vrse.get_y() << std::endl;
			return true;
		case cgv::gui::SA_MOVE:
		case cgv::gui::SA_DRAG:
			return true;
			std::cout << "stick " << vrse.get_stick_index()
				<< " of controller " << vrse.get_controller_index()
				<< " " << cgv::gui::get_stick_action_string(vrse.get_action())
				<< " from " << vrse.get_last_x() << ", " << vrse.get_last_y()
				<< " to " << vrse.get_x() << ", " << vrse.get_y() << std::endl;
			return true;
		}
		return true;
	}
	case cgv::gui::EID_POSE:
		cgv::gui::vr_pose_event& vrpe = static_cast<cgv::gui::vr_pose_event&>(e);
		// check for controller pose events
		int ci = vrpe.get_trackable_index();
		if (ci != -1) {
			if (state[ci] == IS_GRAB) {
				// in grab mode apply relative transformation to grabbed boxes

				// get previous and current controller position
				vec3 last_pos = vrpe.get_last_position();
				vec3 pos = vrpe.get_position();
				// get rotation from previous to current orientation
				// this is the current orientation matrix times the
				// inverse (or transpose) of last orientation matrix:
				// vrpe.get_orientation()*transpose(vrpe.get_last_orientation())
				mat3 rotation = vrpe.get_rotation_matrix();
				// iterate intersection points of current controller
				for (size_t i = 0; i < intersection_points.size(); ++i) {
					if (intersection_controller_indices[i] != ci)
						continue;
					// extract box index
					unsigned bi = intersection_box_indices[i];
					// update translation with position change and rotation
					movable_box_translations[bi] = 
						rotation * (movable_box_translations[bi] - last_pos) + pos;
					// update orientation with rotation, note that quaternions
					// need to be multiplied in oposite order. In case of matrices
					// one would write box_orientation_matrix *= rotation
					movable_box_rotations[bi] = quat(rotation) * movable_box_rotations[bi];
					// update intersection points
					intersection_points[i] = rotation * (intersection_points[i] - last_pos) + pos;
				}
			}
			else {// not grab
				// clear intersections of current controller 
				size_t i = 0;
				while (i < intersection_points.size()) {
					if (intersection_controller_indices[i] == ci) {
						intersection_points.erase(intersection_points.begin() + i);
						intersection_colors.erase(intersection_colors.begin() + i);
						intersection_box_indices.erase(intersection_box_indices.begin() + i);
						intersection_controller_indices.erase(intersection_controller_indices.begin() + i);
					}
					else
						++i;
				}

				// compute intersections
				vec3 origin, direction;
				vrpe.get_state().controller[ci].put_ray(&origin(0), &direction(0));
				compute_intersections(origin, direction, ci, ci == 0 ? rgb(1, 0, 0) : rgb(0, 0, 1));
				label_outofdate = true;


				// update state based on whether we have found at least 
				// one intersection with controller ray
				if (intersection_points.size() == i)
					state[ci] = IS_NONE;
				else
					if (state[ci] == IS_NONE)
						state[ci] = IS_OVER;
			}
			post_redraw();
		}
		return true;
	}
	return false;
}

bool pose_Vis::init(cgv::render::context& ctx)
{
	if (!cgv::utils::has_option("NO_OPENVR"))
		ctx.set_gamma(1.0f);

	if (!seethrough.build_program(ctx, "seethrough.glpr"))
		cgv::gui::message("could not build seethrough program");
	
	cgv::media::mesh::simple_mesh<> M;
#ifdef _DEBUG
	if (M.read("D:/data/surface/meshes/obj/Max-Planck_lowres.obj")) {
#else
	if (M.read("D:/data/surface/meshes/obj/Max-Planck_highres.obj")) {
#endif
		MI.construct(ctx, M);
		MI.bind(ctx, ctx.ref_surface_shader_program(true), true);
	}

	cgv::gui::connect_vr_server(true);

	auto view_ptr = find_view_as_node();
	if (view_ptr) {
		view_ptr->set_eye_keep_view_angle(dvec3(0, 4, -4));
		// if the view points to a vr_view_interactor
		vr_view_ptr = dynamic_cast<vr_view_interactor*>(view_ptr);
		if (vr_view_ptr) {
			// configure vr event processing
			vr_view_ptr->set_event_type_flags(
				cgv::gui::VREventTypeFlags(
					cgv::gui::VRE_DEVICE +
					cgv::gui::VRE_STATUS +
					cgv::gui::VRE_KEY +
					cgv::gui::VRE_ONE_AXIS_GENERATES_KEY +
					cgv::gui::VRE_ONE_AXIS +
					cgv::gui::VRE_TWO_AXES +
					cgv::gui::VRE_TWO_AXES_GENERATES_DPAD +
					cgv::gui::VRE_POSE
				));
			vr_view_ptr->enable_vr_event_debugging(false);
			// configure vr rendering
			vr_view_ptr->draw_action_zone(false);
			vr_view_ptr->draw_vr_kits(true);
			vr_view_ptr->enable_blit_vr_views(true);
			vr_view_ptr->set_blit_vr_view_width(200);
		}
	}

	cgv::render::ref_box_renderer(ctx, 1);
	cgv::render::ref_sphere_renderer(ctx, 1);
	cgv::render::ref_rounded_cone_renderer(ctx, 1);

	rl::mdl::XmlFactory factory;
	//std::shared_ptr<rl::mdl::Model> model(factory.create("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlmdl\\unimation-puma560.xml"));
	std::shared_ptr<rl::mdl::Model> model(factory.create("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlmdl\\comau-racer-999.xml"));
	char buff[1000];
	_getcwd(buff, sizeof(buff));
	
	//std::shared_ptr<rl::mdl::Model> model(factory.create("..\\data\\comau-racer-999.xml"));
	//kinematics = dynamic_cast<rl::mdl::Kinematic*>(factory.create("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlmdl\\unimation-puma560.xml"));
	kinematics = dynamic_cast<rl::mdl::Kinematic*>(model.get());
	rl::mdl::JacobianInverseKinematics ik(kinematics);
	std::cout << "rl::mdl::JacobianInverseKinematics";
	//ik.duration = std::chrono::milliseconds(1);

	vector<vector<double>> finalres = calPonit(-x_length, y_length, z_length, a_length, b_length, c_length, ik);
	for (int i = 0; i < finalres.size(); ++i) {
		data_position.push_back(vec3(finalres.at(i).at(0), finalres.at(i).at(1), finalres.at(i).at(2)));
		point_colors.push_back(rgb(finalres.at(i).at(3), finalres.at(i).at(3), finalres.at(i).at(3)));
		arc_normal.push_back(vec3(0, 1, 0));
		radi.push_back(0.03);
	}
	//render problems
	SoDB::init();
	//rl::sg::so::Scene sc1;
	sc1 = new rl::sg::so::Scene();
	sc2 = new rl::sg::solid::Scene();
	//std::shared_ptr<rl::mdl::Model> model(factory.create("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlsg\\unimation-puma560\\unimation-puma560.xml"));
	//sc1->load("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlsg\\unimation-puma560_boxes.xml");
	//sc2->load("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlsg\\unimation-puma560_boxes.xml");
	sc1->load("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlsg\\comau-racer-999_boxes.xml");
	sc2->load("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlsg\\comau-racer-999_boxes.convex.xml");
	//sc1->load("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlsg\\unimation-puma560\\unimation-puma560.xml");
	//SbViewportRegion myView(ctx.get_width(), ctx.get_height());
	//SoGLRenderAction myAction(myView);
	//myAction.apply(sc1.root);
	for (std::size_t i = 0; i < sc1->getModel(0)->getNumBodies(); ++i)
	{
		sc1->getModel(0)->getBody(i)->setFrame(kinematics->getFrame(i));
	}

	/**
	ofstream myout("D:\\Robot\\myIK\\IKtranslation1.txt");
	myout.setf(ios::fixed, ios::floatfield);
	myout.precision(2);
	myout << "7" << endl;
	myout << "TRANSLARIONX: x" << endl;
	myout << "TRANSLARIONY: y" << endl;
	myout << "TRANSLARIONZ: z" << endl;
	myout << "ORIENTATIONA: a" << endl;
	myout << "ORIENTATIONB: b" << endl;
	myout << "ORIENTATIONC: c" << endl;
	myout << "IK Result: result (0: not in posespace,1: in posespace)" << endl;

	//calculation

	double a, b, c = 30;
	for (double x = 0; x < 1.0; x = x + 0.5) {
		for (double y = 0; y < 1.0; y = y + 0.5) {
			for (double z = 0; z < 1.6; z = z + 0.4) {
				rl::math::Transform trans = setTransform(x, y, z, a, b, c);
				ik.goals.push_back(::std::make_pair(trans, 0));
				bool result = ik.solve();
				ik.goals.clear();
				myout << x << " " << y << " " << z << " " << a << " " << b << " " << c << " " << result << " " << endl;
			}
		}
	}
	myout.close();


	//load robot xml data
	char buffer[256];
	fstream outFile;
	outFile.open("D:\\Robot\\myIK\\IKtranslation.txt", ios::in);
	cout << "inFile.txt" << "--- all file is as follows:---" << endl;
	int zeile = 9;
	while (!outFile.eof())
	{
		outFile.getline(buffer, 256, '\n');
		std::string s(buffer);
		if (zeile != 0) {
			zeile = zeile - 1;
		}
		if (zeile == 0) {
			vector<string> a;
			string delimiter = " ";
			size_t pos = 0;
			string token;
			while ((pos = s.find(delimiter)) != string::npos) {
				token = s.substr(0, pos);
				a.push_back(token);

				s.erase(0, pos + delimiter.length());
			}
			if (a.size() == 7) {
				if (a[6] == "1") {
					float x = stof(a[0]);
					float y = stof(a[1])+1.0f;
					float z = stof(a[2]);
					data_position.push_back(vec3(x, y, z));
					point_colors.push_back(rgb(x , y , z ));
					std::cout << x << "====" << y << "====" << z << std::endl;
				}
			}
			a.empty();
		}
	}
	outFile.close();
	*/

	/**
	vector<vector<double>> finalres = calLine(x_length, 0, 0, 30, 30, 30, ik);
	for (int i = 0; i < finalres.size(); ++i) {
		data_position.push_back(vec3(finalres.at(i).at(0), finalres.at(i).at(1) + 1.0, finalres.at(i).at(2)));
		point_colors.push_back(rgb(finalres.at(i).at(3), finalres.at(i).at(3), finalres.at(i).at(3)));
	}
	*/
	return true;
}

rl::math::Transform pose_Vis::setTransform(double x, double y, double z, double a, double b, double c) {
	rl::math::Matrix33 rotationmatrix(
		//Um Z Drehen
		rl::math::AngleAxis(a * rl::math::DEG2RAD, rl::math::Vector3::UnitY()) *
		//Um X Drehen
		rl::math::AngleAxis(-c * rl::math::DEG2RAD, rl::math::Vector3::UnitX()) *
		//Um Y Drehen
		rl::math::AngleAxis(b * rl::math::DEG2RAD, rl::math::Vector3::UnitZ())
	);
	rl::math::Transform rotation(rotationmatrix);
	rl::math::Transform translation(rl::math::Translation(x, y, z));
	translation.rotate(rotation.rotation());
	return translation;
}

vector<vector<double>> pose_Vis::calEbene(double x, double y, double z, double a, double b, double c, rl::mdl::JacobianInverseKinematics ik) {
	vector<vector<double>> finalres;
	vector<double> res;
	for (double x1 = -1.0; x1 < 1.0; x1 = x1 + 0.1) {
		for (double y1 = -1.0; y1 < 1.0; y1 = y1 + 0.1) {
			rl::math::Transform trans = setTransform(x1, y1, z, a, b, c);
			ik.goals.push_back(::std::make_pair(trans, 0));
			bool result = ik.solve();
			ik.goals.clear();
			res.push_back(x1);
			res.push_back(y1);
			res.push_back(z);
			res.push_back(result);
			finalres.push_back(res);
			res.clear();
		}
	};
	for (double x1 = -1.0; x1 < 1.0; x1 = x1 + 0.1) {
		for (double z1 = -0.2; z1 < 1.6; z1 = z1 + 0.1) {
			rl::math::Transform trans = setTransform(x1, y, z1, a, b, c);
			ik.goals.push_back(::std::make_pair(trans, 0));
			bool result = ik.solve();
			ik.goals.clear();
			res.push_back(x1);
			res.push_back(y);
			res.push_back(z1);
			res.push_back(result);
			finalres.push_back(res);
			res.clear();
		}
	};
	for (double y1 = -1.0; y1 < 1.0; y1 = y1 + 0.1) {
		for (double z1 = -0.2; z1 < 1.6; z1 = z1 + 0.1) {
			rl::math::Transform trans = setTransform(x, y1, z1, a, b, c);
			ik.goals.push_back(::std::make_pair(trans, 0));
			bool result = ik.solve();
			ik.goals.clear();
			res.push_back(x);
			res.push_back(y1);
			res.push_back(z1);
			res.push_back(result);
			finalres.push_back(res);
			res.clear();
		}
	};
	return finalres;
};

vector<vector<double>> pose_Vis::calLine(double x, double y, double z, double a, double b, double c, rl::mdl::JacobianInverseKinematics ik) {
	vector<vector<double>> finalres;
	vector<double> res;
	for (double x1 = -1.0; x1 < 1.0; x1 = x1 + 0.2) {
		rl::math::Transform trans = setTransform(x1, y, z, a, b, c);
		ik.goals.push_back(::std::make_pair(trans, 0));
		bool result = ik.solve();
		ik.goals.clear();
		res.push_back(x1);
		res.push_back(y);
		res.push_back(z);
		res.push_back(result);
		finalres.push_back(res);
		res.clear();
	};
	for (double y1 = -1.0; y1 < 1.0; y1 = y1 + 0.2) {
		rl::math::Transform trans = setTransform(x, y1, z, a, b, c);
		ik.goals.push_back(::std::make_pair(trans, 0));
		bool result = ik.solve();
		ik.goals.clear();
		res.push_back(x);
		res.push_back(y1);
		res.push_back(z);
		res.push_back(result);
		finalres.push_back(res);
		res.clear();
	};
	for (double z1 = -0.2; z1 < 1.6; z1 = z1 + 0.2) {
		rl::math::Transform trans = setTransform(x, y, z1, a, b, c);
		ik.goals.push_back(::std::make_pair(trans, 0));
		bool result = ik.solve();
		ik.goals.clear();
		res.push_back(x);
		res.push_back(y);
		res.push_back(z1);
		res.push_back(result);
		finalres.push_back(res);
		res.clear();
	};
	return finalres;
};

vector<vector<double>> pose_Vis::calPonit(double x, double y, double z, double a, double b, double c, rl::mdl::JacobianInverseKinematics ik) {
	vector<vector<double>> finalres;
	vector<double> res;
	//ik.duration = std::chrono::milliseconds(10);
	rl::math::Transform trans = setTransform(x, y, z, a, b, c);
	ik.goals.push_back(::std::make_pair(trans, 0));
	bool result = ik.solve();
	ik.goals.clear();
	res.push_back(x);
	res.push_back(z);
	res.push_back(-y);
	res.push_back(result);
	finalres.push_back(res);
	res.clear();
	return finalres;
};


void pose_Vis::clear(cgv::render::context& ctx)
{
	cgv::render::ref_box_renderer(ctx, -1);
	cgv::render::ref_sphere_renderer(ctx, -1);
	cgv::render::ref_rounded_cone_renderer(ctx, -1);
}

void pose_Vis::init_frame(cgv::render::context& ctx)
{
	if (label_fbo.get_width() != label_resolution) {
		label_tex.destruct(ctx);
		label_fbo.destruct(ctx);
	}
	if (!label_fbo.is_created()) {
		label_tex.create(ctx, cgv::render::TT_2D, label_resolution, label_resolution);
		label_fbo.create(ctx, label_resolution, label_resolution);
		label_tex.set_min_filter(cgv::render::TF_LINEAR_MIPMAP_LINEAR);
		label_tex.set_mag_filter(cgv::render::TF_LINEAR);
		label_fbo.attach(ctx, label_tex);
		label_outofdate = true;
	}
	if (label_outofdate && label_fbo.is_complete(ctx)) {
		glPushAttrib(GL_COLOR_BUFFER_BIT);
		label_fbo.enable(ctx);
		label_fbo.push_viewport(ctx);
		ctx.push_pixel_coords();
			glClearColor(0.5f,0.5f,0.5f,1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			glColor4f(label_color[0], label_color[1], label_color[2], 1);
			ctx.set_cursor(20, (int)ceil(label_size) + 20);
			ctx.enable_font_face(label_font_face, label_size);
			ctx.output_stream() << label_text << "\n";
			ctx.output_stream().flush(); // make sure to flush the stream before change of font size or font face

			ctx.enable_font_face(label_font_face, 0.7f*label_size);
			for (size_t i = 0; i < intersection_points.size(); ++i) {
				ctx.output_stream()
					<< "box " << intersection_box_indices[i]
					<< " at (" << intersection_points[i]
					<< ") with controller " << intersection_controller_indices[i] << "\n";
			}
			ctx.output_stream().flush();

		ctx.pop_pixel_coords();
		label_fbo.pop_viewport(ctx);
		label_fbo.disable(ctx);
		glPopAttrib();
		label_outofdate = false;

		label_tex.generate_mipmaps(ctx);
	}
	if (vr_view_ptr && vr_view_ptr->get_rendered_vr_kit() != 0 && vr_view_ptr->get_rendered_eye() == 0 && vr_view_ptr->get_rendered_vr_kit() == vr_view_ptr->get_current_vr_kit()) {
		vr::vr_kit* kit_ptr = vr_view_ptr->get_current_vr_kit();
		if (kit_ptr) {
			vr::vr_camera* camera_ptr = kit_ptr->get_camera();
			if (camera_ptr && camera_ptr->get_state() == vr::CS_STARTED) {
				uint32_t width = frame_width, height = frame_height, split = frame_split;
				if (shared_texture) {
					box2 tex_range;
					if (camera_ptr->get_gl_texture_id(camera_tex_id, width, height, undistorted, &tex_range.ref_min_pnt()(0))) {
						camera_aspect = (float)width / height;
						split = camera_ptr->get_frame_split();
						switch (split) {
						case vr::CFS_VERTICAL:
							camera_aspect *= 2;
							break;
						case vr::CFS_HORIZONTAL:
							camera_aspect *= 0.5f;
							break;
						}
					}
					else
						camera_tex_id = -1;
				}
				else {
					std::vector<uint8_t> frame_data;
					if (camera_ptr->get_frame(frame_data, width, height, undistorted, max_rectangle)) {
						camera_aspect = (float)width / height;
						split = camera_ptr->get_frame_split();
						switch (split) {
						case vr::CFS_VERTICAL:
							camera_aspect *= 2;
							break;
						case vr::CFS_HORIZONTAL:
							camera_aspect *= 0.5f;
							break;
						}
						cgv::data::data_format df(width, height, cgv::type::info::TI_UINT8, cgv::data::CF_RGBA);
						cgv::data::data_view dv(&df, frame_data.data());
						if (camera_tex.is_created()) {
							if (camera_tex.get_width() != width || camera_tex.get_height() != height)
								camera_tex.destruct(ctx);
							else
								camera_tex.replace(ctx, 0, 0, dv);
						}
						if (!camera_tex.is_created())
							camera_tex.create(ctx, dv);
					}
					else if (camera_ptr->has_error())
						cgv::gui::message(camera_ptr->get_last_error());
				}
				if (frame_width != width || frame_height != height) {
					frame_width = width;
					frame_height = height;

					center_left(0) = camera_centers[2](0) / frame_width;
					center_left(1) = camera_centers[2](1) / frame_height;
					center_right(0) = camera_centers[3](0) / frame_width;
					center_right(1) = camera_centers[3](1) / frame_height;

					update_member(&frame_width);
					update_member(&frame_height);
					update_member(&center_left(0));
					update_member(&center_left(1));
					update_member(&center_right(0));
					update_member(&center_right(1));
				}
				if (split != frame_split) {
					frame_split = split;
					update_member(&frame_split);
				}
			}
		}
	}
}

void pose_Vis::draw(cgv::render::context& ctx)
{

	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	SbViewportRegion myView(vp[2], vp[3]);
	myView.setViewportPixels(vp[0], vp[1], vp[2], vp[3]);
	
	SoGLRenderAction myAction(myView);
	myAction.setTransparencyType(SoGLRenderAction::SCREEN_DOOR);
	ctx.push_modelview_matrix();
	mat4 P;
	quat(vec3(1, 0, 0), rl::math::DEG2RAD * 270).put_homogeneous_matrix(P);
	ctx.mul_modelview_matrix(P);
	myAction.apply(sc1->root);
	ctx.pop_modelview_matrix();

	if (MI.is_constructed()) {
		dmat4 R;
		mesh_orientation.put_homogeneous_matrix(R);
		ctx.push_modelview_matrix();
		ctx.mul_modelview_matrix(
			cgv::math::translate4<double>(mesh_location)*
			cgv::math::scale4<double>(mesh_scale, mesh_scale, mesh_scale) *
			R);
		MI.draw_all(ctx);
		ctx.pop_modelview_matrix();

	}

	if (vr_view_ptr) {
		if ((!shared_texture && camera_tex.is_created()) || (shared_texture && camera_tex_id != -1)) {
			if (vr_view_ptr->get_rendered_vr_kit() != 0 && vr_view_ptr->get_rendered_vr_kit() == vr_view_ptr->get_current_vr_kit()) {
				int eye = vr_view_ptr->get_rendered_eye();

				// compute billboard
				dvec3 vd = vr_view_ptr->get_view_dir_of_kit();
				dvec3 y = vr_view_ptr->get_view_up_dir_of_kit();
				dvec3 x = normalize(cross(vd, y));
				y = normalize(cross(x, vd));
				x *= camera_aspect * background_extent * background_distance;
				y *= background_extent * background_distance;
				vd *= background_distance;
				dvec3 eye_pos = vr_view_ptr->get_eye_of_kit(eye);
				std::vector<vec3> P;
				std::vector<vec2> T;
				P.push_back(eye_pos + vd - x - y);
				P.push_back(eye_pos + vd + x - y);
				P.push_back(eye_pos + vd - x + y);
				P.push_back(eye_pos + vd + x + y);
				double v_offset = 0.5 * (1 - eye);
				T.push_back(dvec2(0.0, 0.5 + v_offset));
				T.push_back(dvec2(1.0, 0.5 + v_offset));
				T.push_back(dvec2(0.0, v_offset));
				T.push_back(dvec2(1.0, v_offset));

				cgv::render::shader_program& prog = seethrough;
				cgv::render::attribute_array_binding::set_global_attribute_array(ctx, prog.get_position_index(), P);
				cgv::render::attribute_array_binding::set_global_attribute_array(ctx, prog.get_texcoord_index(), T);
				cgv::render::attribute_array_binding::enable_global_array(ctx, prog.get_position_index());
				cgv::render::attribute_array_binding::enable_global_array(ctx, prog.get_texcoord_index());

				GLint active_texture, texture_binding;
				if (shared_texture) {
					glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
					glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_binding);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, camera_tex_id);
				}
				else
					camera_tex.enable(ctx, 0);
				prog.set_uniform(ctx, "texture", 0);
				prog.set_uniform(ctx, "seethrough_gamma", seethrough_gamma);
				prog.set_uniform(ctx, "use_matrix", use_matrix);

				// use of convenience function
				vr::configure_seethrough_shader_program(ctx, prog, frame_width, frame_height,
					vr_view_ptr->get_current_vr_kit(), *vr_view_ptr->get_current_vr_state(),
					0.01f, 2 * background_distance, eye, undistorted);

				/* equivalent detailed code relies on more knowledge on program parameters
				mat4 TM = vr::get_texture_transform(vr_view_ptr->get_current_vr_kit(), *vr_view_ptr->get_current_vr_state(), 0.01f, 2 * background_distance, eye, undistorted);
				prog.set_uniform(ctx, "texture_matrix", TM);

				prog.set_uniform(ctx, "extent_texcrd", extent_texcrd);
				prog.set_uniform(ctx, "frame_split", frame_split);
				prog.set_uniform(ctx, "center_left", center_left);
				prog.set_uniform(ctx, "center_right", center_right);
				prog.set_uniform(ctx, "eye", eye);
				*/
				prog.enable(ctx);
				ctx.set_color(rgba(1, 1, 1, 1));

				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


				prog.disable(ctx);

				if (shared_texture) {
					glActiveTexture(active_texture);
					glBindTexture(GL_TEXTURE_2D, texture_binding);
				}
				else
					camera_tex.disable(ctx);

				cgv::render::attribute_array_binding::disable_global_array(ctx, prog.get_position_index());
				cgv::render::attribute_array_binding::disable_global_array(ctx, prog.get_texcoord_index());
			}
		}

		if (vr_view_ptr) {
			std::vector<vec3> P;
			std::vector<float> R;
			std::vector<rgb> C;
			const vr::vr_kit_state* state_ptr = vr_view_ptr->get_current_vr_state();
			if (state_ptr) {
				for (int ci = 0; ci < 4; ++ci) if (state_ptr->controller[ci].status == vr::VRS_TRACKED) {
					vec3 ray_origin, ray_direction;
					state_ptr->controller[ci].put_ray(&ray_origin(0), &ray_direction(0));
					P.push_back(ray_origin);
					R.push_back(0.002f);
					P.push_back(ray_origin + ray_length * ray_direction);
					R.push_back(0.003f);
					rgb c(float(1 - ci), 0.5f * (int)state[ci], float(ci));
					C.push_back(c);
					C.push_back(c);
				}
			}
			if (P.size() > 0) {
				auto& cr = cgv::render::ref_rounded_cone_renderer(ctx);
				cr.set_render_style(cone_style);
				//cr.set_eye_position(vr_view_ptr->get_eye_of_kit());
				cr.set_position_array(ctx, P);
				cr.set_color_array(ctx, C);
				cr.set_radius_array(ctx, R);
				if (!cr.render(ctx, 0, P.size())) {
					cgv::render::shader_program& prog = ctx.ref_default_shader_program();
					int pi = prog.get_position_index();
					int ci = prog.get_color_index();
					cgv::render::attribute_array_binding::set_global_attribute_array(ctx, pi, P);
					cgv::render::attribute_array_binding::enable_global_array(ctx, pi);
					cgv::render::attribute_array_binding::set_global_attribute_array(ctx, ci, C);
					cgv::render::attribute_array_binding::enable_global_array(ctx, ci);
					glLineWidth(3);
					prog.enable(ctx);
					glDrawArrays(GL_LINES, 0, (GLsizei)P.size());


					prog.disable(ctx);
					cgv::render::attribute_array_binding::disable_global_array(ctx, pi);
					cgv::render::attribute_array_binding::disable_global_array(ctx, ci);
					glLineWidth(1);
				}
			}
		}
	}
	/*
	cgv::render::box_renderer& renderer = cgv::render::ref_box_renderer(ctx);

	// draw dynamic boxes 
	
	renderer.set_render_style(movable_style);
	renderer.set_box_array(ctx, movable_boxes);
	renderer.set_color_array(ctx, movable_box_colors);
	renderer.set_translation_array(ctx, movable_box_translations);
	renderer.set_rotation_array(ctx, movable_box_rotations);
	if (renderer.validate_and_enable(ctx)) {
		if (show_seethrough) {
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			renderer.draw(ctx, 0, 3);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			renderer.draw(ctx, 3, movable_boxes.size() - 3);
		}
		else
			renderer.draw(ctx, 0, movable_boxes.size());
	}
	renderer.disable(ctx);

	
	// draw static boxes
	renderer.set_render_style(style);
	renderer.set_box_array(ctx, boxes);
	renderer.set_color_array(ctx, box_colors);
	renderer.render(ctx, 0, boxes.size());
	*/


	// draw intersection points
	if (!intersection_points.empty()) {
		auto& sr = cgv::render::ref_sphere_renderer(ctx);
		sr.set_position_array(ctx, intersection_points);
		sr.set_color_array(ctx, intersection_colors);
		sr.set_render_style(srs);
		sr.render(ctx, 0, intersection_points.size());
	}
	
	/**
		char buffer[256];
		fstream outFile;
		outFile.open("D:\\Robot\\myIK\\IKrotation.txt", ios::in);
		cout << "inFile.txt" << "--- all file is as follows:---" << endl;
		int zeile = 9;
		while (!outFile.eof())
		{
			outFile.getline(buffer, 256, '\n');
			std::string s(buffer);
			if (zeile != 0) {
				zeile = zeile - 1;
			}
			if (zeile == 0) {
				vector<string> a;
				string delimiter = " ";
				size_t pos = 0;
				string token;
				while ((pos = s.find(delimiter)) != string::npos) {
					token = s.substr(0, pos);
					a.push_back(token);

					s.erase(0, pos + delimiter.length());
				}
				if (a.size() == 7) {
					if (a[6] == "1") {
						float x = stof(a[0]);
						float y = stof(a[1]);
						float z = stof(a[2])+1.0f;
						data_position.push_back(vec3(x, y, z));
						std::cout << x <<"===="<<y<<"===="<<z<< std::endl;
					}
				}
				a.empty();
			}
		}
		outFile.close();
		*/
		//data_position.push_back(vec3(0, 1, 0));
		//SoDB::init();
		//rl::sg::so::Scene sc1;
		//sc1.load("C:\\Program Files\\Robotics Library\\0.7.0\\MSVC\\14.1\\x64\\share\\rl-0.7.0\\examples\\rlsg\\unimation-puma560_boxes.xml");
	
		
	// draw our point

	//auto& ball = cgv::render::ref_sphere_renderer(ctx);
	//ball.set_position_array(ctx, data_position);
	//ball.set_color_array(ctx, point_colors);
	//ball.set_render_style(srs);
	//ball.set_radius_array(ctx, radi);
	//ball.render(ctx, 0, data_position.size());

	//if (ball.validate_and_enable(ctx)) {
	//	glDrawArrays(GL_POINTS, 0, (GLsizei)data_position.size());
	//	ball.disable(ctx);
	//}
	// draw arrow
	/*
	auto& ball = cgv::render::ref_sphere_renderer(ctx);
	ball.set_position_array(ctx, data_position);
	ball.set_color_array(ctx, point_colors);
	ball.set_render_style(srs);
	ball.set_radius_array(ctx, radi);
	ball.render(ctx, 0, data_position.size());
	*/
	auto& ar = cgv::render::ref_arrow_renderer(ctx);
	ars.length_scale = 0.05f;
	ar.set_render_style(ars);
	ar.set_position_array(ctx, data_position);
	ar.set_direction_array(ctx, arc_normal);
	ar.set_color_array(ctx, point_colors);
	ar.render(ctx, 0, (GLsizei)data_position.size());

	// draw label
	/*
	if (vr_view_ptr && label_tex.is_created()) {
		cgv::render::shader_program& prog = ctx.ref_default_shader_program(true);
		int pi = prog.get_position_index();
		int ti = prog.get_texcoord_index();
		vec3 p(0, 1.5f, 0);
		vec3 y = label_upright ? vec3(0, 1.0f, 0) : normalize(vr_view_ptr->get_view_up_dir_of_kit());
		vec3 x = normalize(cross(vec3(vr_view_ptr->get_view_dir_of_kit()), y));
		float w = 0.5f, h = 0.5f;
		std::vector<vec3> P;
		std::vector<vec2> T;
		P.push_back(p - 0.5f * w * x - 0.5f * h * y); T.push_back(vec2(0.0f, 0.0f));
		P.push_back(p + 0.5f * w * x - 0.5f * h * y); T.push_back(vec2(1.0f, 0.0f));
		P.push_back(p - 0.5f * w * x + 0.5f * h * y); T.push_back(vec2(0.0f, 1.0f));
		P.push_back(p + 0.5f * w * x + 0.5f * h * y); T.push_back(vec2(1.0f, 1.0f));
		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, pi, P);
		cgv::render::attribute_array_binding::enable_global_array(ctx, pi);
		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, ti, T);
		cgv::render::attribute_array_binding::enable_global_array(ctx, ti);
		prog.enable(ctx);
		label_tex.enable(ctx);
		ctx.set_color(rgb(1, 1, 1));
		glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)P.size());
		label_tex.disable(ctx);
		prog.disable(ctx);
		cgv::render::attribute_array_binding::disable_global_array(ctx, pi);
		cgv::render::attribute_array_binding::disable_global_array(ctx, ti);
	}
	*/
}

void pose_Vis::finish_draw(cgv::render::context& ctx)
{
	return;
	if ((!shared_texture && camera_tex.is_created()) || (shared_texture && camera_tex_id != -1)) {
		cgv::render::shader_program& prog = ctx.ref_default_shader_program(true);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GLint active_texture, texture_binding;
		if (shared_texture) {
			glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_binding);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, camera_tex_id);
		}
		else
			camera_tex.enable(ctx, 0);

		prog.set_uniform(ctx, "texture", 0);
		ctx.push_modelview_matrix();
		ctx.mul_modelview_matrix(cgv::math::translate4<double>(0, 3, 0));
		prog.enable(ctx);
		ctx.set_color(rgba(1, 1, 1, 0.8f));
		ctx.tesselate_unit_square();
		prog.disable(ctx);
		if (shared_texture) {
			glActiveTexture(active_texture);
			glBindTexture(GL_TEXTURE_2D, texture_binding);
		}
		else
			camera_tex.disable(ctx);
		ctx.pop_modelview_matrix();
		glDisable(GL_BLEND);
	}
}

void pose_Vis::create_gui() {
	add_decorator("pose_Vis", "heading", "level=2");
	add_member_control(this, "mesh_scale", mesh_scale, "value_slider", "min=0.1;max=10;log=true;ticks=true");
	add_gui("mesh_location", mesh_location, "vector", "options='min=-3;max=3;ticks=true");
	add_gui("mesh_orientation", static_cast<dvec4&>(mesh_orientation), "direction", "options='min=-1;max=1;ticks=true");
	add_member_control(this, "ray_length", ray_length, "value_slider", "min=0.1;max=10;log=true;ticks=true");
	add_member_control(this, "show_seethrough", show_seethrough, "check");

	//Setting of parameter
	add_member_control(this, "x_length", x_length, "value_slider", "min=-1;max=1;step=0.05;log=false;ticks=true");
	add_member_control(this, "y_length", z_length, "value_slider", "min=-0.2;max=1.6;step=0.05;log=false;ticks=true");
	add_member_control(this, "z_length", y_length, "value_slider", "min=-1;max=1;step=0.05;log=false;ticks=true");
	add_member_control(this, "UM_Z", a_length, "value_slider", "min=-180;max=180;step=72;log=false;ticks=true");
	add_member_control(this, "UM_X", c_length, "value_slider", "min=-180;max=180;step=72;log=false;ticks=true");
	add_member_control(this, "Rotation", b_length, "value_slider", "min=-180;max=180;step=72;log=false;ticks=true");


	if(last_kit_handle) {
		add_decorator("cameras", "heading", "level=3");
		add_view("nr", nr_cameras);
		if(nr_cameras > 0) {
			connect_copy(add_button("start")->click, cgv::signal::rebind(this, &pose_Vis::start_camera));
			connect_copy(add_button("stop")->click, cgv::signal::rebind(this, &pose_Vis::stop_camera));
			add_view("frame_width", frame_width, "", "w=20", "  ");
			add_view("height", frame_height, "", "w=20", "  ");
			add_view("split", frame_split, "", "w=50");
			add_member_control(this, "undistorted", undistorted, "check");
			add_member_control(this, "shared_texture", shared_texture, "check");
			add_member_control(this, "max_rectangle", max_rectangle, "check");
			add_member_control(this, "use_matrix", use_matrix, "check");
			add_member_control(this, "gamma", seethrough_gamma, "value_slider", "min=0.1;max=10;log=true;ticks=true");
			add_member_control(this, "extent_x", extent_texcrd[0], "value_slider", "min=0.2;max=2;ticks=true");
			add_member_control(this, "extent_y", extent_texcrd[1], "value_slider", "min=0.2;max=2;ticks=true");
			add_member_control(this, "center_left_x", center_left[0], "value_slider", "min=0.2;max=0.8;ticks=true");
			add_member_control(this, "center_left_y", center_left[1], "value_slider", "min=0.2;max=0.8;ticks=true");
			add_member_control(this, "center_right_x", center_right[0], "value_slider", "min=0.2;max=0.8;ticks=true");
			add_member_control(this, "center_right_y", center_right[1], "value_slider", "min=0.2;max=0.8;ticks=true");
			add_member_control(this, "background_distance", background_distance, "value_slider", "min=0.1;max=10;log=true;ticks=true");
			add_member_control(this, "background_extent", background_extent, "value_slider", "min=0.01;max=10;log=true;ticks=true");
		}
		vr::vr_kit* kit_ptr = vr::get_vr_kit(last_kit_handle);
		if (kit_ptr) {
			add_decorator("controller input configs", "heading", "level=3");
			int ti = 0, si = 0, pi = 0;
			const auto& CI = kit_ptr->get_device_info().controller[0];
			for (int ii = 0; ii < (int)left_inp_cfg.size(); ++ii) {
				std::string prefix;
				switch (CI.input_type[ii]) {
				case vr::VRI_TRIGGER: prefix = std::string("trigger[") + cgv::utils::to_string(ti++) + "]"; break;
				case vr::VRI_PAD:     prefix = std::string("pad[") + cgv::utils::to_string(pi++) + "]"; break;
				case vr::VRI_STICK:   prefix = std::string("strick[") + cgv::utils::to_string(si++) + "]"; break;
				default:              prefix = std::string("unknown[") + cgv::utils::to_string(ii) + "]";
				}
				add_member_control(this, prefix + ".dead_zone", left_inp_cfg[ii].dead_zone, "value_slider", "min=0;max=1;ticks=true;log=true");
				add_member_control(this, prefix + ".precision", left_inp_cfg[ii].precision, "value_slider", "min=0;max=1;ticks=true;log=true");
				add_member_control(this, prefix + ".threshold", left_inp_cfg[ii].threshold, "value_slider", "min=0;max=1;ticks=true");
			}
		}
	}
	if (begin_tree_node("box style", style)) {
		align("\a");
		add_gui("box style", style);
		align("\b");
		end_tree_node(style);
	}
	if (begin_tree_node("cone style", cone_style)) {
		align("\a");
		add_gui("cone style", cone_style);
		align("\b");
		end_tree_node(cone_style);
	}
	if(begin_tree_node("movable box style", movable_style)) {
		align("\a");
		add_gui("movable box style", movable_style);
		align("\b");
		end_tree_node(movable_style);
	}
	if(begin_tree_node("intersections", srs)) {
		align("\a");
		add_gui("sphere style", srs);
		align("\b");
		end_tree_node(srs);
	}
	if(begin_tree_node("mesh", mesh_scale)) {
		align("\a");
		add_member_control(this, "scale", mesh_scale, "value_slider", "min=0.0001;step=0.0000001;max=100;log=true;ticks=true");
		add_gui("location", mesh_location, "", "main_label='';long_label=true;gui_type='value_slider';options='min=-2;max=2;step=0.001;ticks=true'");
		add_gui("orientation", static_cast<dvec4&>(mesh_orientation), "direction", "main_label='';long_label=true;gui_type='value_slider';options='min=-1;max=1;step=0.001;ticks=true'");
		align("\b");
		end_tree_node(mesh_scale);
	}

	if(begin_tree_node("label", label_size)) {
		align("\a");
		add_member_control(this, "text", label_text);
		add_member_control(this, "upright", label_upright);
		add_member_control(this, "font", (cgv::type::DummyEnum&)label_font_idx, "dropdown", font_enum_decl);
		add_member_control(this, "face", (cgv::type::DummyEnum&)label_face_type, "dropdown", "enums='regular,bold,italics,bold+italics'");
		add_member_control(this, "size", label_size, "value_slider", "min=8;max=64;ticks=true");
		add_member_control(this, "color", label_color);
		add_member_control(this, "resolution", (cgv::type::DummyEnum&)label_resolution, "dropdown", "enums='256=256,512=512,1024=1024,2048=2048'");
		align("\b");
		end_tree_node(label_size);
	}
	if (begin_tree_node("arrow style", ars)) {
		align("\a");
		add_gui("arrows", ars);
		align("\b");
		end_tree_node(ars);
	}
}

#include <cgv/base/register.h>

cgv::base::object_registration<pose_Vis> pose_Vis_reg("pose_Vis");
