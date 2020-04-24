#include <GLFW/glfw3.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <complex>
#include <mutex>
#include <atomic>
#include <chrono>
#include <exception>

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::cout;
using std::endl;
using std::cin;
using std::thread;
using std::vector;
using std::complex;
using std::mutex;
using std::unique_lock;
using std::atomic_bool;
using std::atomic_int;
using std::condition_variable;
using std::exception;

// clock for performance measures
typedef std::chrono::steady_clock timer;

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

// maximum number of threads to calculate mandelbrot (30 is overkill)
#define MAX_THREADS 30

// When the mandlebrot function reaches this many iterations, it is considered stable (does not go to infinity)
#define MAX_ITERATIONS 500 

// Scale of the cursor zoom box
#define CURSOR_BOX_SCALE 0.01

// Width and Height of the cursor zoom box. Dependant on the window size and scale of the cursor box
const double CURSOR_BOX_WIDTH = WINDOW_WIDTH * CURSOR_BOX_SCALE, CURSOR_BOX_HEIGHT = WINDOW_HEIGHT * CURSOR_BOX_SCALE;

// color of the cursor box
const float colour[] = { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0 };

/*** ~ GLOBAL VARIABLES ~ ***/

// position values of the box to draw around the cursor, used for zoom functionality also
double cursorBox[8];

// number of threads to compute mandlebrot set
atomic_int threadCount = 1;

// mutex to protect the 'threadCount' variable
mutex threadCountMutex;

// list of threads to compute mandlebrot set
vector<thread> threadList;

// current mandlebrot zoom values
long double left, right, top, bottom;

// 2D array of pixels which fill the window
MclPixel pixels[WINDOW_WIDTH][WINDOW_HEIGHT];

// size of each slice depending on number of threads
int slice = 0;

// mutexes to protect each "slice" (tile)
mutex sliceMutexArray[MAX_THREADS];

// used for pausing the main thread loop (signaling)
mutex m;
condition_variable pauseMandelbrotLoop;

// should the mandelbrot set be recacalculated?
atomic_bool recalculate = true;

// has the GLFW window been closed?
atomic_bool windowClosed = false;

/*** ~ FUNCTIONS ~ ***/

/* compute the mandlebrot set given zoom values and "slice" of the screen.
Store pixel data to a global array. If 'running = false' computation will stop */
void computeMandelbrot(int _threadTileId, long double _left, long double _right, long double _top, long double _bottom, int _startY, int _endY)
{
	try
	{
		for (int y = _startY; y < _endY; ++y)
		{
			if (recalculate) return;

			for (int x = 0; x < WINDOW_WIDTH; ++x)
			{
				if (recalculate) return;

				complex<long double> c(_left + (x * (_right - _left) / WINDOW_WIDTH), _top + (y * (_bottom - _top) / WINDOW_HEIGHT));
				complex<long double> z(0.0, 0.0);

				int iterations = 0;
				while (abs(z) < 2.0 && iterations < MAX_ITERATIONS)
				{
					z = (z * z) + c;
					++iterations;
				}

				float r = 1.0f, g = 1.0f, b = 1.0f;
				if (iterations < MAX_ITERATIONS)
				{
					float q = ((float)(iterations)) / MAX_ITERATIONS;
					r = q;
					g = 0;
					b = q;
				}

				sliceMutexArray[_threadTileId].lock();
				pixels[x][y] = MclPixel(r, g, b);
				sliceMutexArray[_threadTileId].unlock();
			}
		}
	}
	catch (const exception& e) { cout << "ERROR (computeMandelbrot)\n" << e.what() << endl; }
}

/* Converts a position on the screen into a position on the complex number plane */
MclPoint getValueOfPixel(int _x, int _y)
{
	return MclPoint(left + (_x * (right - left) / WINDOW_WIDTH), top + (_y * (bottom - top) / WINDOW_HEIGHT));
}

/* Gets id of the "slice" (tile) which contains the row of pixels _y */
int getTileId(int _y)
{
	return _y / slice;
}

/* Clears all pixels in the window */
void clearPixels()
{
	for (int y = 0; y < WINDOW_HEIGHT; y++)
	{
		for (int x = 0; x < WINDOW_WIDTH; x++) pixels[x][y] = MclPixel();
	}
}

/* signal main thread loop to recalculate mandelbrot */
void signalRecalculation()
{
	try
	{
		recalculate = true;
		pauseMandelbrotLoop.notify_one();
	}
	catch (const exception& e) { cout << "ERROR (signalRecalculation)\n" << e.what() << endl; }
}

/* sets the number of threads to compute mandelbrot */
bool setThreadCount(int _newThreadCount)
{
	bool valid = false;
	try
	{

		if (_newThreadCount < 1)
		{
			cout << "Cannot have less than 1 thread." << endl;
		}
		else if (_newThreadCount > MAX_THREADS)
		{
			cout << "Cannot have more than than " << MAX_THREADS << " threads." << endl;
		}
		else
		{
			threadCountMutex.lock();
			threadCount.store(_newThreadCount);
			slice = WINDOW_HEIGHT / threadCount;
			threadCountMutex.unlock();
			signalRecalculation();
			cout << "Using " << _newThreadCount << " threads." << endl;
			valid = true;
		}
	}
	catch (const exception& e) { cout << "ERROR (setThreadCount)\n" << e.what() << endl; }
	return valid;
}

int getThreadCount()
{
	int $return;
	threadCountMutex.lock();
	$return = threadCount.load();
	threadCountMutex.unlock();
	return $return;
}

/* Sets global zoom values and signals main thread to re-compute mandelbrot */
void setZoom(long double _left, long double _right, long double _top, long double _bottom)
{
	left = _left;
	right = _right;
	top = _top;
	bottom = _bottom;
	signalRecalculation();
}

/* Sets the zoom values to display the whole mandelbrot set */
void resetZoom()
{
	setZoom(-2.0, 1.0, 1.125, -1.125);
}

/*** ~ CALLBACK FUNCTIONS ~ ***/

/*  Called when the cursor is moved. Stores the cursor position into global variables. */
void cursorPositionCallback(GLFWwindow* _window, double _x, double _y)
{
	cursorBox[0] = _x - CURSOR_BOX_WIDTH;
	cursorBox[1] = _y - CURSOR_BOX_HEIGHT;

	cursorBox[2] = _x + CURSOR_BOX_WIDTH;
	cursorBox[3] = _y - CURSOR_BOX_HEIGHT;

	cursorBox[4] = _x + CURSOR_BOX_WIDTH;
	cursorBox[5] = _y + CURSOR_BOX_HEIGHT;

	cursorBox[6] = _x - CURSOR_BOX_WIDTH;
	cursorBox[7] = _y + CURSOR_BOX_HEIGHT;
}

/* Called when the mouse is clicked. If left click, zoom in. If right click, zoom out to the default view */
void mouseClickCallback(GLFWwindow* _window, int _butt, int _action, int _mods)
{
	try
	{
		if (_butt == GLFW_MOUSE_BUTTON_LEFT && _action == GLFW_RELEASE)
		{
			double zoomBoxSize = 10;
			MclPoint TopLeft = getValueOfPixel(cursorBox[0], cursorBox[1]);
			MclPoint BottomRight = getValueOfPixel(cursorBox[4], cursorBox[5]);
			setZoom(TopLeft.x, BottomRight.x, TopLeft.y, BottomRight.y);
		}
		else if (_butt == GLFW_MOUSE_BUTTON_RIGHT && _action == GLFW_RELEASE) resetZoom();
	}
	catch (const exception& e) { cout << "ERROR (mouseClickCallback)\n" << e.what() << endl; }
}

void keypressCallback(GLFWwindow* _window, int _key, int _scancode, int _action, int _mods)
{
	if (_key == GLFW_KEY_UP && _action == GLFW_RELEASE)	setThreadCount(getThreadCount() + 1);
	else if (_key == GLFW_KEY_DOWN && _action == GLFW_RELEASE) setThreadCount(getThreadCount() - 1);
}

/*** ~~~ ***/

/* main rendering thread function. Handles GLFW setup calls and main rendering loop */
void render()
{
	// GLFW setup
	GLFWwindow* window;
	try
	{
		if (!glfwInit()) return;
		window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Mandelbrot", NULL, NULL);
		glfwSetCursorPosCallback(window, cursorPositionCallback);
		glfwSetMouseButtonCallback(window, mouseClickCallback);
		glfwSetKeyCallback(window, keypressCallback);
		if (!window) { glfwTerminate(); return; }
		glfwMakeContextCurrent(window);
		glViewport(0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}
	catch (const exception& e) { cout << "ERROR (GLFW setup)\n" << e.what() << endl; }

	// create list of pixel locations on the screen for GLFW to assign colors
	vector<int> pixelsToRender;
	for (int y = 0; y < WINDOW_HEIGHT; y++)
	{
		for (int x = 0; x < WINDOW_WIDTH; x++)
		{
			pixelsToRender.push_back(x);
			pixelsToRender.push_back(y);
		}
	}
	const int TOTAL_VERTICIES = pixelsToRender.size() / 2;

	// Main render loop
	try
	{
		while (!glfwWindowShouldClose(window))
		{
			glClear(GL_COLOR_BUFFER_BIT);

			// Render pixels (Mandlebrot)
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glPointSize(1);
			glVertexPointer(2, GL_INT, 0, pixelsToRender.data());

			// get color values from global array
			vector<float> pixelColors;
			for (int y = 0; y < WINDOW_HEIGHT; y++)
			{
				int tileID = getTileId(y);
				sliceMutexArray[tileID].lock(); // lock the mutex for each horizontal row of pixels 
				for (int x = 0; x < WINDOW_WIDTH; x++)
				{
					float* tmp = pixels[x][y].colour;
					pixelColors.push_back(tmp[0]);
					pixelColors.push_back(tmp[1]);
					pixelColors.push_back(tmp[2]);

				}
				sliceMutexArray[tileID].unlock();
			}

			glColorPointer(3, GL_FLOAT, 0, pixelColors.data());
			glDrawArrays(GL_POINTS, 0, TOTAL_VERTICIES);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_VERTEX_ARRAY);

			// Render the cursor box
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
	}
	catch (const exception& e) { cout << "ERROR (GLFW rendering loop)\n" << e.what() << endl; }

	windowClosed = true;
	recalculate = true;

	glfwTerminate();
}

/*** ~ MAIN FUNCTION ~ ***/
int main()
{
	try
	{
		// get # of threads from the user, validate input
		{
			int localThreadCount = -1;
			do
			{
				cout << "Enter number of threads: ";
				cin >> localThreadCount;
			} while (!setThreadCount(localThreadCount));
		}

		// display some information
		cout << "Please wait...\n" << endl;
		cout << "Left Click - Zoom in.\nRight Click - Reset zoom.\nUp Key - Increase threads by one.\nDown Key - Decrease threads by one." << endl;
		cout << "\n\nPERFORMANCE MEASUREMENTS:" << endl;

		// set zoom values to default
		resetZoom();

		// start rendering thread
		thread renderingThread(render);

		// mandelbrot calculation loop
		try
		{
			while (!windowClosed)
			{
				clearPixels();
				recalculate = false;

				// start timer
				timer::time_point start = timer::now();

				// start mandelbrot threads
				int tmpLocalThreadCount = getThreadCount();
				for (int i = 0; i < tmpLocalThreadCount; i++)
				{
					int startY = i * slice;
					threadList.push_back(thread(computeMandelbrot, i, left, right, top, bottom, startY, startY + slice));
				}

				// join mandelbrot threads
				for (int i = 0; i < threadList.size(); i++)
				{
					if (threadList[i].joinable()) threadList[i].join();
				}
				threadList.clear();

				// end timer and
				timer::time_point end = timer::now();
				int time_taken = duration_cast<milliseconds>(end - start).count();
				if (!recalculate)
				{
					cout << time_taken << "ms" << endl; // display computation time
					{ unique_lock<mutex> lk(m); pauseMandelbrotLoop.wait(lk); } // pause and wait
				}
			}
		}
		catch (const exception& e) { cout << "ERROR (mandelbrot calculation loop)\n" << e.what() << endl; }

		renderingThread.join();

		cout << "\n\nDone.\n\n" << endl;
		system("pause");
	}
	catch (const exception& e) { cout << "ERROR (main)\n" << e.what() << endl; }

	return 0;
}