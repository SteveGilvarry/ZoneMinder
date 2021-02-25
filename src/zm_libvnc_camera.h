
#ifndef ZN_LIBVNC_CAMERA_H
#define ZN_LIBVNC_CAMERA_H

#include "zm_camera.h"
#include "zm_swscale.h"

#if HAVE_LIBVNC
#include <rfb/rfbclient.h>

// Older versions of libvncserver defined a max macro in rfb/rfbproto.h
// Undef it here so it doesn't collide with std::max
// TODO: Remove this once xenial support is dropped
#ifdef max
#undef max
#endif

// Used by vnc callbacks
struct VncPrivateData {
  uint8_t *buffer;
  uint8_t width; 
  uint8_t height;
};

class VncCamera : public Camera {
protected:
  rfbClient *mRfb;
  VncPrivateData mVncData;
  SWScale scale;
  AVPixelFormat mImgPixFmt;
  std::string mHost;
  std::string mPort;
  std::string mUser;
  std::string mPass;
public:
  VncCamera(
      const Monitor *monitor,
      const std::string &host,
      const std::string &port,
      const std::string &user,
      const std::string &pass,
      int p_width,
      int p_height,
      int p_colours,
      int p_brightness,
      int p_contrast,
      int p_hue,
      int p_colour,
      bool p_capture,
      bool p_record_audio);
    
  ~VncCamera();

  int PreCapture();
  int PrimeCapture();
  int Capture(ZMPacket &packet);
  int PostCapture();
  int Close();
};

#endif // HAVE_LIBVNC
#endif // ZN_LIBVNC_CAMERA_H
