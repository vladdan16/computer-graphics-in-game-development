#include "rasterizer_renderer.h"

#include "utils/resource_utils.h"

#include <linalg.h>

// this function is used to compute fog factor - the further the object is, the more fog is applied
float computeFogFactor(float depth, float fogStart, float fogEnd) {
	// I decided to make such change of z value for convenience
	auto z = static_cast<float>(static_cast<int>(depth * 1000000) % 1000);
	return linalg::clamp((fogEnd - z) / (fogEnd - fogStart), 0.0f, 1.0f);
}

// this function is used to compute color of the pixel with fog
cg::color lerp(const cg::color& a, const cg::color& b, float t) {
	cg::color result{};
	result.r = a.r * (1.f - t) + b.r * t;
	result.g = a.g * (1.f - t) + b.g * t;
	result.b = a.b * (1.f - t) + b.b * t;
	return result;
}

cg::color fog_color{0.0f, 0.0f, 0.0f}; // Default to black fog
// I noticed that z value varies approximately from 0.999789 to 0.999810, so this is the reason for such values
float fog_start = 789.0f;           // Default start of fog
float fog_end = 810.0f;             // Default end of fog

void cg::renderer::rasterization_renderer::init()
{
	rasterizer = std::make_shared<cg::renderer::rasterizer<cg::vertex, cg::unsigned_color>>();
	rasterizer->set_viewport(settings->width, settings->height);

	render_target = std::make_shared<cg::resource<cg::unsigned_color>>(settings->width, settings->height);

	model = std::make_shared<cg::world::model>();
	model->load_obj(settings->model_path);

	camera = std::make_shared<cg::world::camera>();
	camera->set_height(static_cast<float>(settings->height));
	camera->set_width(static_cast<float>(settings->width));
	camera->set_position(float3{
			settings->camera_position[0],
			settings->camera_position[1],
			settings->camera_position[2],
	});
	camera->set_theta(settings->camera_theta);
	camera->set_phi(settings->camera_phi);
	camera->set_angle_of_view(settings->camera_angle_of_view);
	camera->set_z_near(settings->camera_z_near);
	camera->set_z_far(settings->camera_z_far);

	depth_buffer = std::make_shared<cg::resource<float>>(settings->width, settings->height);
	rasterizer->set_render_target(render_target, depth_buffer);
}
void cg::renderer::rasterization_renderer::render()
{
	float4x4 matrix = mul(
			camera->get_projection_matrix(),
			camera->get_view_matrix(),
			model->get_world_matrix());
	rasterizer->vertex_shader = [&](float4 vertex, cg::vertex data) {
		auto processed = mul(matrix, vertex);
		return std::make_pair(processed, data);
	};
	rasterizer->pixel_shader = [](const cg::vertex& data, const float z) {
		cg::color original_color{
				data.ambient_r,
				data.ambient_g,
				data.ambient_b,
		};

		float fogFactor = computeFogFactor(z, fog_start, fog_end);
		cg::color fogged_color = lerp(original_color, fog_color, (1.f - fogFactor) / 2);

		return fogged_color;
	};

	auto start = std::chrono::high_resolution_clock::now();
	rasterizer->clear_render_target({111, 5, 243});
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> clear_duration = end - start;
	std::cout << "Clearing took: " << clear_duration.count() << "ms" << std::endl;

	start = std::chrono::high_resolution_clock::now();
	for (size_t shape_id = 0; shape_id < model->get_index_buffers().size(); shape_id++)
	{
		rasterizer->set_vertex_buffer(model->get_vertex_buffers()[shape_id]);
		rasterizer->set_index_buffer(model->get_index_buffers()[shape_id]);
		rasterizer->draw(model->get_index_buffers()[shape_id]->get_number_of_elements(), 0);
	}
	end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> rendering_duration = end - start;
	std::cout << "Rendering took: " << rendering_duration.count() << "ms" << std::endl;

	cg::utils::save_resource(*render_target, settings->result_path);
}

void cg::renderer::rasterization_renderer::destroy() {}

void cg::renderer::rasterization_renderer::update() {}