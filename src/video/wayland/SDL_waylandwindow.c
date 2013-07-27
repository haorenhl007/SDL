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

#include "../SDL_sysvideo.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylandvideo.h"


void Wayland_ShowWindow(_THIS, SDL_Window * window)
{
	SDL_WaylandWindow *wind = (SDL_WaylandWindow*) window->driverdata;

	wl_surface_map_toplevel(wind->surface);
	/*
	wl_surface_map(wind->surface,
		       window->x, window->y,
		       window->w, window->h);
		       */
}


int Wayland_CreateWindow(_THIS, SDL_Window * window)
{

    SDL_WaylandWindow *data;
    struct wl_visual *visual;
	int i;
	SDL_WaylandData *c;

	data = malloc(sizeof *data);
	if (data == NULL)
		return 0;
    
    c = _this->driverdata;
    window->driverdata = data;
    
    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        SDL_GL_LoadLibrary(NULL);
        window->flags &= SDL_WINDOW_OPENGL;
    }

	if (window->x == SDL_WINDOWPOS_UNDEFINED) {
		window->x = 0;
	}
	if (window->y == SDL_WINDOWPOS_UNDEFINED) {
		window->y = 0;
	}
	
	data->waylandData = c;
	data->sdlwindow = window;
    
	data->surface =
		wl_compositor_create_surface(c->compositor);
	wl_surface_set_user_data(data->surface, data);

    data->egl_surface = wl_egl_create_native_surface(data->surface,
		    				     window->w,
						     window->h);
    data->esurf =
        eglCreateWindowSurface(c->edpy, c->econf, 
                (EGLNativeWindowType) data->egl_surface, NULL);

    if (data->esurf == EGL_NO_SURFACE) {
	    fprintf(stderr, "failed to create window surface\n");
	    return -1;
    }
    printf("created window\n");

    return 0;
}

void Wayland_DestroyWindow(_THIS, SDL_Window * window)
{
	SDL_WaylandData *data = (SDL_WaylandData *) _this->driverdata;
	SDL_WaylandWindow *wind = (SDL_WaylandWindow *) window->driverdata;

	window->driverdata = NULL;
	int i;

	if (data) {
		eglDestroySurface(data->edpy, wind->esurf);
		wl_egl_destroy_native_surface(wind->egl_surface);
		wl_surface_destroy(wind->surface);
		SDL_free(wind);
	}
	printf("destroyed window\n");
}
