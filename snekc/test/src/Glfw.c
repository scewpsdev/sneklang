
/*
Initialization, version and error
*/

const uint GLFW_TRUE = 1;
const uint GLFW_FALSE = 0;
const uint GLFW_JOYSTICK_HAT_BUTTONS = 0x00050001;
const uint GLFW_COCOA_CHDIR_RESOURCES = 0x00051001;
const uint GLFW_COCOA_MENUBAR = 0x00051002;

typedef GLFWerrorfun : void(int, char*);

int glfwInit();
void glfwTerminate();
void glfwInitHint(int hint, int value);
void glfwGetVersion(int* major, int* minor, int* rev);
char* glfwGetVersionString();
int glfwGetError(char** description);
GLFWerrorfun glfwSetErrorCallback(GLFWerrorfun callback);


/*
Monitor
*/

struct GLFWmonitor;
typedef GLFWmonitorfun : void(GLFWmonitor*, int);
struct GLFWvidmode;
struct GLFWgammaramp;

GLFWmonitor** glfwGetMonitors(int* count);
GLFWmonitor* glfwGetPrimaryMonitor();
void glfwGetMonitorPos(GLFWmonitor* monitor, int* xpos, int* ypos);
void glfwGetMonitorWorkarea(GLFWmonitor* monitor, int* xpos, int* ypos, int* width, int* height);
void glfwGetMonitorPhysicalSize(GLFWmonitor* monitor, int* widthMM, int* heightMM);
void glfwGetMonitorContentScale(GLFWmonitor* monitor, float* xscale, float* yscale);
char* glfwGetMonitorName(GLFWmonitor* monitor);
void glfwSetMonitorUserPointer(GLFWmonitor* monitor, void* pointer);
void* glfwGetMonitorUserPointer(GLFWmonitor* monitor);
GLFWmonitorfun glfwSetMonitorCallback(GLFWmonitor* monitor, GLFWmonitorfun callback);
GLFWvidmode* glfwGetVideoModes(GLFWmonitor* monitor, int* count);
GLFWvidmode* glfwGetVideoMode(GLFWmonitor* monitor);
void glfwSetGamma(GLFWmonitor* monitor, float gamma);
GLFWgammaramp* glfwGetGammaRamp(GLFWmonitor* monitor);
void glfwSetGammaRamp(GLFWmonitor* monitor, GLFWgammaramp* ramp);


/*
Window
*/

const uint GLFW_FOCUSED = 0x00020001;
const uint GLFW_ICONIFIED = 0x00020002;
const uint GLFW_RESIZABLE = 0x00020003;
const uint GLFW_VISIBLE = 0x00020004;
const uint GLFW_DECORATED = 0x00020005;
const uint GLFW_AUTO_ICONIFY = 0x00020006;
const uint GLFW_FLOATING = 0x00020007;
const uint GLFW_MAXIMIZED = 0x00020008;
const uint GLFW_CENTER_CURSOR = 0x00020009;
const uint GLFW_TRANSPARENT_FRAMEBUFFER = 0x0002000A;
const uint GLFW_HOVERED = 0x0002000B;
const uint GLFW_FOCUS_ON_SHOW = 0x0002000C;
const uint GLFW_RED_BITS = 0x00021001;
const uint GLFW_GREEN_BITS = 0x00021002;
const uint GLFW_BLUE_BITS = 0x00021003;
const uint GLFW_ALPHA_BITS = 0x00021004;
const uint GLFW_DEPTH_BITS = 0x00021005;
const uint GLFW_STENCIL_BITS = 0x00021006;
const uint GLFW_ACCUM_RED_BITS = 0x00021007;
const uint GLFW_ACCUM_GREEN_BITS = 0x00021008;
const uint GLFW_ACCUM_BLUE_BITS = 0x00021009;
const uint GLFW_ACCUM_ALPHA_BITS = 0x0002100A;
const uint GLFW_AUX_BUFFERS = 0x0002100B;
const uint GLFW_STEREO = 0x0002100C;
const uint GLFW_SAMPLES = 0x0002100D;
const uint GLFW_SRGB_CAPABLE = 0x0002100E;
const uint GLFW_REFRESH_RATE = 0x0002100F;
const uint GLFW_DOUBLEBUFFER = 0x00021010;
const uint GLFW_CLIENT_API = 0x00022001;
const uint GLFW_CONTEXT_VERSION_MAJOR = 0x00022002;
const uint GLFW_CONTEXT_VERSION_MINOR = 0x00022003;
const uint GLFW_CONTEXT_REVISION = 0x00022004;
const uint GLFW_CONTEXT_ROBUSTNESS = 0x00022005;
const uint GLFW_OPENGL_FORWARD_COMPAT = 0x00022006;
const uint GLFW_OPENGL_DEBUG_CONTEXT = 0x00022007;
const uint GLFW_OPENGL_PROFILE = 0x00022008;
const uint GLFW_CONTEXT_RELEASE_BEHAVIOR = 0x00022009;
const uint GLFW_CONTEXT_NO_ERROR = 0x0002200A;
const uint GLFW_CONTEXT_CREATION_API = 0x0002200B;
const uint GLFW_SCALE_TO_MONITOR = 0x0002200C;
const uint GLFW_COCOA_RETINA_FRAMEBUFFER = 0x00023001;
const uint GLFW_COCOA_FRAME_NAME = 0x00023002;
const uint GLFW_COCOA_GRAPHICS_SWITCHING = 0x00023003;
const uint GLFW_X11_CLASS_NAME = 0x00024001;
const uint GLFW_X11_INSTANCE_NAME = 0x00024002;

struct GLFWwindow;
typedef GLFWwindowposfun : void(GLFWwindow*, int, int);
typedef GLFWwindowsizefun : void(GLFWwindow*, int, int);
typedef GLFWwindowclosefun : void(GLFWwindow*);
typedef GLFWwindowrefreshfun : void(GLFWwindow*);
typedef GLFWwindowfocusfun : void(GLFWwindow*, int);
typedef GLFWwindowiconifyfun : void(GLFWwindow*, int);
typedef GLFWwindowmaximizefun : void(GLFWwindow*, int);
typedef GLFWframebuffersizefun : void(GLFWwindow*, int, int);
typedef GLFWwindowcontentscalefun : void(GLFWwindow*, float float);
struct GLFWimage;

void glfwDefaultWindowHints();
void glfwWindowHint(int hint, int value);
void glfwWindowHintString(int hint, char* value);
GLFWwindow* glfwCreateWindow(int with, int height, char* title, GLFWmonitor* monitor, GLFWwindow* share);
void glfwDestroyWindow(GLFWwindow* window);
int glfwWindowShouldClose(GLFWwindow* window);
void glfwSetWindowShouldClose(GLFWwindow* window, int value);
void glfwSetWindowTitle(GLFWwindow* window, char* title);
void glfwSetWindowIcon(GLFWwindow* window, int count, GLFWimage* images);
void glfwGetWindowPos(GLFWwindow* window, int* xpos, int* ypos);
void glfwSetWindowPos(GLFWwindow* window, int xpos, int ypos);
void glfwGetWindowSize(GLFWwindow* window, int* width, int* height);
void glfwSetWindowSizeLimits(GLFWwindow* window, int minwidth, int minheight, int maxwidth, int maxheight);
void glfwSetWindowAspectRatio(GLFWwindow* window, int numer, int denom);
void glfwSetWindowSize(GLFWwindow* window, int width, int height);
void glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height);
void glfwGetWindowFrameSize(GLFWwindow* window, int* left, int* top, int* right, int* bottom);
void glfwGetWindowContentScale(GLFWwindow* window, float* xscale, float* yscale);
float glfwGetWindowOpacity(GLFWwindow* window);
void glfwSetWindowOpacity(GLFWwindow* window, float opacity);
void glfwIconifyWindow(GLFWwindow* window);
void glfwRestoreWindow(GLFWwindow* window);
void glfwMaximizeWindow(GLFWwindow* window);
void glfwShowWindow(GLFWwindow* window);
void glfwHideWindow(GLFWwindow* window);
void glfwFocusWindow(GLFWwindow* window);
void glfwRequestWindowAttention(GLFWwindow* window);
GLFWmonitor* glfwGetWindowMonitor(GLFWwindow* window);
void glfwSetWindowMonitor(GLFWwindow* window, GLFWmonitor* monitor, int xpos, int ypos, int width, int height, int refreshRate);
int glfwGetWindowAttrib(GLFWwindow* window, int attrib);
void glfwSetWindowAttrib(GLFWwindow* window, int attrib, int value);
void glfwSetWindowUserPointer(GLFWwindow* window, void* pointer);
void* glfwGetWindowUserPointer(GLFWwindow* window);
GLFWwindowposfun glfwSetWindowPosCallback(GLFWwindow* window, GLFWwindowposfun callback);
GLFWwindowsizefun glfwSetWindowSizeCallback(GLFWwindow* window, GLFWwindowsizefun callback);
GLFWwindowclosefun glfwSetWindowCloseCallback(GLFWwindow* window, GLFWwindowclosefun callback);
GLFWwindowrefreshfun glfwSetWindowRefreshCallback(GLFWwindow* window, GLFWwindowrefreshfun callback);
GLFWwindowfocusfun glfwSetWindowFocusCallback(GLFWwindow* window, GLFWwindowfocusfun callback);
GLFWwindowiconifyfun glfwSetWindowIconifyCallback(GLFWwindow* window, GLFWwindowiconifyfun callback);
GLFWwindowmaximizefun glfwSetWindowMaximizeCallback(GLFWwindow* window, GLFWwindowmaximizefun callback);
GLFWframebuffersizefun glfwSetFramebufferSizeCallback(GLFWwindow* window, GLFWframebuffersizefun callback);
GLFWwindowcontentscalefun glfwSetWindowContentScaleCallback(GLFWwindow* window, GLFWwindowcontentscalefun callback);
void glfwPollEvents();
void glfwWaitEvents();
void glfwWaitEventsTimeout(double timeout);
void glfwPostEmptyEvent();
void glfwSwapBuffers(GLFWwindow* window);


/*
Context
*/

typedef GLFWglproc : void();

void glfwMakeContextCurrent(GLFWwindow* window);
GLFWwindow* glfwGetCurrentContext();
void glfwSwapInterval(int interval);
int glfwExtensionSupported(char* extension);
GLFWglproc glfwGetProcAddress(char* procname);
