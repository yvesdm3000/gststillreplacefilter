/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2019 Yves De Muyter <yves@alfavisio.be>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-stillreplacefilter
 *
 * Replace still image on the source with another still image.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! stillreplacefilter ! fakesink silent=FALSE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "gststillreplacefilter.h"

GST_DEBUG_CATEGORY_STATIC (stillreplacefilter_debug);
#define GST_CAT_DEFAULT stillreplacefilter_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_COMPARELINES,
  PROP_PSNR,
  PROP_RESAMPLE
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
#define VIDEO_STILLREPLACE_CAPS                     \
  GST_VIDEO_CAPS_MAKE ("{ RGBx, xRGB, BGRx, xBGR, " \
                       "RGBA, ARGB, BGRA, ABGR, RGB, BGR }")

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_STILLREPLACE_CAPS)
    );

static GstStaticPadTemplate replacesink_factory = GST_STATIC_PAD_TEMPLATE ("replacesink",
    GST_PAD_SINK,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (VIDEO_STILLREPLACE_CAPS)
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_STILLREPLACE_CAPS)
    );

#define stillreplacefilter_parent_class parent_class
G_DEFINE_TYPE (GstStillReplaceFilter, stillreplacefilter, GST_TYPE_ELEMENT);

static void stillreplacefilter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void stillreplacefilter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean stillreplacefilter_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn stillreplacefilter_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

static gboolean stillreplacefilter_replacepad_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn stillreplacefilter_replacepad_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

/* GObject vmethod implementations */
static void stillreplacefilter_finalize (GObject * object);

/* initialize the stillreplacefilter's class */
static void
stillreplacefilter_class_init (GstStillReplaceFilterClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = stillreplacefilter_set_property;
  gobject_class->get_property = stillreplacefilter_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          TRUE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_COMPARELINES,
      g_param_spec_uint ("compare_lines", "Compare lines", "Amount of lines to compare",
          0, 32000, 0, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_PSNR,
      g_param_spec_uint ("psnr", "Peak Signal to Noise Ratio", "Peak Signal to Noise Ratio: Higher value: less allowed difference [0-255].",
          0, 255, 100, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_RESAMPLE,
      g_param_spec_boolean ("resample", "Resample", "Resample reference image from input",
          TRUE, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | GST_PARAM_MUTABLE_PLAYING));

  gst_element_class_set_details_simple(gstelement_class,
    "Still replace filter",
    "Effect/Video",
    "Replaces a specific still image with another",
    "Yves De Muyter <yves@alfavisio.be>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  gobject_class->finalize = GST_DEBUG_FUNCPTR( stillreplacefilter_finalize );
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
stillreplacefilter_init (GstStillReplaceFilter * filter)
{
  g_mutex_init (&filter->replacesinkMutex);
  g_cond_init (&filter->replacesinkEvent);

  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(stillreplacefilter_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(stillreplacefilter_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->replacesinkpad = gst_pad_new_from_static_template (&replacesink_factory, "replacesink");
  gst_pad_set_event_function (filter->replacesinkpad,
                              GST_DEBUG_FUNCPTR(stillreplacefilter_replacepad_sink_event));
  gst_pad_set_chain_function (filter->replacesinkpad,
                              GST_DEBUG_FUNCPTR(stillreplacefilter_replacepad_chain));
  GST_PAD_SET_PROXY_CAPS (filter->replacesinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->replacesinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->eos = FALSE;
  filter->flushing = FALSE;

  filter->silent = TRUE;
  filter->compare_lines = 0;
  filter->refImageBuffer = NULL;
  filter->nextReplaceBuffer = NULL;
}
static void
stillreplacefilter_finalize (GObject * object)
{
  GstStillReplaceFilter *filter = GST_STILLREPLACEFILTER (object);
  if (filter->refImageBuffer)
    gst_buffer_replace( &filter->refImageBuffer, NULL );
  if (filter->nextReplaceBuffer)
    gst_buffer_replace( &filter->nextReplaceBuffer, NULL );
  g_cond_clear (&filter->replacesinkEvent);
  g_mutex_clear (&filter->replacesinkMutex);
}

static void
stillreplacefilter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstStillReplaceFilter *filter = GST_STILLREPLACEFILTER (object);

  switch (prop_id)
  {
    case PROP_SILENT:
      GST_OBJECT_LOCK(filter);
      filter->silent = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK(filter);
      break;
    case PROP_COMPARELINES:
      GST_OBJECT_LOCK(filter);
      filter->compare_lines = g_value_get_uint (value);
      GST_OBJECT_UNLOCK(filter);
      break;
    case PROP_PSNR:
      GST_OBJECT_LOCK(filter);
      filter->psnr = g_value_get_uint (value);
      GST_OBJECT_UNLOCK(filter);
      break;
    case PROP_RESAMPLE:
      gst_buffer_replace( &filter->refImageBuffer, NULL );
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
stillreplacefilter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstStillReplaceFilter *filter = GST_STILLREPLACEFILTER (object);

  switch (prop_id) {
    case PROP_SILENT:
      GST_OBJECT_LOCK(filter);
      g_value_set_boolean (value, filter->silent);
      GST_OBJECT_UNLOCK(filter);
      break;
    case PROP_COMPARELINES:
      GST_OBJECT_LOCK(filter);
      g_value_set_uint (value, filter->compare_lines);
      GST_OBJECT_UNLOCK(filter);
      break;
    case PROP_PSNR:
      GST_OBJECT_LOCK(filter);
      g_value_set_uint (value, filter->psnr);
      GST_OBJECT_UNLOCK(filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
stillreplacefilter_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstStillReplaceFilter *filter;
  gboolean ret;

  filter = GST_STILLREPLACEFILTER (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;
      GstVideoInfo info;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */
      if (!gst_video_info_from_caps (&info, caps))
      {
        return FALSE;
      }
      GST_OBJECT_LOCK(filter);
      filter->sink_info = info;
      GST_OBJECT_UNLOCK(filter);

      if (!gst_pad_set_caps( filter->replacesinkpad, caps )) {
        GST_ERROR_OBJECT( filter, "Caps negotiation failed: Replacesink pad must allow the same caps as the main sink");
        return FALSE;
      }

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_EOS:
    {
      g_mutex_lock (&filter->replacesinkMutex);
      filter->eos = TRUE;
      g_cond_broadcast( &filter->replacesinkEvent );
      g_mutex_unlock (&filter->replacesinkMutex);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

static GstBuffer* getNextReplaceBuffer( GstStillReplaceFilter* filter )
{
  GstBuffer* ret = NULL;
  g_mutex_lock( &filter->replacesinkMutex );
  if ( !gst_pad_is_linked( filter->replacesinkpad) )
  {
    if (filter->nextReplaceBuffer)
    {
      ret = gst_buffer_ref( filter->nextReplaceBuffer );
    }
    g_mutex_unlock( &filter->replacesinkMutex );
    return ret;
  }
  while ((filter->nextReplaceBuffer == NULL)&&(!filter->eos)&&(!filter->flushing))
  {
    g_cond_wait( &filter->replacesinkEvent, &filter->replacesinkMutex );
  }
  ret = filter->nextReplaceBuffer;
  filter->nextReplaceBuffer = NULL;
  g_cond_broadcast( &filter->replacesinkEvent );
  g_mutex_unlock( &filter->replacesinkMutex );
  return ret;
}

static double squareError( guint8* p1, guint8* p2, gsize d, gsize size )
{
  guint64 sad = 0;
  for( int i=0; i<size; i+=d)
  {
    int tmp = p1[i] - p2[i];
    sad += tmp*tmp;
  }
  return (double)sad;
  //return (double)(10 * log10(65025.0f * (size/d)/sad));
}
static double psnr( double mse )
{
  return (double)(10 * log10(65025.0f/mse));
}


/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
stillreplacefilter_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstStillReplaceFilter *filter;

  filter = GST_STILLREPLACEFILTER (parent);

  GstBuffer* refImageBuffer = NULL;
  GST_OBJECT_LOCK( filter );
  if (filter->refImageBuffer) 
  {
    refImageBuffer = gst_buffer_ref(filter->refImageBuffer);
  }
  GST_OBJECT_UNLOCK( filter );

  gboolean replace = FALSE;
  if (!refImageBuffer)
  {
    if (filter->silent == FALSE)
      GST_INFO("replacing refImageBuffer\n");
    gst_buffer_replace( &filter->refImageBuffer, buf );
  }
  else if (filter->psnr > 0)
  {
    // Compare frame
    GstVideoFrame refFrame;
    if (gst_video_frame_map (&refFrame, &filter->sink_info, refImageBuffer, GST_MAP_READ))
    {
      GstVideoFrame frame;
      if (gst_video_frame_map (&frame, &filter->sink_info, buf, GST_MAP_READ))
      {
        guint lines = filter->compare_lines;
        if ((lines == 0)||(lines > filter->sink_info.height))
        {
          lines = filter->sink_info.height;
        }
        int planes = GST_VIDEO_FRAME_N_PLANES(&refFrame);
        if (planes > GST_VIDEO_FRAME_N_PLANES(&frame) )
        {
          planes = GST_VIDEO_FRAME_N_PLANES(&frame);
        }
        for ( int plane=0; plane<planes && replace == FALSE; ++plane)
        {
          guint8 *pRefData = GST_VIDEO_FRAME_PLANE_DATA(&refFrame, plane);
          guint8 *pData = GST_VIDEO_FRAME_PLANE_DATA(&frame, plane);
          gsize rowbytes = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, plane);
          guint comps = GST_VIDEO_FRAME_N_COMPONENTS(&frame);
          for ( int comp = 0; comp < comps && replace == FALSE; ++comp) {
            double mse = squareError(pRefData + comp, pData + comp, GST_VIDEO_FRAME_N_COMPONENTS(&frame), rowbytes * lines)/(lines*(rowbytes/comps));
            double val = psnr( mse );
            if (filter->silent == FALSE )
              GST_INFO("psnr: %f  \n", val);
            if (val > (double)filter->psnr)
            {
              replace = TRUE;
            }
          }
        }
        gst_video_frame_unmap( &frame );
      }
      gst_video_frame_unmap( &refFrame );
    }
  }
  if (replace)
  {
    buf = gst_buffer_make_writable(buf);
    GstBuffer* replaceBuffer = getNextReplaceBuffer( filter );
    if (replaceBuffer)
    {
      GstVideoFrame srcFrame;
      if (gst_video_frame_map (&srcFrame, &filter->replacesink_info, replaceBuffer, GST_MAP_READ))
      {
        GstVideoFrame destFrame;
        if (gst_video_frame_map (&destFrame, &filter->sink_info, buf, GST_MAP_WRITE))
        {
          int planes = GST_VIDEO_FRAME_N_PLANES(&destFrame);
          for ( int plane=0; plane<planes; ++plane)
          {
            guint8 *pData = GST_VIDEO_FRAME_PLANE_DATA(&destFrame, plane);
            int height = GST_VIDEO_FRAME_COMP_HEIGHT(&destFrame,0);
            if (height > GST_VIDEO_FRAME_COMP_HEIGHT(&srcFrame,0))
              height = GST_VIDEO_FRAME_COMP_HEIGHT(&srcFrame,0);
            int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&destFrame, plane);
            if (stride > GST_VIDEO_FRAME_PLANE_STRIDE(&srcFrame, plane))
              stride = GST_VIDEO_FRAME_PLANE_STRIDE(&srcFrame, plane);
            guint8 *pSrcData = GST_VIDEO_FRAME_PLANE_DATA(&srcFrame, plane);
            //memcpy (pData, pSrcData, GST_VIDEO_FRAME_COMP_HEIGHT(&destFrame, 0) * GST_VIDEO_FRAME_PLANE_STRIDE(&destFrame, plane) );
            for ( int line = 0; line < height; ++line )
            {
              memcpy( pData, pSrcData, stride );
              pData += GST_VIDEO_FRAME_PLANE_STRIDE(&destFrame, plane);
              pSrcData += GST_VIDEO_FRAME_PLANE_STRIDE(&srcFrame, plane);
            }
          }
          gst_video_frame_unmap( &destFrame );
        }
        else
        {
          GST_ERROR_OBJECT(filter, "mapping destFrame failed\n");
        }
        gst_video_frame_unmap( &srcFrame );
      }
      else
      {
        GST_ERROR_OBJECT(filter, "mapping srcFrame failed\n");
      }
      gst_buffer_unref( replaceBuffer );
    }
  }
  GstFlowReturn ret = gst_pad_push (filter->srcpad, buf);
  if (filter->silent == FALSE)
  {
    if (ret != GST_FLOW_OK )
      GST_INFO("%s done ret=%d\n", __PRETTY_FUNCTION__, ret);
  }
  if (ret == GST_FLOW_FLUSHING)
  {
    g_mutex_lock (&filter->replacesinkMutex);
    filter->flushing = TRUE; 
    g_cond_broadcast( &filter->replacesinkEvent );
    g_mutex_unlock (&filter->replacesinkMutex);
  }
  if (ret == GST_FLOW_ERROR)
  {
    g_mutex_lock (&filter->replacesinkMutex);
    g_cond_broadcast( &filter->replacesinkEvent );
    g_mutex_unlock (&filter->replacesinkMutex);
  }
  if (refImageBuffer) 
  {
    gst_buffer_unref( refImageBuffer );
  }
  return ret;
}

static gboolean
stillreplacefilter_replacepad_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstStillReplaceFilter *filter = GST_STILLREPLACEFILTER (parent);
  gboolean ret;

  GST_LOG_OBJECT (filter, "Replacesink: Received %s event: %" GST_PTR_FORMAT, GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event))
  {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;
      GstVideoInfo info;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */
      if (!gst_video_info_from_caps (&info, caps))
      {
        return FALSE;
      }
      GST_OBJECT_LOCK(filter);
      filter->replacesink_info = info;
      GST_OBJECT_UNLOCK(filter);

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    case GST_EVENT_EOS:
    {
      g_mutex_lock (&filter->replacesinkMutex);
      g_cond_broadcast( &filter->replacesinkEvent );
      g_mutex_unlock (&filter->replacesinkMutex);
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}
static GstFlowReturn
stillreplacefilter_replacepad_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstStillReplaceFilter *filter;

  filter = GST_STILLREPLACEFILTER (parent);
  g_mutex_lock (&filter->replacesinkMutex);

  if (filter->eos)
  {
    g_cond_broadcast( &filter->replacesinkEvent );
    g_mutex_unlock (&filter->replacesinkMutex);
    gst_buffer_unref( buf );
    return GST_FLOW_EOS;
  }
  if (filter->flushing)
  {
    g_cond_broadcast( &filter->replacesinkEvent );
    g_mutex_unlock (&filter->replacesinkMutex);
    gst_buffer_unref( buf );
    return GST_FLOW_FLUSHING;
  }

  while ( (filter->nextReplaceBuffer != NULL)&&(!filter->eos)&&(!filter->flushing) )
    g_cond_wait( &filter->replacesinkEvent, &filter->replacesinkMutex );
  if (filter->eos)
  {
    ret = GST_FLOW_EOS;
  }
  else if (filter->flushing)
  {
    if (filter->nextReplaceBuffer)
    {
      gst_buffer_unref( filter->nextReplaceBuffer );
    }
    filter->nextReplaceBuffer = NULL;
    ret = GST_FLOW_FLUSHING;
  }
  else
  {
    if (filter->nextReplaceBuffer)
    {
      gst_buffer_unref( filter->nextReplaceBuffer );
    }
    filter->nextReplaceBuffer = buf;
    g_cond_broadcast( &filter->replacesinkEvent );
  }
  g_mutex_unlock (&filter->replacesinkMutex);
  return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
plugin_init (GstPlugin * stillreplacefilter)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template stillreplacefilter' with your description
   */
  GST_DEBUG_CATEGORY_INIT (stillreplacefilter_debug, "stillreplacefilter",
      0, "Still replace filter");

  return gst_element_register (stillreplacefilter, "stillreplacefilter", GST_RANK_NONE,
      GST_TYPE_STILLREPLACEFILTER);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "stillreplacefilter"
#endif

/* gstreamer looks for this structure to register stillreplacefilters
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    stillreplacefilter,
    "Still Replace Filter",
    plugin_init,
    PACKAGE_VERSION,
    GST_LICENSE,
    GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)
