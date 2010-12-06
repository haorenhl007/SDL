/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2010 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"


#include <fcntl.h>
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandrender_c.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylandgl.h"


#define WAYLANDVID_DRIVER_NAME "wayland"

/* Initialization/Query functions */
static int Wayland_VideoInit(_THIS);
static int Wayland_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode);
static void Wayland_VideoQuit(_THIS);


/* Wayland driver bootstrap functions */

static int
Wayland_Available(void)
{
    const char *envr = SDL_getenv("SDL_VIDEODRIVER");
    if ((envr) && (SDL_strcmp(envr, WAYLANDVID_DRIVER_NAME) == 0)) {
        return (1);
    }

    return (0);
}

static void
Wayland_DeleteDevice(SDL_VideoDevice * device)
{
    SDL_free(device);
}

static SDL_VideoDevice *
Wayland_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        if (device) {
            SDL_free(device);
        }
        return (0);
    }

    /* Set the function pointers */
    device->VideoInit = Wayland_VideoInit;
    device->VideoQuit = Wayland_VideoQuit;
    device->SetDisplayMode = Wayland_SetDisplayMode;
    device->PumpEvents = Wayland_PumpEvents;
    
    device->GL_SwapWindow = Wayland_GL_SwapWindow;
    device->GL_GetSwapInterval = Wayland_GL_GetSwapInterval;
    device->GL_SetSwapInterval = Wayland_GL_SetSwapInterval;
    device->GL_MakeCurrent = Wayland_GL_MakeCurrent;
    device->GL_CreateContext = Wayland_GL_CreateContext;
    device->GL_LoadLibrary = Wayland_GL_LoadLibrary;
    device->GL_UnloadLibrary = Wayland_GL_UnloadLibrary;
    

    device->CreateWindow = Wayland_CreateWindow;
    device->ShowWindow = Wayland_ShowWindow;

    device->free = Wayland_DeleteDevice;

    return device;
}

VideoBootStrap Wayland_bootstrap = {
    WAYLANDVID_DRIVER_NAME, "SDL Wayland video driver",
    Wayland_Available, Wayland_CreateDevice
};

static void
display_handle_geometry(void *data,
			struct wl_output *output,
			int32_t width, int32_t height)
{
    SDL_WaylandData *d = data;

    d->screen_allocation.x = 0;
    d->screen_allocation.y = 0;
    d->screen_allocation.width = width;
    d->screen_allocation.height = height;
}


static const struct wl_output_listener output_listener = {
	display_handle_geometry,
};

static void
handle_configure(void *data, struct wl_shell *shell,
		 uint32_t time, uint32_t edges,
		 struct wl_surface *surface,
		 int32_t x, int32_t y, int32_t width, int32_t height)
{

}

static const struct wl_shell_listener shell_listener = {
	handle_configure,
};


static void
drm_handle_device(void *data, struct wl_drm *drm, const char *device)
{
	SDL_WaylandData *c = data;

	c->device_name = strdup(device);
}

static void drm_handle_authenticated(void *data, struct wl_drm *drm)
{
	SDL_WaylandData *c = data;

	c->authenticated = 1;
}

static const struct wl_drm_listener drm_listener = {
	drm_handle_device,
	drm_handle_authenticated
};


static void
display_handle_global(struct wl_display *display, uint32_t id,
		      const char *interface, uint32_t version, void *data)
{
	SDL_WaylandData *d = data;

	if (strcmp(interface, "compositor") == 0) {
		d->compositor = wl_compositor_create(display, id);
	} else if (strcmp(interface, "output") == 0) {
		d->output = wl_output_create(display, id);
		wl_output_add_listener(d->output, &output_listener, d);
	} else if (strcmp(interface, "input_device") == 0) {
		//display_add_input(d, id);
	} else if (strcmp(interface, "shell") == 0) {
		d->shell = wl_shell_create(display, id);
		wl_shell_add_listener(d->shell, &shell_listener, d);
	} else if (strcmp(interface, "drm") == 0) {
		d->drm = wl_drm_create(display, id);
		wl_drm_add_listener(d->drm, &drm_listener, d);
	}
}


static int update_event_mask(uint32_t mask, void *data)
{
    SDL_WaylandData *d = data;

    d->event_mask = mask;
    printf("updated event_mask: %d\n", mask);

#if 0
    if (mask & WL_DISPLAY_READABLE)
        wl_display_iterate(d->display, WL_DISPLAY_READABLE);
#endif
    if (mask & WL_DISPLAY_WRITABLE)
        wl_display_iterate(d->display, WL_DISPLAY_WRITABLE);
    
    return 0;
}

int
Wayland_VideoInit(_THIS)
{
    SDL_WaylandData *data;
    data = malloc(sizeof *data);
    if (data == NULL)
	    return 0;

    _this->driverdata = data;
	
    data->display = wl_display_connect(NULL);
    if (data->display == NULL) {
	    fprintf(stderr, "failed to create display: %m\n");
	    return 0;
    }
    
    wl_display_add_global_listener(data->display,
			    display_handle_global, data);

    wl_display_iterate(data->display, WL_DISPLAY_READABLE);

    /*if (wayland_compositor_init_egl(c) < 0)
	    return NULL;*/
	    
    data->drm_fd = open (data->device_name, O_RDWR);
    if (data->drm_fd < 0)
    {
	    fprintf(stderr, "Failed to open drm device\n");

	    return 0;
    }

    int magic;
    if (drmGetMagic (data->drm_fd, &magic))
    {
	    fprintf(stderr, "Failed to get drm magic\n");

	    return 0;
    }
    wl_drm_authenticate (data->drm, magic);

    wl_display_iterate (data->display, WL_DISPLAY_WRITABLE);
    while (!data->authenticated)
	    wl_display_iterate (data->display, WL_DISPLAY_READABLE);


    data->event_fd = wl_display_get_fd(data->display, update_event_mask, data);

    SDL_VideoDisplay display;
    SDL_DisplayMode mode;

    /* Use a fake 32-bpp desktop mode */
    mode.format = SDL_PIXELFORMAT_RGB888;
    mode.w = 1024;
    mode.h = 768;
    mode.refresh_rate = 0;
    mode.driverdata = NULL;
    SDL_zero(display);
    display.desktop_mode = mode;
    display.current_mode = mode;
    display.driverdata = NULL;
    SDL_AddVideoDisplay(&display);

    return 0;
}

static int
Wayland_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}

void
Wayland_VideoQuit(_THIS)
{
}

/* vi: set ts=4 sw=4 expandtab: */
