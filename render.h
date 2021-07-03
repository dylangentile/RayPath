#pragma once
#include <vulkan/vulkan.h>


class Renderer
{
public:
	Renderer(const char* render_name, int width, int height, bool display_live);
	~Renderer();

	void init();
	void quit();

	void render_frame();

private:
	const char* m_render_name;
	int m_width;
	int m_height;
	bool m_display_live;

private:
	void create_instance();








private:
	VkInstance m_instance;
};