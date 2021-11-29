import Glfw;
import Console;


class Window {
	GLFWwindow* handle;

	this(int width, int height, string title, bool visible)
	{
		glfwInit();

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
		glfwWindowHint(GLFW_VISIBLE, (int)visible);

		GLFWwindow* window = glfwCreateWindow(width, height, title.buffer, null, null);
		glfwMakeContextCurrent(window);
		glfwSwapInterval(0);

		this.handle = window;
	}

	void update()
	{
		glfwSwapBuffers(this.handle);
		glfwPollEvents();
	}

	bool isOpen()
	{
		return glfwWindowShouldClose(this.handle) == GLFW_FALSE;
	}
}