#ifndef GLES3_E3_0_INSTANCINGSIMPLE_E3_0_INSTANCINGSIMPLE_HPP
#define GLES3_E3_0_INSTANCINGSIMPLE_E3_0_INSTANCINGSIMPLE_HPP
/*
 * OpenGL ES 3.0 Tutorial 3
 *
 * Draws n number of cubes using instanced draw calls.
 */

#include <FslDemoApp/OpenGLES3/DemoAppGLES3.hpp>
#include <FslUtil/OpenGLES3/GLProgram.hpp>

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NUM_INSTANCES 100

namespace Fsl
{
  // NOLINTNEXTLINE(readability-identifier-naming)
  class E3_0_InstancingSimple : public DemoAppGLES3
  {
    struct UserData
    {
      GLuint PositionVbo{GLES3::GLValues::InvalidHandle};
      GLuint ColorVbo{GLES3::GLValues::InvalidHandle};
      GLuint MvpVbo{GLES3::GLValues::InvalidHandle};
      GLuint IndicesIbo{GLES3::GLValues::InvalidHandle};

      // Number of indices
      int NumIndices{0};

      // Rotation angle
      GLfloat Angle[NUM_INSTANCES]{};    // NOLINT(modernize-avoid-c-arrays)

      UserData()

      {
        for (float& rAngle : Angle)
        {
          rAngle = 0;
        }
      }
    };

    GLES3::GLProgram m_program;
    UserData m_userData;

  public:
    explicit E3_0_InstancingSimple(const DemoAppConfig& config);
    ~E3_0_InstancingSimple() override;

  protected:
    void Update(const DemoTime& demoTime) override;
    void Draw(const FrameInfo& frameInfo) override;

  private:
    void Cleanup();
  };
}

#endif
