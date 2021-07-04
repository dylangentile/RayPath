#include "render.h"
#include "volk.h"

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <set>

Renderer::Renderer(const char* render_name, int width, int height, bool display_live) : m_render_name(render_name), m_width(width), m_height(height), m_display_live(display_live) {}
Renderer::~Renderer(){}


#define CHECKVK(expr, msg) if((expr) != VK_SUCCESS){fprintf(stderr, "%s:%d - %s\n", __FILE__, __LINE__, msg); exit(1);}

//for shaders
void*
read_binary(const char* path, size_t* size)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	std::streamsize fsize = file.tellg();
	*size = (size_t)fsize;

	void* data = calloc(1, *size);
	file.seekg(0, std::ios::beg);
	if (!file.read((char*)data, fsize))
	{
		free(data);
		return nullptr;
	}

	return data;
}


void
Renderer::create_instance()
{
	CHECKVK(volkInitialize(), "failed to load vulkan dynamic library!");
	
	const size_t layer_array_size = 1;
	const char* layer_array[layer_array_size] = {
		"VK_LAYER_KHRONOS_validation",
	};

	const char* ext_array[] = {
		"VK_KHR_get_physical_device_properties2"
	};


	VkApplicationInfo appinfo;
	appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appinfo.pNext = nullptr;
	appinfo.pApplicationName = "RayPath";
	appinfo.applicationVersion = 1;
	appinfo.pEngineName = "RayPath";
	appinfo.engineVersion = 1;
	appinfo.apiVersion = VK_API_VERSION_1_0;


	VkInstanceCreateInfo instance_cinfo;
	instance_cinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_cinfo.pNext = nullptr;
	instance_cinfo.flags = 0;
	instance_cinfo.pApplicationInfo = &appinfo;
	instance_cinfo.enabledLayerCount = layer_array_size;
	instance_cinfo.ppEnabledLayerNames = layer_array;
	instance_cinfo.enabledExtensionCount = 1;
	instance_cinfo.ppEnabledExtensionNames = ext_array;

	CHECKVK(vkCreateInstance(&instance_cinfo, nullptr, &m_instance) , "failed to create vulkan instance!");
	volkLoadInstance(m_instance);
}

void
Renderer::choose_pdev()
{
	std::vector<VkPhysicalDevice> pdev_vec;
	uint32_t pdev_count;
	CHECKVK(vkEnumeratePhysicalDevices(m_instance, &pdev_count, nullptr), 
		"Failed to enumerate physical devices");
	pdev_vec.resize(pdev_count);
	CHECKVK(vkEnumeratePhysicalDevices(m_instance, &pdev_count, pdev_vec.data()), 
		"Failed to enumerate physical devices");

	std::vector<VkPhysicalDevice> ideal_pdev_vec;

	for(VkPhysicalDevice pdev : pdev_vec)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(pdev, &props);
		if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			ideal_pdev_vec.push_back(pdev);
	}

	if(ideal_pdev_vec.empty())
	{
		fprintf(stderr, "WARNING: failed to find discrete gpu, using alternative...\n");
		m_pdev = pdev_vec[0];
	}
	else
		m_pdev = ideal_pdev_vec[0];

}


//this function is atrociously bad but it's not a huge deal that it works well
void 
Renderer::choose_queue_families()
{
	//ideally we have 1 compute queue, and 1 transfer queue
	// -- NVIDIA cards have dedicated transfer queues that are better than the compute & graphics & transfer queue families
	std::vector<VkQueueFamilyProperties> q_props_vec;
	
	uint32_t family_count;
	vkGetPhysicalDeviceQueueFamilyProperties(m_pdev, &family_count, nullptr);
	q_props_vec.resize(family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(m_pdev, &family_count, q_props_vec.data());

	std::set<uint32_t> compute_capable;
	std::set<uint32_t> transfer_capable;

	for(uint32_t i = 0; i < family_count; i++)
	{
		if(q_props_vec[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
			compute_capable.insert(i);

		if(q_props_vec[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
			transfer_capable.insert(i);
	}




	if(compute_capable.empty() || transfer_capable.empty())
	{
		fprintf(stderr, "could not find required queue families!\n");
		exit(1);
	}


	bool found_t = false;
	for(auto t_it : transfer_capable)
	{
		//dedicated transfer queue
		if(compute_capable.find(t_it) == compute_capable.end() && (q_props_vec[t_it].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
		{
			m_t_queue_idx = t_it;
			found_t = true;
			break;
		}
	}

	if(!found_t)
	{
		for(auto c_it : compute_capable)
		{
			if(q_props_vec[c_it].queueCount > 1 && q_props_vec[c_it].queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				m_c_queue_idx = c_it;
				m_t_queue_idx = c_it;
				found_t = true;
				return;
			}
		}
	}
	
	m_c_queue_idx = *compute_capable.begin();




	if(!found_t)
	{
		for(auto t_it : transfer_capable)
		{
			if(t_it != m_c_queue_idx)
			{
				m_t_queue_idx = t_it;
				found_t = true;
				break;
			}
		}

		
	}

	if(!found_t)
	{
		fprintf(stderr, "failed to find seperate transfer/compute queues!\n");
		exit(1);
	}

}


void 
Renderer::create_device()
{

	size_t queue_family_count = 0;
	VkDeviceQueueCreateInfo dq_cinfo_array[2];


	float priorities[] = { 1.0f, 1.0f };
	if(m_t_queue_idx == m_c_queue_idx)
	{
		VkDeviceQueueCreateInfo& dq_cinfo = dq_cinfo_array[0];
		dq_cinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		dq_cinfo.pNext = nullptr;
		dq_cinfo.flags = 0;
		dq_cinfo.queueFamilyIndex = m_c_queue_idx;
		dq_cinfo.queueCount = 2;
		dq_cinfo.pQueuePriorities = priorities;

		queue_family_count = 1;
	}
	else
	{
		VkDeviceQueueCreateInfo& c_dq_cinfo = dq_cinfo_array[0];
		c_dq_cinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		c_dq_cinfo.pNext = nullptr;
		c_dq_cinfo.flags = 0;
		c_dq_cinfo.queueFamilyIndex = m_c_queue_idx;
		c_dq_cinfo.queueCount = 1;
		c_dq_cinfo.pQueuePriorities = priorities;


		VkDeviceQueueCreateInfo& t_dq_cinfo = dq_cinfo_array[1];
		t_dq_cinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		t_dq_cinfo.pNext = nullptr;
		t_dq_cinfo.flags = 0;
		t_dq_cinfo.queueFamilyIndex = m_t_queue_idx;
		t_dq_cinfo.queueCount = 1;
		t_dq_cinfo.pQueuePriorities = priorities;

		queue_family_count = 2;
	}


	VkDeviceCreateInfo dev_cinfo;
	dev_cinfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dev_cinfo.pNext = nullptr;
	dev_cinfo.flags = 0;
	dev_cinfo.queueCreateInfoCount = queue_family_count;
	dev_cinfo.pQueueCreateInfos = dq_cinfo_array;
	dev_cinfo.enabledLayerCount = 0;
	dev_cinfo.ppEnabledLayerNames = nullptr;
#ifndef USING_MOLTEN_VK
	dev_cinfo.enabledExtensionCount = 0;
	dev_cinfo.ppEnabledExtensionNames = nullptr;
#else 
	const char* ext[] = { "VK_KHR_portability_subset"};
	dev_cinfo.enabledExtensionCount = 1;
	dev_cinfo.ppEnabledExtensionNames = ext;
#endif
	dev_cinfo.pEnabledFeatures = nullptr;

	CHECKVK(vkCreateDevice(m_pdev, &dev_cinfo, nullptr, &m_dev), 
		"failed to create logical device!");

	vkGetDeviceQueue(m_dev, m_c_queue_idx, 0, &m_c_queue);
	if(m_c_queue_idx == m_t_queue_idx)
		vkGetDeviceQueue(m_dev, m_c_queue_idx, 1, &m_t_queue);
	else
		vkGetDeviceQueue(m_dev, m_t_queue_idx, 0, &m_t_queue);

}

void
Renderer::create_allocator()
{
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
	allocatorInfo.physicalDevice = m_pdev;
	allocatorInfo.device = m_dev;
	allocatorInfo.instance = m_instance;

	CHECKVK(vmaCreateAllocator(&allocatorInfo, &m_vma), 
		"failed to create VMA allocator!");


}


void 
Renderer::create_command_pools()
{
	VkCommandPoolCreateInfo pool_cinfo;
	pool_cinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_cinfo.pNext = nullptr;
	pool_cinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_cinfo.queueFamilyIndex = m_c_queue_idx;

	CHECKVK(vkCreateCommandPool(m_dev, &pool_cinfo, nullptr, &m_c_cmd_pool), 
		"failed to create compute command pool!");

	pool_cinfo.flags = 0;
	pool_cinfo.queueFamilyIndex = m_t_queue_idx;

	CHECKVK(vkCreateCommandPool(m_dev, &pool_cinfo, nullptr, &m_t_cmd_pool), 
		"failed to create transfer command pool!");

}

void 
Renderer::create_command_buffers()
{
	VkCommandBufferAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.commandPool = m_c_cmd_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 2;


	CHECKVK(vkAllocateCommandBuffers(m_dev, &alloc_info, m_c_cmd_buf_array),
		"failed to allocate compute command buffers!");
	
	alloc_info.commandPool = m_t_cmd_pool;
	alloc_info.commandBufferCount = 1;

	CHECKVK(vkAllocateCommandBuffers(m_dev, &alloc_info, &m_t_cmd_buf),
		"failed to allocate transfer command buffer!");

}


void 
Renderer::create_pipeline()
{
	const size_t pool_size_array_size = 2;
	VkDescriptorPoolSize pool_size_array[pool_size_array_size];

	pool_size_array[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	pool_size_array[0].descriptorCount = 4;

	pool_size_array[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_size_array[1].descriptorCount = 2;


	VkDescriptorPoolCreateInfo pool_cinfo;
	pool_cinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_cinfo.pNext = nullptr;
	pool_cinfo.flags = 0;
	pool_cinfo.maxSets = 2;
	pool_cinfo.poolSizeCount = pool_size_array_size;
	pool_cinfo.pPoolSizes = pool_size_array;	


	CHECKVK(vkCreateDescriptorPool(m_dev, &pool_cinfo, nullptr, &m_descriptor_pool), 
		"failed to create descriptor pool!");




	const size_t binding_array_size = 3;
	VkDescriptorSetLayoutBinding binding_array[binding_array_size];
	
	binding_array[0].binding = 0;
	binding_array[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding_array[0].descriptorCount = 1;
	binding_array[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binding_array[0].pImmutableSamplers = nullptr;

	binding_array[1].binding = 1;
	binding_array[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binding_array[1].descriptorCount = 1;
	binding_array[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binding_array[1].pImmutableSamplers = nullptr;

	binding_array[2].binding = 2;
	binding_array[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binding_array[2].descriptorCount = 1;
	binding_array[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	binding_array[2].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo dset_layout_cinfo;
	dset_layout_cinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dset_layout_cinfo.pNext = nullptr;
	dset_layout_cinfo.flags = 0;
	dset_layout_cinfo.bindingCount = binding_array_size;
	dset_layout_cinfo.pBindings = binding_array;


	CHECKVK(vkCreateDescriptorSetLayout(m_dev, &dset_layout_cinfo, nullptr, &m_dset_layout),
		"failed to create descriptor set layout!");




	VkPipelineLayoutCreateInfo layout_cinfo;
	layout_cinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_cinfo.pNext = nullptr;
	layout_cinfo.flags = 0;
	layout_cinfo.setLayoutCount = 1;
	layout_cinfo.pSetLayouts = &m_dset_layout;
	layout_cinfo.pushConstantRangeCount = 0;
	layout_cinfo.pPushConstantRanges = nullptr;

	CHECKVK(vkCreatePipelineLayout(m_dev, &layout_cinfo, nullptr, &m_pipeline_layout),
		"failed to create pipeline layout");


	size_t shader_code_size;
	void* shader_code = read_binary("path.comp.spv", &shader_code_size);
	if(shader_code == nullptr)
	{
		fprintf(stderr, "failed to read shader binary from disk!\n");
		exit(1);
	}

	VkShaderModuleCreateInfo shader_module_cinfo;
	shader_module_cinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module_cinfo.pNext = nullptr;
	shader_module_cinfo.flags = 0;
	shader_module_cinfo.codeSize = shader_code_size;
	shader_module_cinfo.pCode = (uint32_t*)shader_code; //glslangValidator produces 4 byte aligned binaries, and the binary reader will read exactly that, so no problem

	VkShaderModule shader_module;
	CHECKVK(vkCreateShaderModule(m_dev, &shader_module_cinfo, nullptr, &shader_module),
		"failed to create shader module!");


	VkPipelineShaderStageCreateInfo shader_stage_cinfo;
	shader_stage_cinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stage_cinfo.pNext = nullptr;
	shader_stage_cinfo.flags = 0;
	shader_stage_cinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shader_stage_cinfo.module = shader_module;
	shader_stage_cinfo.pName = "main";
	shader_stage_cinfo.pSpecializationInfo = nullptr;



	VkComputePipelineCreateInfo pipe_cinfo;
	pipe_cinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_cinfo.pNext = nullptr;
	pipe_cinfo.flags = 0;
	pipe_cinfo.stage = shader_stage_cinfo;
	pipe_cinfo.layout = m_pipeline_layout;
	pipe_cinfo.basePipelineHandle = VK_NULL_HANDLE;
	pipe_cinfo.basePipelineIndex = 0;

	CHECKVK(vkCreateComputePipelines(m_dev, VK_NULL_HANDLE, 1, &pipe_cinfo, nullptr, &m_compute_pipeline),
		"failed to create compute pipeline!");

}


void
Renderer::init()
{
	create_instance();
	choose_pdev();
	choose_queue_families();
	create_device();
	create_allocator();
	create_command_pools();
	create_command_buffers();
	create_pipeline();
}



void
Renderer::quit()
{


	vkDestroyCommandPool(m_dev, m_t_cmd_pool, nullptr);
	vkDestroyCommandPool(m_dev, m_c_cmd_pool, nullptr);
	vmaDestroyAllocator(m_vma);
	vkDestroyDevice(m_dev, nullptr);
	vkDestroyInstance(m_instance, nullptr);
	//no cleanup for volk
}


void
Renderer::render_frame()
{

}




bool 
Renderer::create_buffer(Buffer* buf, const size_t size)
{
	VkBufferCreateInfo buf_cinfo;
	buf_cinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_cinfo.pNext = nullptr;
	buf_cinfo.flags = 0; 
	buf_cinfo.size = size;
	buf_cinfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; 
	buf_cinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buf_cinfo.queueFamilyIndexCount = 0;
	buf_cinfo.pQueueFamilyIndices = nullptr;

	VmaAllocationCreateInfo ainfo = {};
	ainfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	buf->size = size;

	return vmaCreateBuffer(m_vma, &buf_cinfo, &ainfo, &buf->handle, &buf->alloc, nullptr) == VK_SUCCESS;
}

void 
Renderer::destroy_buffer(Buffer* buf)
{
	vmaDestroyBuffer(m_vma, buf->handle, buf->alloc);
}


bool
Renderer::copy_to_buffer(VkCommandBuffer cmd_buf, Buffer* buf, const void* data, const size_t size)
{
	if(size > buf->size)
		return false;


	Buffer staging_buf;

	VkBufferCreateInfo buf_cinfo;
	buf_cinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_cinfo.pNext = nullptr;
	buf_cinfo.flags = 0; 
	buf_cinfo.size = size;
	buf_cinfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; 
	buf_cinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buf_cinfo.queueFamilyIndexCount = 0;
	buf_cinfo.pQueueFamilyIndices = nullptr;

	VmaAllocationCreateInfo ainfo = {};
	ainfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;


	if(vmaCreateBuffer(m_vma, &buf_cinfo, &ainfo, &staging_buf.handle, &staging_buf.alloc, nullptr) != VK_SUCCESS)
		return false;

	{
		void* memory;
		vmaMapMemory(m_vma, staging_buf.alloc, &memory);
			memcpy(memory, data, size);
		vmaUnmapMemory(m_vma, staging_buf.alloc);
	}

	//transfer ownership from compute queue to transfer queue
	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = m_c_queue_idx;
	barrier.dstQueueFamilyIndex = m_t_queue_idx;
	barrier.buffer = buf->handle;
	barrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

	VkBufferCopy copy_region;
	copy_region.srcOffset = 0;
	copy_region.dstOffset = 0;
	copy_region.size = size;

	vkCmdCopyBuffer(cmd_buf, staging_buf.handle, buf->handle, 1, &copy_region);


	//transfer ownership back to compute queue
	barrier.srcQueueFamilyIndex = m_t_queue_idx;
	barrier.dstQueueFamilyIndex = m_c_queue_idx;

	vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT,  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

	m_destroy_after_transfer_vec.push_back(staging_buf);
	return true;

}