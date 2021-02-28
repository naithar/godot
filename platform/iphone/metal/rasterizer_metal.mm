/*************************************************************************/
/*  rasterizer_metal.mm                                                  */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "rasterizer_metal.h"

#include "core/os/os.h"
#include "core/project_settings.h"

RasterizerStorage *RasterizerMetal::get_storage() {

	return storage;
}

RasterizerCanvas *RasterizerMetal::get_canvas() {

	return canvas;
}

RasterizerScene *RasterizerMetal::get_scene() {

	return scene;
}

Error RasterizerMetal::is_viable() {
    return OK;
}

Rasterizer *RasterizerMetal::_create_current() {

    return memnew(RasterizerMetal);
}

void RasterizerMetal::make_current() {
    _create_func = _create_current;
}
void RasterizerMetal::register_config() {
    
}

RasterizerMetal::RasterizerMetal() {

	storage = memnew(RasterizerStorageMetal);
	canvas = memnew(RasterizerCanvasMetal);
	scene = memnew(RasterizerSceneMetal);
//	canvas->storage = storage;
//	canvas->scene_render = scene;
//	storage->canvas = canvas;
//	scene->storage = storage;
//	storage->scene = scene;

	time_total = 0;
	time_scale = 1;
}

RasterizerMetal::~RasterizerMetal() {

	memdelete(storage);
	memdelete(canvas);
	memdelete(scene);
}
