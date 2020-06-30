/*
*
* Copyright (C) 2016 OtherCrashOverride@users.noreply.github.com.
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2, as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
*/

#include "X11Window.h"

#include "Egl.h"
#include "GL.h"

#include <cstdlib>
#include <cstdio>


//void X11AmlWindow::IntializeEgl()
//{
//	eglDisplay = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, display, NULL);
//	if (eglDisplay == EGL_NO_DISPLAY)
//	{
//		throw Exception("eglGetPlatformDisplay failed.\n");
//	}
//
//
//	// Initialize EGL
//	EGLint major;
//	EGLint minor;
//	EGLBoolean success = eglInitialize(eglDisplay, &major, &minor);
//	if (success != EGL_TRUE)
//	{
//		Egl::CheckError();
//	}
//
//	printf("EGL: major=%d, minor=%d\n", major, minor);
//	printf("EGL: Vendor=%s\n", eglQueryString(eglDisplay, EGL_VENDOR));
//	printf("EGL: Version=%s\n", eglQueryString(eglDisplay, EGL_VERSION));
//	printf("EGL: ClientAPIs=%s\n", eglQueryString(eglDisplay, EGL_CLIENT_APIS));
//	printf("EGL: Extensions=%s\n", eglQueryString(eglDisplay, EGL_EXTENSIONS));
//	printf("EGL: ClientExtensions=%s\n", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
//	printf("\n");
//
//}

//void X11AmlWindow::FindEglConfig(EGLConfig* eglConfigOut, int* xVisualOut)
//{
//	EGLint configAttributes[] =
//	{
//		EGL_RED_SIZE,            8,
//		EGL_GREEN_SIZE,          8,
//		EGL_BLUE_SIZE,           8,
//		EGL_ALPHA_SIZE,          8,
//
//		EGL_DEPTH_SIZE,          24,
//		EGL_STENCIL_SIZE,        8,
//
//		EGL_SURFACE_TYPE,        EGL_WINDOW_BIT,
//
//		EGL_NONE
//	};
//
//
//	int num_configs;
//	EGLBoolean success = eglChooseConfig(eglDisplay, configAttributes, NULL, 0, &num_configs);
//	if (success != EGL_TRUE)
//	{
//		Egl::CheckError();
//	}
//
//
//	EGLConfig* configs = new EGLConfig[num_configs];
//	success = eglChooseConfig(eglDisplay, configAttributes, configs, num_configs, &num_configs);
//	if (success != EGL_TRUE)
//	{
//		Egl::CheckError();
//	}
//
//
//	EGLConfig match = 0;
//
//	for (int i = 0; i < num_configs; ++i)
//	{
//		EGLint configRedSize;
//		EGLint configGreenSize;
//		EGLint configBlueSize;
//		EGLint configAlphaSize;
//		EGLint configDepthSize;
//		EGLint configStencilSize;
//
//		eglGetConfigAttrib(eglDisplay, configs[i], EGL_RED_SIZE, &configRedSize);
//		eglGetConfigAttrib(eglDisplay, configs[i], EGL_GREEN_SIZE, &configGreenSize);
//		eglGetConfigAttrib(eglDisplay, configs[i], EGL_BLUE_SIZE, &configBlueSize);
//		eglGetConfigAttrib(eglDisplay, configs[i], EGL_ALPHA_SIZE, &configAlphaSize);
//		eglGetConfigAttrib(eglDisplay, configs[i], EGL_DEPTH_SIZE, &configDepthSize);
//		eglGetConfigAttrib(eglDisplay, configs[i], EGL_STENCIL_SIZE, &configStencilSize);
//
//		if (configRedSize == 8 &&
//			configBlueSize == 8 &&
//			configGreenSize == 8 &&
//			configAlphaSize == 8 &&
//			configDepthSize == 24 &&
//			configStencilSize == 8)
//		{
//			match = configs[i];
//			break;
//		}
//	}
//
//	delete[] configs;
//
//	if (match == 0)
//	{
//		throw Exception("No eglConfig match found.\n");
//	}
//
//	*eglConfigOut = match;
//	printf("EGLConfig match found: (%p)\n", match);
//
//	// Get the native visual id associated with the config
//	eglGetConfigAttrib(eglDisplay, match, EGL_NATIVE_VISUAL_ID, xVisualOut);
//
//
//	//EGLAttrib windowAttr[] = {
//	//	EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
//	//	EGL_NONE };
//
//	//EGLSurface surface = eglCreatePlatformWindowSurface(display, match, &window, windowAttr);
//
//	//if (surface == EGL_NO_SURFACE)
//	//{
//	//	Egl_CheckError();
//	//}
//}


static bool create_xcb_atoms(xcb_connection_t* connection, xcb_atom_t* atoms, const unsigned int atoms_size, const char** names)
{
	bool success = true;
	xcb_intern_atom_cookie_t atom_cookies[atoms_size];

	for (unsigned int i = 0; i < atoms_size; ++i)
	{
		atom_cookies[i] = xcb_intern_atom(connection, 0, strlen(names[i]), names[i]);
	}

	for (unsigned int i = 0; i < atoms_size; ++i)
	{
		xcb_generic_error_t* error = nullptr;
		xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, atom_cookies[i], &error);

		if ((reply == nullptr) || (error != nullptr))
		{
			success = false;
			free(error);
		}
		else
		{
			atoms[i] = reply->atom;
		}

		free(reply);
	}

	return success;
}


X11AmlWindow::X11AmlWindow()
	: AmlWindow()
{
	int screen_number = 0;


	connection = xcb_connect(nullptr, &screen_number);
	if (connection == nullptr)
	{
		throw Exception("xcb_connect failed.");
	}


	// Screen
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
	for (int i = 0; iter.rem; ++i, xcb_screen_next(&iter))
	{
		if (i == screen_number)
		{
			screen = iter.data;
			break;
		}
	}

	if (screen == nullptr)
	{
		xcb_disconnect(connection);
		throw Exception("Failed to get default screen.");
	}

	width = screen->width_in_pixels;
	height = screen->height_in_pixels;
	printf("X11Window: width=%d, height=%d\n", width, height);


	// Egl
	eglDisplay = Egl::Initialize(EGL_PLATFORM_X11_KHR, EGL_DEFAULT_DISPLAY);

	EGLConfig eglConfig = Egl::FindConfig(eglDisplay, 8, 8, 8, 8, 24, 8);
	if (eglConfig == 0)
	{
		xcb_disconnect(connection);
		throw Exception("Compatible EGL config not found.");
	}


	// Get the native visual id associated with the config
	EGLint xVisualId;
	if (!eglGetConfigAttrib(eglDisplay, eglConfig, EGL_NATIVE_VISUAL_ID, &xVisualId))
	{
		xcb_disconnect(connection);
		throw Exception("Failed to get native visual id.");
	}


	// Window
	xcb_void_cookie_t cookie;
	xcb_generic_error_t* error;


	xcb_colormap_t colormap = xcb_generate_id(connection);

	cookie = xcb_create_colormap_checked(connection,
		XCB_COLORMAP_ALLOC_NONE,
		colormap,
		screen->root,
		xVisualId);

	error = xcb_request_check(connection, cookie);
	if ((error != nullptr) || xcb_connection_has_error(connection))
	{
		xcb_disconnect(connection);
		free(error);
		throw Exception("xcb_create_colormap failed.");
	}



	uint32_t attr[] = {
		screen->black_pixel,
		screen->black_pixel,
		(XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION),
		colormap };

	uint32_t mask = (XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP);

	xwin = xcb_generate_id(connection);

	cookie = xcb_create_window_checked(connection,
		XCB_COPY_FROM_PARENT, //32,	// Alpha required
		xwin,
		screen->root,
		0,
		0,
		DEFAULT_WIDTH, //width,
		DEFAULT_HEIGHT, //height,
		0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		xVisualId,
		mask,
		attr);

	error = xcb_request_check(connection, cookie);
	if ((error != nullptr) || xcb_connection_has_error(connection))
	{
		xcb_disconnect(connection);
		free(error);
		throw Exception("xcb_create_window failed.");
	}

	printf("X11Window: xwin = %u\n", xwin);


	//xcb_icccm_wm_hints_t hints = { 0 };
	//hints.input = true;
	//hints.flags = XCB_ICCCM_WM_HINT_INPUT;

	//xcb_change_property(connection, XCB_PROP_MODE_REPLACE, xwin, XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 32, sizeof(hints)/4, &hints);


	// Set the window name
	xcb_change_property(connection, XCB_PROP_MODE_REPLACE, xwin, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(WINDOW_TITLE), WINDOW_TITLE);

	// Show the window
	cookie = xcb_map_window_checked(connection, xwin);

	error = xcb_request_check(connection, cookie);
	if ((error != nullptr) || xcb_connection_has_error(connection))
	{
		xcb_destroy_window(connection, xwin);
		xcb_disconnect(connection);
		free(error);
		throw Exception("xcb_map_window failed.");
	}


	// Register to be notified when window is closed
	static const char* wm_protocols_names[] = { "WM_PROTOCOLS", "WM_DELETE_WINDOW" };
	xcb_atom_t atoms[5] = { 0 };
	xcb_atom_t& wm_protocols = atoms[0];

	if (!create_xcb_atoms(connection, atoms, 2, wm_protocols_names))
	{
		xcb_destroy_window(connection, xwin);
		xcb_disconnect(connection);
		throw Exception("xcb_intern_atom failed.");
	}

	wm_delete_window = atoms[1];
	xcb_change_property(connection, XCB_PROP_MODE_REPLACE, xwin, wm_protocols, XCB_ATOM_ATOM, 32, 1, &wm_delete_window);


#if 1
	// Fullscreen
	static const char* wm_state_names[] = { "_NET_WM_STATE", "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_ABOVE" };
	xcb_atom_t& wm_state = atoms[2];
	xcb_atom_t& fullscreen = atoms[3];
	xcb_atom_t& above_other_windows = atoms[4];

	if (!create_xcb_atoms(connection, atoms+2, 3, wm_state_names))
	{
		xcb_destroy_window(connection, xwin);
		xcb_disconnect(connection);
		throw Exception("xcb_intern_atom failed.");
	}

	xcb_client_message_event_t xcmev = { 0 };
	xcmev.response_type = XCB_CLIENT_MESSAGE;
	xcmev.window = xwin;
	xcmev.type = wm_state;
	xcmev.format = 32;
	xcmev.data.data32[0] = 1;
	xcmev.data.data32[1] = fullscreen;
	xcmev.data.data32[2] = above_other_windows;
	xcmev.data.data32[3] = 1;

	xcb_send_event(connection,
		0,
		screen->root,
		(XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY),
		(char*) &xcmev);


	HideMouse();
#endif

	//WriteToFile("/sys/class/graphics/fb0/blank", "0");

	//video_fd = open("/dev/amvideo", O_RDWR);
	//if (video_fd < 0)
	//{
	//	throw Exception("open /dev/amvideo failed.");
	//}

	EGLAttrib windowAttr[] = {
		EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
		EGL_NONE };

	surface = eglCreatePlatformWindowSurface(eglDisplay, eglConfig, &xwin, windowAttr);

	if (surface == EGL_NO_SURFACE)
	{
		Egl::CheckError();
	}


	// Create a context
	eglBindAPI(EGL_OPENGL_ES_API);

	EGLint contextAttributes[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE };

	context = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttributes);
	if (context == EGL_NO_CONTEXT)
	{
		Egl::CheckError();
	}

	EGLBoolean success = eglMakeCurrent(eglDisplay, surface, surface, context);
	if (success != EGL_TRUE)
	{
		Egl::CheckError();
	}

	// experimental workaround for wrong sized gl surface
	glViewport(0, 0, width, height);


	success = eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (success != EGL_TRUE)
	{
		Egl::CheckError();
	}
}

X11AmlWindow::~X11AmlWindow()
{
	//WriteToFile("/sys/class/video/axis", "0 0 0 0");
	SetVideoAxis(0, 0, 0, 0);

	xcb_destroy_window(connection, xwin);
	xcb_flush(connection);
	xcb_disconnect(connection);
}


void X11AmlWindow::WaitForMessage()
{
	//xcb_generic_event_t* event = xcb_wait_for_event(connection);
	//free(event);
}

bool X11AmlWindow::ProcessMessages()
{
	bool run = true;
	xcb_generic_event_t* event;

	while ((event = xcb_poll_for_event(connection)))
	{
		switch (event->response_type & ~0x80)
		{
			case XCB_CONFIGURE_NOTIFY:
			{
				xcb_configure_notify_event_t* xcnev = (xcb_configure_notify_event_t*)event;

				xcb_generic_error_t* error = nullptr;
				xcb_translate_coordinates_cookie_t cookie;
				xcb_translate_coordinates_reply_t* reply;

				cookie = xcb_translate_coordinates(connection, xwin, screen->root, 0, 0);
				reply = xcb_translate_coordinates_reply(connection, cookie, &error);

				if ((reply == nullptr) || (error != nullptr))
				{
					xcb_destroy_window(connection, xwin);
					xcb_disconnect(connection);
					free(error);
					free(reply);
					throw Exception("xcb_translate_coordinates failed.");
				}

				SetVideoAxis(reply->dst_x, reply->dst_y, xcnev->width, xcnev->height);

				free(reply);
				break;
			}

			case XCB_CLIENT_MESSAGE:
			{
				xcb_client_message_event_t* xcmev = (xcb_client_message_event_t*)event;

				if (xcmev->data.data32[0] == wm_delete_window)
				{
					printf("X11Window: Window closed.\n");
					run = false;
				}
			}
			break;
		}

		free(event);
	}

	return run;
}

void X11AmlWindow::HideMouse()
{
	static uint8_t bitmap[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	xcb_pixmap_t pixmap = xcb_create_pixmap_from_bitmap_data(connection,
		xwin,
		bitmap,
		8,
		8,
		1,
		screen->black_pixel,
		screen->black_pixel,
		nullptr);

	xcb_cursor_t cursor = xcb_generate_id(connection);
	xcb_void_cookie_t cookie = xcb_create_cursor_checked(connection,
		cursor,
		pixmap,
		pixmap,
		0, 0, 0,
		0, 0, 0,
		0,
		0);

	xcb_generic_error_t* error = xcb_request_check(connection, cookie);
	if ((error != nullptr) || xcb_connection_has_error(connection))
	{
		xcb_free_pixmap(connection, pixmap);
		xcb_destroy_window(connection, xwin);
		xcb_disconnect(connection);
		free(error);
		throw Exception("xcb_create_cursor failed.");
	}

	xcb_change_window_attributes(connection, xwin, XCB_CW_CURSOR, &cursor);

	xcb_free_cursor(connection, cursor);
	xcb_free_pixmap(connection, pixmap);
}

void X11AmlWindow::UnHideMouse()
{
	xcb_cursor_t cursor = XCB_CURSOR_NONE;
	xcb_change_window_attributes(connection, xwin, XCB_CW_CURSOR, &cursor);
}

