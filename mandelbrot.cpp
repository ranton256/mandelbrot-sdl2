#include <SDL.h>

#include <cstdlib>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <condition_variable>

using namespace std;

// colors are 0xAARRGGBB
constexpr Uint32 MakeColor(uint8_t r, uint8_t g, uint8_t b)
{
	return 0xFF000000 | (((Uint32)r) << 16) | (((Uint32)g) << 8) | b;
}

typedef struct {
	int top, left, bottom, right;
} MRect;

constexpr Uint32 kBackgroundColor = MakeColor(0,0,0);

constexpr int kPaletteMax = 1000;

// Palette of colors is global, calculated on Setup, then never changes.
uint32_t colors[kPaletteMax];

constexpr int kThreadX = 5;
constexpr int kThreadY = 4;
constexpr int kThreads = kThreadX * kThreadY;

struct GameState;

typedef struct {
	int n;
	MRect r;
	GameState* state;
} RenderThreadState;

struct GameState {
    SDL_Rect windowRect;
    SDL_Window* window;
    SDL_Renderer* renderer; 

    Uint32 * pixels;
    SDL_Texture * pixelsTexture;

	double scale = 1.0;
	double xOffset = 0.0;
	double yOffset = 0.0;

	RenderThreadState threadState[kThreads];

	std::atomic<bool> finished = ATOMIC_FLAG_INIT;
	std::atomic<int> rendered = ATOMIC_FLAG_INIT; // is this right?

	std::mutex dirtyMutex;
	bool dirtyFlag;

	std::condition_variable dirtyCV;
	std::condition_variable renderedCV;
};

typedef enum {
    kInputEventType_Nothing = 0,
    kInputEventType_MouseDown,
    kInputEventType_MouseUp,
    kInputEventType_MouseMove,
    kInputEventType_KeyDown,
    kInputEventType_KeyUp,
    kInputEventType_Quit        
} InputEventType;

struct InputEvent_t {
    InputEventType eventType;
    // mouse event
    Uint32 x, y;
    Uint32 button;
    // keyboard events
    Uint32 scanCode;
    Uint32 keyCode;
    // modifier keys
    Uint32 modifiers;
};
typedef struct InputEvent_t InputEvent;



bool Setup(GameState* state);
void Cleanup(GameState* state);
bool InitSDL();
SDL_Window* CreateWindow(const SDL_Rect& windowRect);
SDL_Renderer* CreateRenderer(SDL_Window* window);

void Render(GameState* state);
void MainLoop(GameState* state);

Uint32 MakeColorHSV(float H, float S,float V);
void HSVtoRGB(float H, float S, float V, uint8_t &out_r, uint8_t &out_g, uint8_t &out_b);

int main( int argc, char* args[] )
{
    std::cout << "Starting..." << std::endl;

    GameState state;
    
    // This sets size and position of window.
    state.windowRect.x = 550;
    state.windowRect.y = 250;
    state.windowRect.w = 600;
    state.windowRect.h = 400;
    
	if ( !Setup(&state) )
    {
        std::cout << "Setup failed.";
		return -1;
    }
    
	MainLoop(&state);

    Cleanup(&state);
   
    std::cout << "Done." << std::endl;

    return 0;
}

// Poll for events, and handle the ones we care about.
bool PollEvent( InputEvent* outEvent )
{
	SDL_Event event;
	int result = SDL_PollEvent(&event);
	if(result != 0)
	{
		outEvent->modifiers = 0;
		outEvent->x = 0;
		outEvent->y = 0;

		switch (event.type) 
		{			
			case SDL_MOUSEBUTTONDOWN:
				outEvent->eventType = kInputEventType_MouseDown;
				outEvent->modifiers = (Uint32)SDL_GetModState();
				outEvent->x = event.button.x;
				outEvent->y = event.button.y;
				outEvent->button = event.button.button;
				break;
			case SDL_MOUSEBUTTONUP:
				outEvent->eventType = kInputEventType_MouseUp;
				outEvent->modifiers = (Uint32)SDL_GetModState();
				outEvent->x = event.button.x;
				outEvent->y = event.button.y;
				outEvent->button = event.button.button;
				break;
			case SDL_MOUSEMOTION:
				outEvent->eventType = kInputEventType_MouseMove;
				outEvent->modifiers = (Uint32)SDL_GetModState();
				outEvent->x = event.motion.x;
				outEvent->y = event.motion.y;
				outEvent->button = 0;
				break;
			case SDL_KEYDOWN:
				outEvent->eventType = kInputEventType_KeyDown;
				outEvent->scanCode = event.key.keysym.scancode;
				outEvent->keyCode = event.key.keysym.sym;
				outEvent->modifiers = (Uint32)event.key.keysym.mod;
				break;
			case SDL_KEYUP:
				outEvent->eventType = kInputEventType_KeyUp;
				outEvent->scanCode = event.key.keysym.scancode;
				outEvent->keyCode = event.key.keysym.sym;
				outEvent->modifiers = (Uint32)event.key.keysym.mod;
				break;
			case SDL_QUIT:
				outEvent->eventType = kInputEventType_Quit;
				break;
			default:
				// do nothing
				outEvent->eventType = kInputEventType_Nothing;
				result = false;
		}
	}
	if(result) {
		// Handle the event.
        if(0) // for debugging
		{
			std::cout << "Had event: " << outEvent->eventType
				<< "sc: " << outEvent->scanCode
				<< "key: " << outEvent->keyCode
				<< "mod: " << outEvent->modifiers
				<< "x,y: " << outEvent->x 
				<< "," << outEvent->y
				<< "btn" << outEvent->button
				<< std::endl;
		}
	}
	return result != 0;
}

void MarkAllDirty(GameState* state)
{
	std::unique_lock<std::mutex> lck(state->dirtyMutex);
	state->dirtyFlag = true;
	lck.unlock();

	state->dirtyCV.notify_all();
}


void Mandelbrot(Uint32 * pixels, int width, int height, MRect dirty, double scale, double xoffset, double yoffset)
{
	
	for (int row = dirty.top; row < dirty.bottom; row++)
	{
		double ypos = (row - height/2) + yoffset;
		double c_im = ypos * 4.0  / (width * scale);

		for (int col = dirty.left; col < dirty.right; col++)
		{
			double xpos = (col - width/2) + xoffset;
			
			double c_re = xpos * 4.0 / (width * scale);
			double x = 0, y = 0;
			int iteration = 0;
			
			double x2 =0;
			double y2 = 0;
			while (x2 + y2 <= 4 && iteration < kPaletteMax)
			{
				y = 2 * x * y +c_im;
				x = x2 - y2 + c_re;
				x2 = x * x;
				y2 = y * y;
				iteration++;
			}

			auto color = (iteration < kPaletteMax) ? colors[iteration] : kBackgroundColor;
			pixels[row * width + col]  = color;	
		}
	}
}

void RenderRect(const RenderThreadState& threadState)
{
	auto state = threadState.state;
	MRect r = threadState.r;
	const int width = state->windowRect.w;
	const int height = state->windowRect.h;
	auto pixels = state->pixels;

	Mandelbrot(pixels, width, height, r, state->scale, state->xOffset, state->yOffset);
}

void RenderThreadProc(RenderThreadState* threadState)
{	
	if(!threadState)
		return; // fail
	auto state = threadState->state;
    while(!state->finished.load()) 
	{
		{ // get lock, check state.
			std::unique_lock<std::mutex> lck(state->dirtyMutex);
			state->dirtyCV.wait(lck);
			
		}
		//std::cout << "Rendering rect in thread " << n << '\n';   
		
		RenderRect(*threadState);
		
		//std::cout << "Work done in thread " << n << '\n';   
		
		// Signal main thread we are done.
		auto result = state->rendered.fetch_add(1);
		if(result == kThreads - 1) {
			//cout << "Last thread done\n";
			state->renderedCV.notify_one();
		}
    }
	std::cout << "Exiting thread " << threadState->n << '\n';
}


void MainLoop(GameState* state)
{
    bool done = false;
	state->finished.store(false);

	cout << "Starting threads\n";
	// Start the threads.
	std::vector<std::thread> v;
    for (int n = 0; n < kThreads; ++n) {
		v.emplace_back(RenderThreadProc, state->threadState + n);
    }
	
	cout << "preparing\n";
	MarkAllDirty(state);

	Render(state);

    do
    {
        InputEvent evt;
        while( PollEvent( &evt ) ) {
			// cout << "poll\n";
		
			// Handle event.
			// Wait until they press space.
			if (evt.eventType == kInputEventType_Quit ||
				(evt.eventType == kInputEventType_KeyDown && evt.keyCode == ' ') )
				done = true;

			
			if(evt.eventType == kInputEventType_KeyDown)
			{
				switch (evt.keyCode) {
					case '=':
						state->scale *= 2.0;
						MarkAllDirty(state);
						break;
					case '-':
						state->scale *= 0.5;
						MarkAllDirty(state);
						break;
					case 'a':
						state->xOffset -= 20 / state->scale;
						MarkAllDirty(state);
						break;
					case 'd':
						state->xOffset += 20 / state->scale;
						MarkAllDirty(state);
						break;
					case 'w':
						state->yOffset -= 20 / state->scale;
						MarkAllDirty(state);
						break;
					case 's':
						state->yOffset += 20 / state->scale;
						MarkAllDirty(state);
						break;
				}
			} 
 		}

		// cout << "no event\n";
		SDL_Delay(1); // save CPU

		Render(state);
    }
    while(!done);

	// flag done and join all threads.
	state->finished.store(true);
	state->dirtyCV.notify_all();
	for (auto& t : v) {
        t.join();
    }

}

void Cleanup(GameState* state)
{
    if(state->pixels)
        delete[] state->pixels;
    if(state->pixelsTexture)
        SDL_DestroyTexture(state->pixelsTexture);
}


void Render(GameState* state)
{
	// Clear the window
	SDL_RenderClear( state->renderer );
	
    const int width = state->windowRect.w;
    const int height = state->windowRect.h;
    auto pixels = state->pixels;
	
	if (state->dirtyFlag)
	{
		auto t1 = std::chrono::high_resolution_clock::now();
		// This would render in main thread all at once

		{
			std::unique_lock<std::mutex> lck(state->dirtyMutex);
			state->rendered.store(0);
		}
		
		// cout << "Waiting for render to finish\n";
		std::cv_status result;
		do 
		{
			// wake up threads.
			state->dirtyCV.notify_all();

			std::unique_lock<std::mutex> lck(state->dirtyMutex);
			result = state->renderedCV.wait_for(lck,std::chrono::milliseconds(200));
			// cout << "waited...\n";
		}
		while(result == std::cv_status::timeout);

		// cout << "Done waiting\n";
		// int rendered = state->rendered.load();
		// cout << "rendered = " << rendered << endl;

		auto t2 = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

		cout << "t us: "<< duration << "\n";

		state->dirtyFlag = false;

		// output parameters
		cout << "scale: " << state->scale << " x: " << state->xOffset << " y: " << state->yOffset << endl;
	}
	
	// This code is for testing palette
	if(0)
	{
		for(int y = 0; y < height; y++) 
		{
			for(int x = 0; x < width; x++)
			{
				int pidx = (y/10)*width + x; // (y/5)*width + (x/5);
				// pidx = 100;
				uint32_t color = kBackgroundColor;
				if(pidx < kPaletteMax)
					color = colors[pidx];
					
				pixels[y * width + x] = color;       
			}
		}
	}

    SDL_UpdateTexture(state->pixelsTexture, NULL, pixels, width * sizeof(Uint32));
	SDL_RenderCopy(state->renderer, state->pixelsTexture, nullptr, nullptr);
	SDL_RenderPresent( state->renderer);
}

bool Setup(GameState* state)
{
	if ( !InitSDL() )
		return false;

    state->window =CreateWindow(state->windowRect);
	if ( !state->window )
		return false;

    state->renderer = CreateRenderer(state->window);
	if ( !state->renderer )
		return false;
    
    SDL_RenderSetLogicalSize( state->renderer, state->windowRect.w, state->windowRect.h );
    SDL_SetRenderDrawColor( state->renderer, 255, 0, 0, 255 );
    
	state->scale = 1.0;
	state->xOffset = 0.0;
	state->yOffset = 0.0;

    const int width = state->windowRect.w;
    const int height = state->windowRect.h;

	// set all dirty
	state->dirtyFlag = true;
	

    state->pixels = new Uint32[width * height];

    state->pixelsTexture = SDL_CreateTexture(state->renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
    
	// Setup palette
	for (int i = 0; i<kPaletteMax; i++) {
		float H = (i / 256.0) * 360.0;
		float B = 100.0 * (i / (8.0 + i));
		colors[i] = MakeColorHSV((int)H % 360, 100, B);
	}

	for(int n = 0; n < kThreads; n++) {
		state->threadState[n].n = n;
		state->threadState[n].state = state;
	}

	// Setup the rects for each thread
	const int rowHeight = height / kThreadY;
	const int colWidth = width / kThreadX;

	int idx = 0;
	for (int y = 0; y < kThreadY; y++)
	{
		MRect r;
		r.top = y * rowHeight;
		r.bottom = r.top + rowHeight;
		if(r.bottom > height) r.bottom = height;
		for (int x = 0; x < kThreadX; x++)
		{
			r.left = x * colWidth;
			r.right = r.left + colWidth;
			if(r.right > width) r.right = width;	
			cout << "Thread " << idx << " assigned "
				 << r.left << " to "
				 << r.right << "  "
				 << r.top << " to "
				 << r.bottom
				 << endl;
			state->threadState[ idx++ ].r = r;
			
		}
	}

	return true;
}

bool InitSDL()
{
	if ( SDL_Init( SDL_INIT_EVERYTHING ) == -1 )
	{
		std::cout << "Error initializing SDL: " << SDL_GetError() << std::endl;
		return false;
	}

	return true;
}

SDL_Window* CreateWindow(const SDL_Rect& windowRect)
{
	SDL_Window* window = SDL_CreateWindow( "Server", windowRect.x, windowRect.y, windowRect.w, windowRect.h, 0 );
	if ( window == nullptr )
	{
		std::cout << "Could not create window: " << SDL_GetError();
	}

	return window;
}

SDL_Renderer* CreateRenderer(SDL_Window* window)
{
	SDL_Renderer* renderer = SDL_CreateRenderer( window, -1, SDL_RENDERER_ACCELERATED );
	if ( renderer == nullptr )
	{
		std::cout << "Failed to create renderer : " << SDL_GetError();
	}

	return renderer;
}
	

void HSVtoRGB(float H, float S, float V, uint8_t &out_r, uint8_t &out_g, uint8_t &out_b)
{
	if (H > 360 || H < 0 || S > 100 || S < 0 || V > 100 || V < 0)
	{
		// out of range
		out_r = out_g = out_b = 0;
		return;
	}

	float s = S / 100;
	float v = V / 100;
	float C = s * v;
	float X = C * (1 - abs(fmod(H / 60.0, 2) - 1));
	float m = v - C;
	float r, g, b;
	if (H >= 0 && H < 60)
	{
		r = C;
		g = X;
		b = 0;
	}
	else if (H >= 60 && H < 120)
	{
		r = X;
		g = C;
		b = 0;
	}
	else if (H >= 120 && H < 180)
	{
		r = 0;
		g = C;
		b = X;
	}
	else if (H >= 180 && H < 240)
	{
		r = 0;
		g = X;
		b = C;
	}
	else if (H >= 240 && H < 300)
	{
		r = X;
		g = 0;
		b = C;
	}
	else
	{
		r = C;
		g = 0;
		b = X;
	}

	out_r = (r + m) * 255;
	out_g = (g + m) * 255;
	out_b = (b + m) * 255;
}

Uint32 MakeColorHSV(float H, float S,float V)
{
	uint8_t r, g, b;
	HSVtoRGB(H, S, V, r, g, b); 
	return MakeColor(r, g,b);
}
