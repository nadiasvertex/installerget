/*******************************
Copyright (c) 2009, Christopher Nelson
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
in the documentation and/or other materials provided with the distribution.

Neither the name of the Christopher Nelson nor the names of his contributors may be used to endorse or promote products derived 
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, 
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************/

// cget.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "cget.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
int					cget_curl_progress_callback(void *client_data,
											   double dltotal,
											   double dlnow,
											   double ultotal,
											   double ulnow);

bool				cget_process_options(CURL *co);


class Progress
{
	HWND hParent;
	HWND hProgress;
	HWND hUrlText;
	HWND hTargetText;
	HWND hInfo;

	int last_step;	
	time_t start_time;

	// bytes per second
	// transfer speed
	double bps;

	// estimated time of arrival
	// in seconds from now
	double eta;

	std::string _url_text;
	std::string _target_text;
	
protected:
	void set_label_text(HWND hwin, std::string text, bool clip=false)
	{
		size_t converted_chars;
		size_t size = (text.length()+1) * sizeof(wchar_t);
		wchar_t* w_text = new wchar_t[size];

		// Convert to wchar_t
		mbstowcs_s(&converted_chars, 
					w_text, 
					size, 
					text.c_str(), 
					text.length());

		if (clip)
		{
			std::wstring clipped_text = shrink_text(hwin, std::wstring(w_text));

			// Set the window text for the label 
			SetWindowText(hwin, clipped_text.c_str());
		}
		else
		{
			// Set the window text for the label 
			SetWindowText(hwin, w_text);
		}

		// Delete the buffer.
		delete [] w_text;
	}

	// This function takes a potentially very long string, and makes it small enough to fit in
	// the given window by removing characters from the middle of the string.
	std::wstring shrink_text(HWND hwin, std::wstring text)
	{		
		HDC hdc=GetDC(hwin);
		RECT label_rect;		
		SIZE text_size;	
				
		std::wstring work=text;
		bool needed_clipping=false;

		// Get the size of the window
		GetClientRect(hwin, &label_rect);
		GetTextExtentPoint32(hdc, work.c_str(), work.length(), &text_size);

		while(text_size.cx > label_rect.right)
		{		
			// Erase one character from the middle, until we are at the right size.
			size_t mid = work.length()>>1;
			work.erase(mid+2, 1);

			needed_clipping=true;
		
			// Recheck extents.
			GetTextExtentPoint32(hdc, work.c_str(), work.length(), &text_size);
		} 

		if (needed_clipping && work.length() >= 3)
		{
			size_t mid = (work.length()>>1)-1;
			work = work.substr(0, mid) + std::wstring(L"...") + work.substr(mid+4, work.length() - (mid + 3));
		}

		return work;
	}


public:
	Progress(HWND parent):hParent(parent), last_step(0)
	{
		InitCommonControls();
		
		hProgress = CreateWindowEx(0, PROGRESS_CLASS, (LPTSTR) NULL,
								  WS_CHILD | PBS_SMOOTH | WS_VISIBLE,
								  0, 0, 0, 0,
								  parent, (HMENU) 0, hInst, NULL);

		hUrlText = CreateWindowEx(0, TEXT("STATIC"), (LPTSTR) NULL,
								  WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
								  0, 0, 0, 0,
								  parent, (HMENU) IDC_STATIC, hInst, NULL);

		hTargetText = CreateWindowEx(0, TEXT("STATIC"), (LPTSTR) NULL,
								  WS_CHILD  | SS_LEFTNOWORDWRAP,
								  0, 0, 0, 0,
								  parent, (HMENU) IDC_STATIC, hInst, NULL);

		hInfo = CreateWindowEx(0, TEXT("STATIC"), (LPTSTR) NULL,
								  WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
								  0, 0, 0, 0,
								  parent, (HMENU) IDC_STATIC, hInst, NULL);

		on_resize();
	}

	void on_resize()
	{
		RECT rcClient;
		int cyVScroll;

		cyVScroll = GetSystemMetrics(SM_CYVSCROLL);
		GetClientRect(hParent, &rcClient);

		MoveWindow(hProgress, rcClient.left, rcClient.bottom - cyVScroll, rcClient.right, cyVScroll, TRUE);
		MoveWindow(hUrlText, rcClient.left, 0, rcClient.right, cyVScroll, TRUE);
		MoveWindow(hTargetText, rcClient.left, cyVScroll, rcClient.right, cyVScroll, TRUE);
		MoveWindow(hInfo, rcClient.left, cyVScroll<<1, rcClient.right, cyVScroll, TRUE);

		// Update the labels.
		set_label_text(hUrlText, _url_text, true);
		set_label_text(hTargetText, _target_text, true);
	}
	
	void set_url(std::string url_text)
	{
		_url_text = url_text;
		set_label_text(hUrlText, _url_text, true);
	}

	void set_target(std::string target_text)
	{
		_target_text = target_text;
		set_label_text(hTargetText, _target_text, true);
	}

	void set_title(std::string title_text)
	{		
		set_label_text(hParent, title_text);
	}

	void set_range(int range)
	{
		SendMessage(hProgress, PBM_SETRANGE, 0,  MAKELPARAM(0, range)); 
		SendMessage(hProgress, PBM_SETSTEP, (WPARAM) 1, 0);
	}	

	void set_step(int step)
	{		
		if (step!=last_step)
		{			
			std::stringstream progress;
			
			// Make the progress label.
			progress << step/10 << "% ";

			// Be somewhat intelligent about displaying the transfer rate
			if (bps < 1<<10) progress << "(" << (int)bps << "bps) ";
			else if (bps >= 1<<10 && bps < 1<<20) progress << "(" << (((int)bps)>>10) << "Kbps) ";
			else if (bps >= 1<<20 && bps < 1<<30) progress << "(" << (((int)bps)>>20) << "Mbps) ";
			else progress << "(" << (((int)bps)>>30) << "Gbps) ";

			// Show the ETA in a somewhat intelligent manner
			progress << "ETA: ";
			if (eta < 60) progress << (int)eta << " seconds ";
			else if (eta >= 60 && eta < 60*60) 
			{				
				int minutes = eta / 60;
				int seconds = eta - (minutes * 60);

				progress << minutes << "m:" << seconds << "s ";
			}
			else if (eta >= 60*60 && eta < 60*60*24) 
			{
				int hours   = eta / 3600;
				int minutes = (eta - (hours * 3600)) / 60;
				int seconds = eta - ((hours * 3600) + (minutes * 60));				

				progress << hours << "h:" << minutes << "m:" << seconds << "s ";
			}
			else 
			{
				int days    = eta / 86400;
				int hours   = (eta - (days * 86400)) / 3600;
				int minutes = (eta - ((days*86400) + (hours * 3600))) / 60;
				int seconds = eta - ((days*86400) + (hours * 3600) + (minutes * 60));				

				progress << days << "d:" << hours << "h:" << minutes << "m:" << seconds << "s ";
			}			

			set_label_text(hInfo, progress.str());
			last_step=step;

			SendMessage(hProgress, PBM_SETPOS, (WPARAM) step, 0);
		}		
	}

	void stepit()
	{
		SendMessage(hProgress, PBM_STEPIT, 0, 0);
	}

	void reset()
	{
		SendMessage(hProgress, PBM_SETPOS, (WPARAM) 0, 0);

		time(&start_time);		
	}

	void update_stats(double bytes_received, double total_bytes)
	{
		time_t now;
		time(&now);

		bps = (bytes_received / (double)(now - start_time));
		eta = (total_bytes - bytes_received) / bps;		
	}

};

// Handles managing the progress bar
Progress *global_progress;

// The list of URLs to download
std::vector<std::string> urls;

// The list of file outputs to write.
std::vector<std::string> outputs;

// Global flag set when we are supposed to quit.
bool global_please_quit=false;

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	HACCEL hAccelTable;
 	
	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_CGET, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CGET));

	// Initialize the libcurl stuff.
	curl_global_init(CURL_GLOBAL_ALL);
	CURL *co = curl_easy_init();

	// Turn off built-in progress meter so that our own function will
	// get called.
	curl_easy_setopt(co, CURLOPT_NOPROGRESS, 0);

	// Setup our own progress function.
	curl_easy_setopt(co, CURLOPT_PROGRESSDATA, hAccelTable);	
	curl_easy_setopt(co, CURLOPT_PROGRESSFUNCTION, cget_curl_progress_callback);

	// Process the command-line options.
	if (!cget_process_options(co))
		return -1;
	
	// Initialize the range of the progress bar.
	global_progress->set_range(1000);

	// Walk through all urls.
	std::vector<std::string>::iterator url_it = urls.begin();
	std::vector<std::string>::iterator out_it = outputs.begin();

	for(; url_it!=urls.end() && out_it!=outputs.end(); ++url_it, ++out_it)
	{
	    // Open the file		
		FILE *stream = fopen(out_it->c_str(), "wb");

		// Tell curl where to write to
		curl_easy_setopt(co, CURLOPT_WRITEDATA, stream);		

		// Tell curl where to read from.
		curl_easy_setopt(co, CURLOPT_URL, url_it->c_str());

		// Fill in labels on window
		global_progress->set_url(url_it->c_str());
		global_progress->set_target(out_it->c_str());

		// Restart the timers and the progress bar
		global_progress->reset();

		// Download the file.
		CURLcode res = curl_easy_perform(co);

		// Log the result.
		fprintf(stderr, "%s\n", curl_easy_strerror(res));

		// Close the file.
		if(NULL!=stream)
			fclose(stream);

		// Return with an error code if we have any problems.
		if (CURLE_OK!=res)
			return res;
	}

	curl_easy_cleanup(co);

	//return (int) msg.wParam;
	return 0;
}

bool
cget_process_options(CURL *co)
{
	unsigned verbose = 0;

	/* short options string */
	char *shortopts = "Vho:m::v::";

	/* long options list */
	struct option longopts[] =
	{
		/* name,        has_arg,           flag, val */ /* longind */		
		{ "truncate",   no_argument,       0,     0  }, /*       0 */
		{ "version",    no_argument,       0,    'V' }, /*       1 */
		{ "help",       no_argument,       0,    'h' }, /*       2 */
		{ "output",     required_argument, 0,    'o' }, /*       3 */
		{ "max-time",   required_argument, 0,    'm' }, /*       4 */
		{ "stderr",		required_argument, 0,     0  }, /*       5 */
		{ "verbose",    optional_argument, 0,    'v' }, /*       6 */
		{ "url",		required_argument, 0,    'u' }, /*       7 */
		{ "title",		required_argument, 0,    't' }, /*       8 */
		/* end-of-list marker */
		{ 0, 0, 0, 0 }
	};

	// Convert command line into MBCS so that the getopt code can deal with it.
	int argc;
	LPWSTR* lp_argv = CommandLineToArgvW(GetCommandLine(), &argc);

	char **argv = new char *[argc];
	for(int i=0; i<argc; ++i)
	{
		size_t size = wcslen(lp_argv[i]) + 1;		
		size_t converted_chars;
		
		argv[i] = new char[size];
		wcstombs_s(&converted_chars, argv[i], size, lp_argv[i], _TRUNCATE);
	}

	char *progname=argv[0];

	/* option result */
	int opt;

	/* long option list index */
	int longind = 0;

	/* parse all options from the command line */
	  while ((opt =getopt_long_only(argc, 
									argv, 
									shortopts, 
									longopts, 
									&longind)) != -1)
	  {
		switch (opt)
		  {
		  case 0: /* a long option without an equivalent short option */
			switch (longind)
			  {
			  case 0: /* -truncate */
				//append = 1;
				break;
			  case 5: /* -stderr */
				//append = 0;
				break;		
			  default: /* something unexpected has happened */
				fprintf(stderr,
						"%s: "
						"getopt_long_only unexpectedly returned %d for `--%s'\n",
						progname,
						opt,
						longopts[longind].name);
				return false;
			  }
			break;
		  case 'V': /* -version */
			//version(progname);
			return 0;
		  case 'h': /* -help */
			//help(progname);
			return 0;		  
		  case 'o': /* -output=FILE */			
			outputs.push_back(optarg);
			break;
		  case 'u': /* -url=URL */	
			urls.push_back(optarg);			
			break;
		  case 't': /* -title="window title" */	
			  global_progress->set_title(std::string(optarg));			
			break;
		  case 'v': /* -verbose[=NUM] */
			if (optarg)
			  {
				/* we use this while trying to parse a numeric argument */
				char ignored;
				if (sscanf(optarg,
						   "%u%c",
						   &verbose,
						   &ignored) != 1)
				{
					fprintf(stderr,
							"%s: "
							"verbosity level `%s' is not a number\n",
							progname,
							optarg);
					//usage(progname);
					return false;
				}
				else
				{
					curl_easy_setopt(co, CURLOPT_VERBOSE, verbose);
				}
			  }
			else
			{
			  ++verbose;
			  curl_easy_setopt(co, CURLOPT_VERBOSE, verbose);
			}
			break;
		  case '?': /* getopt_long_only noticed an error */
			//usage(progname);
			return 2;
		  default: /* something unexpected has happened */
			fprintf(stderr,
					"%s: "
					"getopt_long_only returned an unexpected value (%d)\n",
					progname,
					opt);
			return false;
		  }
	}

	return true;
}


int 
cget_curl_progress_callback(void *client_data,
                           double dltotal,
                           double dlnow,
                           double ultotal,
                           double ulnow)
{	
	MSG msg;

	int step = (int)((dlnow*1000.0)/dltotal);
	global_progress->set_step(step);
	global_progress->update_stats(dlnow, dltotal);

	// Process all available messages, but don't wait around for
	// more to be posted.
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{	
			TranslateMessage(&msg);
			DispatchMessage(&msg);
	}

	return (global_please_quit ? 1 : 0);
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CGET));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL; //MAKEINTRESOURCE(IDC_CGET);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;
   int cyVScroll = GetSystemMetrics(SM_CYVSCROLL);
   
   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindowEx(WS_EX_TOOLWINDOW, szWindowClass, szTitle,  WS_SYSMENU,
      CW_USEDEFAULT, CW_USEDEFAULT, 400, 92, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   global_progress = new Progress(hWnd);
   
   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;

	case WM_CTLCOLORSTATIC:
	{
		HWND ctl = (HWND)lParam;
		hdc = (HDC)wParam;		

		SelectObject(hdc, GetStockObject(WHITE_BRUSH));
	} break;

	case WM_SIZE:
		global_progress->on_resize();
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		global_please_quit=true;
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
