#ifndef FLOOR_H
#define FLOOR_H

#include <Camera.h>
#include <Colour.h>
#include <TransformStack.h>
#include <Vector.h>
#include <string>

class Floor {
public:
  Floor();
  Floor(float width, float length, int pWidth, int pLength, Vector position);
  
  void setPosition(Vector position);
  void setColor(Colour color);
  void setFloorWireframe(bool setEnable);
  const Vector &getPosition();
  
  void draw(const std::string &_shaderName, TransformStack &_transform, Camera *_cam) const;

  Vector m_position;
  bool m_floorWireframe;

private:
  float m_width;
  float m_length;
  Colour m_color;
};

#endif
