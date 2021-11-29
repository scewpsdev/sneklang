import Console;
import Glfw;
import Window;
import Time;


const int WIDTH = 1280;
const int HEIGHT = 720;
const string TITLE = "abc";


void updateApp(float dt)
{
}

void renderApp()
{
}

void main()
{
	Window window = new Window(WIDTH, HEIGHT, TITLE, true);

	long lastUpdate = getNanos();
	long lastSecond = getNanos();
	int frames = 0;

	while window.isOpen()
	{
		long now = getNanos();
		long delta = now - lastUpdate;
		lastUpdate = now;

		if (now - lastSecond >= 1000000000)
		{
			printf("%d FPS\n".buffer, frames);
			lastSecond = now;
			frames = 0;
		}

		window.update();

		updateApp(delta / 1000000000.0);
		renderApp();

		frames++;
	}

	free window;
}
