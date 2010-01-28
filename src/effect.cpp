/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Contact:
 *   Moonlight List (moonlight-list@lists.ximian.com)
 *
 * Copyright 2009 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

#include <config.h>

#include <cairo.h>
#include <glib.h>

#include "effect.h"
#include "eventargs.h"

#ifdef USE_GALLIUM
#undef CLAMP

#include "pipe/internal/p_winsys_screen.h"/* port to just p_screen */
#include "pipe/p_format.h"
#include "pipe/p_context.h"
#include "pipe/p_inlines.h"
#include "softpipe/sp_winsys.h"
#include "cso_cache/cso_context.h"
#include "pipe/p_state.h"
#include "pipe/p_shader_tokens.h"
#include "pipe/p_inlines.h"
#include "util/u_draw_quad.h"
#include "util/u_memory.h"
#include "util/u_math.h"
#include "util/u_simple_shaders.h"
#include "tgsi/tgsi_text.h"
#include "trace/tr_screen.h"
#include "trace/tr_context.h"

struct pipe_screen;
struct pipe_context;

struct st_winsys
{
	struct pipe_screen  *(*screen_create)  (void);
	struct pipe_context *(*context_create) (struct pipe_screen *screen);
};

struct st_softpipe_buffer
{
	struct pipe_buffer base;
	boolean userBuffer;  /** Is this a user-space buffer? */
	void *data;
	void *mapped;
};

struct cso_context;
struct pipe_screen;
struct pipe_context;
struct st_winsys;

struct st_surface
{
	struct pipe_texture *texture;
	unsigned face;
	unsigned level;
	unsigned zslice;
};

struct st_context {
	struct pipe_reference reference;

	struct st_device *st_dev;

	struct pipe_context *pipe;

	struct cso_context *cso;

	void *vs;
	void *fs;

	struct pipe_texture *default_texture;
	struct pipe_texture *sampler_textures[PIPE_MAX_SAMPLERS];

	unsigned num_vertex_buffers;
	struct pipe_vertex_buffer vertex_buffers[PIPE_MAX_ATTRIBS];

	unsigned num_vertex_elements;
	struct pipe_vertex_element vertex_elements[PIPE_MAX_ATTRIBS];

	struct pipe_framebuffer_state framebuffer;
};

struct st_device {
	struct pipe_reference reference;

	const struct st_winsys *st_ws;

	struct pipe_screen *screen;
};

static struct pipe_surface *
st_pipe_surface (struct st_surface *surface, unsigned usage)
{
	struct pipe_texture *texture = surface->texture;
	struct pipe_screen *screen = texture->screen;

	return screen->get_tex_surface (screen, texture, surface->face,
					surface->level, surface->zslice,
					usage);
}

static void
st_device_really_destroy (struct st_device *st_dev)
{
	if (st_dev->screen)
		st_dev->screen->destroy (st_dev->screen);

	FREE (st_dev);
}

static void
st_device_reference (struct st_device **ptr, struct st_device *st_dev)
{
	struct st_device *old_dev = *ptr;

	if (pipe_reference (&(*ptr)->reference, &st_dev->reference))
		st_device_really_destroy (old_dev);
	*ptr = st_dev;
}

static struct st_device *
st_device_create_from_st_winsys (const struct st_winsys *st_ws)
{
	struct st_device *st_dev;

	if (!st_ws->screen_create || !st_ws->context_create)
		return NULL;

	st_dev = CALLOC_STRUCT (st_device);
	if (!st_dev)
		return NULL;

	pipe_reference_init (&st_dev->reference, 1);
	st_dev->st_ws = st_ws;

	st_dev->screen = st_ws->screen_create ();
	if (!st_dev->screen) {
		st_device_reference (&st_dev, NULL);
		return NULL;
	}

	return st_dev;
}

static void
st_context_really_destroy (struct st_context *st_ctx)
{
	unsigned i;

	if (st_ctx) {
		struct st_device *st_dev = st_ctx->st_dev;

		if (st_ctx->cso) {
			cso_delete_vertex_shader (st_ctx->cso, st_ctx->vs);
			cso_delete_fragment_shader (st_ctx->cso, st_ctx->fs);

			cso_destroy_context (st_ctx->cso);
		}

		if (st_ctx->pipe)
			st_ctx->pipe->destroy (st_ctx->pipe);

		for(i = 0; i < PIPE_MAX_SAMPLERS; ++i)
			pipe_texture_reference (&st_ctx->sampler_textures[i], NULL);
		pipe_texture_reference (&st_ctx->default_texture, NULL);

		FREE (st_ctx);

		st_device_reference (&st_dev, NULL);
	}
}

static struct st_context *
st_context_create (struct st_device *st_dev)
{
	struct st_context *st_ctx;

	st_ctx = CALLOC_STRUCT (st_context);
	if (!st_ctx)
		return NULL;

	pipe_reference_init (&st_ctx->reference, 1);

	st_device_reference (&st_ctx->st_dev, st_dev);

	st_ctx->pipe = st_dev->st_ws->context_create (st_dev->screen);
	if (!st_ctx->pipe) {
		st_context_really_destroy (st_ctx);
		return NULL;
	}

	st_ctx->cso = cso_create_context (st_ctx->pipe);
	if (!st_ctx->cso) {
		st_context_really_destroy (st_ctx);
		return NULL;
	}

	/* disable blending/masking */
	{
		struct pipe_blend_state blend;
		memset(&blend, 0, sizeof(blend));
		blend.rgb_src_factor = PIPE_BLENDFACTOR_ONE;
		blend.alpha_src_factor = PIPE_BLENDFACTOR_ONE;
		blend.rgb_dst_factor = PIPE_BLENDFACTOR_ZERO;
		blend.alpha_dst_factor = PIPE_BLENDFACTOR_ZERO;
		blend.colormask = PIPE_MASK_RGBA;
		cso_set_blend(st_ctx->cso, &blend);
	}

	/* no-op depth/stencil/alpha */
	{
		struct pipe_depth_stencil_alpha_state depthstencil;
		memset(&depthstencil, 0, sizeof(depthstencil));
		cso_set_depth_stencil_alpha(st_ctx->cso, &depthstencil);
	}

	/* rasterizer */
	{
		struct pipe_rasterizer_state rasterizer;
		memset(&rasterizer, 0, sizeof(rasterizer));
		rasterizer.front_winding = PIPE_WINDING_CW;
		rasterizer.cull_mode = PIPE_WINDING_NONE;
		cso_set_rasterizer(st_ctx->cso, &rasterizer);
	}

	/* clip */
	{
		struct pipe_clip_state clip;
		memset(&clip, 0, sizeof(clip));
		st_ctx->pipe->set_clip_state(st_ctx->pipe, &clip);
	}

	/* identity viewport */
	{
		struct pipe_viewport_state viewport;
		viewport.scale[0] = 1.0;
		viewport.scale[1] = 1.0;
		viewport.scale[2] = 1.0;
		viewport.scale[3] = 1.0;
		viewport.translate[0] = 0.0;
		viewport.translate[1] = 0.0;
		viewport.translate[2] = 0.0;
		viewport.translate[3] = 0.0;
		cso_set_viewport(st_ctx->cso, &viewport);
	}

	/* samplers */
	{
		struct pipe_sampler_state sampler;
		unsigned i;
		memset(&sampler, 0, sizeof(sampler));
		sampler.wrap_s = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
		sampler.wrap_t = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
		sampler.wrap_r = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
		sampler.min_mip_filter = PIPE_TEX_MIPFILTER_NEAREST;
		sampler.min_img_filter = PIPE_TEX_MIPFILTER_NEAREST;
		sampler.mag_img_filter = PIPE_TEX_MIPFILTER_NEAREST;
		sampler.normalized_coords = 1;
		for (i = 0; i < PIPE_MAX_SAMPLERS; i++)
			cso_single_sampler(st_ctx->cso, i, &sampler);
		cso_single_sampler_done(st_ctx->cso);
	}

	/* default textures */
	{
		struct pipe_screen *screen = st_dev->screen;
		struct pipe_texture templat;
		struct pipe_transfer *transfer;
		unsigned i;

		memset( &templat, 0, sizeof( templat ) );
		templat.target = PIPE_TEXTURE_2D;
		templat.format = PIPE_FORMAT_A8R8G8B8_UNORM;
		templat.width0 = 1;
		templat.height0 = 1;
		templat.depth0 = 1;
		templat.last_level = 0;

		st_ctx->default_texture = screen->texture_create( screen, &templat );
		if(st_ctx->default_texture) {
			transfer = screen->get_tex_transfer(screen,
							    st_ctx->default_texture,
							    0, 0, 0,
							    PIPE_TRANSFER_WRITE,
							    0, 0,
							    st_ctx->default_texture->width0,
							    st_ctx->default_texture->height0);
			if (transfer) {
				uint32_t *map;
				map = (uint32_t *) screen->transfer_map(screen, transfer);
				if(map) {
					*map = 0x00000000;
					screen->transfer_unmap(screen, transfer);
				}
				screen->tex_transfer_destroy(transfer);
			}
		}

		for (i = 0; i < PIPE_MAX_SAMPLERS; i++)
			pipe_texture_reference(&st_ctx->sampler_textures[i], st_ctx->default_texture);

		cso_set_sampler_textures(st_ctx->cso, PIPE_MAX_SAMPLERS, st_ctx->sampler_textures);
	}

	/* vertex shader */
	{
		const uint semantic_names[] = { TGSI_SEMANTIC_POSITION,
						TGSI_SEMANTIC_GENERIC };
		const uint semantic_indexes[] = { 0, 0 };

		st_ctx->vs = util_make_vertex_passthrough_shader (st_ctx->pipe,
								  2,
								  semantic_names,
								  semantic_indexes);
		cso_set_vertex_shader_handle (st_ctx->cso, st_ctx->vs);
	}

	/* fragment shader */
	{
		st_ctx->fs = util_make_fragment_tex_shader (st_ctx->pipe);
		cso_set_fragment_shader_handle (st_ctx->cso, st_ctx->fs);
	}

	return st_ctx;
}

static void
st_context_reference (struct st_context **ptr, struct st_context *st_ctx)
{
	struct st_context *old_ctx = *ptr;

	if (pipe_reference (&(*ptr)->reference, &st_ctx->reference))
		st_context_really_destroy (old_ctx);
	*ptr = st_ctx;
}

static struct st_softpipe_buffer *
st_softpipe_buffer (struct pipe_buffer *buf)
{
	return (struct st_softpipe_buffer *) buf;
}

static void *
st_softpipe_buffer_map (struct pipe_winsys *winsys,
			struct pipe_buffer *buf,
			unsigned flags)
{
	struct st_softpipe_buffer *st_softpipe_buf = st_softpipe_buffer (buf);
	st_softpipe_buf->mapped = st_softpipe_buf->data;
	return st_softpipe_buf->mapped;
}

static void
st_softpipe_buffer_unmap (struct pipe_winsys *winsys,
			  struct pipe_buffer *buf)
{
	struct st_softpipe_buffer *st_softpipe_buf = st_softpipe_buffer (buf);
	st_softpipe_buf->mapped = NULL;
}

static void
st_softpipe_buffer_destroy (struct pipe_buffer *buf)
{
	struct st_softpipe_buffer *oldBuf = st_softpipe_buffer(buf);

	if (oldBuf->data) {
		if (!oldBuf->userBuffer)
			align_free (oldBuf->data);

		oldBuf->data = NULL;
	}

	FREE (oldBuf);
}

static void
st_softpipe_flush_frontbuffer (struct pipe_winsys *winsys,
			       struct pipe_surface *surf,
			       void *context_private)
{
}

static const char *
st_softpipe_get_name (struct pipe_winsys *winsys)
{
	return "moon-softpipe";
}

static struct pipe_buffer *
st_softpipe_buffer_create (struct pipe_winsys *winsys,
			   unsigned alignment,
			   unsigned usage,
			   unsigned size)
{
	struct st_softpipe_buffer *buffer = CALLOC_STRUCT (st_softpipe_buffer);

	pipe_reference_init (&buffer->base.reference, 1);
	buffer->base.alignment = alignment;
	buffer->base.usage = usage;
	buffer->base.size = size;

	buffer->data = align_malloc (size, alignment);

	return &buffer->base;
}

/**
 * Create buffer which wraps user-space data.
 */
static struct pipe_buffer *
st_softpipe_user_buffer_create (struct pipe_winsys *winsys,
				void *ptr,
				unsigned bytes)
{
	struct st_softpipe_buffer *buffer;

	buffer = CALLOC_STRUCT (st_softpipe_buffer);
	if (!buffer)
		return NULL;

	pipe_reference_init (&buffer->base.reference, 1);
	buffer->base.size = bytes;
	buffer->userBuffer = TRUE;
	buffer->data = ptr;

	return &buffer->base;
}

static void *surface_data = NULL;
static unsigned surface_stride = 0;

static struct pipe_buffer *
st_softpipe_surface_buffer_create (struct pipe_winsys *winsys,
				   unsigned width, unsigned height,
				   enum pipe_format format,
				   unsigned usage,
				   unsigned tex_usage,
				   unsigned *stride)
{
	const unsigned alignment = 64;
	unsigned nblocksy;

	nblocksy = pf_get_nblocksy (format, height);

	if (surface_data && surface_stride)
	{
		*stride = surface_stride;
		return winsys->user_buffer_create (winsys,
						   surface_data,
						   *stride * nblocksy);
	}
	else
	{
		*stride = align (pf_get_stride (format, width), alignment);
		return winsys->buffer_create (winsys, alignment,
					      usage,
					      *stride * nblocksy);
	}
}

static void
st_softpipe_fence_reference (struct pipe_winsys *winsys,
			     struct pipe_fence_handle **ptr,
			     struct pipe_fence_handle *fence)
{
}

static int
st_softpipe_fence_signalled (struct pipe_winsys *winsys,
			     struct pipe_fence_handle *fence,
			     unsigned flag)
{
	return 0;
}

static int
st_softpipe_fence_finish (struct pipe_winsys *winsys,
			  struct pipe_fence_handle *fence,
			  unsigned flag)
{
	return 0;
}

static void
st_softpipe_destroy (struct pipe_winsys *winsys)
{
	FREE (winsys);
}

static struct pipe_screen *
st_softpipe_screen_create (void)
{
	static struct pipe_winsys *winsys;
	struct pipe_screen *screen;

	winsys = CALLOC_STRUCT (pipe_winsys);
	if (!winsys)
		return NULL;

	winsys->destroy = st_softpipe_destroy;

	winsys->buffer_create = st_softpipe_buffer_create;
	winsys->user_buffer_create = st_softpipe_user_buffer_create;
	winsys->buffer_map = st_softpipe_buffer_map;
	winsys->buffer_unmap = st_softpipe_buffer_unmap;
	winsys->buffer_destroy = st_softpipe_buffer_destroy;

	winsys->surface_buffer_create = st_softpipe_surface_buffer_create;

	winsys->fence_reference = st_softpipe_fence_reference;
	winsys->fence_signalled = st_softpipe_fence_signalled;
	winsys->fence_finish = st_softpipe_fence_finish;

	winsys->flush_frontbuffer = st_softpipe_flush_frontbuffer;
	winsys->get_name = st_softpipe_get_name;

	screen = softpipe_create_screen (winsys);
	if (!screen)
		st_softpipe_destroy (winsys);

	return screen;
}

static struct pipe_context *
st_softpipe_context_create (struct pipe_screen *screen)
{
	return softpipe_create (screen);
}

const struct st_winsys st_softpipe_winsys = {
	&st_softpipe_screen_create,
	&st_softpipe_context_create
};
#endif

struct st_context *Effect::st_context;

void
Effect::Initialize ()
{

#ifdef USE_GALLIUM
	struct st_device *dev;
	dev = st_device_create_from_st_winsys (&st_softpipe_winsys);
	st_context = st_context_create (dev);
	st_device_reference (&dev, NULL);
#endif

}

void
Effect::Shutdown ()
{

#ifdef USE_GALLIUM
	st_context_reference (&st_context, NULL);
#endif

}

Effect::Effect ()
{
	SetObjectType (Type::EFFECT);

	vs = fs = NULL;
	constants = NULL;
}

Effect::~Effect ()
{

#ifdef USE_GALLIUM
	struct st_context *ctx = st_context;

	if (fs)
		ctx->pipe->delete_fs_state (ctx->pipe, fs);
	if (vs)
		ctx->pipe->delete_vs_state (ctx->pipe, vs);

	if (constants)
		pipe_buffer_reference (&constants, NULL);
#endif

}

double
Effect::GetPaddingTop ()
{
	return 0.0;
}

double
Effect::GetPaddingBottom ()
{
	return 0.0;
}

double
Effect::GetPaddingLeft ()
{
	return 0.0;
}

double
Effect::GetPaddingRight ()
{
	return 0.0;
}

bool
Effect::Composite (cairo_t         *cr,
		   cairo_surface_t *dst,
		   cairo_surface_t *src,
		   int             src_x,
		   int             src_y,
		   int             x,
		   int             y,
		   unsigned int    width,
		   unsigned int    height)
{

#ifdef USE_GALLIUM
	struct st_context *ctx = st_context;

	if (cairo_surface_get_type (dst) != CAIRO_SURFACE_TYPE_IMAGE ||
	    cairo_surface_get_type (src) != CAIRO_SURFACE_TYPE_IMAGE)
		return 0;

	MaybeUpdateShader ();

	int dst_width  = cairo_image_surface_get_width (dst);
	int dst_height = cairo_image_surface_get_height (dst);
	int src_width  = cairo_image_surface_get_width (src);
	int src_height = cairo_image_surface_get_height (src);

	struct pipe_blend_state blend;
	memset(&blend, 0, sizeof(struct pipe_blend_state));
	blend.blend_enable = 1;
	blend.colormask |= PIPE_MASK_RGBA;
	blend.rgb_src_factor = PIPE_BLENDFACTOR_ONE;
	blend.alpha_src_factor = PIPE_BLENDFACTOR_ONE;
	blend.rgb_dst_factor = PIPE_BLENDFACTOR_INV_SRC_ALPHA;
	blend.alpha_dst_factor = PIPE_BLENDFACTOR_INV_SRC_ALPHA;
	cso_set_blend(ctx->cso, &blend);

	struct pipe_depth_stencil_alpha_state dsa;
	memset(&dsa, 0, sizeof(struct pipe_depth_stencil_alpha_state));
	cso_set_depth_stencil_alpha(ctx->cso, &dsa);

	struct pipe_rasterizer_state raster;
	memset(&raster, 0, sizeof(struct pipe_rasterizer_state));
	raster.front_winding = PIPE_WINDING_CW;
	raster.cull_mode = PIPE_WINDING_NONE;
	raster.scissor = 1;
	cso_set_rasterizer(ctx->cso, &raster);

	struct pipe_viewport_state viewport;
	memset(&viewport, 0, sizeof(struct pipe_viewport_state));
	viewport.scale[0] =  dst_width / 2.f;
	viewport.scale[1] =  dst_height / 2.f;
	viewport.scale[2] =  1.0;
	viewport.scale[3] =  1.0;
	viewport.translate[0] = dst_width / 2.f;
	viewport.translate[1] = dst_height / 2.f;
	viewport.translate[2] = 0.0;
	viewport.translate[3] = 0.0;
	cso_set_viewport(ctx->cso, &viewport);

	struct pipe_sampler_state sampler;
	memset(&sampler, 0, sizeof(struct pipe_sampler_state));
	sampler.wrap_s = PIPE_TEX_WRAP_CLAMP_TO_BORDER;
	sampler.wrap_t = PIPE_TEX_WRAP_CLAMP_TO_BORDER;
	sampler.wrap_r = PIPE_TEX_WRAP_CLAMP_TO_BORDER;
	sampler.min_mip_filter = PIPE_TEX_MIPFILTER_NONE;
	sampler.min_img_filter = PIPE_TEX_MIPFILTER_NEAREST;
	sampler.mag_img_filter = PIPE_TEX_MIPFILTER_NEAREST;
	sampler.normalized_coords = 0;
	cso_single_sampler(ctx->cso, 0, &sampler);
	cso_single_sampler_done(ctx->cso);

	struct pipe_scissor_state scissor;
	memset(&scissor, 0, sizeof(struct pipe_scissor_state));
	scissor.minx = 0;
	scissor.miny = 0;
	scissor.maxx = dst_width;
	scissor.maxy = dst_height;
	ctx->pipe->set_scissor_state(ctx->pipe, &scissor);

	struct pipe_clip_state clip;
	memset(&clip, 0, sizeof(struct pipe_clip_state));
	clip.nr = 0;
	ctx->pipe->set_clip_state(ctx->pipe, &clip);

	struct pipe_texture templat;

	struct pipe_surface *csurface;
	struct st_surface cbuf;
	memset(&templat, 0, sizeof(templat));
	templat.format = PIPE_FORMAT_A8R8G8B8_UNORM;
	templat.width0 = dst_width;
	templat.height0 = dst_height;
	templat.depth0 = 1;
	templat.last_level = 0;
	templat.target = PIPE_TEXTURE_2D;
	templat.tex_usage = PIPE_TEXTURE_USAGE_DISPLAY_TARGET;

	surface_data = cairo_image_surface_get_data (dst);
	surface_stride = cairo_image_surface_get_stride (dst);

	cbuf.texture = ctx->st_dev->screen->texture_create (ctx->st_dev->screen, &templat);
	cbuf.face = 0;
	cbuf.level = 0;
	cbuf.zslice = 0;
	csurface = st_pipe_surface(&cbuf, PIPE_BUFFER_USAGE_GPU_WRITE | PIPE_BUFFER_USAGE_GPU_READ);

	struct pipe_texture *texture;
	memset(&templat, 0, sizeof(templat));
	templat.format = PIPE_FORMAT_A8R8G8B8_UNORM;
	templat.width0 = src_width;
	templat.height0 = src_height;
	templat.depth0 = 1;
	templat.last_level = 0;
	templat.target = PIPE_TEXTURE_2D;
	templat.tex_usage = PIPE_TEXTURE_USAGE_SAMPLER | PIPE_TEXTURE_USAGE_DISPLAY_TARGET;

	surface_data = cairo_image_surface_get_data (src);
	surface_stride = cairo_image_surface_get_stride (src);

	texture = ctx->st_dev->screen->texture_create (ctx->st_dev->screen, &templat);
	cso_set_sampler_textures( ctx->cso, 1, &texture );

	struct pipe_framebuffer_state fb;
	memset(&fb, 0, sizeof(struct pipe_framebuffer_state));
	fb.width = dst_width;
	fb.height = dst_height;
	fb.nr_cbufs = 1;
	fb.cbufs[0] = csurface;
	memcpy(&ctx->framebuffer, &fb, sizeof(struct pipe_framebuffer_state));
	cso_set_framebuffer(ctx->cso, &fb);

	if (vs && cso_set_vertex_shader_handle (ctx->cso, vs) != PIPE_OK)
		g_warning ("set vertext shader failed\n");
	if (fs && cso_set_fragment_shader_handle (ctx->cso, fs) != PIPE_OK)
		g_warning ("set fragment shader failed\n");

	struct pipe_constant_buffer kbuf;

	if (constants)
	{
		kbuf.buffer = constants;
		ctx->pipe->set_constant_buffer (ctx->pipe,
						PIPE_SHADER_FRAGMENT,
						0, &kbuf);
	}

	struct pipe_buffer* vbuf;

	vbuf = pipe_buffer_create (ctx->pipe->screen, 32, PIPE_BUFFER_USAGE_VERTEX,
				   sizeof (float) * 4 * 2 * 4);
	if (vbuf)
	{
		float *verts;

		verts = (float *) pipe_buffer_map (ctx->pipe->screen, vbuf,
						   PIPE_BUFFER_USAGE_CPU_WRITE);
		if (verts)
		{
			double x1 = (2.0 / dst_width)  * x - 1.0;
			double y1 = (2.0 / dst_height) * y - 1.0;
			double x2 = (2.0 / dst_width)  * (x + width)  - 1.0;
			double y2 = (2.0 / dst_height) * (y + height) - 1.0;

			double s1 = src_x + 0.5;
			double t1 = src_y + 0.5;
			double s2 = src_x + width  + 0.5;
			double t2 = src_y + height + 0.5;

			verts[ 0] =    x1; // x1
			verts[ 1] =    y2; // y1
			verts[ 2] =   0.0; // z1
			verts[ 3] =   1.0; // w1
			verts[ 4] =    s1; // s1
			verts[ 5] =    t2; // t1
			verts[ 6] =   0.0;
			verts[ 7] =   0.0;
			verts[ 8] =    x1; // x2
			verts[ 9] =    y1; // y2
			verts[10] =   0.0; // z2
			verts[11] =   1.0; // w2
			verts[12] =    s1; // s2
			verts[13] =    t1; // t2
			verts[14] =   0.0;
			verts[15] =   0.0;
			verts[16] =    x2; // x3
			verts[17] =    y1; // y3
			verts[18] =   0.0; // z3
			verts[19] =   1.0; // w3
			verts[20] =    s2; // s3
			verts[21] =    t1; // t3
			verts[22] =   0.0;
			verts[23] =   0.0;
			verts[24] =    x2; // x4
			verts[25] =    y2; // y4
			verts[26] =   0.0; // z4
			verts[27] =   1.0; // w4
			verts[28] =    s2; // s4
			verts[29] =    t2; // t4
			verts[30] =   0.0;
			verts[31] =   0.0;

			pipe_buffer_unmap (ctx->pipe->screen, vbuf);

			util_draw_vertex_buffer (ctx->pipe, vbuf, 0, PIPE_PRIM_QUADS, 4, 2);
		}

		pipe_buffer_reference (&vbuf, NULL);
	}

	struct pipe_fence_handle *fence = NULL;
	ctx->pipe->flush (ctx->pipe, PIPE_FLUSH_RENDER_CACHE, &fence);
	if (fence)
	{
		/* TODO: allow asynchronous operation */
		ctx->pipe->screen->fence_finish (ctx->pipe->screen, fence, 0);
		ctx->pipe->screen->fence_reference (ctx->pipe->screen, &fence, NULL);
	}

	ctx->pipe->set_constant_buffer (ctx->pipe,
					PIPE_SHADER_FRAGMENT,
					0, NULL);

	cso_set_fragment_shader_handle (ctx->cso, ctx->fs);
	cso_set_vertex_shader_handle (ctx->cso, ctx->vs);

	cso_set_sampler_textures (ctx->cso, 0, NULL);

	memset (&fb, 0, sizeof (struct pipe_framebuffer_state));
	memcpy (&ctx->framebuffer, &fb, sizeof (struct pipe_framebuffer_state));
	cso_set_framebuffer (ctx->cso, &fb);

	ctx->st_dev->screen->texture_destroy (texture);
	ctx->st_dev->screen->tex_surface_destroy (csurface);
	ctx->st_dev->screen->texture_destroy (cbuf.texture);

	return 1;
#else
	return 0;
#endif

}

void
Effect::OnPropertyChanged (PropertyChangedEventArgs *args, MoonError *error)
{
	if (args->GetProperty ()->GetOwnerType() == Type::DEPENDENCY_OBJECT) {
		DependencyObject::OnPropertyChanged (args, error);
		return;
	}

	need_update = true;
	//
	// If the effect changes, we need to notify our owners
	// that they must repaint (all of our properties have
	// a visible effect.
	//
	NotifyListenersOfPropertyChange (args, error);
}

void
Effect::UpdateShader ()
{
	g_warning ("Effect::UpdateShader has been called. The derived class should have overridden it.");
}

void
Effect::MaybeUpdateShader ()
{
	if (need_update) {
		UpdateShader ();
		need_update = false;
	}
}

BlurEffect::BlurEffect ()
{
	SetObjectType (Type::BLUREFFECT);
}

double
BlurEffect::GetPaddingTop ()
{
	return GetRadius ();
}

double
BlurEffect::GetPaddingBottom ()
{
	return GetRadius ();
}

double
BlurEffect::GetPaddingLeft ()
{
	return GetRadius ();
}

double
BlurEffect::GetPaddingRight ()
{
	return GetRadius ();
}

Rect
BlurEffect::GrowDirtyRectangle (Rect bounds, Rect rect)
{
	return rect.GrowBy (GetRadius ());
}

void
BlurEffect::UpdateShader ()
{

#ifdef USE_GALLIUM
	struct st_context *ctx = st_context;

	double radius = GetRadius ();
	double sigma = radius / 2.0;
	double alpha = radius;
	double scale, xy_scale;
	int    size, half_size;

	if (constants)
		pipe_buffer_reference (&constants, NULL);

	if (fs) {
		ctx->pipe->delete_fs_state (ctx->pipe, fs);
		fs = NULL;
	}

	if (vs) {
		ctx->pipe->delete_vs_state (ctx->pipe, vs);
		vs = NULL;
	}

	scale = 1.0f / (2.0f * M_PI * sigma * sigma);
	half_size = alpha + 0.5f;

	if (half_size == 0)
		half_size = 1;

	size = half_size * 2 + 1;
	xy_scale = 2.0f * radius / size;

	if (size < 3)
		return;

	struct tgsi_token *tokens;

	const char vs_convolution_asm[] =
		"VERT\n"
		"DCL IN[0]\n"
		"DCL IN[1]\n"
		"DCL OUT[0], POSITION\n"
		"DCL OUT[1], GENERIC\n"
		"0: MOV OUT[0], IN[0]\n"
		"1: MOV OUT[1], IN[1]\n"
		"2: END\n";

	tokens = (struct tgsi_token *) g_malloc (sizeof (struct tgsi_token) * 1024);

	if (tgsi_text_translate (vs_convolution_asm, tokens, 1024))
	{
		struct pipe_shader_state state;
		memset (&state, 0, sizeof (struct pipe_shader_state));
		state.tokens = tokens;

		vs = ctx->pipe->create_vs_state (ctx->pipe, &state);
	}

	g_free (tokens);

	const char fs_convolution_asm[] =
			"FRAG\n"
			"DCL IN[0], GENERIC[0], PERSPECTIVE\n"
			"DCL OUT[0], COLOR, CONSTANT\n"
			"DCL TEMP[0..4], CONSTANT\n"
			"DCL ADDR[0], CONSTANT\n"
			"DCL CONST[0..%d], CONSTANT\n"
			"DCL SAMP[0], CONSTANT\n"
			"0: MOV TEMP[0], CONST[0].xxxx\n"
			"1: MOV TEMP[1], CONST[0].xxxx\n"
			"2: BGNLOOP :14\n"
			"3: SGE TEMP[0].z, TEMP[0].yyyy, CONST[0].wwww\n"
			"4: IF TEMP[0].zzzz :7\n"
			"5: BRK\n"
			"6: ENDIF\n"
			"7: ARL ADDR[0].x, TEMP[0].yyyy\n"
			"8: MOV TEMP[3], CONST[ADDR[0]+1]\n"
			"9: ADD TEMP[4].xy, IN[0], TEMP[3]\n"
			"10: TEX TEMP[2], TEMP[4], SAMP[0], 2D\n"
			"11: MOV TEMP[3], CONST[ADDR[0]+%d]\n"
			"12: MAD TEMP[1], TEMP[2], TEMP[3], TEMP[1]\n"
			"13: ADD TEMP[0].y, TEMP[0].yyyy, CONST[0].yyyy\n"
			"14: ENDLOOP :2\n"
			"15: MOV OUT[0], TEMP[1]\n"
			"16: END\n";
	gchar *text;

	text = g_strdup_printf (fs_convolution_asm,
				1 + 2 * size * size,
				1 + size * size);

	tokens = (struct tgsi_token *) g_malloc (sizeof (struct tgsi_token) * 1024);

	if (tgsi_text_translate (text, tokens, 1024))
	{
		struct pipe_shader_state state;
		memset (&state, 0, sizeof (struct pipe_shader_state));
		state.tokens = tokens;

		fs = ctx->pipe->create_fs_state (ctx->pipe, &state);
	}

	g_free (tokens);
	g_free (text);

	constants = pipe_buffer_create (ctx->pipe->screen, 16,
					PIPE_BUFFER_USAGE_CONSTANT,
					sizeof (float) * (4 + 4 * 2 * size * size));

	if (constants)
	{
		float *kernel;

		kernel = (float *) pipe_buffer_map (ctx->pipe->screen,
						    constants,
						    PIPE_BUFFER_USAGE_CPU_WRITE);
		if (kernel)
		{
			double sum = 0.0;
			int    idx, i, j;

			kernel[0] = 0.f;
			kernel[1] = 1.f;
			kernel[2] = 0.f;
			kernel[3] = size * size;

			idx = 4;
			for (j = 0; j < size; ++j) {
				for (i = 0; i < size; ++i) {
					int index = j * size + i;

					kernel[idx + index * 4 + 0] = i - (size / 2);
					kernel[idx + index * 4 + 1] = j - (size / 2);
					kernel[idx + index * 4 + 2] = 0.f;
					kernel[idx + index * 4 + 3] = 0.f;
				}
			}

			idx = 4 + 4 * size * size;
			for (j = 0; j < size; ++j) {
				double fy = xy_scale * (j - half_size);

				for (i = 0; i < size; ++i) {
					int    index = j * size + i;
					double fx = xy_scale * (i - half_size);
					double weight;

					weight = scale * exp (-((fx * fx + fy * fy) /
								(2.0f * sigma * sigma)));

					sum += weight;
					kernel[idx + index * 4] = weight;
				}
			}

			if (sum != 0.0) {
				for (j = 0; j < size; ++j) {
					for (i = 0; i < size; ++i) {
						int    index = j * size + i;
						double weight;

						weight = kernel[idx + index * 4] / sum;

						kernel[idx + index * 4 + 0] = weight;
						kernel[idx + index * 4 + 1] = weight;
						kernel[idx + index * 4 + 2] = weight;
						kernel[idx + index * 4 + 3] = weight;
					}
				}
			}

			pipe_buffer_unmap (ctx->pipe->screen, constants);
		}
	}
#endif

}

DropShadowEffect::DropShadowEffect ()
{
	SetObjectType (Type::DROPSHADOWEFFECT);
}

ShaderEffect::ShaderEffect ()
{
	SetObjectType (Type::SHADEREFFECT);
}

PixelShader::PixelShader ()
{
	SetObjectType (Type::PIXELSHADER);
}
