#include "render.h"
#include "volk.h"

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>

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
	instance_cinfo.enabledExtensionCount = 0;
	instance_cinfo.ppEnabledExtensionNames = nullptr;

	CHECKVK(vkCreateInstance(&instance_cinfo, nullptr, &m_instance) , "failed to create vulkan instance!");
	volkLoadInstance(m_instance);
}





void
Renderer::init()
{
	create_instance();
	
}



void
Renderer::quit()
{


	vkDestroyInstance(m_instance, nullptr);
	//no cleanup for volk
}


void
Renderer::render_frame()
{

}