#include "plugins/AnimationPlugin.h"
#include "websocket.h"
#ifdef ESP32
#include <mutex>

// frames are changed from the websocket task while the screen task plays
// them, adding a frame can move the list in memory mid read
static std::mutex framesLock;
#define FRAMES_GUARD std::lock_guard<std::mutex> guard(framesLock)
#else
#define FRAMES_GUARD
#endif

void AnimationPlugin::setup()
{
  this->step = 0;
  if (customAnimationFrames.size() == 0)
  {
    Screen.setPixel(7, 4, 1);
    Screen.setPixel(8, 4, 1);
    Screen.setPixel(7, 5, 1);
    Screen.setPixel(8, 5, 1);
    Screen.setPixel(7, 6, 1);
    Screen.setPixel(8, 6, 1);
    Screen.setPixel(7, 7, 1);
    Screen.setPixel(8, 7, 1);
    Screen.setPixel(7, 8, 1);
    Screen.setPixel(8, 8, 1);

    Screen.setPixel(7, 10, 1);
    Screen.setPixel(8, 10, 1);
    Screen.setPixel(7, 11, 1);
    Screen.setPixel(8, 11, 1);
  }
}

void AnimationPlugin::loop()
{
  std::vector<int> frame;
  {
    // copy the frame, the wait below must not hold the lock
    FRAMES_GUARD;
    size_t size = customAnimationFrames.size();
    if (size == 0)
    {
      return;
    }

    // The frame list can shrink under us (resetFrames, a shorter upload)
    if (this->step >= size)
    {
      this->step = 0;
    }

    frame = customAnimationFrames[this->step];

    this->step++;
    if (this->step >= size)
    {
      this->step = 0;
    }
  }

  std::vector<int> bits = Screen.readBytes(frame);

  for (int i = 0; i < bits.size(); i++)
  {
    Screen.setPixelAtIndex(i, bits[i]);
  }

  Screen.presentAndWait(frameDelay);
}

void AnimationPlugin::websocketHook(JsonDocument &request)
{
  FRAMES_GUARD;
  const char *event = request["event"];
  if (!strcmp(event, "upload") || !strcmp(event, "resetFrames") || !strcmp(event, "configAnimation")) {
    if (request["frameDelay"].is<int>())
    {
      frameDelay = request["frameDelay"].as<int>();
      if (frameDelay < 10)
        frameDelay = 10;
      if (frameDelay > 10000)
        frameDelay = 10000;
    }
  }

  if (!strcmp(event, "resetFrames")) {
    customAnimationFrames.resize(0);
  } else if (!strcmp(event, "addFrame")) {
    if (customAnimationFrames.size() >= MAX_FRAMES)
    {
      Serial.println(F("Animation: frame limit reached, frame ignored"));
      return;
    }

    int currentSize = customAnimationFrames.size();
    int targetSize = currentSize + 1;
    customAnimationFrames.resize(targetSize);

    for (int k = 0; k < 32; k++)
    {
      if (k == 0)
      {
        customAnimationFrames[currentSize].resize(32);
      }
      customAnimationFrames[currentSize][k] = (int)request["data"][k];
    }
    
  } else if (!strcmp(event, "upload")) {
    int size = request["screens"] | -1;
    if (size < 0 || size > (int)MAX_FRAMES)
    {
      Serial.println(F("Animation: invalid frame count, upload ignored"));
      return;
    }

    customAnimationFrames.resize(size);
    for (int i = 0; i < size; i++)
    {
      for (int k = 0; k < 32; k++)
      {
        if (k == 0)
        {
          customAnimationFrames[i].resize(32);
        }
        customAnimationFrames[i][k] = (int)request["data"][i][k];
      }
    }
  } else if (!strcmp(event, "animationStatus")) {
    JsonDocument jsonDocument;
    jsonDocument["event"] = "animationStatus";
    jsonDocument["screens"] = customAnimationFrames.size();
    jsonDocument["frameDelay"] = frameDelay;
    String output;
    serializeJson(jsonDocument, output);
    sendWSMessage(output);
    jsonDocument.clear();
  }
}

const char *AnimationPlugin::getName() const
{
  return "Animation";
}
