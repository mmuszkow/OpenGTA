/************************************************************************
* Copyright (c) 2005-2006 tok@openlinux.org.uk                          *
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
#include <cmath>
#include <cassert>
#include "gl_cityview.h"
#include "gl_spritecache.h"
#include "spritemanager.h"
#include "buffercache.h"
#include "gl_camera.h"
#include "dataholder.h"
#include "math3d.h"
#include "localplayer.h"
#include "log.h"
#include "gl_screen.h"
#include "blockdata.h"
#include "image_loader.h"

float slope_height_offset(unsigned char slope_type, float dx, float dz);
namespace OpenGTA {

/*
  float slope_raw_data[45][5][4][3] = {
#include "slope1_data.h"
  };
  float slope_tex_data[45][4][4][2] = {
#include "slope1_tcoords.h"
  };

  float lid_normal_data[45][3] = { 
#include "lid_normal_data.h"
  };
*/

/*
  GLuint createGLTexture(GLsizei w, GLsizei h, bool rgba, const void* pixels) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (rgba)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      //gluBuild2DMipmaps(GL_TEXTURE_2D, 4, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    else
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
      //gluBuild2DMipmaps(GL_TEXTURE_2D, 3, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    GL_CHECKERROR;
    return tex;
  }
*/

  CityView::CityView() {
    setNull();
    /*
    Pedestrian p(Vector3D(0.5f, 0.5f, 0.5f), Vector3D(4, 5.01f, 4), 0xffffffff);
    p.m_control = &LocalPlayer::Instance();
    SpriteManagerHolder::Instance().addPed(p);
    */
  }
  void CityView::setNull() {
    loadedMap = NULL;
    sideCache = NULL;
    lidCache  = NULL;
    camVec[0] = 0.0f;
    camVec[1] = 1.0f;
    camVec[2] = 0.0f;
    zoomLevel = 1.0f;
    visibleRange = 15;
    topDownView = true;
    drawTextured = true;
    drawLines = false;
    setPosition(0.0f, 0.0f, 20.0f);

    scene_display_list = 0;
    texFlipTest = 0;
    drawHeadingMarkers = false;
    current_sector = NULL;
  }
  CityView::~CityView() {
    cleanup();
  }
  void CityView::resetTextures() {
    sideCache->clearAll();
    lidCache->clearAll();
  }
  void CityView::setVisibleRange(int r) {
    visibleRange = r;
    scene_is_dirty = true;
  }
  int CityView::getVisibleRange() {
    return visibleRange;
  }
  bool CityView::getDrawTextured() { return drawTextured; }
  bool CityView::getDrawLines() { return drawLines; }
  void CityView::setDrawTextured(bool v) { drawTextured = v; }
  void CityView::setDrawLines(bool v) { drawLines= v; }
  void CityView::cleanup() {
    //if (loadedMap)
    //  delete loadedMap;
    if (sideCache)
      delete sideCache;
    if (lidCache)
      delete lidCache;
    if (scene_display_list)
      glDeleteLists(scene_display_list, 1);
    setNull();
  }
  void CityView::setViewMode(bool topDown) {
    topDownView = topDown;
  }
  void CityView::loadMap(const std::string &map, const std::string &style_f) {
    cleanup();
    //loadedMap = new Map(map);
    MapHolder::Instance().load(map);
    loadedMap = &MapHolder::Instance().get();
    StyleHolder::Instance().load(style_f);
    style = &StyleHolder::Instance().get();
    sideCache = new OpenGL::TextureCache<uint8_t>("SideCache");
    lidCache = new OpenGL::TextureCache<uint8_t>("LidCache");
    sideCache->setClearMagic(5);
    lidCache->setClearMagic(8);
    scene_is_dirty = true;
    lastCacheEmptyTicks = 0;

    scene_display_list = glGenLists(1);

    SpriteManagerHolder::Instance().clear();
    for (PHYSFS_uint16 oc = 0; oc < loadedMap->numObjects; oc++) {
      createLevelObject(&loadedMap->objects[oc]);
    }
  }
  void CityView::createLevelObject(OpenGTA::Map::ObjectPosition *obj) {
    SpriteManager & s_man = SpriteManagerHolder::Instance();
    if (obj->remap >= 128) {
      Car car(*obj);
      s_man.addCar(car);
    }
    else {
      GameObject gobj(*obj);
      s_man.addObject(gobj);
    }
  }
  void CityView::setZoom(const GLfloat zoom) {
    zoomLevel = zoom; 
  }
  void CityView::setPosition(const GLfloat & x, const GLfloat & y, const GLfloat & z) {
    camPos[0] = x;
    camPos[1] = y;
    camPos[2] = z;
    scene_is_dirty = true;
    //INFO << "Position: " << x << ", " << z << " (" << y << ")" << std::endl;
    if (loadedMap) {
      PHYSFS_uint8 _x = PHYSFS_uint8((x >= 1.0f) ? ((x < 255.0f) ? x : 254) : 1); // FIXME: crashes on 0 or 255
      PHYSFS_uint8 _y = PHYSFS_uint8((z >= 1.0f) ? ((z < 255.0f) ? z : 254) : 1); // why???
      NavData::Sector* in_sector = loadedMap->nav->getSectorAt(_x, _y);
      assert(in_sector);
      if (in_sector != current_sector) {
        current_sector = in_sector;
      }
    }

    //if (loadedMap && !topDownView)
    //  getTerrainHeight(camPos[0], camPos[1], camPos[2]);
  }

  NavData::Sector* CityView::getCurrentSector() {
    return current_sector;
  }

/*
  void CityView::setCamVector(const GLfloat & x, const GLfloat & y, const GLfloat & z) {
    camVec[1] = x;
    camVec[2] = y;
    camVec[3] = z;
    scene_is_dirty = true;
  }
*/

  void CityView::setTopDownView(const GLfloat & height) {
    camPos[1] = height;
    scene_is_dirty = true;
    setViewMode(true);
  }

  void CityView::getTerrainHeight(GLfloat & x, GLfloat & y, GLfloat & z) {
    int xi = int(x);
    int yi = int(z);
    //int zi = int(z);
    float h = 0.5f;
    PHYSFS_uint16 emptycount = loadedMap->getNumBlocksAt(xi, yi);
    for (int c=6-emptycount; c >= 1; c--) {
      OpenGTA::Map::BlockInfo* bi = loadedMap->getBlockAt(xi, yi, c);
      if (bi->blockType() == 0) {
        //camPos[1] = h;
        break;
      }

      /*INFO << "block " << c << " lid: " << int(bi->lid) << std::endl;
      if (bi->blockType() > 0) {
        INFO << "not air but: " << int(bi->blockType()) << std::endl;
      }*/
      h += 1.0f;
    }
    y = h;
  }

  OpenGL::PagedTexture CityView::renderMap2Texture() {
    uint32_t width = OpenGL::ScreenHolder::Instance().getWidth();
    uint32_t height = OpenGL::ScreenHolder::Instance().getHeight();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    OpenGL::ScreenHolder::Instance().set3DProjection();
    glRotatef(180, 0, 0, 1);
    gluLookAt(128, 230, 128, 128, 0, 128, 0.0f, 0.0f, 1.0f);
    glTranslatef(-42, 0, 0);
    for (int i = 0; i <= 255; i++) {
      for (int j= 0; j <= 255; j++) {
        glPushMatrix();
        glTranslatef(1.0f*j, 0.0f, 1.0f*i);
        PHYSFS_uint16 maxcount = loadedMap->getNumBlocksAtNew(j,i);
        for (int c=0; c < maxcount; ++c) {
          glPushMatrix();
          drawBlock(loadedMap->getBlockAtNew(j, i, c));
          glPopMatrix();
          glTranslatef(0.0f, 1.0f, 0.0f);
        }
        glPopMatrix();
      }
    }
    GL_CHECKERROR;
    
    uint32_t gl_h = 1;
    while (gl_h < height)
      gl_h <<= 1;
    uint32_t img_size = gl_h * gl_h * 3; 
    uint8_t *img_buf = Util::BufferCacheHolder::Instance().requestBuffer(img_size);

    glReadBuffer(GL_BACK);
    for (uint32_t i = 0; i < gl_h; i++) {
      glReadPixels(0, i, gl_h, 1, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)(img_buf + gl_h * 3 * i));
    }
    GL_CHECKERROR;

    sideCache->sink();
    sideCache->sink();
    lidCache->sink();
    lidCache->sink();
    sideCache->clear();
    lidCache->clear();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    GLuint tex = ImageUtil::createGLTexture(gl_h, gl_h, false, img_buf);
    float f_h = float(height) / gl_h;
    float f_w = float(width)  / gl_h;
    //float horiz_corr = (1.0f - f_w) / 2.0f;
    //return OpenGL::PagedTexture(tex, 0+horiz_corr, 0, f_w+horiz_corr, f_h);
    return OpenGL::PagedTexture(tex, 0, 0, f_w, f_h);
  }

  void CityView::draw(Uint32 ticks) {
    GL_CHECKERROR;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    /*
    sideCache->clearStats();
    lidCache->clearStats();
    */
    glLoadIdentity();
    int x1, y1, x2, y2;
    if (topDownView) {
      glRotatef(180, 0, 0, 1);
      gluLookAt(camPos[0], camPos[1], camPos[2], camPos[0], 0.0f, camPos[2], 0.0f, 0.0f, 1.0f);

      x1 = int(camPos[0]) - visibleRange;
      y1 = int(camPos[2]) - visibleRange;
      x2 = int(camPos[0]) + visibleRange;
      y2 = int(camPos[2]) + visibleRange;

    }
    else {
      OpenGL::Camera & cam = OpenGL::CameraHolder::Instance();
      //gluLookAt(camPos[0], camPos[1], camPos[2], camPos[0]+5, 0, (camPos[2])+5, camVec[0], camVec[1], camVec[2]);
      cam.update(ticks);
      Vector3D & e = cam.getEye();
      //INFO << "eye: " << e.x << ", " << e.y << ", " << e.z << std::endl;
      setPosition(e.x, e.y, e.z);
      x1 = int(e.x) - visibleRange;
      y1 = int(e.z) - visibleRange;
      x2 = int(e.x) + visibleRange;
      y2 = int(e.z) + visibleRange;
      //PHYSFS_uint16 emptycount = loadedMap->getNumBlocksAt(int(e.x),int(e.z));
      //INFO << "cam-h is " << int(e.y)<< " ec: " << emptycount << std::endl;
/*
      int cc = 0;
      for (int i = 6 - emptycount; i >= 0; i--) {
         OpenGTA::Map::BlockInfo* bi = loadedMap->getBlockAt(int(e.x), int(e.z), i);
         //INFO << i << " type: "<< int(bi->blockType()) <<  " lid: " << int(bi->lid)<< std::endl;
         if (bi->blockType() >=2 && bi->blockType() <= 4) {
          c.y = c.y - e.y + float(cc)+1.0f;
          e.y = float(cc)+1.0f;
          break;
         }
         cc++;
      }
*/

      //getTerrainHeight(e.x, e.y, e.z);
    }
    frustum.CalculateFrustum();

    if (x1 < 0)
      x1 = 0;
    if (y1 < 0)
      y1 = 0;
    if (x2 > 255)
      x2 = 255;
    if (y2 > 255)
      y2 = 255;

    bool use_display_list = false;

    GL_CHECKERROR;
    if ((!scene_is_dirty) && (use_display_list)) {
      glCallList(scene_display_list);
    }
    else {
      scene_rendered_blocks = scene_rendered_vertices = 0;

      if (use_display_list)
        glNewList(scene_display_list, GL_COMPILE);
      for (int i = y1; i <= y2; i++) {
        //glPushMatrix();
        //glTranslatef(0.0f, 0.0f, 1.0f*i);
        for (int j= x1; j <= x2; j++) {
          if (!frustum.BlockInFrustum(0.5f+j, 0.5f+i, 0.5f))
            continue;
          glPushMatrix();
          glTranslatef(1.0f*j, 0.0f, 1.0f*i);
          //PHYSFS_uint16 emptycount = loadedMap->getNumBlocksAt(j,i);
          PHYSFS_uint16 maxcount = loadedMap->getNumBlocksAtNew(j,i);
          //for (int c=6-emptycount; c >= 1; c--) {
          for (int c=0; c < maxcount; ++c) {
            ++scene_rendered_blocks;
            glPushMatrix();
            drawBlock(loadedMap->getBlockAtNew(j, i, c));
            glPopMatrix();
            glTranslatef(0.0f, 1.0f, 0.0f);
          }
          glPopMatrix();
        }
        //glPopMatrix();
      }
      if (use_display_list) {
        glEndList();
        glCallList(scene_display_list);
        scene_is_dirty = false;
      }
      //INFO << scene_rendered_blocks << " blocks drawn with " << scene_rendered_vertices << " vertices" << std::endl;
    }
    GL_CHECKERROR;
    /*
    for (PHYSFS_uint16 oc = 0; oc < loadedMap->numObjects; oc++) {
      if (frustum.BlockInFrustum((loadedMap->objects[oc].x >> 6) + 0.5f, 
        (loadedMap->objects[oc].y >> 6) + 0.5f, 0.5f))
        drawObject(&loadedMap->objects[oc]);
    }*/
    SDL_Rect r;
    r.x = x1;
    r.y = y1;
    r.w = x2 - x1;
    r.h = y2 - y1;
    //INFO << "active rect: " << r.x << "," <<r.y << " - " << r.x + r.w << "," << r.y+r.h<<std::endl;
    GL_CHECKERROR;
    SpriteManagerHolder::Instance().drawInRect(r);
    
    lastCacheEmptyTicks += ticks;
    if (lastCacheEmptyTicks > 4000) {
      lidCache->sink();
      lidCache->sink();
      sideCache->sink();
      sideCache->clear();
      lidCache->clear();
      lastCacheEmptyTicks = 0;
      //lidCache->status();
      //sideCache->status();
    }
    GL_CHECKERROR;
  }

#if 0
  OpenGL::PagedTexture CityView::createSprite(size_t sprite_num, GraphicsBase::SpriteInfo* info) {
    unsigned char* src = style->getSpriteBitmap(sprite_num, -1 , 0);
    unsigned int glwidth = 1;
    unsigned int glheight = 1;

    while(glwidth < info->w)
      glwidth <<= 1;
    while(glheight < info->h)
      glheight <<= 1;
    unsigned char* dst = Util::BufferCacheHolder::Instance().requestBuffer(glwidth * glheight * 4);
    Util::BufferCacheHolder::Instance().unlockBuffer(src);
    assert(dst != NULL);
    unsigned char * t = dst;
    unsigned char * r = src;
    for (unsigned int i = 0; i < info->h; i++) {
      memcpy(t, r, info->w * 4);
      t += glwidth * 4;
      r += info->w * 4;
    }
    GLuint texid;
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glwidth, glheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    return OpenGL::PagedTexture(texid, 0, 0, 
      float(info->w)/float(glwidth), float(info->h)/float(glheight));
  }
#endif

  void CityView::drawObject(OpenGTA::Map::ObjectPosition* obj) {
    
    float w, h;
    float x = float(obj->x >> 6) + float(obj->x % 64)/64.0f;
    float y = float(obj->y >> 6) + float(obj->y % 64)/64.0f;
    float z = float(obj->z >> 6) + float(obj->z % 64)/64.0f;
    size_t spriteNumAbs, sprNum;
    GraphicsBase::SpriteInfo *info = NULL;
    GraphicsBase::SpriteNumbers::SpriteTypes st;
    if (obj->remap >= 128) { // car
      GraphicsBase::CarInfo* cinfo = style->findCarByModel(obj->type);
      assert(cinfo);
      sprNum = cinfo->sprNum;
      spriteNumAbs = style->spriteNumbers.reIndex(cinfo->sprNum, GraphicsBase::SpriteNumbers::CAR);
      info = style->getSprite(spriteNumAbs);
      w = float(info->w) / 64.0f;
      h = float(info->h) / 64.0f;
      st = GraphicsBase::SpriteNumbers::CAR;
    }
    else {
      spriteNumAbs = style->spriteNumbers.reIndex(style->objectInfos[obj->type]->sprNum, GraphicsBase::SpriteNumbers::OBJECT);
      sprNum = style->objectInfos[obj->type]->sprNum;
      info = style->getSprite(spriteNumAbs);
      assert(info);
      w = float(info->w) / 64.0f;
      h = float(info->h) / 64.0f;
      st = GraphicsBase::SpriteNumbers::OBJECT;
    }
    
    OpenGL::PagedTexture t;
    if (OpenGL::SpriteCacheHolder::Instance().has(spriteNumAbs))
      t = OpenGL::SpriteCacheHolder::Instance().get(spriteNumAbs);
    else {
      //t = createSprite(spriteNum, info);
      //OpenGL::SpriteCacheHolder::Instance().add(spriteNum, t);
      t = OpenGL::SpriteCacheHolder::Instance().create(sprNum, st, -1);
    }

    glPushMatrix();
    glTranslatef(x, 6.1f - z, y); // ups, seems: up<->down
    glRotatef(obj->rotation / 1035.0f * 360.0f, 0, 1, 0); // 1035 is guesswork!
    //glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, t.inPage);
    glBegin(GL_QUADS);
      glTexCoord2f(t.coords[0].u, t.coords[1].v);
      glVertex3f(-w/2, 0.0f, h/2);
      glTexCoord2f(t.coords[1].u, t.coords[1].v);
      glVertex3f(w/2,  0.0f, h/2);
      glTexCoord2f(t.coords[1].u, t.coords[0].v);
      glVertex3f(w/2,  0.0f, -h/2);
      glTexCoord2f(t.coords[0].u, t.coords[0].v);
      glVertex3f(-w/2, 0.0f, -h/2);
    glEnd();
    glPopMatrix();
    //glEnable(GL_TEXTURE_2D);
  }

  void CityView::drawBlock(OpenGTA::Map::BlockInfo* bi) {

    GL_CHECKERROR;
    /*
       std::cout << "is flat: " << bi->isFlat() << " up ok: " << bi->upOk() <<
       " down " << bi->downOk() << " left " << bi->leftOk() << " right " << bi->rightOk() << std::endl;
       std::cout << "block type: "<< int(bi->blockType()) << " slope " << int(bi->slopeType()) <<
       " rotation: " << int(bi->rotation()) << " remap: " << int(bi->remapIndex()) <<
       " TB " << bi->flipTopBottom() << " LR " << bi->flipLeftRight() << std::endl;
       */
    uint8_t which = bi->slopeType();
    float nx = LID_NORMAL_DATA[which][0];
    float ny = LID_NORMAL_DATA[which][1];
    float nz = LID_NORMAL_DATA[which][2];

    int jj = 0;
    GLfloat lidTex[8] = {0.0f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f};
    //GLfloat sideTex1_bak[8] = {1.0f, 0.0f,  0.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f};
    GLfloat sideTex1[8] = {1.0f, 0.0f,  0.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f};

#ifdef MSWAP
#undef MSWAP
#endif
#define MSWAP(a,b) { GLfloat tmp = sideTex1[a]; sideTex1[a] = sideTex1[b]; sideTex1[b] = tmp; }

#define SLOPE_TEX_CP(k) memcpy(sideTex1, SLOPE_TEX_DATA[which][k], 8 * sizeof(GLfloat))
#ifdef GLTEX_HELPER
#undef GLTEX_HELPER
#endif
#define GLTEX_HELPER \
    glTexCoord2f(lidTex[jj], lidTex[jj+1]); jj += 2; if (jj > 6) { jj = 0; }
#define GLTEX1_HELPER \
    glTexCoord2f(sideTex1[jj], sideTex1[jj+1]); jj += 2; if (jj > 6) { jj = 0; }

    bool is_flat = bi->isFlat(); // transparency in the texture ?
    GLuint lid_tex = 0, left_tex = 0, right_tex = 0, top_tex = 0, bottom_tex = 0;

    // FIXME: no remaps used!
    if (bi->lid) {
      if (!lidCache->hasTexture(bi->lid)) {
        lid_tex = ImageUtil::createGLTexture(64, 64, is_flat, 
              style->getLid(static_cast<unsigned int>(bi->lid), 0, is_flat));
        lidCache->addTexture(bi->lid, lid_tex); 
      }
      else
        lid_tex = lidCache->getTextureWithId(bi->lid);
    }
    if (bi->left) {
      if (!sideCache->hasTexture(bi->left)) {
        left_tex = ImageUtil::createGLTexture(64, 64, is_flat,
              style->getSide(static_cast<unsigned int>(bi->left), 0, is_flat));
        sideCache->addTexture(bi->left, left_tex); 
      }
      else
        left_tex = sideCache->getTextureWithId(bi->left);
    }
    if (bi->right) {
      if (!sideCache->hasTexture(bi->right)) {
        right_tex = ImageUtil::createGLTexture(64, 64, is_flat,
              style->getSide(static_cast<unsigned int>(bi->right), 0, is_flat));
        sideCache->addTexture(bi->right, right_tex); 
      }
      else
        right_tex = sideCache->getTextureWithId(bi->right);
    }
    if (bi->top) {
      if (!sideCache->hasTexture(bi->top)) {
        top_tex = ImageUtil::createGLTexture(64, 64, is_flat,
              style->getSide(static_cast<unsigned int>(bi->top), 0, is_flat));
        sideCache->addTexture(bi->top, top_tex);
      }
      else
        top_tex = sideCache->getTextureWithId(bi->top);
    }
    if (bi->bottom) {
      if (!sideCache->hasTexture(bi->bottom)) {
        bottom_tex = ImageUtil::createGLTexture(64, 64, is_flat,
              style->getSide(static_cast<unsigned int>(bi->bottom), 0, is_flat));
        sideCache->addTexture(bi->bottom, bottom_tex); 
      }
      else
        bottom_tex = sideCache->getTextureWithId(bi->bottom);
    }

    // handle flat/transparent case
    if (is_flat) {
      if (bi->lid) {
        jj = 0;
        if (bi->typeMap & 16384) {
          jj += 2;
        }
        if (bi->typeMap & 32768) {
          jj += 4;
        }
        if(drawTextured) {
          glBindTexture(GL_TEXTURE_2D, lid_tex);
          glBegin(GL_QUADS);
          glNormal3f(nx, ny, nz);
          for (int j=0; j < 4; j++) {
            GLTEX_HELPER;
            scene_rendered_vertices += 1;
            glVertex3f(SLOPE_RAW_DATA[which][0][j][0],
                SLOPE_RAW_DATA[which][0][j][1],
                SLOPE_RAW_DATA[which][0][j][2]);
          }
          glEnd();
        }
        // lines
        if (drawLines) {
          glDisable(GL_TEXTURE_2D);
          glBegin(GL_LINE_STRIP);
          for (int j=0; j < 4; j++) {
            glVertex3f(SLOPE_RAW_DATA[which][0][j][0],
                SLOPE_RAW_DATA[which][0][j][1],
                SLOPE_RAW_DATA[which][0][j][2]);
          }
          glVertex3f(SLOPE_RAW_DATA[which][0][0][0],
              SLOPE_RAW_DATA[which][0][0][1],
              SLOPE_RAW_DATA[which][0][0][2]);
          glEnd();
          glEnable(GL_TEXTURE_2D);
          // end-of-lines
        }
      }
#undef MSWAP
#define MSWAP(a, b) { sideTex1[a] = 1.0f - sideTex1[a]; sideTex1[b] = 1.0f - sideTex1[b]; }
      jj = 0; // only 'lid' rotated
      if (bi->top) {
        SLOPE_TEX_CP(1);
        if (bi->flipTopBottom()) {
          MSWAP(0, 2);
          MSWAP(4, 6);
        }
        if (drawTextured) {
          glBindTexture(GL_TEXTURE_2D, top_tex);
          glBegin(GL_QUADS);
          for (int j=0; j < 4; j++) {
            GLTEX1_HELPER;
            scene_rendered_vertices += 1;
            glVertex3f(SLOPE_RAW_DATA[which][2][j][0],
                SLOPE_RAW_DATA[which][2][j][1],
                SLOPE_RAW_DATA[which][2][j][2]);
          }
          jj = 0;
          SLOPE_TEX_CP(0);
          if (!bi->flipTopBottom()) {
            MSWAP(0, 2);
            MSWAP(4, 6);
          }
          for (int j=0; j < 4; j++) {
            GLTEX1_HELPER;
            scene_rendered_vertices += 1;
            glVertex3f(SLOPE_RAW_DATA[which][1][j][0],
                SLOPE_RAW_DATA[which][1][j][1],
                SLOPE_RAW_DATA[which][1][j][2]-1.001f);  // -1.001f
            // offset by .001 to fix NYC bridges; polygon z-fighting
          }
          glEnd();
        }
        // lines
        if (drawLines) {
          glDisable(GL_TEXTURE_2D);
          glBegin(GL_LINE_STRIP);
          for (int j=0; j < 4; j++) {
            glVertex3f(SLOPE_RAW_DATA[which][2][j][0],
                SLOPE_RAW_DATA[which][2][j][1],
                SLOPE_RAW_DATA[which][2][j][2]);
          }
          glVertex3f(SLOPE_RAW_DATA[which][2][0][0],
              SLOPE_RAW_DATA[which][2][0][1],
              SLOPE_RAW_DATA[which][2][0][2]);
          for (int j=0; j < 4; j++) {
            glVertex3f(SLOPE_RAW_DATA[which][1][j][0],
                SLOPE_RAW_DATA[which][1][j][1],
                SLOPE_RAW_DATA[which][1][j][2]-1.001f);
          }
          glVertex3f(SLOPE_RAW_DATA[which][1][0][0],
              SLOPE_RAW_DATA[which][1][0][1],
              SLOPE_RAW_DATA[which][1][0][2]-1.001f);
          glEnd();
          glEnable(GL_TEXTURE_2D);
          // end-of-lines
        }
      }
      jj = 0;
      //memcpy(sideTex1, sideTex1_bak, 8*sizeof(GLfloat));
      if (bi->left) {
        SLOPE_TEX_CP(2);
        if (bi->flipLeftRight()) {
           MSWAP(0, 2);
           MSWAP(4, 6);
        }
        if (drawTextured) {
          glBindTexture(GL_TEXTURE_2D, left_tex);
          glBegin(GL_QUADS);
          for (int j=0; j < 4; j++) {
            GLTEX1_HELPER;
            scene_rendered_vertices += 1;
            glVertex3f(SLOPE_RAW_DATA[which][3][j][0],
                SLOPE_RAW_DATA[which][3][j][1],
                SLOPE_RAW_DATA[which][3][j][2]);
          }
          jj = 0;
          SLOPE_TEX_CP(3);
          if (!bi->flipLeftRight()) {
            MSWAP(0, 2);
            MSWAP(4, 6);
          }
          for (int j=0; j < 4; j++) {
            GLTEX1_HELPER;
            scene_rendered_vertices += 1;
            glVertex3f(SLOPE_RAW_DATA[which][4][j][0]-1.001f,  // -1.001f
                SLOPE_RAW_DATA[which][4][j][1],
                SLOPE_RAW_DATA[which][4][j][2]);
          }
          glEnd();        
        }
        // lines
        if (drawLines) {
          glDisable(GL_TEXTURE_2D);
          glBegin(GL_LINE_STRIP);
          for (int j=0; j < 4; j++) {
            glVertex3f(SLOPE_RAW_DATA[which][3][j][0],
                SLOPE_RAW_DATA[which][3][j][1],
                SLOPE_RAW_DATA[which][3][j][2]);
          }
          glVertex3f(SLOPE_RAW_DATA[which][3][0][0],
              SLOPE_RAW_DATA[which][3][0][1],
              SLOPE_RAW_DATA[which][3][0][2]);
          for (int j=0; j < 4; j++) {
            glVertex3f(SLOPE_RAW_DATA[which][4][j][0]-1.001f,
                SLOPE_RAW_DATA[which][4][j][1],
                SLOPE_RAW_DATA[which][4][j][2]);
          }
          glVertex3f(SLOPE_RAW_DATA[which][4][0][0]-1.001f,
              SLOPE_RAW_DATA[which][4][0][1],
              SLOPE_RAW_DATA[which][4][0][2]);
          glEnd();
          glEnable(GL_TEXTURE_2D);
          // end-of-lines
        }
      }
    }
    else {
      if (bi->lid) {
        if (drawTextured) {
          glBindTexture(GL_TEXTURE_2D, lid_tex);
          jj = 0;
          if (bi->typeMap & 16384) {
            jj += 2;
          }
          if (bi->typeMap & 32768) {
            jj += 4;
          }
          glBegin(GL_QUADS);
          glNormal3f(nx, ny, nz);
          for (int j=0; j < 4; j++) {
            GLTEX_HELPER;
            scene_rendered_vertices += 1;
            glVertex3f(SLOPE_RAW_DATA[which][0][j][0],
                SLOPE_RAW_DATA[which][0][j][1],
                SLOPE_RAW_DATA[which][0][j][2]);
          }
          glEnd();
        }
        // lines
        if (drawLines) {
          glDisable(GL_TEXTURE_2D);
          glBegin(GL_LINE_STRIP);
          for (int j=0; j < 4; j++) {
            glVertex3f(SLOPE_RAW_DATA[which][0][j][0],
                SLOPE_RAW_DATA[which][0][j][1],
                SLOPE_RAW_DATA[which][0][j][2]);
          }
          glVertex3f(SLOPE_RAW_DATA[which][0][0][0],
              SLOPE_RAW_DATA[which][0][0][1],
              SLOPE_RAW_DATA[which][0][0][2]);
          glEnd();
          glEnable(GL_TEXTURE_2D);
          // end-of-lines
        }
      }
      jj = 0; // only 'lid' rotated
#undef MSWAP
#define MSWAP(a, b) { sideTex1[a] = 1.0f - sideTex1[a]; sideTex1[b] = 1.0f - sideTex1[b]; }
      /*
      if (bi->flipTopBottom()) {
        //MSWAP(1, 5);
        //MSWAP(3, 7);
        
        MSWAP(0, 2);
        MSWAP(4, 6);
      }*/
      if (bi->top && (which != 42)) {
        if (drawTextured) {
          glBindTexture(GL_TEXTURE_2D, top_tex);
          glBegin(GL_QUADS);
          SLOPE_TEX_CP(1);
          if (bi->flipTopBottom()) {
            MSWAP(0, 2);
            MSWAP(4, 6);
          }
          for (int j=0; j < 4; j++) {
            //if (!bi->flipTopBottom())
            GLTEX1_HELPER;
            /*
               glTexCoord2f(SLOPE_TEX_DATA[which][1][j][0],
               SLOPE_TEX_DATA[which][1][j][1]);
               */
            scene_rendered_vertices += 1;
            glVertex3f(SLOPE_RAW_DATA[which][2][j][0],
                SLOPE_RAW_DATA[which][2][j][1],
                SLOPE_RAW_DATA[which][2][j][2]);
          }
          glEnd();
        }
        // lines
        if (drawLines) {
          glDisable(GL_TEXTURE_2D);
          glBegin(GL_LINE_STRIP);
          for (int j=0; j < 4; j++) {
            glVertex3f(SLOPE_RAW_DATA[which][2][j][0],
                SLOPE_RAW_DATA[which][2][j][1],
                SLOPE_RAW_DATA[which][2][j][2]);
          }
          glVertex3f(SLOPE_RAW_DATA[which][2][0][0],
              SLOPE_RAW_DATA[which][2][0][1],
              SLOPE_RAW_DATA[which][2][0][2]);
          glEnd();
          glEnable(GL_TEXTURE_2D);
          // end-of-lines
        }
      }
      jj = 0;
      if (bi->bottom && (which != 41)) {
        if (drawTextured) {
          glBindTexture(GL_TEXTURE_2D, bottom_tex);
          glBegin(GL_QUADS);
          SLOPE_TEX_CP(0);
          if (bi->flipTopBottom()) {
            MSWAP(0, 2);
            MSWAP(4, 6);
          }

          for (int j=0; j < 4; j++) {
            //if (!bi->flipTopBottom())
            GLTEX1_HELPER;
            /*
               glTexCoord2f(SLOPE_TEX_DATA[which][0][j][0],
               SLOPE_TEX_DATA[which][0][j][1]);
               */
            scene_rendered_vertices += 1;
            glVertex3f(SLOPE_RAW_DATA[which][1][j][0],
                SLOPE_RAW_DATA[which][1][j][1],
                SLOPE_RAW_DATA[which][1][j][2]);
          }
          glEnd();
        }
        // lines
        if (drawLines) {
          glDisable(GL_TEXTURE_2D);
          glBegin(GL_LINE_STRIP);
          for (int j=0; j < 4; j++) {
            glVertex3f(SLOPE_RAW_DATA[which][1][j][0],
                SLOPE_RAW_DATA[which][1][j][1],
                SLOPE_RAW_DATA[which][1][j][2]);
          }
          glVertex3f(SLOPE_RAW_DATA[which][1][0][0],
              SLOPE_RAW_DATA[which][1][0][1],
              SLOPE_RAW_DATA[which][1][0][2]);
          glEnd();
          glEnable(GL_TEXTURE_2D);
          // end-of-lines
        }
      }
      /*memcpy(sideTex1, sideTex1_bak, 8 * sizeof(GLfloat));
      if (bi->flipLeftRight()) {
        MSWAP(0, 2);
        MSWAP(4, 6);
      }*/

      jj = 0;
      if (bi->left && (which != 44)) {
        if (drawTextured) {
          glBindTexture(GL_TEXTURE_2D, left_tex);
          glBegin(GL_QUADS);
          SLOPE_TEX_CP(2);
          if (bi->flipLeftRight()) {
            MSWAP(0, 2);
            MSWAP(4, 6);
          }

          for (int j=0; j < 4; j++) {
            //if (!bi->flipLeftRight())
            GLTEX1_HELPER;
            /*
               glTexCoord2f(SLOPE_TEX_DATA[which][2][j][0],
               SLOPE_TEX_DATA[which][2][j][1]);
               */
            scene_rendered_vertices += 1;
            glVertex3f(SLOPE_RAW_DATA[which][3][j][0],
                SLOPE_RAW_DATA[which][3][j][1],
                SLOPE_RAW_DATA[which][3][j][2]);
          }
          glEnd();
        }
        // lines
        if (drawLines) {
          glDisable(GL_TEXTURE_2D);
          glBegin(GL_LINE_STRIP);
          for (int j=0; j < 4; j++) {
            glVertex3f(SLOPE_RAW_DATA[which][3][j][0],
                SLOPE_RAW_DATA[which][3][j][1],
                SLOPE_RAW_DATA[which][3][j][2]);
          }
          glVertex3f(SLOPE_RAW_DATA[which][3][0][0],
              SLOPE_RAW_DATA[which][3][0][1],
              SLOPE_RAW_DATA[which][3][0][2]);
          glEnd();
          glEnable(GL_TEXTURE_2D);
          // end-of-lines
        }
      }
      jj = 0;
      if (bi->right && (which != 43)) {
        if (drawTextured) {
          glBindTexture(GL_TEXTURE_2D, right_tex);
          glBegin(GL_QUADS);
          SLOPE_TEX_CP(3);
          if (bi->flipLeftRight()) {
            MSWAP(0, 2);
            MSWAP(4, 6);
          }

          for (int j=0; j < 4; j++) {
            //if (!bi->flipLeftRight())
            GLTEX1_HELPER;
            /*
               glTexCoord2f(SLOPE_TEX_DATA[which][3][j][0],
               SLOPE_TEX_DATA[which][3][j][1]);
               */
            scene_rendered_vertices += 1;
            glVertex3f(SLOPE_RAW_DATA[which][4][j][0],
                SLOPE_RAW_DATA[which][4][j][1],
                SLOPE_RAW_DATA[which][4][j][2]);
          }
          glEnd();
        }
        // lines
        if (drawLines) {
          glDisable(GL_TEXTURE_2D);
          glBegin(GL_LINE_STRIP);
          for (int j=0; j < 4; j++) {
            glVertex3f(SLOPE_RAW_DATA[which][4][j][0],
                SLOPE_RAW_DATA[which][4][j][1],
                SLOPE_RAW_DATA[which][4][j][2]);
          }
          glVertex3f(SLOPE_RAW_DATA[which][4][0][0],
              SLOPE_RAW_DATA[which][4][0][1],
              SLOPE_RAW_DATA[which][4][0][2]);
          glEnd();
          glEnable(GL_TEXTURE_2D);
          // end-of-lines
        }
      }


#if 0
      
      for (int i = 0; i < 5; ++i) { // RESEARCH ME: is this correct? needed above as well?
        if ((which == 41) && (i == 1))
          continue;
        if ((which == 42) && (i == 2))
          continue;
        if ((which == 43) && (i == 4))
          continue;
        if ((which == 44) && (i == 3))
          continue;

        jj = 0;
        if (i == 0)
          if (!bi->lid)
            continue;
          else {
            glBindTexture(GL_TEXTURE_2D, lidCache->getTextureWithId(bi->lid));
            if (bi->typeMap & 16384) {
              jj += 2;
            }
            if (bi->typeMap & 32768) {
              jj += 4;
            }
        }

        if (i == 2)
          if (!bi->top)
            continue;
          else
            glBindTexture(GL_TEXTURE_2D, sideCache->getTextureWithId(bi->top));
        if (i == 1)
          if (!bi->bottom)
            continue;
          else
            glBindTexture(GL_TEXTURE_2D, sideCache->getTextureWithId(bi->bottom));
        if (i == 4)
          if (!bi->right)
            continue;
          else
            glBindTexture(GL_TEXTURE_2D, sideCache->getTextureWithId(bi->right));
        if (i == 3)
          if (!bi->left)
            continue;
          else
            glBindTexture(GL_TEXTURE_2D, sideCache->getTextureWithId(bi->left));

        glBegin(GL_QUADS);
        // FIXME: normals are completely fucked up
        switch(i) {
          case 0:
            glNormal3f(nx, ny, nz);
            break;
          case 1:
            glNormal3f(0.0f, 0.0f, 1.0f);
            break;
          case 2:
            glNormal3f(0.0f, 0.0f, -1.0f);
            break;
          case 3:
            glNormal3f(-1.0f, 0.0f, 0.0f);
            break;
          case 4:
            glNormal3f(1.0f, 0.0f, 0.0f);
            break;
        }
        for (int j=0; j < 4; j++) {
          GLTEX_HELPER;
          scene_rendered_vertices += 3;
          glVertex3f(SLOPE_RAW_DATA[which][i][j][0],
              SLOPE_RAW_DATA[which][i][j][1],
              SLOPE_RAW_DATA[which][i][j][2]);
        }
        glEnd();
      }
#endif
    } // else

    if (!drawHeadingMarkers)
      return;
    float offset_correction = 0.1f;
    if (which == 0)
      offset_correction = -0.9f;
    //glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
#define HEIGHT_VERTEX(a, b, c) glVertex3f(a, offset_correction + slope_height_offset(which, a, c), c)
    if (bi->downOk()) {
      glBegin(GL_LINES);
      HEIGHT_VERTEX(0.5f, 0.1f, 0.2f);
      HEIGHT_VERTEX(0.5f, 0.1f, 0.8f);
      HEIGHT_VERTEX(0.45f, 0.1f, 0.7f);
      HEIGHT_VERTEX(0.5f, 0.1f, 0.8f);
      HEIGHT_VERTEX(0.55f, 0.1f, 0.7f);
      HEIGHT_VERTEX(0.5f, 0.1f, 0.8f);
      glEnd();
    }
    if (bi->upOk()) {
      glBegin(GL_LINES);
      HEIGHT_VERTEX(0.5f, 0.1f, 0.8f);
      HEIGHT_VERTEX(0.5f, 0.1f, 0.2f);
      HEIGHT_VERTEX(0.45f, 0.1f, 0.3f);
      HEIGHT_VERTEX(0.5f, 0.1f, 0.2f);
      HEIGHT_VERTEX(0.55f, 0.1f, 0.3f);
      HEIGHT_VERTEX(0.5f, 0.1f, 0.2f);
      glEnd();
    }
    if (bi->leftOk()) {
      glBegin(GL_LINES);
      HEIGHT_VERTEX(0.2f, 0.1f, 0.5f);
      HEIGHT_VERTEX(0.8f, 0.1f, 0.5f);
      HEIGHT_VERTEX(0.3f, 0.1f, 0.45f);
      HEIGHT_VERTEX(0.2f, 0.1f, 0.5f);
      HEIGHT_VERTEX(0.3f, 0.1f, 0.55f);
      HEIGHT_VERTEX(0.2f, 0.1f, 0.5f);
      glEnd();
    }
    if (bi->rightOk()) {
      glBegin(GL_LINES);
      HEIGHT_VERTEX(0.8f, 0.1f, 0.5f);
      HEIGHT_VERTEX(0.2f, 0.1f, 0.5f);
      HEIGHT_VERTEX(0.7f, 0.1f, 0.45f);
      HEIGHT_VERTEX(0.8f, 0.1f, 0.5f);
      HEIGHT_VERTEX(0.7f, 0.1f, 0.55f);
      HEIGHT_VERTEX(0.8f, 0.1f, 0.5f);
      glEnd();
    }
    glEnable(GL_TEXTURE_2D);
    // block lid normals
#if 0
#define NORMAL_POS(a, b) glVertex3f(a, slope_height_offset(which, a, b), b)
#define NORMAL_POS2(a, b) glVertex3f(a + nx, slope_height_offset(which, a, b) + ny, b + nz)
    glBegin(GL_LINES);
    if (bi->lid) {
      NORMAL_POS(0.5f, 0.5f);
      NORMAL_POS2(0.5f, 0.5f);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
#endif
    
    GL_CHECKERROR;
  }
}