#ifndef SPHEREOBSTACLE_H
#define SPHEREOBSTACLE_H
#include <Camera.h>
#include <Colour.h>
#include <ShaderLib.h>
#include <TransformStack.h>
#include <Vector.h>
#include <vector>
// #include <glad/gl.h>

/// @file sphereobstacle.h
/// @brief the obstacle class. We create the sphere that will collide with the
/// cloth.
/// @author Dionysios Toufexis
/// @version 1.0
/// @date 23/07/12
/// Revision History :
/// @class SphereObstacle
/// @brief  we create the obstacle sphere and the variables to control the
/// sphere in the gui.

class SphereObstacle {
public:
  /// @brief our ctor
  SphereObstacle();
  //---------------------------------------------------------------------------------------------
  /// @brief our dtor
  ~SphereObstacle();
  //---------------------------------------------------------------------------------------------
  /// @brief a variable to store the sphere radius.
  float m_obstRadius;
  //---------------------------------------------------------------------------------------------
  /// @brief a method to get the sphere radius.
  float getRadius();
  //---------------------------------------------------------------------------------------------
  /// @brief Get effective radius at a specific point (accounts for deformation)
  /// @param worldPos the world position to check
  /// @return the deformed radius at that angle
  float getDeformedRadius(const Vector& worldPos) const;
  //---------------------------------------------------------------------------------------------
  /// @brief variable to store the previous position of the sphere. Needed for
  /// the verlet intergration.
  Vector m_obstPosition;
  //---------------------------------------------------------------------------------------------
  /// @brief a method to get the position of the sphere.
  Vector getPosition();
  //---------------------------------------------------------------------------------------------
  /// @brief a method to set the position of the sphere.
  /// @param[in] x the x axis position of the sphere.
  /// @param[in] y the y axis position of the sphere.
  /// @param[in] x the z axis position of the sphere.
  /// @param[in] m_obstPosition a variable to store a vector with the x,y,z
  /// coords.
  void setPosition(float x, float y, float z) {
    m_obstPosition = Vector(x, y, z);
  }
  //---------------------------------------------------------------------------------------------
  /// @brief a method to set the position of the sphere.
  /// @param[in] position a vector position.
  void setPosition(Vector position) { m_obstPosition = position; }
  //---------------------------------------------------------------------------------------------
  /// @brief variable to store the color of the sphere.
  Colour m_colour;
  //---------------------------------------------------------------------------------------------
  /// @brief the draw method of the obstacle sphere.
  void draw(const std::string &_shaderName, TransformStack &_transform,
            Camera *_cam) const;
  //---------------------------------------------------------------------------------------------
  /// @brief Render sphere geometry only (for shadow pass, no shader setup)
  void renderGeometryOnly() const;
  //---------------------------------------------------------------------------------------------
  /// @brief a variable to store the value for the wireframe option.
  bool m_sphereWireframe;
  //---------------------------------------------------------------------------------------------
  /// @brief a method to set the wireframe option.
  void setObstacleWireframe(bool setEnable);
  //---------------------------------------------------------------------------------------------
  /// @brief Update deformation animation (call each frame)
  void updateDeformation(float deltaTime);
  //---------------------------------------------------------------------------------------------
  /// @brief Deformation noise parameters
  bool m_deformationEnabled;
  float m_deformationStrength;  // How much to deform (0-1 as fraction of radius)
  float m_deformationFrequency; // Spatial frequency of noise
  float m_deformationSpeed;     // Animation speed
  float m_deformationTime;      // Current animation time
  int m_deformationOctaves;     // Noise detail layers

private:
  void generateDeformedSphere();
  //---------------------------------------------------------------------------------------------
  /// @brief Noise function for deformation
  float noiseFunction(float x, float y, float z) const;
  //---------------------------------------------------------------------------------------------
  /// @brief Vertex data for deformed sphere
  std::vector<float> m_deformedVertices;
  std::vector<float> m_deformedNormals;
  std::vector<unsigned int> m_indices;
  unsigned int m_vao, m_vbo, m_nbo, m_ebo;
  int m_sphereSegments;
  bool m_bufferInitialized;
};

#endif // SPHEREOBSTACLE_H
