#pragma once
#include "vma.h"

#include <cstdint>
#include <cstddef>
#include <vector>
#include <vulkan/vulkan.h>

struct Buffer
{
	VkBuffer handle = VK_NULL_HANDLE;
	VmaAllocation alloc;
	size_t size;
};


class Renderer
{
public:
	Renderer(const char* render_name, int width, int height, bool display_live = false);
	~Renderer();

	void init();
	void quit();

	void render_frame();

private:
	const char* const m_render_name;
	const int m_width;
	const int m_height;
	const bool m_display_live;

//init funcs
private:
	void create_instance();
	void choose_pdev();
	void choose_queue_families();
	void create_device();
	void create_allocator();
	void create_command_pools();
	void create_command_buffers();
	void create_pipeline();



//memory/transfer funcs
private:
	bool create_buffer(Buffer* buf, const size_t size);
	void destroy_buffer(Buffer* buf);
	bool copy_to_buffer(VkCommandBuffer cmd_buf, Buffer* buf, const void* data, const size_t size);







private:
	VkInstance m_instance;
	VkPhysicalDevice m_pdev;

	uint32_t m_c_queue_idx;
	uint32_t m_t_queue_idx;

	VkDevice m_dev;
	VkQueue m_c_queue;
	VkQueue m_t_queue;

	VmaAllocator m_vma;


	VkCommandPool m_c_cmd_pool;
	VkCommandPool m_t_cmd_pool;

	VkCommandBuffer m_t_cmd_buf;
	VkCommandBuffer m_c_cmd_buf_array[2];


	VkDescriptorPool m_descriptor_pool;
	VkDescriptorSetLayout m_dset_layout;
	VkPipelineLayout m_pipeline_layout;
	VkPipeline m_compute_pipeline;


private:
	std::vector<Buffer> m_destroy_after_transfer_vec;



};