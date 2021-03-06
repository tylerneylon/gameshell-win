// img.cpp
//
// https://github.com/tylerneylon/oswrap in oswrap_windows
//

extern "C" {
#include "img.h"
#include "winutil.h"
}

#include <assert.h>
#include <gdiplus.h>
using namespace Gdiplus;


// Internal types and globals.

// This should always mirror the struct defined in draw.c behind draw__Bitmap.
typedef struct {
  HBITMAP bitmap;
  char *  bytes;
} Bitmap_;


// Public functions.

extern "C" draw__Bitmap img__new_bitmap(const char *path, int *w, int *h) {
  assert(w && h);

  if (path == NULL) {
    char *msg;
    asprintf(&msg, "Error in %s: path is NULL.\n", __FUNCTION__);
    OutputDebugString(msg);
    return NULL;
  }

  // We need a wchar version of path for input to the
  // Gdiplus::Bitmap constructor that loads a file.
  size_t path_len = strlen(path) + 1;
  wchar_t *wchar_path = (wchar_t *)malloc(2 * path_len);
  int worked = MultiByteToWideChar(
    CP_UTF8,  // Input code page.
    0,        // flags; used mainly for non-UTF8 code pages.
    path,
    -1,       // Input str length; -1 indicates that path is null-terminated.
    wchar_path,
    path_len);
  if (!worked) {
    char *msg;
    asprintf(&msg, "Error in %s: call to MultiByteToWideChar failed.\n", __FUNCTION__);
    OutputDebugString(msg);
    free(wchar_path);
    return NULL;
  }

  GdiplusStartupInput gdiplus_config;
  ULONG_PTR gdiplus_token;
  GdiplusStartup(&gdiplus_token, &gdiplus_config, NULL /* startup output */);

  // This can't be a static variable since we need to delete it before gdiplus shutdown.
  Bitmap *gdi_bitmap = new Bitmap(wchar_path);

  *w = gdi_bitmap->GetWidth();
  *h = gdi_bitmap->GetHeight();
  Rect full_rect(0, 0, *w, *h);

  Bitmap_ *draw_bitmap = (Bitmap_ *)draw__new_bitmap(*w, *h);
  BitmapData bitmap_data;
  bitmap_data.Width = *w;
  bitmap_data.Height = *h;
  // A negative stride effectively reverses the image's y-direction.
  bitmap_data.Stride = *w * -4;
  // Since Stride < 0, we want Scan0 to point to the last row in data.
  bitmap_data.Scan0 = draw_bitmap->bytes + (*h - 1) * (*w * 4);
  bitmap_data.PixelFormat = PixelFormat32bppPARGB;

  Status status = gdi_bitmap->LockBits(
    &full_rect,
    ImageLockModeRead | ImageLockModeUserInputBuf,
    PixelFormat32bppPARGB,
    &bitmap_data);

  if (status != Ok) {
    char *msg;
    asprintf(&msg, "Error in %s: call to Bitmap::LockBits failed.\n", __FUNCTION__);
    OutputDebugString(msg);
    // Fall through to deallocate resources; user must still delete the return value.
  }

  delete(gdi_bitmap);
  GdiplusShutdown(gdiplus_token);
  free(wchar_path);

  return (draw__Bitmap)draw_bitmap;
}
