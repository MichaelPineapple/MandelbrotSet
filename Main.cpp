#include <GLFW/glfw3.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <complex>
#include <mutex>

using std::cout;
using std::endl;
using std::cin;
using std::thread;
using std::vector;
using std::complex;
using std::mutex;

/*** ~ STRUCTS ~ ***/

/* Contains Red, Green, and Blue, color values.
Intended to represent the color of a pixel within the window */
struct MclPixel
{
	float colour[3] = { 0, 0, 0 };
	MclPixel(float _r, float _g, float _b)
	{
		colour[0] = _r;
		colour[1] = _g;
		colour[2] = _b;
	}
	MclPixel() {}
};

/* Contains two double values which are intended to
represent x and y positions on the complex number plane */
struct MclPoint
{
	double x = 0.0, y = 0.0;
	MclPoint(double _x, double _y)
	{
		x = _x;
		y = _y;
	}
	MclPoint(){}
};

/*** ~ GLOBAL CONSTANTS ~ ***/

// Size of the window
#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 600

// When the mandlebrot function reaches this many iterations, it is considered stable (does not go to infinity)
#define MAX_ITERATIONS 500 

// Scale of the cursor zoom box
#define CURSOR_BOX_SCALE 0.01

// Width and Height of the cursor zoom box. Dependant on the window size and scale of the cursor box
const double CURSOR_BOX_WIDTH = WINDOW_WIDTH * CURSOR_BOX_SCALE, CURSOR_BOX_HEIGHT = WINDOW_HEIGHT * CURSOR_BOX_SCALE;

// color of the cursor box
const GLfloat colour[] = { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0 };

/*** ~ GLOBAL VARIABLES ~ ***/

// mouse position
double mouse[2];

// number of threads to compute mandlebrot set
int threadCount = 1;

// list of threads to compute mandlebrot set
vector<thread> threadList;

// condition variable to signal mandlebrot computation should stop
bool running = true;

// current mandlebrot zoom values
double curL, curR, curT, curB;

// 2D array of pixels which fill the window
MclPixel pixels[WINDOW_WIDTH][WINDOW_HEIGHT];

/*** ~ FUNCTIONS ~ ***/

/* compute the mandlebrot set given zoom values and "slice" of the screen.
Store pixel data to a global array. If 'running = false' computation will stop */
void compute_mandelbrot(double _left, double _right, double _top, double _bottom, int _startY, int _endY)
{
	for (int y = _startY; y < _endY; ++y)
	{
		if (!running) return;

		for (int x = 0; x < WINDOW_WIDTH; ++x)
		{
			if (!running) return;

			complex<double> c(_left + (x * (_right - _left) / WINDOW_WIDTH), _top + (y * (_bottom - _top) / WINDOW_HEIGHT));
			complex<double> z(0.0, 0.0);

			int iterations = 0;
			while (abs(z) < 2.0 && iterations < MAX_ITERATIONS)
			{
				z = (z * z) + c;
				++iterations;
			}

			if (iterations < MAX_ITERATIONS)
			{
				float q = ((float)(iterations)) / MAX_ITERATIONS;
				pixels[x][y] = MclPixel(q, 0, q);
			}
			else pixels[x][y] = MclPixel(1.0f, 1.0f, 1.0f);
		}
	}
}

/* Clears all pixels in the window */
void clearPixels()
{
	for (int x = 0; x < WINDOW_WIDTH; x++)
	{
		for (int y = 0; y < WINDOW_HEIGHT; y++) pixels[x][y] = MclPixel();
	}
}

/* stop all mandlebrot computation threads gracefully */
void stopThreads()
{
	running = false;
	for (int i = 0; i < threadList.size(); i++) threadList[i].join();
	threadList.clear();
	clearPixels();
	running = true;
}

/* start multi-threaded mandlebrot computation based on the given zoom values */
void startThreads(double _left, double _right, double _top, double _bottom)
{
	stopThreads();

	curL = _left;
	curR = _right;
	curT = _top;
	curB = _bottom;

	int slice = WINDOW_HEIGHT / threadCount;
	for (int i = 0; i < threadCount; i++)
	{
		int startY = i * slice;
		threadList.push_back(thread(compute_mandelbrot, _left, _right, _top, _bottom, startY, startY + slice));
	}
}

/* reset the view to default zoom */
void resetView()
{
	startThreads(-2.0, 1.0, 1.125, -1.125);
}

/* Converts a position on the screen into a position on the complex number plane */
MclPoint getValueOfPixel(double _left, double _right, double _top, double _bottom, int _x, int _y)
{
	return MclPoint(_left + (_x * (_right - _left) / WINDOW_WIDTH), _top + (_y * (_bottom - _top) / WINDOW_HEIGHT));
}

/*  Called when the cursor is moved. Stores the cursor position into global variables. */
static void cursorPositionCallback(GLFWwindow* _window, double _xpos, double _ypos)
{
	mouse[0] = _xpos;
	mouse[1] = _ypos;
}

/* Called when the mouse is clicked. If left click, zoom in. If right click, zoom out to the default view */
static void mouseClickCallback(GLFWwindow* _window, int _butt, int _action, int _mods)
{
	if (_butt == GLFW_MOUSE_BUTTON_LEFT && _action == GLFW_RELEASE)
	{
		double zoomBoxSize = 10;
		MclPoint TopLeft = getValueOfPixel(curL, curR, curT, curB, mouse[0] - CURSOR_BOX_WIDTH, mouse[1] - CURSOR_BOX_HEIGHT);
		MclPoint BottomRight = getValueOfPixel(curL, curR, curT, curB, mouse[0] + CURSOR_BOX_WIDTH, mouse[1] + CURSOR_BOX_HEIGHT);
		startThreads(TopLeft.x, BottomRight.x, TopLeft.y, BottomRight.y);
	}
	else if (_butt == GLFW_MOUSE_BUTTON_RIGHT && _action == GLFW_RELEASE) resetView();
}

/* main rendering thread function. Handles GLFW setup calls and main rendering loop */
void render()
{
	// GLFW setup
	GLFWwindow* window;
	if (!glfwInit()) return;
	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Mandelbrot", NULL, NULL);
	glfwSetCursorPosCallback(window, cursorPositionCallback);
	glfwSetMouseButtonCallback(window, mouseClickCallback);
	if (!window) { glfwTerminate(); return; }
	glfwMakeContextCurrent(window);
	glViewport(0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Main render loop
	while (!glfwWindowShouldClose(window))
	{
		glClear(GL_COLOR_BUFFER_BIT);

		// Render pixels (Mandlebrot)
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glPointSize(1);
		for (int x = 0; x < WINDOW_WIDTH; x++)
		{
			for (int y = 0; y < WINDOW_HEIGHT; y++)
			{
				int point[2] = { x, y };
				glVertexPointer(2, GL_INT, 0, point);
				glColorPointer(3, GL_FLOAT, 0, pixels[x][y].colour);
				glDrawArrays(GL_POINTS, 0, 1);
			}
		}
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);

		// Render the cursor box
		GLdouble cursorBox[8] =
		{
			mouse[0] - CURSOR_BOX_WIDTH, mouse[1] - CURSOR_BOX_HEIGHT,
			mouse[0] + CURSOR_BOX_WIDTH, mouse[1] - CURSOR_BOX_HEIGHT,
			mouse[0] + CURSOR_BOX_WIDTH, mouse[1] + CURSOR_BOX_HEIGHT,
			mouse[0] - CURSOR_BOX_WIDTH, mouse[1] + CURSOR_BOX_HEIGHT,
		};
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glVertexPointer(2, GL_DOUBLE, 0, cursorBox);
		glColorPointer(3, GL_FLOAT, 0, colour);
		glDrawArrays(GL_LINE_LOOP, 0, 4);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	stopThreads();

	glfwTerminate();
}

/*** ~ MAIN FUNCTION ~ ***/
int main()
{
	// get # of threads from the user
	cout << "# of threads: ";
	cin >> threadCount;
	if (threadCount < 1 || threadCount > 100)
	{
		cout << "Haha very funny..." << endl;
		return 0;
	}
	cout << "Please wait..." << endl;

	// start rendering thread
	thread renderingThread(render);

	// start mandlebrot computation threads
	resetView();

	// join render func with main thread
	renderingThread.join();

	cout << "Done." << endl;

	return 0;
}
