#pragma once
#include "vma.h"


#include <cstdint>
#include <cstddef>
#include <vulkan/vulkan.h>


class Renderer
{
public:
	Renderer(const char* render_name, int width, int height, bool display_live = false);
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
	void choose_pdev();
	void choose_queue_families();
	void create_device();
	void create_allocator();








private:
	VkInstance m_instance;
	VkPhysicalDevice m_pdev;

	uint32_t m_c_queue_idx;
	uint32_t m_t_queue_idx;

	VkDevice m_dev;
	VkQueue m_c_queue;
	VkQueue m_t_queue;

	VmaAllocator m_vma;



};