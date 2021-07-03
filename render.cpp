#include "render.h"
#include "volk.h"

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <set>

Renderer::Renderer(const char* render_name, int width, int height, bool display_live) : m_render_name(render_name), m_width(width), m_height(height), m_display_live(display_live) {}
Renderer::~Renderer(){}


#define CHECKVK(expr, msg) if((expr) != VK_SUCCESS){fprintf(stderr, "%s:%d - %s\n", __FILE__, __LINE__, msg); exit(1);}

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

void 
Renderer::choose_queue_families()
{
	//ideally we have 1 compute queue, and 1 transfer queue
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
				break;
			}
		}
	}
	else 
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
Renderer::init()
{
	create_instance();
	choose_pdev();
	choose_queue_families();
	create_device();
	create_allocator();

}



void
Renderer::quit()
{

	vkDestroyDevice(m_dev, nullptr);
	vkDestroyInstance(m_instance, nullptr);
	//no cleanup for volk
}


void
Renderer::render_frame()
{

}