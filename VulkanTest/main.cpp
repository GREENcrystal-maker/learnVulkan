#define NOMINMAX//否则用max函数会报错

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

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
#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>//精确计时功能函数
#include <unordered_map>


const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const int MAX_FRAMES_IN_FLIGHT = 2;//两个飞行中的帧，即允许一帧的渲染（gpu）不干扰下一帧的录制（cpu），而不是必须等待前一帧完成才能开始渲染下一帧，这会导致主机不必要的空闲。
const int ITEM_COUNT = 2;//物体数量

const std::vector<std::string> MODEL_PATHS = {"models/bunny.obj", "models/sphere.obj"};//模型和纹理的位置
const std::vector<std::string> TEXTURE_PATHS = { "textures/bunny.png","textures/basketball.png" };

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
struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;//纹理的（u，v）
	//描述如何将此数据传到内存后传递给顶点着色器，两种结构体
	static VkVertexInputBindingDescription getBindingDescription() {//顶点绑定结构体
		VkVertexInputBindingDescription bindingDescription{};//顶点数据都打包在一个向量里，所以只需一个绑定
		bindingDescription.binding = 0;//绑定索引，唯一的一个
		bindingDescription.stride = sizeof(Vertex);//条目间步长字节数
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//在每个顶点之后移动到下一个数据条目
		return bindingDescription;
	}
	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {//属性描述结构体
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};//3指位置、颜色和纹理坐标
		attributeDescriptions[0].binding = 0;//唯一的绑定
		attributeDescriptions[0].location = 0;//索引
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;//表示有三个32位浮点分量。个数用颜色格式，RGBA分别对应单值、vec2，vec3等，此处即vec3
		attributeDescriptions[0].offset = offsetof(Vertex, pos);//自每个顶点数据的开始读取的字节数，自动计算
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;//即vec3
		attributeDescriptions[1].offset = offsetof(Vertex, color);
		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;//即vec2
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
		return attributeDescriptions;
	}
	bool operator==(const Vertex& other) const {//重载相等运算，用于unoderedmap的比较键是否存在
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}
};
namespace std {//为Vertex实现哈希函数，用于unoderedmap
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}
/*使用模型，不需要另写顶点
const std::vector<Vertex> vertices = {
<<<<<<< HEAD
	//{{位置}, {颜色}}，位置是三维的，颜色是三维
	{{ -0.75f, -0.25f ,0.0f}, {1.0f, 0.0f, 0.0f}},
	{{-0.25f , -0.25f,0.0f}, {0.0f, 1.0f, 0.0f}},
	{{-0.25f , 0.25f,0.0f}, {0.0f, 0.0f, 1.0f}},
	{{-0.75f , 0.25f,0.0f}, {1.0f, 1.0f, 1.0f}},

	{{ 0.25f, -0.25f ,0.0f}, {1.0f, 0.0f, 0.0f}},
	{{0.75f , -0.25f,0.0f}, {0.0f, 1.0f, 0.0f}},
	{{0.75f , 0.25f,0.0f}, {0.0f, 0.0f, 1.0f}},
	{{0.25f , 0.25f,0.0f}, {1.0f, 1.0f, 1.0f}}


=======
	//{{位置}, {颜色}, {纹理坐标}}，位置是二维的，颜色是三维，纹理坐标是二维
	{{ -0.5f, -0.5f, 0.0f }, {1.0f, 0.0f, 0.0f},{1.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f},{0.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f},{0.0f, 1.0f}},
	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f},{1.0f, 1.0f}},

	{{ -0.5f, -0.5f, -0.5f }, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },
	{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
>>>>>>> origin/master
};

const std::vector<uint16_t> indices = {
	//顶点索引
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};
*/
struct DirectionalLight {
	alignas(16) glm::vec3 dir; // 指向光源的方向
	alignas(16) glm::vec3 color;     // 光照颜色
	alignas(16) glm::vec3 intensity; //强度
};
struct PointLight {
	alignas(16) glm::vec3 pos;
	alignas(16) glm::vec3 color;
	alignas(16) glm::vec3 viewPos;
	alignas(16) glm::vec2 args;//x=强度系数，y=最大作用距离
	//alignas(16) glm::vec3 attenuationParams; // 衰减方程的系数，x=常数项的, y=线性~, z=二次方~ 只用1+d^2作分母所以不需要此项
};
struct SpotLight {
	alignas(16) glm::vec3 pos;
	alignas(16) glm::vec3 dir;
	alignas(16) glm::vec3 color;
	alignas(16) glm::vec4 args; // x=内圆锥角余弦值，y=外圆锥角余弦值，z=强度系数，w=最大作用距离
};
struct UniformBufferObject {
	//三个4*4矩阵，描述一个3d模型的显示到2d屏幕所需的所有信息
	//物体的3d位置可以认为是(x,y,z,w)的四维列向量，w是1代表这是一个三维空间的点，当一个4*4矩阵乘它时，得到一个新的四维列向量，w仍然是1，前面三个分量是变换后的三维位置，所以这个矩阵所存储的就是变换（平移缩放旋转）信息，存储方式详见“资源”。于是我们用根据物体信息，摄像头信息创建出来这三个矩阵，用来记载这些信息要求的变换（平移缩放旋转），当他们依次乘上四维向量，就得到了2d显示所需的x，y，z（图层深度）。分成三个是因为要根据的信息被分成三块，分别为：物体3d模型样貌，摄像头摆放信息，摄像头视野性质。
	//那么每个矩阵各位置含义是什么？综合“资源”所示变换中各位置含义可得，将左上角3*3每行视作物体的x，y，z方向，第四列视作x,y,z,w,剩下三个为与透视有关信息
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;

	alignas(16) DirectionalLight dirLights[1];   // 通常场景只有一个主方向光(太阳)
	alignas(16) PointLight pointLights[2];       // 最多8个点光源
	alignas(16) SpotLight spotLights[1];         // 最多4个聚光灯

	alignas(16) glm::ivec3 lightCounts;          // x=dirCount, y=pointCount, z=spotCount
	alignas(16) glm::vec4 ambientArgs; // xyz，w=rgb，强度
	alignas(16) glm::vec2 strength;//x=漫反射强度系数，y=镜面反射强度系数,即公式的两个p幂
};
struct ModelInfo {// 记录每个模型的索引数量和起始偏移
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t vertexOffset;
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
	std::vector<VkImageView> swapChainImageViews;//每个交换链图像的图像视图
	VkRenderPass renderPass;//渲染过程
	VkDescriptorSetLayout descriptorSetLayout;//描述符集布局，描述符是着色器访问资源的接口，描述符集布局定义了着色器中使用的资源类型和数量
	VkPipelineLayout pipelineLayout;//管线布局
	VkPipeline graphicsPipeline;//管线
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkCommandPool commandPool;//命令池，管理内存分配和命令缓冲区的生命周期
	std::vector<VkCommandBuffer> commandBuffers;//命令缓冲区，记录要提交给图形队列的渲染命令
	std::vector<VkSemaphore> imageAvailableSemaphores;//两个信号量和一个栅栏
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;//以上做成向量是为了同时处理多帧
	uint32_t currentFrame = 0;//记录是两帧里的哪一帧
	bool framebufferResized = false;//记录窗口大小是否发生变化

	std::vector<Vertex> vertices;//作为private变量的顶点信息和顶点索引
	std::vector<uint32_t> indices;//顶点个数多于65535时，需32位int

	VkBuffer vertexBuffer;//顶点缓冲区句柄
	VkDeviceMemory vertexBufferMemory;//区的内存	
	VkBuffer indexBuffer;//索引缓冲区句柄
	VkDeviceMemory indexBufferMemory;//它的内存
	std::vector<VkBuffer> uniformBuffers;//与正在处理帧数一样多的统一缓冲区句柄
	std::vector<VkDeviceMemory> uniformBuffersMemory;//它们的内存
	std::vector<void*> uniformBuffersMapped;//存储每个统一缓冲区的映射内存地址的向量，用于memcpy，需要成员记录地址是因为memcpy需要在create外调用
	VkDescriptorPool descriptorPool;//描述符池，管理描述符集的内存分配
	std::vector<VkDescriptorSet> descriptorSets;//描述符集，描述符的集合，每帧分配一个，存储在向量中
	std::vector<VkImage> textureImage;//纹理的vulkan的图像对象，其像素称为纹素
	std::vector <VkDeviceMemory> textureImageMemory;//图像对象类比缓冲区，需要内存
	std::vector <VkImageView> textureImageView;//纹理的图像视图，纹理图像不能直接使用，需要创建一个图像视图来描述如何访问纹理图像以及使用哪个颜色通道
	VkSampler textureSampler;
	VkImage depthImage;//深度图像
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;
	std::vector<float> rotateAngle;

	float lastTime = 0.0f;
	std::vector<float> scale;
	std::vector<float> trans_z;
	uint32_t modelChosen = 1;
	std::array<ModelInfo, 2> modelInfos;
	void initWindow() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);//为glfw存储this指针，以资使用
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);//窗口大小变化时，会调用2nd参数的函数
	}
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {//static因为glfw没有this指针概念
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));//相当于得到了this指针
		app->framebufferResized = true;
	}
	void initVulkan() {
		controllInit();
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createCommandPool();
		createDepthResources();
		createFramebuffers();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		loadModel();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffer();
		createSyncObjects();
	}
	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			drawFrame();
		}
	}
	void cleanupSwapChain() {//清理交换链
		vkDestroyImageView(device, depthImageView, nullptr);
		vkDestroyImage(device, depthImage, nullptr);
		vkFreeMemory(device, depthImageMemory, nullptr);
		for (auto framebuffer : swapChainFramebuffers) {
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		for (auto imageView : swapChainImageViews) {
			vkDestroyImageView(device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}
	void cleanUp() {
		cleanupSwapChain();
		vkDestroySampler(device, textureSampler, nullptr);
		for (size_t i = 0; i < ITEM_COUNT; i++) {
			vkDestroyImageView(device, textureImageView[i], nullptr);
			vkDestroyImage(device, textureImage[i], nullptr);
			vkFreeMemory(device, textureImageMemory[i], nullptr);
		}
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT * ITEM_COUNT; i++) {
			vkDestroyBuffer(device, uniformBuffers[i], nullptr);
			vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
		}
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);//同时完成池内分配句柄释放
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		vkDestroyBuffer(device, indexBuffer, nullptr);
		vkFreeMemory(device, indexBufferMemory, nullptr);
		vkDestroyBuffer(device, vertexBuffer, nullptr);
		vkFreeMemory(device, vertexBufferMemory, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}
		vkDestroyCommandPool(device, commandPool, nullptr);

		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);

		vkDestroyDevice(device, nullptr);//先销毁逻辑设备
		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}
		vkDestroySurfaceKHR(instance, surface, nullptr);//确保在实例之前销毁表面
		vkDestroyInstance(instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();

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
		VkPhysicalDeviceFeatures supportedFeatures;//获取物理设备的支持功能，.出具体功能检查支持性
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
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
		deviceFeatures.samplerAnisotropy = VK_TRUE;//启用各向异性过滤功能
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
	/*void createImageViews() {//交换链图像视图创建
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
	}*/
	void createImageViews() {//主逻辑在纹理相关
		swapChainImageViews.resize(swapChainImages.size());

		for (uint32_t i = 0; i < swapChainImages.size(); i++) {
			swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}

	void recreateSwapChain() {//重新创建交换链
		//用于窗口大小发生变化，导致原交换链失效时，根据新大小重新创建
		int width = 0, height = 0;//处理最小化窗口情况，此时交换链失效且无需呈现，故暂停程序而不是新建交换链
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(device);

		cleanupSwapChain();//清理失效交换链

		createSwapChain();
		createImageViews();
		createDepthResources();//根据新的宽高数据重建深度缓冲，以匹配新的颜色附件分辨率
		createFramebuffers();
		//可选：渲染通道重新创建。仅部分情况下需要。
	}

	//缓冲区相关
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;//缓冲区大小，单位为字节
		bufferInfo.usage = usage;//指定目的
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;//缓冲区可由某队列族独有或多个队列族共享，此处仅从图形队列访问，设为独有
		if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create vertex buffer!");
		}
		//分配内存//内存管理是缓冲区的重要步骤
		VkMemoryRequirements memRequirements;//一个结构体，描述了内存需求，包括大小、字节偏移量和内存类型的适用位域
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);//查询内存需求
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
		if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate vertex buffer memory!");
		}
		vkBindBufferMemory(device, buffer, bufferMemory, 0);//关联申请的内存和缓冲区
	}
	/*
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {//实现内容在缓冲区之间复制传输
		//使用命令缓冲区执行复制
		//此函数内快速完成发出命令的各阶段（类比绘制命令的发出）：命令缓冲区分配、记录、提交
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;//用已有的命令池即可
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;//只使用一次命令缓冲区，并等待函数返回，直到复制操作完成执行

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkBufferCopy copyRegion{};//要复制的区域
		copyRegion.srcOffset = 0; //缓冲区偏移量
		copyRegion.dstOffset = 0; // 目标缓冲区偏移量
		copyRegion.size = size;//缓冲区大小
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);//传输内容，指定源与目的
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);//不需要等待任何东西，只等待队列空闲
		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}*/
	VkCommandBuffer beginSingleTimeCommands() {//copy里命令缓冲区快速创建，分配、记录的部分
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}
	void endSingleTimeCommands(VkCommandBuffer commandBuffer) {//copy里命令缓冲区完成任务后的提交部分。分开是因为，现在想在提交前，函数外，用快速创建的命令缓冲区完成别的功能，再提交。目前这功能即是复制传输数据到图像对象
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {//调用快速创建命令缓冲区，然后用来完成copybuffer传输，然后提交命令缓冲区
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		endSingleTimeCommands(commandBuffer);
	}
	//Vulkan 中的缓冲区是用于存储可由显卡读取的任意数据的内存区域。它们可以用来存储顶点数据，也可以用于许多其他目的
	void createVertexBuffer() {//创建顶点缓冲区
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();//计算顶点缓冲区大小
		//使用临时和顶点两个缓冲区，原因是：最佳内存具有 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 标志，并且通常在专用图形卡上无法通过 CPU 访问，为使用最佳内存，我们创建一个临时缓冲区，使用它来将数据从 CPU 内存复制到 GPU 内存，以提高性能
		VkBuffer stagingBuffer;//临时缓冲区
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);//创建临时缓冲区，传SRC_BIT:缓冲区可以用作内存传输操作的源。

		void* data;//将顶点数据复制到缓冲区
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);//创建真正的顶点缓冲区，传DST_BIT:缓冲区可以用作内存传输操作的目的地
		copyBuffer(stagingBuffer, vertexBuffer, bufferSize);//数据从临时缓冲区复制到顶点缓冲区
		vkDestroyBuffer(device, stagingBuffer, nullptr);//清理临时缓冲区
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		//显卡可以提供不同类型的内存来分配。每种类型的内存在允许的操作和性能特征方面都有所不同。我们需要结合缓冲区的需求和我们自己的应用程序需求，找到要使用的正确内存类型
		VkPhysicalDeviceMemoryProperties memProperties;//结构体
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {//找到适合缓冲区的内存类型
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {//typeFilter 参数将用于指定适合的内存类型的位域，遍历并检查是否将相应的位设置为 1
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}
	void createIndexBuffer() {//除了缓冲区类型和大小，其余同顶点缓冲区
		//使用索引缓冲区是因为，图像由多个三角形组成，当它们共顶点时，使用索引缓冲区可以避免重复存储顶点数据，从而节省内存并提高性能
		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

		copyBuffer(stagingBuffer, indexBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}
	void createDescriptorSetLayout() {
		VkDescriptorSetLayoutBinding uboLayoutBinding{};//统一缓冲区的绑定
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;//描述符类型，表示绑定资源的类型，此处为统一缓冲区
		uboLayoutBinding.descriptorCount = 1;//描述符数量
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;//在哪些着色器阶段引用描述符，此处为顶点着色器
		uboLayoutBinding.pImmutableSamplers = nullptr; // Optional
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};//采样器的绑定
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;//在片段着色器阶段引用描述符

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}
	void createUniformBuffers() {//创建统一缓冲区
		//统一缓冲区，用来存放cpu传给gpu的全局、动态数据，它们是cpu每帧需要动态计算和传递的，而不是顶点缓冲区那样写死的数据
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT * ITEM_COUNT);
		uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT * ITEM_COUNT);
		uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT * ITEM_COUNT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT * ITEM_COUNT; i++) {
			createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

			vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);//使用 vkMapMemory 在创建后立即映射缓冲区。不unmap，持续每帧更新指针数据
		}
	}
	void updateUniformBuffer(uint32_t currentImage, int sign) {//描述每帧进行的变换
		//drawFrame 函数中提交下一帧之前添加对其的调用，更新uniform数据
		processInput(window);
		UniformBufferObject ubo{};//以下计算出下一帧该有的2d坐标，并存储在ubo结构体中，传递给顶点着色器进行变换

		ubo.lightCounts = glm::ivec3(1, 2, 1);
		ubo.dirLights[0].dir = glm::vec3(-1.0f, -1.0f, 0.0f);//光源方向，传递给片段着色器进行光照计算
		ubo.dirLights[0].color = glm::vec3(1.0f, 1.0f, 1.0f);//光源颜色，传递给片段着色器进行光照计算
		ubo.dirLights[0].intensity = glm::vec3(1.0f, 1.0f, 1.0f);


		ubo.pointLights[0].pos = glm::vec3(0.0f, 0.0f, 10.0f);//光源位置，传递给片段着色器进行光照计算
		ubo.pointLights[0].viewPos = glm::vec3(0.0f, 0.0f, 10.0f);
		ubo.pointLights[0].color = glm::vec3(1.0f, 1.0f, 1.0f);//光源颜色，传递给片段着色器进行光照计算
		ubo.pointLights[0].args = glm::vec3(1.0f, 1.0f, 1.0f);
		ubo.pointLights[1].pos = glm::vec3(0.0f, 0.0f, 10.0f);//光源位置，传递给片段着色器进行光照计算
		ubo.pointLights[1].viewPos = glm::vec3(0.0f, 0.0f, 10.0f);
		ubo.pointLights[1].color = glm::vec3(1.0f, 1.0f, 1.0f);
		ubo.pointLights[1].args = glm::vec3(1.0f, 1.0f, 1.0f);

		ubo.spotLights[0].pos = glm::vec3(0.0f, 0.0f, 10.0f);//光源位置，传递给片段着色器进行光照计算
		ubo.spotLights[0].dir = glm::vec3(0.0f, 0.0f, 10.0f);
		ubo.spotLights[0].color = glm::vec3(0.0f, 0.0f, 10.0f);
		ubo.spotLights[0].args = glm::vec4(0.0f, 0.0f, 10.0f, 1.0f);
		//模型转换，描述模型每帧进行的变化，即把以3d的物体局部坐标（及其变化）投射到世界坐标
		ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, trans_z[sign])) *
			glm::scale(glm::mat4(1.0f), glm::vec3(scale[sign], scale[sign], 1.0f)) *
			glm::rotate(glm::mat4(1.0f), rotateAngle[sign], glm::vec3(0.0f, 1.0f, 0.0f));//参数：开始变换的初始矩阵、旋转角度、旋转轴。此处：单位矩阵作为基础样貌，旋转角度为每过了一秒增加九十度，即每秒旋转九十度；旋转轴为z轴

		//视图转换，指定怎么从3d世界坐标转换到摄像头画面的2d坐标，根据是摄像头摆放情况
		ubo.view = glm::lookAt(glm::vec3(0, 0, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.5f, 0));//参数：眼睛（摄像头）位置、观察中心位置、向上轴。向上轴是一个方向向量，指示摄像头的正上为哪个方向。此处：相当于上方以 45 度角查看几何体
		//投影转换，指定观察者需要的物体远近透视、生成比例，以明确物体各世界坐标应该怎样投射到2d，根据是摄像头的视野情况
		ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);//参数：zoom，画面比例，近裁剪面和远裁剪面。zoom决定了虚拟摄像机镜头的“张开程度”，可以把它完全等同于现实相机的镜头焦距，裁剪面规定了距离镜头距离多少范围可被显示，要够大。此处：一般使用的45度适中zoom，用交换链图像大小作为看东西视口的大小
		ubo.proj[1][1] *= -1;//GLM 以 OpenGL 的方式处理坐标，vulkan的y轴是反的，所以需要翻转y轴
		//三个函数都是生成4*4矩阵存储在ubo结构体中
		uint32_t bufferIndex = currentImage * ITEM_COUNT + sign;
		memcpy(uniformBuffersMapped[bufferIndex], &ubo, sizeof(ubo));//数据复制到当前统一缓冲区，与我们对顶点缓冲区所做的操作完全相同，只是没有临时缓冲区
	}
	void createDescriptorPool() {//创建描述符池，描述符池存储统一缓冲区和纹理采样器的信息
		std::array<VkDescriptorPoolSize, 2> poolSizes{};//描述符池大小，指定了每种类型的描述符需要多少个
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;//包含的描述符类型，第一个为统一缓冲区
		poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT*ITEM_COUNT);//描述符数量，为每一帧分配一个描述符
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;//第二个为采样器
		poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * ITEM_COUNT);//只需一个采样器即可

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());//描述符池中不同类型的描述符数量，此处为统一缓冲区和采样器两种
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT*ITEM_COUNT);//描述符集的最大数量，为每一帧分配一个描述符集
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}

	}
	void createDescriptorSets() {//创建描述符集，必须像命令缓冲区一样从池中分配
		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT * ITEM_COUNT, descriptorSetLayout);//每一帧一个描述符集，每个描述符集都使用相同的布局
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;//指定描述符池
		allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * ITEM_COUNT);//描述符集数量，为每一帧分配一个描述符集
		allocInfo.pSetLayouts = layouts.data();//指定使用的描述符集布局
		descriptorSets.resize(MAX_FRAMES_IN_FLIGHT * ITEM_COUNT);
		if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets!");
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT * ITEM_COUNT; i++) {//配置描述符
			VkDescriptorBufferInfo bufferInfo{};//对引用缓冲区的描述符（此处为统一缓冲区的）用此结构体进行配置
			bufferInfo.buffer = uniformBuffers[i];//指定缓冲区
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);//指定区中含有描述符数据的区域

			VkDescriptorImageInfo imageInfo{};//此结构体将实际的图像和采样器资源绑定到描述符集中的描述符
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			int modelIndex = i % ITEM_COUNT;// 计算当前描述符集对应的是第几个模型 (0 或 1)
			imageInfo.imageView = textureImageView[modelIndex];
			imageInfo.sampler = textureSampler;

			//更新配置

			std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
			//统一缓冲区部分
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptorSets[i];//要更新的描述符集
			descriptorWrites[0].dstBinding = 0;//给描述符绑定索引，必须与着色器中定义的绑定点匹配，此处为0
			descriptorWrites[0].dstArrayElement = 0;//描述符可以是数组，指定要更新的数组的第一个索引，没用数组所以为0
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // 所更新的描述符类型，此处为统一缓冲区
			descriptorWrites[0].descriptorCount = 1;//更新数组元素的数量，没用数组，只有一个
			descriptorWrites[0].pBufferInfo = &bufferInfo;//配置信息，用pBufferInfo是因为描述符类型是统一缓冲区
			//采样器部分
			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}

	//纹理相关
	/*
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {//创建图像对象
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;//指定为2维图像
		imageInfo.extent.width = static_cast<uint32_t>(width);//指定图像尺寸
		imageInfo.extent.height = static_cast<uint32_t>(height);
		imageInfo.extent.depth = 1;//2d图像的深度为1
		imageInfo.mipLevels = 1;// mipmaps的数量，mipmaps是同一图像的不同分辨率版本，较小的版本在物体远离摄像机时使用，以提高性能和减少锯齿。此处不使用，设为1
		imageInfo.arrayLayers = 1;//数组层的数量，对于2d纹理来说，深度和数组层都为1
		imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;//指定像素格式，此处为每个像素4个字节，分别为红绿蓝和alpha通道，与stbi_load加载的格式一致。这样做因为复制数据要求格式一致
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;//指定图像数据的内存布局，OPTIMAL表示由实现选择最佳布局，LINEAR表示行主序布局。OPTIMAL通常提供更好的性能，但不能直接访问像素数据，因此需要使用命令缓冲区进行复制
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//指定图像的初始布局，UNDEFINED表示不关心图像的初始内容，丢弃初始的纹素。除非用LINEAR直接访问像素才需保留初始纹素
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;//同于缓冲区创建的usage。此处表示图像将作为内存传输操作的目的地（从临时缓冲区复制数据）和着色器访问的采样图像
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;//图像访问模式，独占或共享，此处仅从图形队列访问，设为独占
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;//采样数量，表示每个像素的采样数，此处不使用多重采样，设为1
		imageInfo.flags = 0;//表示创建一个普通的图像对象，而不是一个特殊的类型，如立方体贴图或多视图图像
		if (vkCreateImage(device, &imageInfo, nullptr, &textureImage) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}
		//分配内存，与缓冲区的类似
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, textureImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &textureImageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(device, textureImage, textureImageMemory, 0);
	}*/
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(device, image, imageMemory, 0);
	}

	void createTextureImage() {//加载图像并将其上传到 Vulkan 图像对象中，使用命令缓冲实现
		textureImage.resize(ITEM_COUNT);
		textureImageMemory.resize( ITEM_COUNT);
		for (size_t i = 0; i < ITEM_COUNT; i++) {
			int texWidth, texHeight, texChannels;//将图像转化为像素数组作为待处理数据
			stbi_uc* pixels = stbi_load(TEXTURE_PATHS[i].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
			VkDeviceSize imageSize = texWidth * texHeight * 4;
			if (!pixels) {
				throw std::runtime_error("failed to load texture image: " + TEXTURE_PATHS[i]);
			}
			VkBuffer stagingBuffer;//接下来将用类似顶点缓冲区的方式，将像素从cpu传至临时缓冲区，再从临时缓冲区传至gpu图像对象
			VkDeviceMemory stagingBufferMemory;
			createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
			void* data;
			vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
			memcpy(data, pixels, static_cast<size_t>(imageSize));
			vkUnmapMemory(device, stagingBufferMemory);
			stbi_image_free(pixels);//清理原始像素数组

			createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage[i], textureImageMemory[i]);
				
			transitionImageLayout(textureImage[i], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);//将纹 理图像布局转换为 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			copyBufferToImage(stagingBuffer, textureImage[i], static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
			transitionImageLayout(textureImage[i], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);//布局转换为 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL，以便在着色器中采样
				
			vkDestroyBuffer(device, stagingBuffer, nullptr);//一定要在每次循环中清理，为下一张纹理使用临时缓冲区做准备
			vkFreeMemory(device, stagingBufferMemory, nullptr);
		}
	}
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {//临时缓冲区传输数据到图像对象，要求图像处于正确的布局中，所以先处理布局转换
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();//开始单次使用的命令缓冲区
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;//新旧布局
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//若屏障用于传输队列所有权，则指定队列索引，此处不需要，必须设为ignored
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;//指定图像对象
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;//指定受影响的图像和图像的特定部分，此处为颜色图像的颜色部分
		barrier.subresourceRange.baseMipLevel = 0;//指定 mipmaps 级别范围，此处不使用 mipmaps，设为0和1
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;//指定数组层范围，此处不使用数组，设为0和1
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {//根据转换的新旧布局，决定屏障前后操作涉及的资源类型以及管线阶段
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		vkCmdPipelineBarrier(//管线屏障提交函数
			commandBuffer,//命令缓冲区
			sourceStage, destinationStage,//屏障之前的操作发生在管线的哪个阶段，屏障之后的操作将发生在管线的哪个阶段，根据顺序阻塞
			0,//依赖标志，通常设为 0
			0, nullptr,//内存屏障
			0, nullptr,//缓冲区内存屏障
			1, &barrier//图像内存屏障，目前使用的唯一屏障类型
		);
		endSingleTimeCommands(commandBuffer);
	}
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {//将缓冲区数据复制到图像对象
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();//开始单次使用的命令缓冲区
		VkBufferImageCopy region{};
		region.bufferOffset = 0;//缓冲区中像素值开始处的字节偏移量
		region.bufferRowLength = 0;//指定像素在内存中的布局方式，图像的行之间可能有一些填充字节，为两者指定 0 表示像素只是紧密排列的，就像我们这里的情况一样
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;//以下指定将像素复制到图像的哪一部分
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,//图像当前布局
			1,
			&region//可以是数组以在一个操作中将来自此缓冲区的许多不同复制执行到图像
		);

		endSingleTimeCommands(commandBuffer);
	}
	/*void createTextureImageView() {//类比交换链的图像视图创建
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = textureImage;//不同
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;//不同
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		if (vkCreateImageView(device, &viewInfo, nullptr, &textureImageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture image view!");
		}
	}*/
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {//单独的创建图像视图逻辑，用于交换链和纹理和深度缓冲的图像视图创建
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;//imageview访问image的哪个区域，即访问哪个方面，所以变量含义即imageview的作用
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image view!");
		}

		return imageView;
	}
	void createTextureImageView() {
		textureImageView.resize(ITEM_COUNT);
		for (size_t i = 0; i < ITEM_COUNT; i++) {
			textureImageView[i] = createImageView(textureImage[i], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}
	void createTextureSampler() {//创建采样器
		//采样器是一个对象，定义了纹理采样时的过滤和寻址模式。它允许我们在着色器中以一致的方式访问纹理，无论纹理的实际尺寸和格式如何
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		//两个过滤器，指定如何插值放大或缩小的纹素可选最近或线性过滤器，最近过滤器选择最接近的纹素，线性过滤器选择周围4个纹素的加权平均值。此处为线性过滤
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;//按轴指定寻址模式，指定纹理坐标超出范围时的行为。REPEAT表示重复纹理，其他有镜像重复，纯色等
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT; //纹理的空间坐标是UVW
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;//启用各向异性过滤，提供更好的纹理质量，解决锐角和远处模糊问题
		VkPhysicalDeviceProperties properties{};//查询设备属性以获取最大各向异性过滤级别，越高越好
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;//当使用边界寻址模式时，指定边界颜色，此处不使用边界寻址模式，但仍需指定一个值
		samplerInfo.unnormalizedCoordinates = VK_FALSE;//使用哪个坐标系寻址纹理，FALSE表示使用归一化坐标（0到1之间），TRUE表示使用非标准化纹理坐标（直接使用像素坐标）
		samplerInfo.compareEnable = VK_FALSE;//比较操作，通常用于阴影贴图
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;//mipmap
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;
		if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture sampler!");
		}
	}

	//深度缓冲相关
	void createDepthResources() {
		VkFormat depthFormat = findDepthFormat();
		createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
		depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);


	}
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {//查找深度图像最适合的格式
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}
	VkFormat findDepthFormat() {//选择一个包含深度分量并且支持用作深度附件的格式
		return findSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}
	bool hasStencilComponent(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	//模型加载相关
	void loadModel() {
		//OBJ 文件由位置、法线、纹理坐标和面组成。面由任意数量的顶点组成，其中每个顶点通过索引引用位置、法线和 / 或纹理坐标
		for (uint32_t i = 0; i < ITEM_COUNT; i++) {
			tinyobj::attrib_t attrib;//该容器拥有位置、法线和纹理坐标的原始信息
			std::vector<tinyobj::shape_t> shapes;//每个shape_t代表模型的一个子部分，通过mesh成员存储其网格，存储形式是，mesh有一个索引数组，指向attrib里与此部分有关的信息
			std::vector<tinyobj::material_t> materials;
			std::string warn, err;
			if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATHS[i].c_str(), nullptr, true)) {
				throw std::runtime_error(warn + err);
			}
			modelInfos[i].vertexOffset = static_cast<uint32_t>(vertices.size());
			modelInfos[i].firstIndex = static_cast<uint32_t>(indices.size());
			std::unordered_map<Vertex, uint32_t> uniqueVertices{};
			for (const auto& shape : shapes) {//遍历来把所有面组合成一个模型
				for (const auto& index : shape.mesh.indices) {
					Vertex vertex{};
					vertex.pos = { //attrib.vertices是所有顶点坐标值组成的一个一维数组，即[x1,y1,z1,x2,y2,...]所以得到xi的i时，要乘3来寻址
						attrib.vertices[3 * index.vertex_index + 0]-1.75+i*2.5,
						attrib.vertices[3 * index.vertex_index + 1],
						attrib.vertices[3 * index.vertex_index + 2]
					};

					vertex.texCoord = {//同理，[u1,v1,u2,v2,u3...]的寻址
						attrib.texcoords[2 * index.texcoord_index + 0],
						1.0f - attrib.texcoords[2 * index.texcoord_index + 1]//vulkan读图y轴以顶部为0，obj格式以底部为0，需翻转
					};

					vertex.color = { 1.0f, 1.0f, 1.0f };

					//vertices.push_back(vertex);
					//indices.push_back(indices.size());//当前顶点下标是已记录顶点数，所以这样获得下标//每个三角形重复使用的顶点会出现多次
					if (uniqueVertices.count(vertex) == 0) {//只有未出现过的才记录，这样索引缓冲区才有用
						uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
						vertices.push_back(vertex);
					}
					indices.push_back(uniqueVertices[vertex]);
				}
			}
			modelInfos[i].indexCount = indices.size() - modelInfos[i].firstIndex; // (当前总索引数 - 开始前的索引数 = 该模型的索引数)
		}
	}
	//键盘input进行旋转逻辑相关
	void controllInit() {//初始化位置数据
		rotateAngle.resize(ITEM_COUNT);
		scale.resize(ITEM_COUNT);
		trans_z.resize(ITEM_COUNT);
		for (uint32_t i = 0; i < ITEM_COUNT; i++) {
			rotateAngle[i] = 0.0f;
			scale[i] = 1.0f;
			trans_z[i] = 0.0f;
		}
	}
	void processInput(GLFWwindow* window) {
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float newTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();//自渲染开始以来以秒为单位的时间（具有float精度）。
		float deltaTime = newTime - lastTime;
		lastTime = newTime;//以上算出帧时间间隔，用于控制实际旋转速度与真实时间关系一致
		float rotationSpeed = 0.5f;//速度
		float scaleSpeed = 0.5f;
		float transSpeed = 0.5f;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {//切换第一个物体
			modelChosen = 0;
		}
		if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {//切换第二个物体
			modelChosen = 1;
		}
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
			rotateAngle[modelChosen] += rotationSpeed * deltaTime; // 向左（逆时针）旋转
		}
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
			rotateAngle[modelChosen] -= rotationSpeed * deltaTime; // 向右（顺时针）旋转
		}
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {//放大正方形
			scale[modelChosen] += scaleSpeed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {//缩小
			scale[modelChosen] -= scaleSpeed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {//z向前（深度变浅）
			trans_z[modelChosen] += transSpeed * deltaTime;
		}
		if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {//z向后（变深）
			trans_z[modelChosen] -= transSpeed * deltaTime;
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

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };//管线阶段数组，记录有哪几个管线阶段，此处为顶点和片段阶段

		//固定功能，即图形管线的大部分阶段，其中大部分被烘焙到不可变的管线状态对象，其他用动态状态设定为可变
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};//传递给顶点着色器的顶点数据的格式
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		auto bindingDescription = Vertex::getBindingDescription();//根据顶点的描述接受数据
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};//输入汇编，它描述了图元如何从顶点数据中被组装出来，以及是否启用重启图元功能
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;//将从顶点绘制的几何图形类型
		inputAssembly.primitiveRestartEnable = VK_FALSE;//不启动，则顶点按顺序从顶点缓冲区按索引加载；否则自己指定索引进行复用等优化

		std::vector<VkDynamicState> dynamicStates = {//动态状态，不重新创建管线就能改变的状态。忽略值的配置，需绘制时指定
			VK_DYNAMIC_STATE_VIEWPORT,//视口和裁剪矩形
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		VkPipelineViewportStateCreateInfo viewportState{};////视口和裁剪矩形的管线创建信息。因为指定为了动态状态，所以只需指定数量，具体值在绘制时指定
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;//只需指定数量，具体值在绘制时指定
		viewportState.scissorCount = 1;
		//当不指定这两个为动态，需创建viewport和scissor对象（指定大小等值），创建数组（因为可以有多个），并将上结构体的pViewports，pScissors成员指向他们

		//图元（Primitive） 指的是由一个或多个顶点（Vertex）按照特定规则组合而成的基本几何形状，它是图形渲染管线中，顶点处理之后、光栅化之前的基本处理单元。前面已topology定义顶点如何构成图元
		VkPipelineRasterizationStateCreateInfo rasterizer{};//光栅化器
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;//超出摄像机深度的近平面和远平面的片段是否将被钳制到它们，此处丢弃超出部分。
		rasterizer.rasterizerDiscardEnable = VK_FALSE;//是否丢弃所有图元，直接跳过光栅化阶段
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;//确定如何为几何图形生成片段，此处用片段填充多边形的区域
		rasterizer.lineWidth = 1.0f;//以片段数量描述线条的粗细
		rasterizer.cullMode = VK_CULL_MODE_NONE;//面剔除类型。可以禁用剔除，剔除正面、剔除背面或两者都剔除。此处剔除背面
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;//指定被认为是正面的面的顶点顺序，可以是顺时针或逆时针。

		VkPipelineMultisampleStateCreateInfo multisampling{};//多重采样，这是执行抗锯齿的方法之一
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional
		//颜色混合，片段着色器返回颜色后，需要将其与帧缓冲区中已有的颜色组合//两个结构体，配置颜色混合方式
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};//包含每个附加帧缓冲区的配置，我们只有一个缓冲区
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;//来自片段着色器的新颜色将直接传递，不作修改，否则会与帧缓冲区中已有的颜色进行混合，混合方式由以下四个成员指定
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

		VkPipelineColorBlendStateCreateInfo colorBlending{};//全局设置
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;//引用所有帧缓冲区的结构体数组，只有一个所以直接引用对象
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		//管线布局
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};//用于创建管线布局的结构体，描述了管线使用的资源类型和数量，如uniform变量、push常量等，此处不使用任何资源，所以成员值为0或nullptr
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;//指定描述符集布局以告知Vulkan 着色器将使用哪些描述符 
		pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}
		//启用深度
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;//是否应将新片段的深度与深度缓冲区进行比较，以查看是否应丢弃它们
		depthStencil.depthWriteEnable = VK_TRUE;//是否应将通过深度测试的片段的新深度实际写入深度缓冲区
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;//保留或丢弃的标准，因为较低深度 = 更近，所以新片段的深度应更小（less）
		depthStencil.depthBoundsTestEnable = VK_FALSE;//此三项，限制只保留某深度范围内片段，不启用
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 1.0f; // Optional
		depthStencil.stencilTestEnable = VK_FALSE;//此三项，模版缓冲区操作，不启用
		depthStencil.front = {}; // Optional
		depthStencil.back = {}; // Optional

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;//顶点和片段
		pipelineInfo.pStages = shaderStages;//引用管线阶段数组
		pipelineInfo.pVertexInputState = &vertexInputInfo;//固定功能阶段所有信息结构体
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = pipelineLayout;//布局
		pipelineInfo.renderPass = renderPass;//引用渲染过程
		pipelineInfo.subpass = 0;//用此管线的子通道的索引
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional 管线派生功能，用现有管线作为基础
		pipelineInfo.basePipelineIndex = -1; // Optional
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}

		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}
	void createRenderPass() {//创建渲染过程对象   ！！渲染过程即渲染通道。
		//渲染过程，是管线创建需要的，描述一次渲染操作的整体流程和规则，包含 渲染时使用的帧缓冲附件 有多少颜色和深度缓冲区 每个缓冲区使用多少个采样 它们的内容在整个渲染操作中应该如何处理，所有这些信息都封装在一个渲染过程对象中
		VkAttachmentDescription colorAttachment{};//描述帧缓冲区附件的结构体，此为颜色附件
		colorAttachment.format = swapChainImageFormat;//使用交换链图像的格式
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;//每个像素使用多少个采样，1表示不使用多重采样
		//决定在渲染之前和渲染之后如何处理附件中的数据
		//应用于颜色和深度附件数据
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//此处为在开始时将值清除为常量
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;//此处为渲染的内容将存储在内存中，并且可以稍后读取。因为这里需要将图像展示到屏幕上，所以需要存储渲染结果
		//应用于模板数据
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;//此应用不会对模板缓冲区执行任何操作，因此加载和存储的结果无关紧要。
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		//渲染前后使用的布局
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//图像在渲染过程开始之前将具有的布局，此处不在乎图像之前的布局是什么
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//渲染过程完成时自动转换到的布局，此处在交换链中呈现的图像，因为希望图像在渲染后可以使用交换链进行呈现
		VkAttachmentReference colorAttachmentRef{};//颜色附件引用
		colorAttachmentRef.attachment = 0;//颜色附件下标为0
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//子过程中的布局，此处提供最佳性能

		VkAttachmentDescription depthAttachment{};//深度附件描述，类似颜色附件
		depthAttachment.format = findDepthFormat();//即深度图像使用的格式
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//不关心之前的布局
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		VkAttachmentReference depthAttachmentRef{};//深度附件引用
		depthAttachmentRef.attachment = 1;//深度附件的下标为1
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};//子过程/子通道 单个渲染过程可以由多个子过程组成以提升性能，此处使用单个子过程
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;//说明这是一个图形子过程，其他还有计算子过程
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;//引用颜色附件
		subpass.pDepthStencilAttachment = &depthAttachmentRef;//引用深度附件
		VkSubpassDependency dependency{};//子通道依赖项，描述子通道之间的依赖关系，以及子通道与外部操作之间的依赖关系//子通道会自动处理图像布局转换。这些转换由子通道依赖关系控制
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;//渲染通道之前或之后的隐式子通道
		dependency.dstSubpass = 0;//索引，我们现在惟一的子通道
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;//要等待的操作以及这些操作发生的阶段
		dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;//应该等待此操作的操作位于颜色附件阶段
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo{};//创建 渲染过程
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;//依赖项数组
		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {//2nd参数引用一个可选的 VkPipelineCache
			throw std::runtime_error("failed to create render pass!");
		}
	}
	void createFramebuffers() {//创建帧缓冲
		//帧缓冲将实际访问图像的规则图像视图和描述了附件规则的渲染过程绑定.//具体是怎么绑定的？是把交换链每个图的图像视图作为渲染过程的附件。//每个交换链里的图像都有一个帧缓冲
		swapChainFramebuffers.resize(swapChainImageViews.size());
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			std::array<VkImageView, 2> attachments = {//取出来所有图像视图，作为渲染过程的附件
				swapChainImageViews[i],
				depthImageView
			};
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;//图像数组中的层数。我们的交换链图像是单个图像，因此层数是 1

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}
	void createCommandPool() {//创建命令池
		//绘制和内存传输这样的操作命令，一起提交，记录在命令缓冲区对象中
		//命令缓冲区分配自命令池，命令池管理和分配内存用于一个或多个命令缓冲区，并且与特定的队列族相关联
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;//此处 允许单独重新记录命令缓冲区，如果没有此标志，则必须一起重置所有命令缓冲区。我们希望在每一帧都记录一个命令缓冲区，因此我们希望能够重置并重新记录它
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();//命令缓冲区提交到设备队列进行执行。每个命令池只能分配在单一类型的队列上提交的命令缓冲区。此处记录用于绘图的命令所以填图形队列族。
		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}
	}

	void createCommandBuffer() {//创建命令缓冲区
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;//指定要分配的命令池
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;//指定是主命令缓冲区还是辅助命令缓冲区。此处为主~，可以提交到队列以执行，但不能从其他命令缓冲区调用。
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();//指定缓冲区数量

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {//命令缓冲区记录，将我们要执行的命令写入命令缓冲区
		VkCommandBufferBeginInfo beginInfo{};//开始记录命令缓冲区的结构体，指定我们将如何使用命令缓冲区以及一些优化提示
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional 指定我们将如何使用命令缓冲区。可选项对我们都不适用，填0
		beginInfo.pInheritanceInfo = nullptr; // Optional 仅与辅助命令缓冲区相关。它指定从调用主命令缓冲区继承哪个状态

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}
		//启动渲染通道，绘制通过开始渲染通道来开始
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;//渲染通道本身
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];//imageindex是交换链图像索引，因为每个交换链图像都有一个帧缓冲，所以用索引找到对应的帧缓冲
		renderPassInfo.renderArea.offset = { 0, 0 };//定义渲染区域，即将渲染的像素范围。此处为整个交换链图像大小，可获得最佳性能
		renderPassInfo.renderArea.extent = swapChainExtent;
		std::array<VkClearValue, 2> clearValues{};//指定清除值
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };//将清除颜色定义为简单的 100% 不透明度的黑色
		clearValues[1].depthStencil = { 1.0f, 0 }; //深度0-1.0，此处清除为最远的可能深度，即 1.0

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);//启动//3rd参数：渲染通道命令将嵌入到主命令缓冲区本身中，并且不会执行辅助命令缓冲区
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);//绑定管线 //2nd参数指定管线类型，此处为图形管线

		VkBuffer vertexBuffers[] = { vertexBuffer };//顶点缓冲区绑定到点，可以有多个
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);//绑定索引缓冲区，区别是只能有一个

		//指定为动态状态的，在此处发出绘制命令之前，指定具体值。这些值设置到命令缓冲区。
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		updateUniformBuffer(currentFrame, 0);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame * ITEM_COUNT], 0, nullptr);
		vkCmdDrawIndexed(commandBuffer, modelInfos[0].indexCount, 1, modelInfos[0].firstIndex, 0, 0);



		updateUniformBuffer(currentFrame, 1);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame * ITEM_COUNT + 1], 0, nullptr);
		vkCmdDrawIndexed(commandBuffer, modelInfos[1].indexCount, 1, modelInfos[1].firstIndex, 0, 0);


		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	//绘制相关
	void createSyncObjects() {//创建同步对象
		//vulkan默认异步；交换链和帧操作需要同步，因为有执行顺序。信号量和栅栏是两种不同的同步对象，信号量用于GPU的同步，而栅栏用于GPU和CPU的同步，分别用于交换链和帧操作
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;//为了第一帧不等待完成信号直接开始，将栅栏设置为“已发出信号”

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {

				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
		}
	}
	void drawFrame() {
		/*渲染帧的步骤
		 等待前一帧完成

		从交换链获取图像

		记录一个命令缓冲区，该缓冲区将场景绘制到该图像上

		提交已记录的命令缓冲区

		呈现交换链图像

		可以看出他们是有顺序的
		*/
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);//等待前一帧完成//3rd参数，等待所有栅栏返回；4th参数，超时时间，此处禁用超时

		uint32_t imageIndex;
		//呈现前交换链失效时（窗口大小变化），重新创建交换链
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);//得到交换链是否不再足够的的信息
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {//交换链已与表面不兼容，无法再用于渲染。通常在窗口调整大小后发生。
			framebufferResized = false;
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {//正常呈现 或 交换链仍然可以成功呈现到表面，但表面属性不再完全匹配：则不做处理。不在这三种中，报错。
			throw std::runtime_error("failed to acquire swap chain image!");
		}



		vkResetFences(device, 1, &inFlightFences[currentFrame]);//重置栅栏，为下一帧做准备//以上重建交换链是没有提交呈现的，所以不重置栅栏，否则因没有提交工作进行执行，重置后的栅栏永远不会被触发，导致永远锁死。因此需要在最后重置栅栏。确保在重建的return后。

		//从交换链获取图像
		vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);//禁用超时，选择完成后发出的信号量，已变为可用的交换链图像索引

		vkResetCommandBuffer(commandBuffers[currentFrame], 0);//初始化命令缓冲区
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex);//记录命令缓冲区
		//提交命令缓冲区
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };//等待哪些信号量
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };//在管道哪个阶段等待，此处为颜色附件输出阶段，因为我们需要在这个阶段之前等待图像可用
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];//实际提交执行的命令缓冲区
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };//执行完成发出的信号量
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {//提交。last参数为执行完成触发的栅栏
			throw std::runtime_error("failed to submit draw command buffer!");
		}
		//呈现
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;//等待缓冲区命令完成执行，即绘制三角形完成，再呈现
		VkSwapchainKHR swapChains[] = { swapChain };//指定要向其显示图像的交换链以及每个交换链的图像索引
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // Optional 指定一个 VkResult 值数组，以检查每个单独的交换链演示是否成功。如果您只使用单个交换链，则没有必要
		result = vkQueuePresentKHR(presentQueue, &presentInfo);//提交将图像呈现给交换链的请求


		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {//呈现后交换链是否失效
			framebufferResized = false;
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;//前进到下一帧
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