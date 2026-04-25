#define NOMINMAX//否则用max函数会报错

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include<iostream>
#include <stdexcept>
#include <cstdlib>
#include<cstring>
#include<vector>
#include<optional>
#include <set>
#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <fstream>


const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};
const std::vector<const char*> deviceExtensions = {//扩展功能
	VK_KHR_SWAPCHAIN_EXTENSION_NAME//支持交换链
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}
struct QueueFamilyIndices {//不同队列族序号变量组成的结构体.用optional包装int，好处是能使用undefined表示族不存在
	std::optional<uint32_t> graphicsFamily;//图像渲染功能的队列族序号
	std::optional<uint32_t> presentFamily;//窗口表面呈现能力的队列族序号

	bool isComplete() {//检查所需族是否存在
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};
struct SwapChainSupportDetails {//交换链支持的详细信息
	VkSurfaceCapabilitiesKHR capabilities;//基本表面功能（交换链中图像的最小/最大数量，图像的最小/最大宽度和高度）
	std::vector<VkSurfaceFormatKHR> formats;//表面格式（像素格式，颜色空间）
	std::vector<VkPresentModeKHR> presentModes;//可用的演示模式
};

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanUp();
	}
private:
	GLFWwindow* window;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;

	VkSurfaceKHR surface;//表面，用于呈现渲染的图像

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;//物理设备：显卡存储在此句柄
	VkDevice device;//存储逻辑设备的句柄，逻辑设备是对物理设备的抽象，提供了与物理设备交互的接口
	VkQueue graphicsQueue;//存储图形队列的句柄，图形队列是逻辑设备提供的一种特殊类型的队列，用于提交图形命令
	VkQueue presentQueue;//存储呈现队列的句柄
	VkSwapchainKHR swapChain;//交换链
	std::vector<VkImage> swapChainImages;//交换链VkImage 的句柄，渲染操作期间引用
	VkFormat swapChainImageFormat;//为交换链图像选择的格式和范围存储在成员变量中
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	void initWindow() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}
	void initVulkan() {
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createGraphicsPipeline();
	}
	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
	}
	void cleanUp() {
		for (auto imageView : swapChainImageViews) {
			vkDestroyImageView(device, imageView, nullptr);
		}
		vkDestroySwapchainKHR(device, swapChain, nullptr);
		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}
		vkDestroySurfaceKHR(instance, surface, nullptr);//确保在实例之前销毁表面
		vkDestroyInstance(instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
		vkDestroyDevice(device, nullptr);//销毁逻辑设备
	}
	void createInstance() {
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}


		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else {
			createInfo.enabledLayerCount = 0;

			createInfo.pNext = nullptr;
		}

		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}
	void createSurface() {
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
	}
	//物理与逻辑设备相关
	void pickPhysicalDevice() {//找显卡
		uint32_t deviceCount = 0;//经典的先查询数量再vector保存所有显卡
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0) {//没有显卡则不保存，报错
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices) {//用isDeviceSuitable将第一个能做所需功能的显卡写入类成员
			if (isDeviceSuitable(device)) {
				physicalDevice = device;
				break;
			}
		}
		if (physicalDevice == VK_NULL_HANDLE) {//没有满足所需功能的显卡，报错
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {//队列族，显卡的不同功能由不同的队列族提供
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;//经典的先查询数量再vector保存所有队列族
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
		
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {//找到能提供所需功能的队列族序号
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {//检查队列族图形能力
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;//检查队列族的窗口表面呈现能力
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.isComplete()) {//都找到了就退出
				break;
			}

			i++;
		}

		return indices;
	}
	bool isDeviceSuitable(VkPhysicalDevice device) {//第五章推荐加，将优质显卡排在前面，用于选择适用且最合适的显卡（此处未实现）
		QueueFamilyIndices indices = findQueueFamilies(device);//检查物理设备是否支持所需功能
		bool extensionsSupported = checkDeviceExtensionSupport(device);//物理设备是否支持所需扩展
		//检查物理设备所需的交换链扩展的详细信息是否满足要求
		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();//交换链必须至少支持一种图像格式和一种演示模式才能被认为是适合的
		}
		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}
	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;//经典的先查询数量再vector保存所有扩展
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());//所需的所有扩展名

		for (const auto& extension : availableExtensions) {//遍历所有扩展，看是否满足已包含所需扩展
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}
	void createLogicalDevice() {//创建逻辑设备
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);//找出提供图形功能的队列族序号
		//根据队列族创建队列信息
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;//每个队列族的队列创建信息
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };//每个队列族的序号，去重
		float queuePriority = 1.0f;//即使只有一个队列，也要为其指定优先级，范围是0.0到1.0。
		for (uint32_t queueFamily : uniqueQueueFamilies) {//每个队列族都要创建一个队列创建信息结构体
			VkDeviceQueueCreateInfo queueCreateInfo{};//在此结构体填入队列信息
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;//每个队列族可以有多个队列，但我们只需要一个
			queueCreateInfo.pQueuePriorities = &queuePriority;//设置优先级
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};//物理设备支持的功能，逻辑设备需要的功能必须在此结构体中指定

		//创建逻辑设备
		//用以上得到的两个结构体完成
		VkDeviceCreateInfo createInfo{};//在此结构体填入逻辑设备信息
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();//添加指向队列创建信息和设备功能结构的指针
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());//扩展数量
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();//扩展名数组

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {//"参数是要交互的物理设备、我们刚刚指定的队列和使用信息、可选的分配回调指针以及指向存储逻辑设备句柄的变量的指针"
			throw std::runtime_error("failed to create logical device!");
		}
		//存储所需队列
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);//参数是逻辑设备、队列族、队列索引以及指向存储队列句柄的变量的指针
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
	}
	//交换链相关
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {//查询交换链细节信息
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);//结构体的第一个变量
		//第二个变量
		uint32_t formatCount;//经典的先查询数量再vector保存所有表面格式
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}
		//第三个变量
		uint32_t presentModeCount;//经典的先查询数量再vector保存所有演示模式
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}
		return details;
	}
	//上一个函数用于保障细节信息支持足够，现在需要找到每项信息的最佳设置
	//表面格式,颜色深度
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {//每个 VkSurfaceFormatKHR 条目都包含一个 format 和一个 colorSpace 成员。
		for (const auto& availableFormat : availableFormats) {//遍历列表
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {//查看首选组合是否可用
				return availableFormat;
			}
		}
		return availableFormats[0];//没有首选，就要第一个可用的
	}
	//呈现模式,表示将图像显示到屏幕的实际条件。
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {//比标准垂直同步更少的延迟问题，通常被称为“三重缓冲”的模式
				return availablePresentMode;
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;//与现代游戏中发现的垂直同步最相似的模式
	}
	//交换范围,交换链图像的分辨率
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}
		else {
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}
	void createSwapChain() {//交换链创建
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);//细节信息的实例，调用三个函数得到三项变量的最佳设置

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;//决定在交换链中想要拥有多少个图像,该实现指定其运行所需的最小数量
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {//确保不大于最大数量
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;//指定表面
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.presentMode = presentMode;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;//每个图像包含的层数，非3d为1.
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//使用图像进行哪种类型的操作，此处为直接渲染到图像上

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
		//根据图像和呈现是否需要不同的队列族，选择并发或独占模式
		if (indices.graphicsFamily != indices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;//此选项提供最佳性能。
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}
		//指定对交换链图像进行变换
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;//不旋转
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;//不与其他窗口混合
		createInfo.clipped = VK_TRUE;//不关心被遮挡像素的颜色
		createInfo.oldSwapchain = VK_NULL_HANDLE;//"未来的章节中了解更多"

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain!");
		}
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}
	void createImageViews() {//交换链图像视图创建
		swapChainImageViews.resize(swapChainImages.size());//每个交换链图像都需要一个图像视图
		for (size_t i = 0; i < swapChainImages.size(); i++) {//每个交换链图像都需要一个图像视图
			VkImageViewCreateInfo createInfo{};//用于创建视图的结构体
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;//解释为2d贴图
			createInfo.format = swapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;//图像的用途以及应访问图像的哪一部分
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create image views!");
			}
		}
	}
	//图形管线相关
	static std::vector<char> readFile(const std::string& filename) {//读取shader文件，得到其二进制码
		std::ifstream file(filename, std::ios::ate | std::ios::binary);//从末尾以二进制读取

		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}
		size_t fileSize = (size_t)file.tellg();//从末尾，故能得到文件大小
		std::vector<char> buffer(fileSize);
		file.seekg(0);//从头按文件大小读入存储
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}
	VkShaderModule createShaderModule(const std::vector<char>& code) {//通过shader的二进制码，把shader信息包装为着色器模块
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());//字节码的大小是以字节为单位指定的，但是字节码指针是 uint32_t 指针，而不是 char 指针，需转换
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}
		return shaderModule;
	}
	void createGraphicsPipeline() {//创建图形管线
		auto vertShaderCode = readFile("shaders/vert.spv");//顶点着色器二进制码
		auto fragShaderCode = readFile("shaders/frag.spv");//片段着色器二进制码

		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);//着色器模块只用于传递shader信息，故不是类成员，在函数内创建销毁。
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		//为实际使用着色器信息，通过结构体分配给特定的管线阶段
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;//必须指定阶段
		vertShaderStageInfo.module = vertShaderModule;//使用的着色器模块
		vertShaderStageInfo.pName = "main";//shader入口函数

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};//同理
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };//管线阶段数组



		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}

	//验证层相关
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}

	std::vector<const char*> getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	bool checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}
};
int main() {
	HelloTriangleApplication	app;
	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}