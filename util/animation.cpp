#include <sstream>
#include "animation.h"
#include "m_exceptions.h"

namespace Util {
  Animation::Animation(uint16_t num, uint16_t fps) :
    callback() {
    status = STOPPED;
    numFrames = num;
    currentFrame = 0;
    delay = 1000 / fps;
    lastChangeTicks = 0;
  }

  Animation::Animation(const Animation & other) {
    status = other.status;
    onDone = other.onDone;
    numFrames = other.numFrames;
    currentFrame = other.currentFrame;
    delay = other.delay;

    lastChangeTicks = other.lastChangeTicks;
    callback = other.callback;
  }

  void Animation::update(const uint32_t & nowTicks) {
    if (status == STOPPED)
      return;
    if (lastChangeTicks == 0)
      lastChangeTicks = nowTicks;
    if (nowTicks < lastChangeTicks + delay)
      return;
    lastChangeTicks = nowTicks;
    if (status == PLAY_FORWARD)
      flipFrame(true);
    else if (status == PLAY_BACKWARD)
      flipFrame(false);
  }

  void Animation::flipFrame(bool forward = true) {
    switch(forward) {
      case true:
        if (currentFrame < numFrames - 1)
          ++currentFrame;
        else if (currentFrame == numFrames - 1)
          isDone();
        break;
      case false:
        if (currentFrame == 0)
          isDone();
        else
          --currentFrame;
    }
  }

  void Animation::jumpToFrame(const uint16_t num, const Status andDo) {
    if (num >= numFrames) { 
      std::ostringstream o;
      o << num << " >= " << numFrames;
      throw E_OUTOFRANGE(o.str());
    }
    currentFrame = num;
    status = andDo;
  }

  void Animation::isDone() {
    if (onDone == STOP) {
      status = STOPPED;
      return;
    }
    if (onDone == REVERSE) {
      status = (status == PLAY_FORWARD) ? PLAY_BACKWARD : PLAY_FORWARD;
      return;
    }
    if (onDone == LOOP) {
      if (status == PLAY_FORWARD)
        jumpToFrame(0, PLAY_FORWARD);
      else if (status == PLAY_BACKWARD)
        jumpToFrame(numFrames - 1, PLAY_BACKWARD);
      return;
    }
    status = STOPPED;
    if (onDone == FCALLBACK) {
      if (callback)
        callback();
      else
        ERROR << "Wanted to call callback, but nobody was there" << std::endl;
    }
  }
}

void AnimCallback() {
  WARN << "EmptyAnimCallback called" << std::endl;
}