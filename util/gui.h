/************************************************************************
* Copyright (c) 2005-2007 tok@openlinux.org.uk                          *
*                                                                       *
* This software is provided as-is, without any express or implied       *
* warranty. In no event will the authors be held liable for any         *
* damages arising from the use of this software.                        *
*                                                                       *
* Permission is granted to anyone to use this software for any purpose, *
* including commercial applications, and to alter it and redistribute   *
* it freely, subject to the following restrictions:                     *
*                                                                       *
* 1. The origin of this software must not be misrepresented; you must   *
* not claim that you wrote the original software. If you use this       *
* software in a product, an acknowledgment in the product documentation *
* would be appreciated but is not required.                             *
*                                                                       *
* 2. Altered source versions must be plainly marked as such, and must   *
* not be misrepresented as being the original software.                 *
*                                                                       *
* 3. This notice may not be removed or altered from any source          *
* distribution.                                                         *
************************************************************************/
#ifndef UTIL_GUI_H
#define UTIL_GUI_H
#include <string>
#include <list>
#include <map>
#include <SDL.h>
#include "Singleton.h"
#include "animation.h"
#include "gl_pagedtexture.h"
#include "image_loader.h"
#include "font_cache.h"

namespace GUI {
  struct Object;
  class Animation;

  class Manager {
    public:
      Manager() {}
      ~Manager();
      void add(Object * obj, uint8_t onLevel);
      void remove(Object * obj);
      void removeById(size_t id);
      Object* findObject(const size_t id);
      void draw();
      void clearObjects();
      void clearCache();
      void cacheImageRAW(const std::string & file, size_t asId);
      void cacheImageRAT(const std::string & file, const std::string & palette, size_t asId);
      void cacheImageSDL(const std::string & file, size_t asId);
      ImageUtil::WidthHeightPair cacheStyleArrowSprite(const size_t id, int remap);
      const OpenGL::PagedTexture & getCachedImage(size_t Id);
      void receive(SDL_MouseButtonEvent & mb_event);
      Animation* findAnimation(uint16_t id);
      void createAnimation(const std::vector<uint16_t> & indices, uint16_t fps, size_t asAnimId);
      void update(uint32_t nowTicks);
    private:
      bool isInside(Object & o, Uint16 x, Uint16 y) const;
      typedef std::map< size_t, OpenGL::PagedTexture > GuiTextureCache;
      GuiTextureCache::iterator findByCacheId(const size_t & Id);

      typedef std::map<uint16_t, Animation*> AnimationMap;
      AnimationMap guiAnimations;
      typedef std::list<Object*> GuiObjectList;
      typedef std::map< uint8_t, GuiObjectList > GuiObjectListMap;
      GuiObjectListMap::iterator findLayer(uint8_t l);
      GuiObjectListMap guiLayers;
      GuiTextureCache texCache;

  };

  typedef Loki::SingletonHolder<Manager, Loki::CreateUsingNew, Loki::DefaultLifetime,
          Loki::SingleThreaded> ManagerHolder;

  class Animation : public Util::Animation {
    public:
      Animation(const std::vector<uint16_t> & _indices, const uint16_t fps) :
        Util::Animation(_indices.size(), fps),
        indices(_indices) {}
      std::vector<uint16_t> indices;
      uint16_t getCurrentFrame();
  };

  struct Object {
    Object(const SDL_Rect & r);
    Object(const size_t Id, const SDL_Rect & r);
    Object(const size_t Id, const SDL_Rect & r, const SDL_Color & c);
    virtual ~Object() {}
    size_t    id;
    SDL_Rect  rect;
    SDL_Color color;
    void copyRect(const SDL_Rect & src);
    void copyColor(const SDL_Color & src);
    virtual void draw();
    virtual void update(Uint32 ticks) {}
    Manager & manager;
  };

  struct TexturedObject : public Object {
    TexturedObject(const SDL_Rect & r, const size_t texid) : Object(r),
    texId(texid) {
    }
    TexturedObject(size_t Id, const SDL_Rect & r, const size_t texid) : Object(Id, r),
    texId(texid) {
    }
    size_t texId;
    void draw();
  };

  struct AnimatedTextureObject : public Object {
    AnimatedTextureObject(const SDL_Rect & r, const size_t animid) : Object(r),
    animId(animid) {
      animation = NULL;
    }
    AnimatedTextureObject(size_t Id, const SDL_Rect & r, const size_t animid) : Object(Id, r),
    animId(animid) {
      animation = NULL;
    }
    size_t animId;
    Animation * animation;
    void draw();
  };

  struct Label : public Object {
    Label(const SDL_Rect & r, const std::string & s, 
        const std::string & fontFile, const size_t fontScale) : Object(r), text(s) {
      OpenGL::DrawableFont & fnt = OpenGTA::FontCacheHolder::Instance().getFont(fontFile, fontScale);
      font = &fnt;
    }
    Label(const size_t Id, const SDL_Rect & r, const std::string & s, 
        const std::string & fontFile, const size_t fontScale) : Object(Id, r), text(s) {
      OpenGL::DrawableFont & fnt = OpenGTA::FontCacheHolder::Instance().getFont(fontFile, fontScale);
      font = &fnt;
    }
    OpenGL::DrawableFont * font;
    std::string text;
    void draw();
  };

  struct Pager : public Object {
    Pager(const size_t Id, const SDL_Rect & r, const size_t texid,
      const std::string & fontFile, const size_t fontScale) : Object(Id, r) {
      OpenGL::DrawableFont & fnt = OpenGTA::FontCacheHolder::Instance().getFont(fontFile, fontScale);
      font = &fnt;
      texId = texid;
      offset = r.w-5;
    }
    OpenGL::DrawableFont * font;
    size_t texId;
    void update(Uint32 ticks);
    void draw();
    std::string lastMsg;
    int offset;
  };
}
#endif
